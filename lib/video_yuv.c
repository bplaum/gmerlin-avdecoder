/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
 *
 * Copyright (c) 2001 - 2024 Members of the Gmerlin project
 * http://github.com/bplaum
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



/* Various uncompressed YUV decoders */

#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <avdec_private.h>
#include <codecs.h>

#define PAD(size, bytes) ((((size)+bytes-1)/bytes)*bytes)

typedef struct
  {
  gavl_video_frame_t * frame;
  bgav_packet_t * p;
  void (*decode_func)(bgav_stream_t * s, bgav_packet_t * p, gavl_video_frame_t * f);
  } yuv_priv_t;

/* Common initialization */

static void init_common(bgav_stream_t * s)
  {
  yuv_priv_t * priv;
  priv = calloc(1, sizeof(*priv));
  s->decoder_priv = priv;
  priv->frame = gavl_video_frame_create(NULL);
  }

/* Decoding functions */

/* yuv2: It's yuyv with signedness swapped and JPEG scaled */

static void decode_yuv2(bgav_stream_t * s, bgav_packet_t * p, gavl_video_frame_t * f)
  {
  int i, j;
  uint8_t * src, *dst_y, *dst_u, *dst_v;
  yuv_priv_t * priv;
  priv = s->decoder_priv;

  priv->frame->planes[0] = p->buf.buf;
  
  for(i = 0; i < s->data.video.format->image_height; i++)
    {
    src = priv->frame->planes[0] + i * priv->frame->strides[0];
    dst_y = f->planes[0]         + i * f->strides[0];
    dst_u = f->planes[1]         + i * f->strides[1];
    dst_v = f->planes[2]         + i * f->strides[2];
    
    for(j = 0; j < s->data.video.format->image_width/2; j++)
      {
      dst_y[0] = src[0];        /* Y */
      dst_u[0] = src[1] ^ 0x80; /* U */
      dst_y[1] = src[2];        /* Y */
      dst_v[0] = src[3] ^ 0x80; /* V */
      src+=4;
      dst_y+=2;
      dst_u++;
      dst_v++;
      }
    }
  }

static int init_yuv2(bgav_stream_t * s)
  {
  yuv_priv_t * priv;
  
  init_common(s);

  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                    "Full scale YUV 4:2:2 packed (yuv2)");

  priv = s->decoder_priv;

  priv->frame->strides[0] = PAD(s->data.video.format->image_width * 2, 4);
  priv->decode_func = decode_yuv2;
  s->data.video.format->pixelformat = GAVL_YUVJ_422_P;
  return 1;
  }

/* v408:  */

static const uint8_t decode_alpha_v408[256] =
{
  0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x06,
  0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x15, 0x16, 0x17, 0x18,
  0x19, 0x1a, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21,
  0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2a, 0x2b,
  0x2c, 0x2d, 0x2e, 0x2f, 0x31, 0x32, 0x33, 0x34,
  0x35, 0x36, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d,
  0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x46, 0x47,
  0x48, 0x49, 0x4a, 0x4b, 0x4d, 0x4e, 0x4f, 0x50,
  0x51, 0x52, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
  0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x62, 0x63,
  0x64, 0x65, 0x66, 0x67, 0x68, 0x6a, 0x6b, 0x6c,
  0x6d, 0x6e, 0x6f, 0x71, 0x72, 0x73, 0x74, 0x75,
  0x76, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7f,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x86, 0x87, 0x88,
  0x89, 0x8a, 0x8b, 0x8d, 0x8e, 0x8f, 0x90, 0x91,
  0x92, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9b,
  0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa2, 0xa3, 0xa4,
  0xa5, 0xa6, 0xa7, 0xa9, 0xaa, 0xab, 0xac, 0xad,
  0xae, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb7,
  0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbf, 0xc0,
  0xc1, 0xc2, 0xc3, 0xc4, 0xc6, 0xc7, 0xc8, 0xc9,
  0xca, 0xcb, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2,
  0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xdb, 0xdc,
  0xdd, 0xde, 0xdf, 0xe0, 0xe2, 0xe3, 0xe4, 0xe5,
  0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee,
  0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf7, 0xf8,
  0xf9, 0xfa, 0xfb, 0xfc, 0xfe, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};


static void decode_v408(bgav_stream_t * s, bgav_packet_t * p, gavl_video_frame_t * f)
  {
  int i, j;
  uint8_t * src, *dst;
  yuv_priv_t * priv;
  priv = s->decoder_priv;

  priv->frame->planes[0] = p->buf.buf;
  
  for(i = 0; i < s->data.video.format->image_height; i++)
    {
    src = priv->frame->planes[0] + i * priv->frame->strides[0];
    dst = f->planes[0]         + i * f->strides[0];
    
    for(j = 0; j < s->data.video.format->image_width; j++)
      {
      dst[0] = src[1];                    /* Y */
      dst[1] = src[0];                    /* U */
      dst[2] = src[2];                    /* V */
      dst[3] = decode_alpha_v408[src[3]]; /* A */
      src+=4;
      dst+=4;
      }
    }
  }

static int init_v408(bgav_stream_t * s)
  {
  yuv_priv_t * priv;
  
  init_common(s);

  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                    "YUVA 4:4:4:4 (v408)");

  priv = s->decoder_priv;

  priv->frame->strides[0] = s->data.video.format->image_width * 4;
  priv->decode_func = decode_v408;
  s->data.video.format->pixelformat = GAVL_YUVA_32;
  return 1;
  }



/*
 *   2vuy: Like yuv2 but WITHOUT swapped chroma sign and
 *   packing order uyvy
 */

static void decode_2vuy(bgav_stream_t * s, bgav_packet_t * p, gavl_video_frame_t * f)
  {
  yuv_priv_t * priv;
  priv = s->decoder_priv;
  priv->frame->planes[0] = p->buf.buf;
  }

static int init_2vuy(bgav_stream_t * s)
  {
  yuv_priv_t * priv;
  
  init_common(s);
  
  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                    "YUV 4:2:2 packed (2vuy)");
  
  priv = s->decoder_priv;

  priv->frame->strides[0] = PAD(s->data.video.format->image_width * 2, 4);
  priv->decode_func = decode_2vuy;
  s->vframe = priv->frame;
  s->data.video.format->pixelformat = GAVL_UYVY;
  return 1;
  }

/*
 *  VYUY is just YUY2 pixelformat!!!
 *  Wow, that was hard to reverse engineer ;-)
 */

static void decode_VYUY(bgav_stream_t * s, bgav_packet_t * p, gavl_video_frame_t * f)
  {
  yuv_priv_t * priv;
  priv = s->decoder_priv;
  priv->frame->planes[0] = p->buf.buf;
  }

static int init_VYUY(bgav_stream_t * s)
  {
  yuv_priv_t * priv;
  
  init_common(s);

  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                    "YUV 4:2:2 packed (VYUY)");
  
  priv = s->decoder_priv;

  priv->frame->strides[0] = PAD(s->data.video.format->image_width * 2, 4);
  priv->decode_func = decode_VYUY;
  s->vframe = priv->frame;
  s->data.video.format->pixelformat = GAVL_YUY2;
  return 1;
  }

/* The following 2 have just the chroma planed swapped */

static void decode_yv12(bgav_stream_t * s, bgav_packet_t * p, gavl_video_frame_t * f)
  {
  yuv_priv_t * priv;
  priv = s->decoder_priv;

  priv->frame->planes[0] = p->buf.buf;
  priv->frame->planes[1] = priv->frame->planes[0] + s->data.video.format->image_height * priv->frame->strides[0];
  priv->frame->planes[2] = priv->frame->planes[1] + s->data.video.format->image_height/2 * priv->frame->strides[1];
  }

static int init_yv12(bgav_stream_t * s)
  {
  yuv_priv_t * priv;
  
  init_common(s);

  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                    "YUV 4:2:0 planar (yv12)");

  priv = s->decoder_priv;

  priv->frame->strides[0] = PAD(s->data.video.format->image_width, 2);
  priv->frame->strides[1] = priv->frame->strides[0]/2;
  priv->frame->strides[2] = priv->frame->strides[1];
  
  priv->decode_func = decode_yv12;
  s->data.video.format->pixelformat = GAVL_YUV_420_P;
  s->vframe = priv->frame;
  
  return 1;
  }

static void decode_YV12(bgav_stream_t * s, bgav_packet_t * p, gavl_video_frame_t * f)
  {
  yuv_priv_t * priv;
  priv = s->decoder_priv;

  priv->frame->planes[0] = p->buf.buf;
  priv->frame->planes[2] = priv->frame->planes[0] + s->data.video.format->image_height * priv->frame->strides[0];
  priv->frame->planes[1] = priv->frame->planes[2] + s->data.video.format->image_height/2 * priv->frame->strides[1];
  gavl_video_frame_copy(s->data.video.format, f, priv->frame);
  }

static int init_YV12(bgav_stream_t * s)
  {
  yuv_priv_t * priv;
  
  init_common(s);

  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                    "YUV 4:2:0 planar (YV12)");

  priv = s->decoder_priv;

  priv->frame->strides[0] = PAD(s->data.video.format->image_width, 2);
  priv->frame->strides[1] = priv->frame->strides[0]/2;
  priv->frame->strides[2] = priv->frame->strides[1];
  
  priv->decode_func = decode_YV12;
  s->data.video.format->pixelformat = GAVL_YUV_420_P;
  return 1;
  }

/*
 *  YVU9 is yuv410 with swapped chroma planes
 */

static void decode_YVU9(bgav_stream_t * s, bgav_packet_t * p, gavl_video_frame_t * f)
  {
  yuv_priv_t * priv;
  priv = s->decoder_priv;

  priv->frame->planes[0] = p->buf.buf;
  priv->frame->planes[2] = priv->frame->planes[0] + s->data.video.format->image_height * priv->frame->strides[0];
  priv->frame->planes[1] = priv->frame->planes[2] + (s->data.video.format->image_height)/4 * priv->frame->strides[1];
  }

static int init_YVU9(bgav_stream_t * s)
  {
  yuv_priv_t * priv;
  
  init_common(s);

  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                    "YVU9");
  
  priv = s->decoder_priv;

  priv->frame->strides[0] = PAD(s->data.video.format->image_width, 8);
  priv->frame->strides[1] = priv->frame->strides[0]/4;
  priv->frame->strides[2] = priv->frame->strides[1];
  
  priv->decode_func = decode_YVU9;
  s->data.video.format->pixelformat = GAVL_YUV_410_P;
  s->vframe = priv->frame;
  return 1;
  }


/* v308: Packed YUV 4:4:4, we make this planar */

static void decode_v308(bgav_stream_t * s, bgav_packet_t * p, gavl_video_frame_t * f)
  {
  int i, j;
  uint8_t * src, *dst_y, *dst_u, *dst_v;
  yuv_priv_t * priv;
  priv = s->decoder_priv;

  priv->frame->planes[0] = p->buf.buf;
  
  for(i = 0; i < s->data.video.format->image_height; i++)
    {
    src = priv->frame->planes[0] + i * priv->frame->strides[0];

    dst_y = f->planes[0] + i * f->strides[0];
    dst_u = f->planes[1] + i * f->strides[1];
    dst_v = f->planes[2] + i * f->strides[2];
    
    for(j = 0; j < s->data.video.format->image_width; j++)
      {
      *dst_y = src[1];
      *dst_u = src[2];
      *dst_v = src[0];
      
      src+=3;
      dst_y++;
      dst_u++;
      dst_v++;
      
      }
    }
  }

static int init_v308(bgav_stream_t * s)
  {
  yuv_priv_t * priv;
  
  init_common(s);

  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                    "YUV 4:4:4 packed (v308)");

  priv = s->decoder_priv;

  priv->frame->strides[0] = s->data.video.format->image_width * 3;
  priv->decode_func = decode_v308;
  s->data.video.format->pixelformat = GAVL_YUV_444_P;
  return 1;
  }

/*
 *  v410: Packed YUV 4:4:4, (10 bit per component)
 *  we make this planar
 */

static void decode_v410(bgav_stream_t * s, bgav_packet_t * p, gavl_video_frame_t * f)
  {
  int i, j;
  uint8_t * src;
  uint16_t *dst_y, *dst_u, *dst_v;
  uint32_t src_i;
  yuv_priv_t * priv;
  priv = s->decoder_priv;

  priv->frame->planes[0] = p->buf.buf;
  
  for(i = 0; i < s->data.video.format->image_height; i++)
    {
    src = priv->frame->planes[0] + i * priv->frame->strides[0];

    dst_y = (uint16_t*)(f->planes[0] + i * f->strides[0]);
    dst_u = (uint16_t*)(f->planes[1] + i * f->strides[1]);
    dst_v = (uint16_t*)(f->planes[2] + i * f->strides[2]);
    
    for(j = 0; j < s->data.video.format->image_width; j++)
      {
      src_i = GAVL_PTR_2_32LE(src);

      *(dst_v++) = (src_i & 0xffc00000) >> 16; /* V */
      *(dst_y++) = (src_i & 0x3ff000) >> 6;    /* Y */
      *(dst_u++) = (src_i & 0xffc) << 4;       /* U */
      
      src+=4;
      }
    }
  }

static int init_v410(bgav_stream_t * s)
  {
  yuv_priv_t * priv;
  
  init_common(s);
  
  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                    "YUV 4:4:4 packed (v410)");

  priv = s->decoder_priv;

  priv->frame->strides[0] = s->data.video.format->image_width * 4;
  priv->decode_func = decode_v410;
  s->data.video.format->pixelformat = GAVL_YUV_444_P_16;
  return 1;
  }

/*
 *  v210: Packed YUV 4:2:2, (10 bit per component)
 *  we make this planar
 */

static void decode_v210(bgav_stream_t * s, bgav_packet_t * p, gavl_video_frame_t * f)
  {
  int i, j;
  uint8_t * src;
  uint16_t *dst_y, *dst_u, *dst_v;
  uint32_t i1, i2, i3, i4;
  yuv_priv_t * priv;
  priv = s->decoder_priv;

  priv->frame->planes[0] = p->buf.buf;
  
  for(i = 0; i < s->data.video.format->image_height; i++)
    {
    src = priv->frame->planes[0] + i * priv->frame->strides[0];

    dst_y = (uint16_t*)(f->planes[0] + i * f->strides[0]);
    dst_u = (uint16_t*)(f->planes[1] + i * f->strides[1]);
    dst_v = (uint16_t*)(f->planes[2] + i * f->strides[2]);
    
    for(j = 0; j < s->data.video.format->image_width/6; j++)
      {
      i1 = GAVL_PTR_2_32LE(src);src+=4;
      i2 = GAVL_PTR_2_32LE(src);src+=4;
      i3 = GAVL_PTR_2_32LE(src);src+=4;
      i4 = GAVL_PTR_2_32LE(src);src+=4;

      /* These are grouped to show the "pixel pairs" of  4:2:2 */
      
      *(dst_u++) = (i1 & 0x3ff) << 6;       /* Cb0 */
      *(dst_y++) = (i1 & 0xffc00) >> 4;     /* Y0 */
      *(dst_v++) = (i1 & 0x3ff00000) >> 14; /* Cr0 */
      *(dst_y++) = (i2 & 0x3ff) << 6;       /* Y1 */
      
      *(dst_u++) = (i2 & 0xffc00) >> 4;     /* Cb1 */
      *(dst_y++) = (i2 & 0x3ff00000) >> 14; /* Y2 */
      *(dst_v++) = (i3 & 0x3ff) << 6;       /* Cr1 */
      *(dst_y++) = (i3 & 0xffc00) >> 4;     /* Y3 */
      
      *(dst_u++) = (i3 & 0x3ff00000) >> 14; /* Cb2 */
      *(dst_y++) = (i4 & 0x3ff) << 6;       /* Y4 */
      *(dst_v++) = (i4 & 0xffc00) >> 4;     /* Cr2 */
      *(dst_y++) = (i4 & 0x3ff00000) >> 14; /* Y5 */
      }

    /* Handle the 2 or 4 pixels possibly remaining */
    j = (s->data.video.format->image_width - ((s->data.video.format->image_width / 6) * 6));
    if (j != 0)
      {
      i1 = GAVL_PTR_2_32LE(src);src+=4;
      i2 = GAVL_PTR_2_32LE(src);src+=4;
      i3 = GAVL_PTR_2_32LE(src);src+=4;
      i4 = GAVL_PTR_2_32LE(src);src+=4;

      *(dst_u++) = (i1 & 0x3ff) << 6;       /* Cb0 */
      *(dst_y++) = (i1 & 0xffc00) >> 4;     /* Y0 */
      *(dst_v++) = (i1 & 0x3ff00000) >> 14; /* Cr0 */
      *(dst_y++) = (i2 & 0x3ff) << 6;       /* Y1 */
      if (j == 4)
        {
        *(dst_u++) = (i2 & 0xffc00) >> 4;     /* Cb1 */
        *(dst_y++) = (i2 & 0x3ff00000) >> 14; /* Y2 */
        *(dst_v++) = (i3 & 0x3ff) << 6;       /* Cr1 */
        *(dst_y++) = (i3 & 0xffc00) >> 4;     /* Y3 */
        }
      }
    }
  }

static int init_v210(bgav_stream_t * s)
  {
  yuv_priv_t * priv;
  
  init_common(s);

  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                    "YUV 4:2:2 packed (v210)");

  priv = s->decoder_priv;

  priv->frame->strides[0] = (PAD(s->data.video.format->image_width, 48) * 8) / 3;
  priv->decode_func = decode_v210;
  s->data.video.format->pixelformat = GAVL_YUV_422_P_16;
  return 1;
  }


/*
 *  yuv4: packed YUV 4:2:0
 *  Pretty weird thing existent exclusively in the
 *  qt4l/lqt universe :-)
 */

static void decode_yuv4(bgav_stream_t * s, bgav_packet_t * p, gavl_video_frame_t * f)
  {
  int i, j;
  uint8_t * src, *dst_y, *dst_u, *dst_v;
  yuv_priv_t * priv;
  priv = s->decoder_priv;

  priv->frame->planes[0] = p->buf.buf;

  /* Packing order for one macropixel is U0V0Y0Y1Y2Y3 */

  for(i = 0; i < s->data.video.format->image_height/2; i++)
    {
    src = priv->frame->planes[0] + i * priv->frame->strides[0];
    dst_y = f->planes[0] + 2 * i * f->strides[0];
    dst_u = f->planes[1] + i * f->strides[1];
    dst_v = f->planes[2] + i * f->strides[2];
    
    for(j = 0; j < s->data.video.format->image_width/2; j++)
      {
      dst_u[0]               = src[0] ^ 0x80;
      dst_v[0]               = src[1] ^ 0x80;
      
      dst_y[0]               = src[2]; /* Top left  */
      dst_y[1]               = src[3]; /* Top right */
      
      dst_y[f->strides[0]]   = src[4]; /* Bottom left  */
      dst_y[f->strides[0]+1] = src[5]; /* Bottom right */
      
      src+=6;
      dst_y+=2;
      dst_u++;
      dst_v++;
      }
    }
  }

static int init_yuv4(bgav_stream_t * s)
  {
  yuv_priv_t * priv;
  
  init_common(s);

  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                    "YUV 4:2:0 packed (yuv4)");
  s->ci->flags &= ~GAVL_COMPRESSION_HAS_P_FRAMES;
  
  priv = s->decoder_priv;

  priv->frame->strides[0] = PAD(s->data.video.format->image_width, 2) * 3;
  priv->decode_func = decode_yuv4;
  s->data.video.format->pixelformat = GAVL_YUV_420_P;
  return 1;
  }


/* Generic decode function */

static gavl_source_status_t decode(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  gavl_source_status_t st;
  yuv_priv_t * priv;
  priv = s->decoder_priv;

  if(priv->p)
    {
    bgav_stream_done_packet_read(s, priv->p);
    priv->p = NULL;
    }
  /* We assume one frame per packet */

  if((st = bgav_stream_get_packet_read(s, &priv->p)) != GAVL_SOURCE_OK)
    return st;

  if(!priv->p->buf.len)
    return GAVL_SOURCE_OK; /* Libquicktime/qt4l bug */
  
  priv->decode_func(s, priv->p, f);
  bgav_set_video_frame_from_packet(priv->p, priv->frame);

  if(f)
    gavl_video_frame_copy_metadata(f, priv->frame);
  
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

static bgav_video_decoder_t yuv2_decoder =
  {
    .name =   "yuv2 video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('y', 'u', 'v', '2'), 0x00  },
    .init =   init_yuv2,
    .decode = decode,
    .resync = resync,
    .close =  close,
  };

#if 1
static bgav_video_decoder_t twovuy_decoder =
  {
    .name =   "2vuy video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('2', 'v', 'u', 'y'), 0x00  },
    .init =   init_2vuy,
    .decode = decode,
    .resync = resync,
    .close =  close,
  };
#endif

static bgav_video_decoder_t yv12_decoder =
  {
    .name =   "yv12 video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('y', 'v', '1', '2'), 0x00  },
    .init =   init_yv12,
    .decode = decode,
    .resync = resync,
    .close =  close,
  };

static bgav_video_decoder_t YV12_decoder =
  {
    .name =   "YV12 video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('Y', 'V', '1', '2'), 0x00  },
    .init =   init_YV12,
    .decode = decode,
    .resync = resync,
    .close =  close,
  };

static bgav_video_decoder_t VYUY_decoder =
  {
    .name =   "VYUY video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('V', 'Y', 'U', 'Y'),
                            BGAV_MK_FOURCC('2', 'v', 'u', 'y'),
                            0x00 },
    .init =   init_VYUY,
    .decode = decode,
    .resync = resync,
    .close =  close,
  };

static bgav_video_decoder_t YVU9_decoder =
  {
    .name =   "YVU9 video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('Y', 'V', 'U', '9'), 0x00  },
    .init =   init_YVU9,
    .decode = decode,
    .resync = resync,
    .close =  close,
  };

static bgav_video_decoder_t v308_decoder =
  {
    .name =   "v308 video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('v', '3', '0', '8'), 0x00  },
    .init =   init_v308,
    .decode = decode,
    .resync = resync,
    .close =  close,
  };

static bgav_video_decoder_t v408_decoder =
  {
    .name =   "v408 video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('v', '4', '0', '8'), 0x00  },
    .init =   init_v408,
    .decode = decode,
    .resync = resync,
    .close =  close,
  };

static bgav_video_decoder_t v410_decoder =
  {
    .name =   "v410 video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('v', '4', '1', '0'), 0x00  },
    .init =   init_v410,
    .decode = decode,
    .resync = resync,
    .close =  close,
  };

static bgav_video_decoder_t v210_decoder =
  {
    .name =   "v210 video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('v', '2', '1', '0'), 0x00  },
    .init =   init_v210,
    .decode = decode,
    .resync = resync,
    .close =  close,
  };


static bgav_video_decoder_t yuv4_decoder =
  {
    .name =   "yuv4 video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('y', 'u', 'v', '4'), 0x00  },
    .init =   init_yuv4,
    .decode = decode,
    .resync = resync,
    .close =  close,
  };

void bgav_init_video_decoders_yuv()
  {
  bgav_video_decoder_register(&yuv2_decoder);
  bgav_video_decoder_register(&twovuy_decoder);
  bgav_video_decoder_register(&yv12_decoder);
  bgav_video_decoder_register(&YV12_decoder);
  bgav_video_decoder_register(&VYUY_decoder);
  bgav_video_decoder_register(&YVU9_decoder);
  bgav_video_decoder_register(&v308_decoder);
  bgav_video_decoder_register(&v408_decoder);
  bgav_video_decoder_register(&v410_decoder);
  bgav_video_decoder_register(&v210_decoder);
  bgav_video_decoder_register(&yuv4_decoder);
  }

