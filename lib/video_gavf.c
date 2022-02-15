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

#include <gavl/gavf.h>

#include <stdlib.h>

typedef struct
  {
  gavl_video_frame_t * frame;
  bgav_packet_t * p;
  gavl_dsp_context_t * dsp;
  } gavf_video_t;

static gavl_source_status_t
decode_frame_gavf_video(bgav_stream_t * s, gavl_video_frame_t * frame)
  {
  gavl_source_status_t st;  
  gavl_packet_t gp;
  gavf_video_t * priv = s->decoder_priv;
  
  if(priv->p)
    {
    bgav_stream_done_packet_read(s, priv->p);
    priv->p = NULL;
    }
  if((st = bgav_stream_get_packet_read(s, &priv->p)) != GAVL_SOURCE_OK)
    return st;

  gavl_packet_init(&gp);
  gp.data     = priv->p->data;
  gp.data_len = priv->p->data_size;
  gp.dst_x    = priv->p->dst_x;
  gp.dst_y    = priv->p->dst_y;
  gavl_rectangle_i_copy(&gp.src_rect, &priv->p->src_rect);
  
  if(s->type == GAVL_STREAM_VIDEO)
    {
    gavf_packet_to_video_frame(&gp, priv->frame, s->data.video.format, s->m, &priv->dsp);
    bgav_set_video_frame_from_packet(priv->p, priv->frame);
    }
  else
    {
    gavf_packet_to_overlay(&gp, frame, s->data.video.format);
    bgav_set_video_frame_from_packet(priv->p, frame);
    }
  return GAVL_SOURCE_OK;
  }

static void close_gavf_video(bgav_stream_t * s)
  {
  gavf_video_t * priv = s->decoder_priv;
  if(priv->frame)
    {
    gavl_video_frame_null(priv->frame);
    gavl_video_frame_destroy(priv->frame);
    }
  free(priv);
  }

static int init_gavf_video(bgav_stream_t * s)
  {
  gavf_video_t * priv;
  priv = calloc(1, sizeof(*priv));

  s->decoder_priv = priv;

  if(s->type == GAVL_STREAM_VIDEO)
    {
    priv->frame = gavl_video_frame_create(NULL);
    s->vframe = priv->frame;
    }
  return 1;
  }

static void resync_gavf_video(bgav_stream_t * s)
  {
  gavf_video_t * priv = s->decoder_priv;
  if(priv->p)
    {
    bgav_stream_done_packet_read(s, priv->p);
    priv->p = NULL;
    }
  }

static bgav_video_decoder_t decoder =
  {
    .fourccs = (uint32_t[]){ BGAV_MK_FOURCC('g','a','v','f'),
                             0x00 },
    .name = "GAVF video decoder",
    .init = init_gavf_video,
    .resync = resync_gavf_video,
    .close = close_gavf_video,
    .decode = decode_frame_gavf_video
  };

void bgav_init_video_decoders_gavf()
  {
  bgav_video_decoder_register(&decoder);
  }
