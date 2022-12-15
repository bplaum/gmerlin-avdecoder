/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
 *
 * Copyright (c) 2001 - 2012 Members of the Gmerlin project
 * gmerlin-general@lists.sourceforge.net
 * http://gmerlin.sourceforge.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * *****************************************************************/

#include <stdlib.h>
#include <string.h>


#include <config.h>
#include <avdec_private.h>
#include <bsf.h>
// #include <bsf_private.h>
#include <h264_header.h>

static const uint8_t nal_header[4] = { 0x00, 0x00, 0x00, 0x01 };

typedef struct
  {
  int nal_size_length;
  int have_header;
  } avcc_t;

static void append_data(bgav_packet_t * p, uint8_t * data, int len,
                        int header_len)
  {
  switch(header_len)
    {
    case 3:
      gavl_buffer_append_data(&p->buf, &nal_header[1], 3);
      break;
    case 4:
      gavl_buffer_append_data(&p->buf, nal_header, 4);
      break;
    }
  
  gavl_buffer_append_data_pad(&p->buf, data, len, GAVL_PACKET_PADDING);
  }


static void
filter_avcc(bgav_packet_filter_t* bsf, bgav_packet_t * in, bgav_packet_t * out)
  {
  uint8_t * ptr, *end;
  int len = 0;
  int nals_sent = 0;
  avcc_t * priv = bsf->priv;
  int unit_type;
  
  ptr = in->buf.buf;
  end = in->buf.buf + in->buf.len;

  out->buf.len = 0;
    
  while(ptr < end - priv->nal_size_length)
    {
    switch(priv->nal_size_length)
      {
      case 1:
        len = *ptr;
        ptr++;
        break;
      case 2:
        len = GAVL_PTR_2_16BE(ptr);
        ptr += 2;
        break;
      case 4:
        len = GAVL_PTR_2_32BE(ptr);
        ptr += 4;
        break;
      default:
        break;
      }

    unit_type = ptr[0] & 0x1f;
    if((unit_type != H264_NAL_SPS) && (unit_type != H264_NAL_PPS))
      {
      append_data(out, ptr, len, nals_sent ? 3 : 4);
      nals_sent++;
      }
    ptr += len;
    }
  }

static void
cleanup_avcc(bgav_packet_filter_t * bsf)
  {
  free(bsf->priv);
  }

static void append_extradata(gavl_buffer_t * buf, uint8_t * data, int len)
  {
  gavl_buffer_append_data(buf, nal_header, 4);
  gavl_buffer_append_data_pad(buf, data, len, GAVL_PACKET_PADDING);
  }

static void convert_header(bgav_packet_filter_t * f)
  {
  uint8_t * ptr;
  int num_units;
  int i;
  int len;

  gavl_dictionary_t * s;
  const gavl_dictionary_t * s_orig;

  gavl_compression_info_t ci_orig;
  gavl_compression_info_t ci;
  avcc_t * priv = f->priv;
  
  gavl_compression_info_init(&ci_orig);
  gavl_compression_info_init(&ci);
  
  s      = gavl_packet_source_get_stream_nc(f->src);
  s_orig = gavl_packet_source_get_stream(f->prev);
                                   
  gavl_stream_get_compression_info(s, &ci);
  gavl_stream_get_compression_info(s_orig, &ci_orig);

  gavl_buffer_reset(&ci.codec_header);

  /* Parse extradata */
  ptr = ci_orig.codec_header.buf;
  
  ptr += 4; // Version, profile, profile compat, level
  priv->nal_size_length = (*ptr & 0x3) + 1;
  ptr++;

  /* SPS */
  num_units = *ptr & 0x1f; ptr++;
  for(i = 0; i < num_units; i++)
    {
    len = GAVL_PTR_2_16BE(ptr); ptr += 2;
    append_extradata(&ci.codec_header, ptr, len);
    ptr += len;
    }

  /* PPS */
  num_units = *ptr; ptr++;
  for(i = 0; i < num_units; i++)
    {
    len = GAVL_PTR_2_16BE(ptr); ptr += 2;
    append_extradata(&ci.codec_header, ptr, len);
    ptr += len;
    }

  ci.id = GAVL_CODEC_ID_H264;
  
  gavl_stream_set_compression_info(s, &ci);
  gavl_compression_info_free(&ci);
  gavl_compression_info_free(&ci_orig);
  }


static gavl_source_status_t source_func_avcc(void * priv, gavl_packet_t ** p)
  {
  avcc_t * avcc;
  bgav_packet_filter_t * bsf;
  gavl_packet_t * in_pkt = NULL;

  gavl_source_status_t st;

  bsf = priv;

  avcc = bsf->priv;

  if(!avcc->have_header)
    {
    convert_header(bsf);
    avcc->have_header = 1;
    }
  
  if((st = gavl_packet_source_read_packet(bsf->prev, &in_pkt)) != GAVL_SOURCE_OK)
    return st;

  filter_avcc(bsf, in_pkt, *p);

  (*p)->flags      = in_pkt->flags;
  (*p)->pts        = in_pkt->pts;
  (*p)->duration   = in_pkt->duration;
  
  return st;
  }


int
bgav_packet_filter_init_avcC(bgav_packet_filter_t * bsf)
  {
  avcc_t * priv;
  
  //  bsf->filter = filter_avcc;
  
  bsf->cleanup = cleanup_avcc;
  bsf->source_func = source_func_avcc;
  
  priv = calloc(1, sizeof(*priv));
  bsf->priv = priv;
  return 1;
  }
