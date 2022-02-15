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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <avdec_private.h>
#include <codecs.h>

#include <vpx/vpx_decoder.h>
#include <vpx/vpx_codec.h>
#include <vpx/vp8dx.h>

#define LOG_DOMAIN "vpx"

// #define DUMP_DECODE

typedef struct
  {
  vpx_codec_ctx_t decoder;
  gavl_video_frame_t * frame;
  } vpx_t;

static int create_decoder(bgav_stream_t * s)
  {
  vpx_t * priv;
  struct vpx_codec_dec_cfg deccfg;
  const struct vpx_codec_iface *iface;

  memset(&deccfg, 0, sizeof(deccfg));
  deccfg.threads = s->opt->threads;
  
  priv = s->data.video.decoder->priv;
  
  iface = &vpx_codec_vp8_dx_algo;

  if(vpx_codec_dec_init(&priv->decoder, iface, &deccfg, 0) != VPX_CODEC_OK)
    {
    const char *error = vpx_codec_error(&priv->decoder);
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Failed to initialize decoder: %s",
             error);
    return 0;
    }
  return 1; 
  }


static int init_vpx(bgav_stream_t * s)
  {
  vpx_t * priv;
  
  priv = calloc(1, sizeof(*priv));
  s->data.video.decoder->priv = priv;

  if(!create_decoder(s))
    return 0;
  
  s->data.video.format.pixelformat = GAVL_YUV_420_P;
  priv->frame = gavl_video_frame_create(NULL);
  s->description = bgav_sprintf("VP8");
  return 1;
  }


static int decode_vpx(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  vpx_t * priv;
  bgav_packet_t * p;
  vpx_image_t *img;
  const void * iter = NULL;
  
  priv = s->data.video.decoder->priv;

  /* We assume one frame per packet */
  
  p = bgav_stream_get_packet_read(s);
  if(!p)
    return 0;

#ifdef DUMP_DECODE
  bgav_packet_dump(p);
#endif
  
  if(vpx_codec_decode(&priv->decoder, p->data, p->data_size, NULL, 0) !=
     VPX_CODEC_OK)
    {
    const char *error  = vpx_codec_error(&priv->decoder);
    const char *detail = vpx_codec_error_detail(&priv->decoder);
    
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Failed to decode frame: %s", error);
    if (detail)
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "  Additional information: %s\n",
               detail);
    return 0;
    }

  img = vpx_codec_get_frame(&priv->decoder, &iter);

  if(!img)
    return 0;
  
  /* Skip frame */
  if(f)
    {
    priv->frame->planes[0] = img->planes[0];
    priv->frame->planes[1] = img->planes[1];
    priv->frame->planes[2] = img->planes[2];
    
    priv->frame->strides[0] = img->stride[0];
    priv->frame->strides[1] = img->stride[1];
    priv->frame->strides[2] = img->stride[2];
    
    gavl_video_frame_copy(&s->data.video.format,
                          f, priv->frame);
    
    f->timestamp = p->pts;
    f->duration = p->duration;
    }

  //  vpx_img_free (img);

  /* Should never happen */
  while((img = vpx_codec_get_frame(&priv->decoder, &iter)))
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "Ignoring additional frame\n");
    }
  
  bgav_stream_done_packet_read(s, p);
  return 1;
  }


static void close_vpx(bgav_stream_t * s)
  {
  vpx_t * priv;
  priv = s->data.video.decoder->priv;

  gavl_video_frame_null(priv->frame);
  gavl_video_frame_destroy(priv->frame);
  
  
  vpx_codec_destroy(&priv->decoder);

  free(priv);
  }

static void resync_vpx(bgav_stream_t * s)
  {
#if 0
  vpx_t * priv;
  priv = s->data.video.decoder->priv;
  /* Recreate decoder */
  vpx_codec_destroy(&priv->decoder);
  create_decoder(s);
#endif
  //  fprintf(stderr, "resync_vpx");
  }

static bgav_video_decoder_t vpx_decoder =
  {
    .name =   "VP8 video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('V', 'P', '8', '0'), 0x00  },
    .init =   init_vpx,
    .decode = decode_vpx,
    .resync = resync_vpx,
    .close =  close_vpx,
  };

void bgav_init_video_decoders_vpx()
  {
  bgav_video_decoder_register(&vpx_decoder);
  }
