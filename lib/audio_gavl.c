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
#include <avdec_private.h>
#include <codecs.h>

typedef struct
  {
  bgav_packet_t * p;
  } gavl_t;

static gavl_source_status_t decode_frame_gavl(bgav_stream_t * s)
  {
  gavl_t * priv;
  gavl_source_status_t st;

  priv = s->decoder_priv;

  if(priv->p)
    {
    bgav_stream_done_packet_read(s, priv->p);
    priv->p = NULL;
    }
 
  if((st = bgav_stream_get_packet_read(s, &priv->p)) != GAVL_SOURCE_OK)
    return st;  
  
  gavl_audio_frame_copy_ptrs(s->data.audio.format, s->data.audio.frame, priv->p->audio_frame);
  return GAVL_SOURCE_OK;
  }

static int init_gavl(bgav_stream_t * s)
  {
  gavl_t * priv;

  if(s->action == BGAV_STREAM_PARSE)
    return 1;
  
  priv = calloc(1, sizeof(*priv));
  s->decoder_priv = priv;

  /* Need to get the first packet because the dv avi decoder
     won't know the format before */
#if 1
  if(bgav_stream_get_packet_read(s, &priv->p) != GAVL_SOURCE_OK)
    return 0;
  if(!priv->p->audio_frame)
    return 0;
#endif

  return 1;
  }

static void close_gavl(bgav_stream_t * s)
  {
  gavl_t * priv;
  priv = s->decoder_priv;
  if(priv)
    free(priv);
  }

static void resync_gavl(bgav_stream_t * s)
  {
  gavl_t * priv;
  priv = s->decoder_priv;

  if(priv->p)
    {
    bgav_stream_done_packet_read(s, priv->p);
    priv->p = NULL;
    }

  }

static bgav_audio_decoder_t decoder =
  {
    .fourccs = (uint32_t[]){ BGAV_MK_FOURCC('g', 'a', 'v', 'l'),
                           0x00 },
    .name = "gavl audio decoder",
    .init = init_gavl,
    .close = close_gavl,
    .resync = resync_gavl,
    .decode_frame = decode_frame_gavl
  };

void bgav_init_audio_decoders_gavl()
  {
  bgav_audio_decoder_register(&decoder);
  }
