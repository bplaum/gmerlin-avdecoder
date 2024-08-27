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



#include <inttypes.h>
#include <string.h>

#include <avdec_private.h>
#include <dirac_header.h>
#include <bitstream.h>

#define LOG_DOMAIN "dirac_header"


int bgav_dirac_get_code(uint8_t * data, int len, int * size)
  {
  uint32_t fourcc;
  uint8_t parse_code;
  
  fourcc = GAVL_PTR_2_32BE(data); data+=4;
  if(fourcc != BGAV_MK_FOURCC('B','B','C','D'))
    return DIRAC_CODE_ERROR; // Lost sync
  parse_code = *data; data++;
  
  if(size)
    {
    if(parse_code == 0x10)
      *size = 13;
    else
      *size = GAVL_PTR_2_32BE(data);
    }
  data+=4;

  if(parse_code == 0x00)
    return DIRAC_CODE_SEQUENCE;
  else if(parse_code == 0x10)
    return DIRAC_CODE_END;
  else if((parse_code & 0x08) == 0x08)
    return DIRAC_CODE_PICTURE;
  return DIRAC_CODE_OTHER;
  }

/*
 *  Predefined video formats
 *  taken from schroedinger/schroedinger/schrovideoformat.c
 *
 *  static SchroVideoFormat schro_video_formats[] = ...
 */

static const struct
  {
  int width;
  int height;
  int timescale;
  int frame_duration;
  int pixel_width;
  int pixel_height;
  int source_sampling;
  int top_first;
  }
video_formats[] =
  {
    { /* Custom */
      .width           = 640,
      .height          = 480,
      .timescale       = 24000,
      .frame_duration  = 1001,
      .pixel_width     = 1,
      .pixel_height    = 1,
      .source_sampling = 0,
      .top_first       = 0,
    },
    { /* QSIF525 */
      .width           = 176,
      .height          = 120,
      .timescale       = 15000,
      .frame_duration  = 1001,
      .pixel_width     = 10,
      .pixel_height    = 11,
      .source_sampling = 0,
      .top_first       = 0,
    },
    { /* QCIF */
      .width           = 176,
      .height          = 144,
      .timescale       = 25,
      .frame_duration  =  2,
      .pixel_width     = 12,
      .pixel_height    = 11,
      .source_sampling = 0,
      .top_first       = 1,
    },
    { /* SIF525 */
      .width           = 352,
      .height          = 240,
      .timescale       = 15000,
      .frame_duration  = 1001,
      .pixel_width     = 10,
      .pixel_height    = 11,
      .source_sampling = 0,
      .top_first       = 0,
    },
    { /* CIF */
      .width           = 352,
      .height          = 288,
      .timescale       = 25,
      .frame_duration  = 2,
      .pixel_width     = 12,
      .pixel_height    = 11,
      .source_sampling = 0,
      .top_first       = 1,
    },
    { /* 4SIF525 */
      .width           = 704,
      .height          = 480,
      .timescale       = 15000,
      .frame_duration  = 1001,
      .pixel_width     = 10,
      .pixel_height    = 11,
      .source_sampling = 0,
      .top_first       = 0,
    },
    { /* 4CIF */
      .width           = 704,
      .height          = 576,
      .timescale       = 25,
      .frame_duration  = 2,
      .pixel_width     = 12,
      .pixel_height    = 11,
      .source_sampling = 0,
      .top_first       = 1,
    },
    { /* SD480I-60 */
      .width           = 720,
      .height          = 480,
      .timescale       = 30000,
      .frame_duration  = 1001,
      .pixel_width     = 10,
      .pixel_height    = 11,
      .source_sampling = 1,
      .top_first       = 0,
    },
    { /* SD576I-50 */
      .width           = 720,
      .height          = 576,
      .timescale       = 25,
      .frame_duration  = 1,
      .pixel_width     = 12,
      .pixel_height    = 11,
      .source_sampling = 1,
      .top_first       = 1,
    },
    { /* HD720P-60 */
      .width           = 1280,
      .height          = 720,
      .timescale       = 60000,
      .frame_duration  = 1001,
      .pixel_width     = 1,
      .pixel_height    = 1,
      .source_sampling = 0,
      .top_first       = 1,
    },
    { /* HD720P-50 */
      .width           = 1280,
      .height          = 720,
      .timescale       = 50,
      .frame_duration  = 1,
      .pixel_width     = 1,
      .pixel_height    = 1,
      .source_sampling = 0,
      .top_first       = 1,
    },
    { /* HD1080I-60 */
      .width           = 1920,
      .height          = 1080,
      .timescale       = 30000,
      .frame_duration  = 1001,
      .pixel_width     = 1,
      .pixel_height    = 1,
      .source_sampling = 1,
      .top_first       = 1,
    },
    { /* HD1080I-50 */
      .width           = 1920,
      .height          = 1080,
      .timescale       = 25,
      .frame_duration  = 1,
      .pixel_width     = 1,
      .pixel_height    = 1,
      .source_sampling = 1,
      .top_first       = 1,
    },
    { /* HD1080P-60 */
      .width           = 1920,
      .height          = 1080,
      .timescale       = 60000,
      .frame_duration  = 1001,
      .pixel_width     = 1,
      .pixel_height    = 1,
      .source_sampling = 0,
      .top_first       = 1,
    },
    { /* HD1080P-50 */
      .width           = 1920,
      .height          = 1080,
      .timescale       = 50,
      .frame_duration  = 1,
      .pixel_width     = 1,
      .pixel_height    = 1,
      .source_sampling = 0,
      .top_first       = 1,
    },
    { /* DC2K */
      .width           = 2048,
      .height          = 1080,
      .timescale       = 24,
      .frame_duration  = 1,
      .pixel_width     = 1,
      .pixel_height    = 1,
      .source_sampling = 0,
      .top_first       = 1,
    },
    { /* DC4K */
      .width           = 4096,
      .height          = 2160,
      .timescale       = 24,
      .frame_duration  = 1,
      .pixel_width     = 1,
      .pixel_height    = 1,
      .source_sampling = 0,
      .top_first       = 1,
    },
    { /* UHDTV 4K-60 */
      .width           = 3840,
      .height          = 2160,
      .timescale       = 60000,
      .frame_duration  = 1001,
      .pixel_width     = 1,
      .pixel_height    = 1,
      .source_sampling = 0,
      .top_first       = 1,
    },
    { /* UHDTV 4K-50 */
      .width           = 3840,
      .height          = 2160,
      .timescale       = 50,
      .frame_duration  = 1,
      .pixel_width     = 1,
      .pixel_height    = 1,
      .source_sampling = 0,
      .top_first       = 1,
    },
    { /* UHDTV 8K-60 */
      .width           = 7680,
      .height          = 4320,
      .timescale       = 60000,
      .frame_duration  = 1001,
      .pixel_width     = 1,
      .pixel_height    = 1,
      .source_sampling = 0,
      .top_first       = 1,
    },
    { /* UHDTV 8K-50 */
      .width           = 7680,
      .height          = 4320,
      .timescale       = 50,
      .frame_duration  = 1,
      .pixel_width     = 1,
      .pixel_height    = 1,
      .source_sampling = 0,
      .top_first       = 1,
    },
    
  };

static const struct
  {
  int timescale;
  int frame_duration;
  }
framerates[] =
  {
    { /* Custom */ },
    { 24000, 1001 },
    {    24,    1 },
    {    25,    1 },
    { 30000, 1001 },
    {    30,    1 },
    {    50,    1 },
    { 60000, 1001 },
    {    60,    1 },
    { 15000, 1001 },
    {    25,    2 },
  };

static const struct
  {
  int pixel_width;
  int pixel_height;
  }
pixel_aspect_ratios[] =
  {
    { /* Custom */ },
    {  1,  1 }, /* Square Pixels                 */
    { 10, 11 }, /* 525-line systems              */
    { 12, 11 }, /* 625-line systems              */
    { 40, 33 }, /* 16:9 525-line systems         */
    { 16, 11 }, /* 16:9 625-line systems         */
    {  4,  3 }, /* reduced horizontal resolution */
  };

/* A.3.2 Unsigned interleaved exp-Golomb codes */

static int read_uint(bgav_bitstream_t * b, int * ret)
  {
  int value = 1;
  int bit;
  while(1)
    {
    if(!bgav_bitstream_get(b, &bit, 1))
      return 0;
    if(!bit)
      {
      value <<= 1;
      if(!bgav_bitstream_get(b, &bit, 1))
        return 0;
      value |= bit;
      }
    else
      break;
    }
  value -= 1;
  *ret = value;
  return 1;
  }

int bgav_dirac_sequence_header_parse(bgav_dirac_sequence_header_t * ret,
                                     const uint8_t * buffer, int len)
  {
  bgav_bitstream_t b;
  int dummy;

  /* Skip parse info */
  buffer += 13;
  len -= 13;

  memset(ret, 0, sizeof(*ret));
  
  bgav_bitstream_init(&b, buffer, len);

  if(!read_uint(&b, &ret->version_major) ||
     !read_uint(&b, &ret->version_minor) ||
     !read_uint(&b, &ret->profile) ||
     !read_uint(&b, &ret->level) ||
     !read_uint(&b, &ret->base_video_format))
    return 0;

  /* Set default video format. Values might be overridden later. */
  
  if((ret->base_video_format >= 0) && 
     (ret->base_video_format <= 20))
    {
    ret->width  = video_formats[ret->base_video_format].width;
    ret->height = video_formats[ret->base_video_format].height;
    ret->timescale = video_formats[ret->base_video_format].timescale;
    ret->frame_duration = video_formats[ret->base_video_format].frame_duration;
    ret->pixel_width  = video_formats[ret->base_video_format].pixel_width;
    ret->pixel_height = video_formats[ret->base_video_format].pixel_height;
    ret->source_sampling    = video_formats[ret->base_video_format].source_sampling;
    ret->top_first    = video_formats[ret->base_video_format].top_first;
    }

  /* 10.3.2 Frame size */
  if(!bgav_bitstream_get(&b, &dummy, 1))
    return 0;
  if(dummy)
    {
    if(!read_uint(&b, &ret->width) ||
       !read_uint(&b, &ret->height))
      return 0;
    }

  /* 10.3.3 Chroma sampling format */
  if(!bgav_bitstream_get(&b, &dummy, 1))
    return 0;
  if(dummy)
    {
    if(!read_uint(&b, &dummy))
      return 0;
    }

  /* 10.3.4 Scan format */
  if(!bgav_bitstream_get(&b, &dummy, 1))
    return 0;
  if(dummy)
    {
    if(!read_uint(&b, &ret->source_sampling))
      return 0;
    }

  /* 10.3.5 Frame rate */
  if(!bgav_bitstream_get(&b, &dummy, 1))
    return 0;
  if(dummy)
    {
    int index;
    if(!read_uint(&b, &index))
      return 0;
    if(!index)
      {
      if(!read_uint(&b, &ret->timescale) ||
         !read_uint(&b, &ret->frame_duration))
        return 0;
      }
    else if((index > 0) && (index <= 10))
      {
      ret->timescale      = framerates[index].timescale;
      ret->frame_duration = framerates[index].frame_duration;
      }
    }

  /* 10.3.6 Pixel aspect ratio */
  if(!bgav_bitstream_get(&b, &dummy, 1))
    return 0;
  if(dummy)
    {
    int index;
    if(!read_uint(&b, &index))
      return 0;
    if(!index)
      {
      if(!read_uint(&b, &ret->pixel_width) ||
         !read_uint(&b, &ret->pixel_height))
        return 0;
      }
    else if((index > 0) && (index <= 6))
      {
      ret->pixel_width  = pixel_aspect_ratios[index].pixel_width;
      ret->pixel_height = pixel_aspect_ratios[index].pixel_height;
      }
    }

  /* 10.3.7 Clean area */
  if(!bgav_bitstream_get(&b, &dummy, 1))
    return 0;
  if(dummy)
    {
    if(!read_uint(&b, &dummy) ||
       !read_uint(&b, &dummy) ||
       !read_uint(&b, &dummy) ||
       !read_uint(&b, &dummy))
      return 0;
    }

  /* 10.3.8 Signal range */

  if(!bgav_bitstream_get(&b, &dummy, 1))
    return 0;
  if(dummy)
    {
    if(!read_uint(&b, &dummy))
      return 0;

    if(!dummy)
      {
      if(!read_uint(&b, &dummy) ||
         !read_uint(&b, &dummy) ||
         !read_uint(&b, &dummy) ||
         !read_uint(&b, &dummy))
        return 0;
      }
    }

  /* 10.3.9 Color specification */

  if(!bgav_bitstream_get(&b, &dummy, 1))
    return 0;
  if(dummy)
    {
    if(!read_uint(&b, &dummy))
      return 0;

    if(!dummy)
      {
      /* 10.3.9.1 Color primaries */
      if(!bgav_bitstream_get(&b, &dummy, 1))
        return 0;
      if(dummy)
        {
        if(!read_uint(&b, &dummy))
          return 0;
        }
      
      /* 10.3.9.2 Color matrix */
      if(!bgav_bitstream_get(&b, &dummy, 1))
        return 0;
      if(dummy)
        {
        if(!read_uint(&b, &dummy))
          return 0;
        }
      
      /* 10.3.9.3 Transfer function */
      if(!bgav_bitstream_get(&b, &dummy, 1))
        return 0;
      if(dummy)
        {
        if(!read_uint(&b, &dummy))
          return 0;
        }
      }
    }
  /* 10.4 Picture coding mode */
  if(!read_uint(&b, &ret->picture_coding_mode))
    return 0;
  
  return 1;
  }

void bgav_dirac_sequence_header_dump(const bgav_dirac_sequence_header_t * h)
  {
  gavl_dprintf("Dirac sequence header:\n");
  gavl_dprintf("  version_major:     %d\n", h->version_major);
  gavl_dprintf("  version_minor:     %d\n", h->version_minor);
  gavl_dprintf("  profile:           %d\n", h->profile);
  gavl_dprintf("  level:             %d\n", h->level);
  gavl_dprintf("  base_video_format: %d\n", h->base_video_format);
  gavl_dprintf("  size:              %dx%d\n", h->width, h->height);
  gavl_dprintf("  framerate:         %d:%d\n", h->timescale , h->frame_duration);
  gavl_dprintf("  pixel_size:        %dx%d\n", h->pixel_width , h->pixel_height);
  }

int bgav_dirac_picture_header_parse(bgav_dirac_picture_header_t * ret,
                                    const uint8_t * buffer, int len)
  {
  int parse_code;
  
  if(len < 17)
    return 0;
  
  parse_code = buffer[4];
  
  ret->num_refs = parse_code & 0x03;
  ret->pic_num = GAVL_PTR_2_32BE(buffer + 13);
  return 1;
  }

void bgav_dirac_picture_header_dump(const bgav_dirac_picture_header_t * h)
  {
  gavl_dprintf("Dirac picture header\n");
  gavl_dprintf("  Num refs: %d\n", h->num_refs);
  gavl_dprintf("  Pic num:  %d\n",     h->pic_num);
  }
