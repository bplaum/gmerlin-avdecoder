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

/* Y4M 'decoder' */

#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <avdec_private.h>
#include <codecs.h>

#define LOG_DOMAIN "video_y4m"

typedef struct
  {
  gavl_video_frame_t * frame;
  void (*decode_func)(bgav_stream_t * s, bgav_packet_t * p, gavl_video_frame_t * f);

  int plane_sizes[2];
  
  bgav_packet_t * p;
  } yuv_priv_t;

/* Specialized decode functions */

static const uint8_t y_8_to_yj_8[256] = 
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x01, 0x02, 0x03, 0x05, 0x06, 0x07, 0x08, 
  0x09, 0x0a, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 
  0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x1a, 0x1b, 
  0x1c, 0x1d, 0x1e, 0x1f, 0x21, 0x22, 0x23, 0x24, 
  0x25, 0x26, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 
  0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x36, 0x37, 
  0x38, 0x39, 0x3a, 0x3b, 0x3d, 0x3e, 0x3f, 0x40, 
  0x41, 0x42, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 
  0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x52, 0x53, 
  0x54, 0x55, 0x56, 0x57, 0x58, 0x5a, 0x5b, 0x5c, 
  0x5d, 0x5e, 0x5f, 0x61, 0x62, 0x63, 0x64, 0x65, 
  0x66, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6f, 
  0x70, 0x71, 0x72, 0x73, 0x74, 0x76, 0x77, 0x78, 
  0x79, 0x7a, 0x7b, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 
  0x82, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8b, 
  0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x92, 0x93, 0x94, 
  0x95, 0x96, 0x97, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 
  0x9e, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa7, 
  0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xaf, 0xb0, 
  0xb1, 0xb2, 0xb3, 0xb4, 0xb6, 0xb7, 0xb8, 0xb9, 
  0xba, 0xbb, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 
  0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xcb, 0xcc, 
  0xcd, 0xce, 0xcf, 0xd0, 0xd2, 0xd3, 0xd4, 0xd5, 
  0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 
  0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe7, 0xe8, 
  0xe9, 0xea, 0xeb, 0xec, 0xee, 0xef, 0xf0, 0xf1, 
  0xf2, 0xf3, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 
  0xfc, 0xfd, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
};


static void decode_mono(bgav_stream_t * s, bgav_packet_t * p, gavl_video_frame_t * f)
  {
  int i, j;

  const uint8_t * src;
  uint8_t * dst;
  
  src = p->buf.buf;
  
  for(i = 0; i < s->data.video.format->image_height; i++)
    {
    dst = f->planes[0] + i * f->strides[0];

    for(j = 0; i < s->data.video.format->image_width; j++)
      {
      dst[j] = y_8_to_yj_8[src[j]];
      src++;
      dst++;
      }
    }
  }

static void decode_yuva(bgav_stream_t * s, bgav_packet_t * p, gavl_video_frame_t * f)
  {
  int i, j;
  const uint8_t * src_y;
  const uint8_t * src_u;
  const uint8_t * src_v;
  const uint8_t * src_a;
  uint8_t * dst;
  
  yuv_priv_t * priv;
  priv = s->decoder_priv;

  src_y = p->buf.buf;
  src_u = src_y + priv->plane_sizes[0];
  src_v = src_u + priv->plane_sizes[1];
  src_a = src_v + priv->plane_sizes[1];
  
  for(i = 0; i < s->data.video.format->image_height; i++)
    {
    dst = f->planes[0] + i * f->strides[0];

    for(j = 0; i < s->data.video.format->image_width; j++)
      {
      *(dst++) = *(src_y++);
      *(dst++) = *(src_u++);
      *(dst++) = *(src_v++);
      *(dst++) = y_8_to_yj_8[*(src_a++)];
      }
    }
  
  }

/* Generic decode function */

static gavl_source_status_t decode(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  gavl_source_status_t st;
  yuv_priv_t * priv;
  priv = s->decoder_priv;
  
  /* We assume one frame per packet */

  if(priv->p)
    {
    bgav_stream_done_packet_read(s, priv->p);
    priv->p = NULL;
    }
  
  if((st = bgav_stream_get_packet_read(s, &priv->p)) != GAVL_SOURCE_OK)
    return st;
  
  if(priv->decode_func)
    {
    /* Skip frame */
    if(!f)
      {
      bgav_stream_done_packet_read(s, priv->p);
      priv->p = NULL;
      return GAVL_SOURCE_OK;
      }
    priv->decode_func(s, priv->p, f);
    bgav_set_video_frame_from_packet(priv->p, f);
    }
  else
    {
    priv->frame->planes[0] = priv->p->buf.buf;
    priv->frame->planes[1] = priv->frame->planes[0] + priv->plane_sizes[0];
    priv->frame->planes[2] = priv->frame->planes[1] + priv->plane_sizes[1];
    bgav_set_video_frame_from_packet(priv->p, priv->frame);
    }
  
  return GAVL_SOURCE_OK;
  }

static void resync(bgav_stream_t * s)
  {
  yuv_priv_t * priv;
  priv = s->decoder_priv;
  if(priv->p)
    {
    bgav_stream_done_packet_read(s, priv->p);
    priv->p = NULL;
    }
  }

/* Generic close function */

static void close(bgav_stream_t * s)
  {
  yuv_priv_t * priv;
  priv = s->decoder_priv;

  gavl_video_frame_null(priv->frame);
  gavl_video_frame_destroy(priv->frame);
  
  free(priv);
  }

/* Decoders */

static int init_y4m(bgav_stream_t * s)
  {
  yuv_priv_t * priv;
  priv = calloc(1, sizeof(*priv));
  s->decoder_priv = priv;
  
  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT, "Y4M");
  
  priv = s->decoder_priv;
  
  if(!s->ci->global_header_len)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Y4M needs extradata");
    return 0;
    }

  if(!strncmp((char*)s->ci->global_header, "420", 3))
    {
    s->data.video.format->pixelformat = GAVL_YUV_420_P;
    
    if(!strncmp((char*)(s->ci->global_header+3), "mpeg2", 5))
      s->data.video.format->chroma_placement = GAVL_CHROMA_PLACEMENT_MPEG2;
    else if(!strncmp((char*)(s->ci->global_header+3), "paldv", 5))
      s->data.video.format->chroma_placement = GAVL_CHROMA_PLACEMENT_DVPAL;
    }
  else if(!strncmp((char*)s->ci->global_header, "422", 3))
    {
    s->data.video.format->pixelformat = GAVL_YUV_422_P;
    }
  else if(!strncmp((char*)s->ci->global_header, "411", 3))
    {
    s->data.video.format->pixelformat = GAVL_YUV_411_P;
    }
  else if(!strncmp((char*)s->ci->global_header, "444", 3))
    {
    if(!strcmp((char*)s->ci->global_header, "444alpha"))
      {
      priv->decode_func = decode_yuva;
      s->data.video.format->pixelformat = GAVL_YUVA_32;
      }
    else // Normal planar 444
      s->data.video.format->pixelformat = GAVL_YUV_444_P;
    }
  else if(!strncmp((char*)s->ci->global_header, "mono", 4))
    {
    priv->decode_func = decode_mono;
    s->data.video.format->pixelformat = GAVL_GRAY_8;
    }

  if(!priv->decode_func)
    {
    int sub_h, sub_v;
    
    priv->frame = gavl_video_frame_create(NULL);

    gavl_pixelformat_chroma_sub(s->data.video.format->pixelformat,
                                &sub_h, &sub_v);
    
    priv->frame->strides[0] = s->data.video.format->image_width;
    priv->frame->strides[1] = s->data.video.format->image_width / sub_h;
    priv->frame->strides[2] = s->data.video.format->image_width / sub_h;
    
    priv->plane_sizes[0] =
      s->data.video.format->image_width *
      s->data.video.format->image_height;
    
    priv->plane_sizes[1] =
      priv->frame->strides[1] * (s->data.video.format->image_height/sub_v);

    s->vframe = priv->frame;
    }
  
  return 1;
  }

static bgav_video_decoder_t y4m_decoder =
  {
    .name =   "y4m video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('y', '4', 'm', ' '), 0x00  },
    .init =   init_y4m,
    .decode = decode,
    .resync = resync,
    .close =  close,
  };

void bgav_init_video_decoders_y4m()
  {
  bgav_video_decoder_register(&y4m_decoder);
  }

