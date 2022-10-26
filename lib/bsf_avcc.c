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
#include <bsf_private.h>
#include <h264_header.h>

static const uint8_t nal_header[4] = { 0x00, 0x00, 0x00, 0x01 };

typedef struct
  {
  int nal_size_length;
  } avcc_t;

static void append_data(bgav_packet_t * p, uint8_t * data, int len,
                        int header_len)
  {
  switch(header_len)
    {
    case 3:
      bgav_packet_alloc(p, p->buf.len + 3 + len);
      memcpy(p->buf.buf + p->buf.len, &nal_header[1], 3);
      p->buf.len += 3;
      break;
    case 4:
      bgav_packet_alloc(p, p->buf.len + 4 + len);
      memcpy(p->buf.buf + p->buf.len, nal_header, 4);
      p->buf.len += 4;
      break;
    }
  memcpy(p->buf.buf + p->buf.len, data, len);
  p->buf.len += len;
  }

static void
filter_avcc(bgav_bsf_t* bsf, bgav_packet_t * in, bgav_packet_t * out)
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
cleanup_avcc(bgav_bsf_t * bsf)
  {
  free(bsf->priv);
  }

static void append_extradata(bgav_bsf_t * bsf, uint8_t * data,
                             int len)
  {
  gavl_compression_info_append_global_header(&bsf->ci, nal_header, 4);
  gavl_compression_info_append_global_header(&bsf->ci, data, len);
  }

int
bgav_bsf_init_avcC(bgav_bsf_t * bsf)
  {
  uint8_t * ptr;
  avcc_t * priv;
  int num_units;
  int i;
  int len;
  
  bsf->filter = filter_avcc;
  bsf->cleanup = cleanup_avcc;
  priv = calloc(1, sizeof(*priv));
  bsf->priv = priv;
  
  memcpy(&bsf->ci, bsf->s->ci, sizeof(bsf->ci));
  gavl_buffer_init(&bsf->ci.codec_header);
  
  /* Parse extradata */
  ptr = bsf->s->ci->codec_header.buf;
  
  ptr += 4; // Version, profile, profile compat, level
  priv->nal_size_length = (*ptr & 0x3) + 1;
  ptr++;

  /* SPS */
  num_units = *ptr & 0x1f; ptr++;
  for(i = 0; i < num_units; i++)
    {
    len = GAVL_PTR_2_16BE(ptr); ptr += 2;
    append_extradata(bsf, ptr, len);
    ptr += len;
    }

  /* PPS */
  num_units = *ptr; ptr++;
  for(i = 0; i < num_units; i++)
    {
    len = GAVL_PTR_2_16BE(ptr); ptr += 2;
    append_extradata(bsf, ptr, len);
    ptr += len;
    }
  bsf->ci.id = GAVL_CODEC_ID_H264;
  return 1;
  }
