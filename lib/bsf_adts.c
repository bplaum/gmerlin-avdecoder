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
#include <adts_header.h>

#include <libavcodec/avcodec.h>
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(58, 91, 100)
# include <libavcodec/bsf.h>
#endif

#define LOG_DOMAIN "bsf_adts"

typedef struct
  {
  AVBSFContext * ctx;
  AVPacket * in_pkt;
  AVPacket * out_pkt;
  
  gavl_packet_t * in_pkt_g;
  gavl_packet_t out_pkt_g;
  int got_header;

  int64_t pts;
  
  } adts_t;

#if 0
static void
filter_adts(bgav_bsf_t* bsf, bgav_packet_t * in, bgav_packet_t * out)
  {
  bgav_adts_header_t h;
  int header_len = 7;
  
  if((in->buf.len < 7) || !bgav_adts_header_read(in->buf.buf, &h))
    {
    out->buf.len = 0;
    return;
    }

  if(!h.protection_absent)
    header_len += 2;

  bgav_packet_alloc(out, in->buf.len - header_len);
  memcpy(out->buf.buf, in->buf.buf + header_len, in->buf.len - header_len);
  out->buf.len = in->buf.len - header_len;
  }
#endif

static void
cleanup_adts(bgav_packet_filter_t * f)
  {
  adts_t * adts = f->priv;

  av_packet_free(&adts->in_pkt);
  av_packet_free(&adts->out_pkt);
  av_bsf_free(&adts->ctx);
  free(adts);
  }

static void reset_adts(bgav_packet_filter_t * f)
  {
  adts_t * adts = f->priv;
  av_bsf_flush(adts->ctx);	
  }

static gavl_source_status_t source_func_adts(void * priv, gavl_packet_t ** p)
  {
  gavl_source_status_t st;
  int err;
  adts_t * adts;
  
  bgav_packet_filter_t * f = priv;
  
  adts = f->priv;

  while(1)
    {
    if(adts->out_pkt->size)
      av_packet_unref(adts->out_pkt);
    
    err = av_bsf_receive_packet(adts->ctx, adts->out_pkt);
    
    if(!err)
      break;
    else if(err == AVERROR(EAGAIN))
      {
      /* Send packet */
      adts->in_pkt_g = NULL;
      
      st = gavl_packet_source_read_packet(f->prev, &adts->in_pkt_g);

      if(st != GAVL_SOURCE_OK)
        return st;
      
      adts->in_pkt->data = adts->in_pkt_g->buf.buf;
      adts->in_pkt->size = adts->in_pkt_g->buf.len;
      av_bsf_send_packet(adts->ctx, adts->in_pkt);
      
      if(adts->pts == GAVL_TIME_UNDEFINED)
        adts->pts = adts->in_pkt_g->pts;
      }
    else
      {
      return GAVL_SOURCE_EOF;
      }
    }

  /* Extract extradata */
  if(!adts->got_header)
    {
    gavl_dictionary_t * s;
    const uint8_t * extradata;
    int extradata_size = 0;
    
    s = gavl_packet_source_get_stream_nc(f->src);
    
    //    gavl_stream_set_compression_tag

    if((extradata = av_packet_get_side_data(adts->out_pkt,
                                            AV_PKT_DATA_NEW_EXTRADATA,
                                            &extradata_size)))
      {
      gavl_compression_info_t ci;
      gavl_compression_info_init(&ci);
      
      gavl_dprintf("Got extradata %d bytes\n", extradata_size);
      gavl_hexdump(extradata, extradata_size, 16);
      
      gavl_stream_get_compression_info(s, &ci);
      gavl_buffer_append_data_pad(&ci.codec_header, extradata, extradata_size, GAVL_PACKET_PADDING);
      ci.id = GAVL_CODEC_ID_AAC;
      gavl_stream_set_compression_info(s, &ci);
      gavl_compression_info_free(&ci);
      }
    else
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Got no codec header");
      return GAVL_SOURCE_EOF;
      }
    adts->got_header = 1;
    }
  
  /* ffmpeg -> gavl */
  adts->out_pkt_g.buf.buf = adts->out_pkt->data;
  adts->out_pkt_g.buf.len = adts->out_pkt->size;

  adts->out_pkt_g.pts      = adts->pts;
  adts->out_pkt_g.duration = 1024;
  adts->pts += adts->out_pkt_g.duration;
  
  *p = &adts->out_pkt_g;
  return GAVL_SOURCE_OK;
  }

int
bgav_packet_filter_init_adts(bgav_packet_filter_t * bsf)
  {
  int ret = 0;
  const AVBitStreamFilter *filter;

  adts_t * priv;
  
  if(!(filter = av_bsf_get_by_name("aac_adtstoasc")))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Bitstream filter aac_adtstoasc not found");
    goto fail;
    }

  priv = calloc(1, sizeof(*priv));

  bsf->priv = priv;
  av_bsf_alloc(filter, &priv->ctx);
  
  priv->in_pkt = av_packet_alloc();
  priv->out_pkt = av_packet_alloc();
  priv->pts = GAVL_TIME_UNDEFINED;
  /* Set codec parameters */
  //  ctx.
  
  /* Fire up a bitstream filter for getting the extradata.
     We'll do the rest by ourselfes */
  
  priv->ctx->par_in->codec_id = AV_CODEC_ID_AAC;
  //  ctx->par_in->
  
  if(av_bsf_init(priv->ctx))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "av_bsf_init failed");
    goto fail;
    }
  
  bsf->reset = reset_adts;
  bsf->source_func = source_func_adts;
  bsf->cleanup = cleanup_adts;
  bsf->src_flags = GAVL_SOURCE_SRC_ALLOC;
  
  ret = 1;
  

  fail:
  
  return ret;
  }
