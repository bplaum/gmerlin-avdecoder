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

/* Ported from libquicktime */
#include <stdlib.h>

#include <avdec_private.h>
#include <codecs.h>

#include <RTjpeg.h>

#define BLOCK_SIZE 16

#define PADD(x) ((x+BLOCK_SIZE-1)/BLOCK_SIZE)*BLOCK_SIZE

typedef struct
  {
  gavl_video_frame_t * frame;
  RTjpeg_t * rtjpeg;
  } rtjpeg_priv_t;

static int init_rtjpeg(bgav_stream_t * s)
  {
  rtjpeg_priv_t * priv;
  priv = calloc(1, sizeof(*priv));
  
  s->decoder_priv = priv;

  priv->rtjpeg = RTjpeg_init();

  s->data.video.format->frame_width = PADD(s->data.video.format->image_width);
  s->data.video.format->frame_height = PADD(s->data.video.format->image_height);
  s->data.video.format->pixelformat = GAVL_YUV_420_P;
  
  priv->frame = gavl_video_frame_create(s->data.video.format);

  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                    "RTjpeg");
  
  return 1;
  }
  
static gavl_source_status_t decode_rtjpeg(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  rtjpeg_priv_t * priv;
  bgav_packet_t * p = NULL;
  gavl_source_status_t st;
  priv = s->decoder_priv;

  /* We assume one frame per packet */
   
  if((st = bgav_stream_get_packet_read(s, &p)) != GAVL_SOURCE_OK)
    return st;

  /* Skip frame */
  if(!f)
    {
    bgav_stream_done_packet_read(s, p);
    return GAVL_SOURCE_OK;
    }

  RTjpeg_decompress(priv->rtjpeg, p->buf.buf, priv->frame->planes);  
  gavl_video_frame_copy(s->data.video.format,
                        f, priv->frame);


  bgav_set_video_frame_from_packet(p, f);

  bgav_stream_done_packet_read(s, p);
  return GAVL_SOURCE_OK;
  }


static void close_rtjpeg(bgav_stream_t * s)
  {
  rtjpeg_priv_t * priv;
  priv = s->decoder_priv;

  RTjpeg_close(priv->rtjpeg);
  gavl_video_frame_destroy(priv->frame);
  free(priv);
  }

static bgav_video_decoder_t rtjpeg_decoder =
  {
    .name =   "rtjpeg video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('R', 'T', 'J', '0'), 0x00  },
    .init =   init_rtjpeg,
    .decode = decode_rtjpeg,
    .close =  close_rtjpeg,
  };

void bgav_init_video_decoders_rtjpeg()
  {
  bgav_video_decoder_register(&rtjpeg_decoder);
  }
