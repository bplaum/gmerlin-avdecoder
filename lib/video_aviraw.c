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
#include <stdio.h>

#define LOG_DOMAIN "video_aviraw"

/* Palette */

#if 0

static void scanline_1(uint8_t * src, uint8_t * dst,
                       int num_pixels, bgav_palette_entry_t * pal)
  {
  
  }

static void scanline_4(uint8_t * src, uint8_t * dst,
                       int num_pixels, bgav_palette_entry_t * pal)
  {
  
  }

#endif

static void scanline_8(uint8_t * src, uint8_t * dst,
                       int num_pixels, bgav_palette_entry_t * pal)
  {
  int i;
  for(i = 0; i < num_pixels; i++)
    {
    BGAV_PALETTE_2_RGB24(pal[*src], dst);
    src++;
    dst+=3;
    }
  }

static void scanline_8_gray(uint8_t * src, uint8_t * dst,
                            int num_pixels, bgav_palette_entry_t * pal)
  {
  int i;
  for(i = 0; i < num_pixels; i++)
    {
    *dst = *src;
    src++;
    dst++;
    }
  }

/* Non palette */

static void scanline_16(uint8_t * src, uint8_t * dst,
                        int num_pixels, bgav_palette_entry_t * pal)
  {
#ifndef WORDS_BIGENDIAN
  memcpy(dst, src, num_pixels * 2);
#else
  int i;
  for(i = 0; i < num_pixels; i++)
    {
    dst[2*i]   = src[2*i+1];
    dst[2*i+1] = src[2*i];
    }
#endif
  }

static void scanline_16_swap(uint8_t * src, uint8_t * dst,
                        int num_pixels, bgav_palette_entry_t * pal)
  {
#ifdef WORDS_BIGENDIAN
  memcpy(dst, src, num_pixels * 2);
#else
  int i;
  for(i = 0; i < num_pixels; i++)
    {
    dst[2*i]   = src[2*i+1];
    dst[2*i+1] = src[2*i];
    }
#endif
  }



static void scanline_24(uint8_t * src, uint8_t * dst,
                        int num_pixels, bgav_palette_entry_t * pal)
  {
  memcpy(dst, src, num_pixels * 3);
  }

static void scanline_32(uint8_t * src, uint8_t * dst,
                        int num_pixels, bgav_palette_entry_t * pal)
  {
  memcpy(dst, src, num_pixels * 4);
  }

typedef struct
  {
  void (*scanline_func)(uint8_t * src, uint8_t * dst,
                        int num_pixels, bgav_palette_entry_t * pal);
  int in_stride;

  /* Updated palette */
  bgav_palette_entry_t * pal;
  int pal_size;
  
  } aviraw_t;

static void close_aviraw(bgav_stream_t * s)
  {
  aviraw_t * priv;
  priv = s->decoder_priv;

  if(priv->pal)
    free(priv->pal);
  
  free(priv);
  }

static gavl_source_status_t decode_aviraw(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  int i;
  bgav_packet_t * p = NULL;
  aviraw_t * priv;
  gavl_source_status_t st;

  uint8_t * src;
  uint8_t * dst;

  priv = s->decoder_priv;

  while(1)
    {
    if((st = bgav_stream_get_packet_read(s, &p)) != GAVL_SOURCE_OK)
      return st;
    if(!p->data_size)
      bgav_stream_done_packet_read(s, p);
    else
      break;
    }

  /* Fetch palette */

  if(p->palette_size)
    {
    if(priv->pal_size && (p->palette_size != priv->pal_size))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Palette size changed %d -> %d",
               priv->pal_size, p->palette_size);
      return GAVL_SOURCE_EOF;
      }
    if(!priv->pal)
      priv->pal = malloc(p->palette_size * sizeof(priv->pal));
    memcpy(priv->pal, p->palette, p->palette_size * sizeof(priv->pal));
    }
  
  if(f)
    {
    /* RGB AVIs are upside down */
    src = p->data;
    dst = f->planes[0] + (s->data.video.format->image_height-1) * f->strides[0];
    for(i = 0; i < s->data.video.format->image_height; i++)
      {
      priv->scanline_func(src, dst, s->data.video.format->image_width,
                          priv->pal);
      src += priv->in_stride;
      dst -= f->strides[0];
      }

    bgav_set_video_frame_from_packet(p, f);
    }
  
  bgav_stream_done_packet_read(s, p);
  
  return GAVL_SOURCE_OK;
  }

static int init_aviraw(bgav_stream_t * s)
  {
  aviraw_t * priv;

  priv = calloc(1, sizeof(*priv));

  s->decoder_priv = priv;
  s->ci->flags &= ~GAVL_COMPRESSION_HAS_P_FRAMES;
  
  switch(s->data.video.depth)
    {
#if 0
    case 1:
      if(s->data.video.palette_size < 2)
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                 "Palette too small %d < 2",
                s->data.video.palette_size);
      priv->scanline_func = scanline_1;
      break;
    case 4:
      if(s->data.video.palette_size < 16)
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                 "Palette too small %d < 16",
                s->data.video.palette_size);
      priv->scanline_func = scanline_4;
      break;
#endif
    case 8:
      /* Depth 8 and no palette means grayscale */
      if(!s->data.video.pal.size)
        {
        priv->scanline_func = scanline_8_gray;
        s->data.video.format->pixelformat = GAVL_GRAY_8;
        }
      else
        {
        if(s->data.video.pal.size < 256)
          gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                   "Palette too small %d < 256",
                   s->data.video.pal.size);
        priv->scanline_func = scanline_8;
        s->data.video.format->pixelformat = GAVL_RGB_24;
        }
      break;
    case 16:
      if(s->fourcc == BGAV_MK_FOURCC('M','T','V',' '))
        {
        s->data.video.format->pixelformat = GAVL_RGB_16;
        priv->scanline_func = scanline_16_swap;
        }
      else
        {
        s->data.video.format->pixelformat = GAVL_RGB_15;
        priv->scanline_func = scanline_16;
        }
      break;
    case 24:
      priv->scanline_func = scanline_24;
      s->data.video.format->pixelformat = GAVL_BGR_24;
      break;
    case 32:
      priv->scanline_func = scanline_32;
      s->data.video.format->pixelformat = GAVL_BGR_32;
      break;
    default:
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Unsupported depth: %d", s->data.video.depth);
      return 0;
      break;
    }

  priv->in_stride = (s->data.video.format->image_width * s->data.video.depth + 7) / 8;

  /* Padd to 4 byte */
  if(priv->in_stride % 4)
    {
    priv->in_stride += (4 - (priv->in_stride % 4));
    }
  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT, "AVI raw");
  return 1;
  }

static bgav_video_decoder_t decoder =
  {
    .name =   "Raw video decoder for AVI",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('R', 'G', 'B', ' '),
                            BGAV_MK_FOURCC('M', 'T', 'V', ' '),
                            /* RGB3 is used by NSV, but seems to be the same as 24 bpp AVI */
                            BGAV_MK_FOURCC('R', 'G', 'B', '3'),
                            0x00  },
    .init =   init_aviraw,
    .decode = decode_aviraw,
    .close =  close_aviraw,
    .resync = NULL,
  };

void bgav_init_video_decoders_aviraw()
  {
  bgav_video_decoder_register(&decoder);
  }
