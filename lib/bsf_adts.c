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
#include <adts_header.h>

#include <libavcodec/avcodec.h>
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(58, 91, 100)
# include <libavcodec/bsf.h>
#endif

#define LOG_DOMAIN "bsf_adts"

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

static void
cleanup_adts(bgav_bsf_t * bsf)
  {

  }

int
bgav_bsf_init_adts(bgav_bsf_t * bsf)
  {
  int ret = 0;
  bgav_packet_t * p = NULL;
  AVBSFContext * ctx;
  const uint8_t * extradata;
  int extradata_size = 0;
  AVPacket pkt;
  
  const AVBitStreamFilter *filter;
  
  if(!(filter = av_bsf_get_by_name("aac_adtstoasc")))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Bitstream filter aac_adtstoasc not found");
    goto fail;
    }
    
  av_bsf_alloc(filter, &ctx);
  
  /* Set codec parameters */
  //  ctx.
  
  /* Fire up a bitstream filter for getting the extradata.
     We'll do the rest by ourselfes */
  
  ctx->par_in->codec_id = AV_CODEC_ID_AAC;
  //  ctx->par_in->
  
  if(av_bsf_init(ctx))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "av_bsf_init failed");
    goto fail;
    }
  /* Get a first packet to obtain the extradata */

  if(bsf->src.peek_func(bsf->src.data, &p, 1) != GAVL_SOURCE_OK)
    goto fail;

  /* Send packet to the filter. Afterward we should have the
     extradata set */

  av_init_packet(&pkt);

  av_new_packet(&pkt, p->buf.len);
  memcpy(pkt.data, p->buf.buf, p->buf.len);
  
  av_bsf_send_packet(ctx, &pkt);

  /* Receive packets */
  while(1)
    {
    if(av_bsf_receive_packet(ctx, &pkt))
      break;

    if((extradata = av_packet_get_side_data(&pkt, AV_PKT_DATA_NEW_EXTRADATA,
                                            &extradata_size)))
      {
      fprintf(stderr, "Got extradata %d bytes\n", extradata_size);
      gavl_hexdump(extradata, extradata_size, 16);
      break;
      }
    
    }
  
  gavl_compression_info_set_global_header(&bsf->ci, extradata, extradata_size);
  
  //  fprintf(stderr, "
  
  //  bgav_packet_dump(p);
  
  
  bsf->filter = filter_adts;
  bsf->cleanup = cleanup_adts;
  
  ret = 1;
  fail:
  
  if(ctx)
    av_bsf_free(&ctx);
  
  return ret;
  }
