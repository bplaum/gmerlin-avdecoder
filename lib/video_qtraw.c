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




#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <config.h>
#include <avdec_private.h>
#include <codecs.h>

#define LOG_DOMAIN "video_qtraw"

typedef struct
  {
  int bytes_per_line;
  void (*scanline_func)(uint8_t * src,
                        uint8_t * dst,
                        int num_pixels,
                        gavl_palette_entry_t * pal);
  } raw_priv_t;

static void scanline_raw_1(uint8_t * src,
                           uint8_t * dst,
                           int num_pixels,
                           gavl_palette_entry_t * pal)
  {
  int i;
  int counter = 0;
  for(i = 0; i < num_pixels; i++)
    {
    if(counter == 8)
      {
      counter = 0;
      src++;
      }
    BGAV_PALETTE_2_RGB24(pal[(*src & 0x80) >> 7], dst);
    *src <<= 1;
    dst += 3;
    counter++;
    }
  }

static void scanline_raw_2(uint8_t * src,
                           uint8_t * dst,
                           int num_pixels,
                           gavl_palette_entry_t * pal)
  {
  int i;
  int counter = 0;
  for(i = 0; i < num_pixels; i++)
    {
    if(counter == 4)
      {
      counter = 0;
      src++;
      }
    BGAV_PALETTE_2_RGB24(pal[(*src & 0xc0) >> 6], dst);
    *src <<= 2;
    dst += 3;
    counter++;
    }
  }

static void scanline_raw_4(uint8_t * src,
                           uint8_t * dst,
                           int num_pixels,
                           gavl_palette_entry_t * pal)
  {
  int i;
  int counter = 0;
  for(i = 0; i < num_pixels; i++)
    {
    if(counter == 2)
      {
      counter = 0;
      src++;
      }
    BGAV_PALETTE_2_RGB24(pal[(*src & 0xF0) >> 4], dst);
    *src <<= 4;
    dst += 3;
    counter++;
    }
  }

static void scanline_raw_8(uint8_t * src,
                           uint8_t * dst,
                           int num_pixels,
                           gavl_palette_entry_t * pal)
  {
  int i;
  for(i = 0; i < num_pixels; i++)
    {
    BGAV_PALETTE_2_RGB24(pal[*src], dst);
    src++;
    dst+=3;
    }
  }

static void scanline_raw_16(uint8_t * src,
                            uint8_t * dst,
                            int num_pixels,
                            gavl_palette_entry_t * pal)
  {
  int i;
  uint16_t pixel;
  
  for(i = 0; i < num_pixels; i++)
    {
    pixel = (src[0] << 8) | (src[1]);
    *((uint16_t*)dst) = pixel;
    src += 2;
    dst += 2;
    }
  }

static void scanline_raw_24(uint8_t * src,
                            uint8_t * dst,
                            int num_pixels,
                            gavl_palette_entry_t * pal)
  {
  memcpy(dst, src, num_pixels * 3);
  }

static void scanline_raw_32(uint8_t * src,
                            uint8_t * dst,
                            int num_pixels,
                            gavl_palette_entry_t * pal)
  {
  int i;
  for(i = 0; i < num_pixels; i++)
    {
    dst[0] = src[1];
    dst[1] = src[2];
    dst[2] = src[3];
    dst[3] = src[0];
    dst += 4;
    src += 4;
    }
  }

static void scanline_raw_2_gray(uint8_t * src,
                                uint8_t * dst,
                                int num_pixels,
                                gavl_palette_entry_t * pal)
  {
  int i;
  int counter = 0;
  for(i = 0; i < num_pixels; i++)
    {
    if(counter == 4)
      {
      counter = 0;
      src++;
      }
    BGAV_PALETTE_2_RGB24(pal[(*src & 0xC0) >> 6], dst);
    
    /* Advance */

    *src <<= 2;
    dst += 3;
    counter++;
    }
  }

static void scanline_raw_4_gray(uint8_t * src,
                                uint8_t * dst,
                                int num_pixels,
                                gavl_palette_entry_t * pal)
  {
  int i;

  int counter = 0;
  for(i = 0; i < num_pixels; i++)
    {
    if(counter == 2)
      {
      counter = 0;
      src++;
      }
    BGAV_PALETTE_2_RGB24(pal[(*src & 0xF0) >> 4], dst);
    
    /* Advance */

    *src <<= 4;
    dst += 3;
    counter++;
    }
  }

static void scanline_raw_8_gray(uint8_t * src,
                                uint8_t * dst,
                                int num_pixels,
                                gavl_palette_entry_t * pal)
  {
  int i;
  for(i = 0; i < num_pixels; i++)
    {
    BGAV_PALETTE_2_RGB24(pal[*src], dst);
    dst += 3;
    src++;
    }
  }


static int init_qtraw(bgav_stream_t * s)
  {
  raw_priv_t * priv;
  int width;
  priv = calloc(1, sizeof(*priv));
  s->decoder_priv = priv;
  s->ci->flags &= ~GAVL_COMPRESSION_HAS_P_FRAMES;
  
  width = s->data.video.format->image_width;
  
  switch(s->data.video.depth)
    {
    case 1:
      /* 1 bpp palette */
      priv->bytes_per_line = width / 8;
      priv->scanline_func = scanline_raw_1;
      if(!s->data.video.pal || (s->data.video.pal->num_entries < 2))
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Palette missing or too small");
        goto fail;
        }
      s->data.video.format->pixelformat = GAVL_RGB_24;

      gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                        "Quickime raw palette");
      gavl_dictionary_set_int(s->m, GAVL_META_VIDEO_BPP, 1);
      
      break;
    case 2:
      /* 2 bpp palette */
      priv->bytes_per_line = width / 4;
      priv->scanline_func = scanline_raw_2;
      if(!s->data.video.pal || (s->data.video.pal->num_entries < 4))
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Palette missing or too small");
        goto fail;
        }
      s->data.video.format->pixelformat = GAVL_RGB_24;

      gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                        "Quickime raw palette");
      gavl_dictionary_set_int(s->m, GAVL_META_VIDEO_BPP, 2);

      break;
    case 4:
      /* 4 bpp palette */
      priv->bytes_per_line = width / 2;
      priv->scanline_func = scanline_raw_4;
      if(s->data.video.pal || (s->data.video.pal->num_entries < 16))
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Palette missing or too small");
        goto fail;
        }
      s->data.video.format->pixelformat = GAVL_RGB_24;

      gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                        "Quickime raw palette");
      gavl_dictionary_set_int(s->m, GAVL_META_VIDEO_BPP, 4);
      break;
    case 8:
      /* 8 bpp palette */
      priv->bytes_per_line = width;
      priv->scanline_func = scanline_raw_8;
      if(!s->data.video.pal || s->data.video.pal->num_entries < 256)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Palette missing or too small");
        goto fail;
        }
      s->data.video.format->pixelformat = GAVL_RGB_24;

      gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                        "Quickime raw palette");
      gavl_dictionary_set_int(s->m, GAVL_META_VIDEO_BPP, 8);
      break;
    case 16:
      /* RGB565 */
      priv->bytes_per_line = width * 2;
      priv->scanline_func = scanline_raw_16;
      s->data.video.format->pixelformat = GAVL_RGB_15;

      gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                        "Quickime raw RGB");
      gavl_dictionary_set_int(s->m, GAVL_META_VIDEO_BPP, 16);
      break;
    case 24:
      /* 24 RGB */
      priv->bytes_per_line = width * 3;
      priv->scanline_func = scanline_raw_24;
      s->data.video.format->pixelformat = GAVL_RGB_24;
      
      gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                        "Quickime raw RGB");
      gavl_dictionary_set_int(s->m, GAVL_META_VIDEO_BPP, 24);
      break;
    case 32:
      /* 32 ARGB */
      priv->bytes_per_line = width * 4;
      priv->scanline_func = scanline_raw_32;
      s->data.video.format->pixelformat = GAVL_RGBA_32;

      gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                        "Quickime raw RGBA");
      gavl_dictionary_set_int(s->m, GAVL_META_VIDEO_BPP, 32);
      break;
    case 34:
      /* 2 bit gray */
      priv->bytes_per_line = width / 4;
      priv->scanline_func = scanline_raw_2_gray;
      s->data.video.format->pixelformat = GAVL_RGB_24;

      gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                        "Quickime raw gray");
      gavl_dictionary_set_int(s->m, GAVL_META_VIDEO_BPP, 2);
      break;
    case 36:
      /* 4 bit gray */
      priv->bytes_per_line = width / 2;
      priv->scanline_func = scanline_raw_4_gray;
      s->data.video.format->pixelformat = GAVL_RGB_24;

      gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                        "Quickime raw gray");
      gavl_dictionary_set_int(s->m, GAVL_META_VIDEO_BPP, 4);

      break;
    case 40:
      /* 8 bit gray */
      priv->bytes_per_line = width;
      priv->scanline_func = scanline_raw_8_gray;
      s->data.video.format->pixelformat = GAVL_RGB_24;
      gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                        "Quickime raw gray");
      gavl_dictionary_set_int(s->m, GAVL_META_VIDEO_BPP, 8);
      break;
    }
  if(priv->bytes_per_line & 1)
    priv->bytes_per_line++;
  
  return 1;
  
  fail:
  free(priv);
  return 0;
  }

static gavl_source_status_t decode_qtraw(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  int i;
  raw_priv_t * priv;
  uint8_t * src, *dst;
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
  src = p->buf.buf;
  dst = f->planes[0];
  
  for(i = 0; i < s->data.video.format->image_height; i++)
    {
    priv->scanline_func(src, dst, s->data.video.format->image_width,
                        s->data.video.pal->entries);
    src += priv->bytes_per_line;
    dst += f->strides[0];
    
    bgav_set_video_frame_from_packet(p, f);
    }
  bgav_stream_done_packet_read(s, p);
  return GAVL_SOURCE_OK;
  }

static void close_qtraw(bgav_stream_t * s)
  {
  raw_priv_t * priv;
  priv = s->decoder_priv;
  free(priv);
  }

static bgav_video_decoder_t decoder =
  {
    .name =   "Quicktime raw video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('r', 'a', 'w', ' '), 0x00  },
    .init =   init_qtraw,
    .decode = decode_qtraw,
    .close =  close_qtraw,
  };

void bgav_init_video_decoders_qtraw()
  {
  bgav_video_decoder_register(&decoder);
  }

