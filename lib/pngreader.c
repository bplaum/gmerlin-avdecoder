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
#include <stdio.h>
#include <string.h>
#include <bgav_version.h>

#include <avdec_private.h>
#include <pngreader.h>

#include <png.h>

#define LOG_DOMAIN "pngreader"

struct bgav_png_reader_s
  {
  png_structp png_ptr;
  png_infop info_ptr;
  png_infop end_info;

  int buffer_position;
  int buffer_size;
  uint8_t * buffer;
  
  gavl_video_format_t format;
  int depth;
  };

bgav_png_reader_t * bgav_png_reader_create(int depth)
  {
  bgav_png_reader_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->depth = depth;
  return ret;
  }

void bgav_png_reader_destroy(bgav_png_reader_t* png)
  {
  bgav_png_reader_reset(png);
  free(png);
  }

/* This function will be passed to libpng for reading data */

static void read_function(png_structp png_ptr,
                          png_bytep data,
                          png_size_t length)
  {
  bgav_png_reader_t * priv = (bgav_png_reader_t*)png_get_io_ptr(png_ptr);
  
  if((long)(length + priv->buffer_position) <= priv->buffer_size)
    {
    memcpy(data, priv->buffer + priv->buffer_position, length);
    priv->buffer_position += length;
    }
  }

int bgav_png_reader_read_header(const bgav_options_t * opt,
                                bgav_png_reader_t * png,
                                uint8_t * buffer, int buffer_size,
                                gavl_video_format_t * format)
  {
  int bit_depth;
  int color_type;
  int has_alpha = 0;
  int is_gray = 0;
  int bits = 8;

  png->buffer          = buffer;
  png->buffer_size     = buffer_size;
  png->buffer_position = 0;
  
  if(png_sig_cmp(png->buffer, 0, 8))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "No png data");
    goto fail;
    }
  png->png_ptr = png_create_read_struct
    (PNG_LIBPNG_VER_STRING, NULL,
     NULL, NULL);

  setjmp(png_jmpbuf(png->png_ptr));

  png->info_ptr = png_create_info_struct(png->png_ptr);

  if(!png->info_ptr)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Creating info struct failed");
    goto fail;
    }
  png->end_info = png_create_info_struct(png->png_ptr);

  if(!png->end_info)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Creating end info failed");
    goto fail;
    }
  png_set_read_fn(png->png_ptr, png, read_function);
  
  png_read_info(png->png_ptr, png->info_ptr);
  
  png->format.frame_width  = png_get_image_width(png->png_ptr, png->info_ptr);
  png->format.frame_height = png_get_image_height(png->png_ptr, png->info_ptr);

  png->format.image_width  = png->format.frame_width;
  png->format.image_height = png->format.frame_height;
  png->format.pixel_width = 1;
  png->format.pixel_height = 1;

  bit_depth  = png_get_bit_depth(png->png_ptr,  png->info_ptr);
  color_type = png_get_color_type(png->png_ptr, png->info_ptr);

  switch(color_type)
    {
    case PNG_COLOR_TYPE_GRAY:       /*  (bit depths 1, 2, 4, 8, 16) */
      if(bit_depth == 16)
        {
#ifndef WORDS_BIGENDIAN
        png_set_swap(png->png_ptr);
#endif
        bits = 16;
        }
      if(bit_depth < 8)
#if BGAV_MAKE_BUILD(PNG_LIBPNG_VER_MAJOR, PNG_LIBPNG_VER_MINOR, PNG_LIBPNG_VER_RELEASE) < BGAV_MAKE_BUILD(1,2,9)
        png_set_gray_1_2_4_to_8(png->png_ptr);
#else
      png_set_expand_gray_1_2_4_to_8(png->png_ptr);
#endif
      if (png_get_valid(png->png_ptr, png->info_ptr, PNG_INFO_tRNS))
        {
        png_set_tRNS_to_alpha(png->png_ptr);
        has_alpha = 1;
        }
      is_gray = 1;
      break;
    case PNG_COLOR_TYPE_GRAY_ALPHA: /*  (bit depths 8, 16) */
      if(bit_depth == 16)
        {
#ifndef WORDS_BIGENDIAN
        png_set_swap(png->png_ptr);
#endif
        bits = 16;
        }
      has_alpha = 1;
      is_gray = 1;
      break;
    case PNG_COLOR_TYPE_PALETTE:    /*  (bit depths 1, 2, 4, 8) */
      png_set_palette_to_rgb(png->png_ptr);
      if (png_get_valid(png->png_ptr, png->info_ptr, PNG_INFO_tRNS))
        {
        png_set_tRNS_to_alpha(png->png_ptr);
        has_alpha = 1;
        }
      break;
    case PNG_COLOR_TYPE_RGB:        /*  (bit_depths 8, 16) */
      if(png_get_valid(png->png_ptr, png->info_ptr, PNG_INFO_tRNS))
        {
        png_set_tRNS_to_alpha(png->png_ptr);
        has_alpha = 1;
        }
      if(bit_depth == 16)
        {
#ifndef WORDS_BIGENDIAN
        png_set_swap(png->png_ptr);
#endif
        bits = 16;
        }
      break;
    case PNG_COLOR_TYPE_RGB_ALPHA:  /*  (bit_depths 8, 16) */
      if(bit_depth == 16)
        {
#ifndef WORDS_BIGENDIAN
        png_set_swap(png->png_ptr);
#endif
        bits = 16;
        }
      has_alpha = 1;
      break;
    }

  if(bits == 8)
    {
    if(is_gray)
      {
      if(has_alpha)
        png->format.pixelformat = GAVL_GRAYA_16;
      else
        png->format.pixelformat = GAVL_GRAY_8;
      }
    else if(has_alpha && (png->depth != 24))
      png->format.pixelformat = GAVL_RGBA_32;
    else
      png->format.pixelformat = GAVL_RGB_24;
    }
  else if(bits == 16)
    {
    if(is_gray)
      {
      if(has_alpha)
        png->format.pixelformat = GAVL_GRAYA_32;
      else
        png->format.pixelformat = GAVL_GRAY_16;
      }
    else if(has_alpha)
      png->format.pixelformat = GAVL_RGBA_64;
    else
      png->format.pixelformat = GAVL_RGB_48;
    }

  if(!gavl_pixelformat_has_alpha(png->format.pixelformat) &&
     has_alpha)
    png_set_strip_alpha(png->png_ptr);
  
  if(format)
    {
    /* We set only frame dimensions and pixelformat since
       all other things are already set by the demuxer */
    format->image_width  = png->format.image_width;
    format->image_height = png->format.image_height;
    format->frame_width  = png->format.frame_width;
    format->frame_height = png->format.frame_height;
    format->pixelformat = png->format.pixelformat;
    }
    
  return 1;
  fail:
  return 0;

  }

void bgav_png_reader_reset(bgav_png_reader_t * png)
  {
  if(png->png_ptr || png->info_ptr || png->end_info)
    png_destroy_read_struct(&png->png_ptr, &png->info_ptr,
                            &png->end_info);
  
  png->png_ptr = NULL;
  png->info_ptr = NULL;
  png->end_info = NULL;
  }

int bgav_png_reader_read_image(bgav_png_reader_t * png,
                               gavl_video_frame_t * frame)
  {
  int i;
  unsigned char ** rows;

  setjmp(png_jmpbuf(png->png_ptr));
  
  rows = malloc(png->format.frame_height * sizeof(*rows));
  for(i = 0; i < png->format.frame_height; i++)
    rows[i] = frame->planes[0] + i * frame->strides[0];
  
  png_read_image(png->png_ptr, rows);
  png_read_end(png->png_ptr, png->end_info);
  
  bgav_png_reader_reset(png);
  free(rows);
  return 1;
  }

