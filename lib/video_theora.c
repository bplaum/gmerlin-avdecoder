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

#include <avdec_private.h>
#include <codecs.h>

#include <theora/theoradec.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG_DOMAIN "video_theora"

typedef struct
  {
  th_info    ti;
  th_comment tc;
  th_setup_info *ts;

  th_dec_ctx * ctx;
  
  gavl_video_frame_t * frame;  

  /* For setting up the video frame */

  int offset_x;
  int offset_y;

  int offset_x_uv;
  int offset_y_uv;

  } theora_priv_t;
  
static int init_theora(bgav_stream_t * s)
  {
  int sub_h, sub_v;
  int i;
  int len;
  ogg_packet op;
  theora_priv_t * priv;
  priv = calloc(1, sizeof(*priv));
  s->decoder_priv = priv;

  /* Initialize theora structures */
  th_info_init(&priv->ti);
  th_comment_init(&priv->tc);

  /* Get header packets */
  if(!s->ci->codec_header.len)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Theora codec requires extradata");
    return 0;
    }
  
  memset(&op, 0, sizeof(op));
  op.b_o_s = 1;

  for(i = 0; i < 3; i++)
    {
    if(i)
      op.b_o_s = 0;

    op.packet = gavl_extract_xiph_header(&s->ci->codec_header,
                                         i, &len);
    op.bytes = len;
    
    if(!op.packet ||
       th_decode_headerin(&priv->ti, &priv->tc, &priv->ts, &op) <= 0)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Theora header broken");
      return 0;
      }
    op.packetno++;
    }
  
  /* Initialize the decoder */

  priv->ctx = th_decode_alloc(&priv->ti, priv->ts);

  /* Set postprocessing level */
  
  if(s->opt->pp_level > 0.0)
    {
    int level, max_level = 0;
    
    th_decode_ctl(priv->ctx,
                  TH_DECCTL_GET_PPLEVEL_MAX,
                  &max_level,
                  sizeof(max_level));

    if(max_level)
      {
      level = (int)((float)max_level * s->opt->pp_level + 0.5);
      if(level > max_level)
        level = max_level;
      th_decode_ctl(priv->ctx,
                    TH_DECCTL_SET_PPLEVEL,
                    &level,
                    sizeof(level));
      }
    }
  
  /* Get format */

  s->data.video.format->image_width  = priv->ti.pic_width;
  s->data.video.format->image_height = priv->ti.pic_height;

  s->data.video.format->frame_width  = priv->ti.frame_width;
  s->data.video.format->frame_height = priv->ti.frame_height;
  
  if(!priv->ti.aspect_numerator || !priv->ti.aspect_denominator)
    {
    s->data.video.format->pixel_width  = 1;
    s->data.video.format->pixel_height = 1;
    }
  else
    {
    s->data.video.format->pixel_width  = priv->ti.aspect_numerator;
    s->data.video.format->pixel_height = priv->ti.aspect_denominator;
    }

  if(!s->data.video.format->timescale)
    {
    s->data.video.format->timescale      = priv->ti.fps_numerator;
    s->data.video.format->frame_duration = priv->ti.fps_denominator;
    }
  
  switch(priv->ti.pixel_fmt)
    {
    case TH_PF_420:
      s->data.video.format->pixelformat = GAVL_YUV_420_P;
      break;
    case TH_PF_422:
      s->data.video.format->pixelformat = GAVL_YUV_422_P;
      break;
    case TH_PF_444:
      s->data.video.format->pixelformat = GAVL_YUV_444_P;
      break;
    default:
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Unknown pixelformat %d",
              priv->ti.pixel_fmt);
      return 0;
    }

  /* Get offsets */

  gavl_pixelformat_chroma_sub(s->data.video.format->pixelformat,
                              &sub_h, &sub_v);
  
  priv->offset_x = priv->ti.pic_x;
  priv->offset_y = priv->ti.pic_y;
  
  priv->offset_x_uv = priv->ti.pic_x / sub_h;
  priv->offset_y_uv = priv->ti.pic_y / sub_v;
  
  /* Create frame */
  priv->frame = gavl_video_frame_create(NULL);

  //  if(!priv->offset_x && !priv->offset_y)
  s->vframe = priv->frame;
  
  gavl_dictionary_set_string_nocopy(s->m, GAVL_META_FORMAT,
                           gavl_sprintf("Theora (Version %d.%d.%d)",
                                        priv->ti.version_major,
                                        priv->ti.version_minor,
                                        priv->ti.version_subminor));
  return 1;
  }

// static int64_t frame_counter = 0;

static gavl_source_status_t 
decode_theora(bgav_stream_t * s, gavl_video_frame_t * frame)
  {
  int i;
  bgav_packet_t * p = NULL;
  ogg_packet op;
  gavl_source_status_t st;  
  th_ycbcr_buffer yuv;
  theora_priv_t * priv;
  priv = s->decoder_priv;
  
  while(1)
    {
    if((st = bgav_stream_get_packet_read(s, &p)) != GAVL_SOURCE_OK)
      return st;

    memset(&op, 0, sizeof(op));

    op.bytes = p->buf.len;
    op.packet = p->buf.buf;
    if(PACKET_GET_LAST(p))
      op.e_o_s = 1;
    
    if(!th_packet_isheader(&op))
      break;
    bgav_stream_done_packet_read(s, p);
    }
  
  th_decode_packetin(priv->ctx, &op, NULL);
  th_decode_ycbcr_out(priv->ctx, yuv);

  for(i = 0; i < 3; i++)
    {
    if(!i)
      priv->frame->planes[i] =
        yuv[i].data + priv->offset_y * yuv[i].stride + priv->offset_x;
    else
      priv->frame->planes[i] =
        yuv[i].data + priv->offset_y_uv * yuv[i].stride + priv->offset_x_uv;
    priv->frame->strides[i] = yuv[i].stride;
    }
  bgav_set_video_frame_from_packet(p, priv->frame);
  
  /* Copy the frame */
#if 0  
  if(!priv->offset_x && !priv->offset_y && frame)
    {
    gavl_video_frame_copy_metadata(frame, priv->frame);
    gavl_video_frame_copy(&s->data.video.format,
                          frame, priv->frame);
    }
#endif
  
  bgav_stream_done_packet_read(s, p);
  return GAVL_SOURCE_OK;
  }

static void close_theora(bgav_stream_t * s)
  {
  theora_priv_t * priv;
  priv = s->decoder_priv;
  
  th_decode_free(priv->ctx);
  th_setup_free(priv->ts);
  th_comment_clear(&priv->tc);
  th_info_clear(&priv->ti);

  if(priv->frame)
    {
    gavl_video_frame_null(priv->frame);
    gavl_video_frame_destroy(priv->frame);
    }
  free(priv);
  }


static bgav_video_decoder_t decoder =
  {
    .name =   "Theora decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('T', 'H', 'R', 'A'),
                            0x00  },
    .init =   init_theora,
    .decode = decode_theora,
    .close =  close_theora,
  };

void bgav_init_video_decoders_theora()
  {
  bgav_video_decoder_register(&decoder);
  }
