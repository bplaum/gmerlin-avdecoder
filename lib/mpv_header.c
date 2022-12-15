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

#include <inttypes.h>
#include <avdec_private.h>
#include <mpv_header.h>
#include <math.h>

#define LOG_DOMAIN "mpv_header"

/* Optimized version 17% faster(for the complete index build)
   than the ffmpeg one */

static inline const uint8_t *
first_zero_byte(const uint8_t *p, int len)
  {
  int num, done = 0, i, j;

  /* Align pointer to 8 byte boundary */
  num = (unsigned long)(p) & 0x07;
  if(num > len)
    num = len;
  
  i = num+1;
  while(--i)
    {
    if(!(*p))
      return p;
    p++;
    }

  done += num;

  /* Main loop: Take 8 bytes at once */
  num = (len - done)/8;
  i = num+1;
  while(--i)
    {
    const uint64_t x = *(const uint64_t*)p;
    
    if((x - 0x0101010101010101LL) & (~x) & 0x8080808080808080LL)
      {
      j = 9;
      while(--j)
        {
        if(!(*p))
          return p;
        p++;
        }
      }
    else
      p += 8;
    }
  done += num * 8;

  /* Remainder */
  num = len - done;
  i = num+1;
  while(--i)
    {
    if(!(*p))
      return p;
    p++;
    }
  return NULL;
  }
  
const uint8_t * bgav_mpv_find_startcode( const uint8_t *p,
                                         const uint8_t *end )
  {
  const uint8_t * ptr;
  /* Subtract 2 because we want to get the *whole* code */
  int len = end - p - 2;

  if(len <= 0) /* Reached end */
    return NULL;
  
  while(1)
    {
    ptr = first_zero_byte(p, len);
    
    if(!ptr) /* Reached end */
      break;

    if((ptr[1] == 0x00) && (ptr[2] == 0x01))  /* Found startcode */
      return ptr;
    
    /* Skip this zero byte */
    p = ptr+1;

    len = end - p - 2;
    
    if(len <= 0) /* Reached end */
      break;
    }
  return NULL;
  }

int bgav_mpv_get_start_code(const uint8_t * data, int get_ext)
  {
  switch(data[3])
    {
    case 0xb3:
      return MPEG_CODE_SEQUENCE;
      break;
    case 0xb5:
      if(!get_ext)
        return MPEG_CODE_EXTENSION;
      switch(data[4] >> 4)
        {
        case 0x01:
          return MPEG_CODE_SEQUENCE_EXT;
          break;
        case 0x08:
          return MPEG_CODE_PICTURE_EXT;
          break;
        case 0x02:
          return MPEG_CODE_SEQUENCE_DISPLAY_EXT;
          break;
        }
      break;
    case 0x00:
      return MPEG_CODE_PICTURE;
      break;
    case 0xb8:
      return MPEG_CODE_GOP;
      break;
    case 0xb7:
      return MPEG_CODE_END;
      break;
    }
  if((data[3] >= 0x01) && (data[3] <= 0xaf))
    return MPEG_CODE_SLICE;
  return 0;
  }

int bgav_mpv_sequence_display_extension_parse(bgav_mpv_sequence_display_extension_t * ret,
                                              const uint8_t * buffer, int len)
  {
  int num = 4 + 4;
  
  buffer += 4;
  len -= 4;

  ret->video_format = (buffer[0] & 0x0f) >> 1;
  
  if(buffer[0] & 0x01)
    {
    if(len < 8)
      return 0;

    ret->has_color_description    = 1;
    ret->color_primaries          = buffer[1];
    ret->transfer_characteristics = buffer[2];
    ret->matrix_coefficients      = buffer[3];
    buffer += 3;
    num += 3;
    }
  else if(len < 5)
    return 0;

  ret->display_width = (buffer[1] << 6) | (buffer[2] >> 2);
  ret->display_height = ((buffer[2]& 1 ) << 13) | (buffer[3] << 5) | (buffer[4] >> 3);

  //  fprintf(stderr, "Got display width: %d %d\n",
  //          ret->display_width, ret->display_height);
  
  return num;
  }

static const struct
  {
  uint32_t timescale;
  uint32_t frame_duration;
  }
framerates[] =
  {
    {    -1,   -1 }, // forbidden
    { 24000, 1001 }, // 24000:1001 (23,976...)
    {    24,    1 }, // 24
    {    25,    1 }, // 25
    { 30000, 1001 }, // 30000:1001 (29,97...)
    {    30,    1 }, // 30
    {    50,    1 }, // 50
    { 60000, 1001 }, // 60000:1001 (59,94...)
    {    60,    1 }, // 60
  // Xing's 15fps: (9)
    {   15,    1},
  // libmpeg3's "Unofficial economy rates": (10-13)
    {    5,    1},
    {   10,    1},
    {   12,    1},
    {   15,    1},
    {    -1,   -1 }, // reserved
  };

void bgav_mpv_get_framerate(int code, uint32_t * timescale, uint32_t * frame_duration)
  {
  *timescale      = framerates[code].timescale;
  *frame_duration = framerates[code].frame_duration;
  }

int bgav_mpv_sequence_header_parse(bgav_mpv_sequence_header_t * ret,
                                   const uint8_t * buffer, int len)
  {
  int i;
  buffer += 4;
  len -= 4;

  /*
   * 12 uimsbf horizontal_size_value
   * 12 uimsbf vertical_size_value
   *  4 uimsbf aspect_ratio_information
   *  4 uimsbf frame_rate_code
   * 18 uimsbf bit_rate_value
   *  1  bslbf marker_bit
   * 10 uimsbf vbv_buffer_size_value 
   */
  
  if(len < 8)
    return 0;

  if((buffer[6] & 0x20) != 0x20)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Cannot read sequence header: missing marker bit");
    return -1;        /* missing marker_bit */
    }

  i = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];
  
  ret->horizontal_size_value = i >> 12;
  ret->vertical_size_value = i & 0xfff;
  ret->aspect_ratio = buffer[3] >> 4;
  ret->frame_rate_index = buffer[3] & 0xf;

  //  ret->timescale      = framerates[frame_rate_index].timescale;
  //  ret->frame_duration = framerates[frame_rate_index].frame_duration;
 
  ret->bitrate = (buffer[4]<<10)|(buffer[5]<<2)|(buffer[6]>>6);
  
  ret->vbv_buffer_size_value =
    (((buffer[6]<<8)|(buffer[7])) >> 3) & 0x000003ff;
    return 7;
  }


int bgav_mpv_sequence_extension_parse(bgav_mpv_sequence_extension_t * ret,
                                      const uint8_t * buffer, int len)
  {
  buffer += 4;
  len -= 4;
  if(len < 6)
    return 0;

  ret->profile_level_id = ((buffer[0] & 0x0f) << 4) | (buffer[1] >> 4);
  ret->progressive_sequence = buffer[1] & (1 << 3);
  
  ret->chroma_format = (buffer[1] & 0x06) >> 1;
  
  ret->horizontal_size_ext  = ((buffer[1] << 13) | (buffer[2] << 5)) & 0x3000;
  ret->vertical_size_ext    = (buffer[2] << 7) & 0x3000;
  
  ret->bitrate_ext          = ((buffer[2] & 0x1F)<<7) | (buffer[3]>>1);

  ret->vbv_buffer_size_ext  = buffer[4];
  
  ret->low_delay            = !!(buffer[5] & 0x80);
  ret->timescale_ext        = (buffer[5] >> 5) & 3;
  ret->frame_duration_ext   = (buffer[5] & 0x1f);

  
  return 1;
  }

int bgav_mpv_picture_header_parse(bgav_mpv_picture_header_t * ret,
                                  const uint8_t * buffer, int len)
  {
  int type;
  buffer += 4;
  len -= 4;

  if(len < 2)
    return 0;
  type = (buffer[1] >> 3) & 7;
  switch(type)
    {
    case 1:
      ret->coding_type = BGAV_CODING_TYPE_I;
      break;
    case 2:
      ret->coding_type = BGAV_CODING_TYPE_P;
      break;
    case 3:
      ret->coding_type = BGAV_CODING_TYPE_B;
      break;
    default:
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Cannot read picture header: Invalid coding type %d", type);
      return -1; // Error
    }
  return 2;
  }

int bgav_mpv_picture_extension_parse(bgav_mpv_picture_extension_t * ret,
                                     const uint8_t * buffer, int len)
  {
  buffer += 4;
  len -= 4;

  if(len < 5)
    return 0;
  ret->picture_structure  = buffer[2] & 0x03;
  ret->top_field_first    = buffer[3] & (1 << 7);
  ret->repeat_first_field = buffer[3] & (1 << 1);
  ret->progressive_frame  = buffer[4] & (1 << 7);
  return 5;
  }

int bgav_mpv_gop_header_parse(bgav_mpv_gop_header_t * ret,
                              const uint8_t * buffer, int len)
  {
  buffer += 4;
  len -= 4;

  if(len < 4)
    return 0;
  
  /* Gop header has a fixed length of 27 bytes */

  ret->drop    = buffer[0] >> 7;
  ret->hours   = (buffer[0] >> 2) & 31;
  ret->minutes = ((buffer[0] << 4) | (buffer[1] >> 4)) & 63;
  ret->seconds = ((buffer[1] << 3) | (buffer[2] >> 5)) & 63;
  ret->frames  = ((buffer[2] << 1) | (buffer[3] >> 7)) & 63;
  
  return 1;
  }

/* Aspect ratio stuff (taken from ffmpeg) */

static const struct
  {
  int num;
  int den;
  }
mpeg1_aspect[16] =
  {
    {   1,   1 }, // 0.0000
    {   1,   1 }, // 1.0000
    {  49,  33 }, // 0.6735
    {  64,  45 }, // 0.7031
    { 239, 182 }, // 0.7615
    {  36,  29 }, // 0.8055
    {  32,  27 }, // 0.8437
    { 169, 151 }, // 0.8935
    { 178, 163 }, // 0.9157
    {  54,  53 }, // 0.9815
    { 196, 201 }, // 1.0255
    { 187, 200 }, // 1.0695
    { 200, 219 }, // 1.0950
    { 127, 147 }, // 1.1575
    { 134, 161 }, // 1.2015
    {   1,   1 }, // 
  };

static const struct
  {
  int num;
  int den;
  }
mpeg2_aspect[16]=
  {
    {0,1},
    {1,1},
    {4,3},
    {16,9},
    {221,100},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
    {0,1},
  };

static int get_gcd(int a, int b)
  {
  if(b)
    return get_gcd(b, a%b);
  else
    return a;
  }

void bgav_mpv_get_pixel_aspect(bgav_mpv_sequence_header_t * h,
                               gavl_video_format_t * ret)
  {
  if(!h->mpeg2) /* MPEG-1 */
    {
    ret->pixel_width = mpeg1_aspect[h->aspect_ratio].num;
    ret->pixel_height = mpeg1_aspect[h->aspect_ratio].den;
    }
  else /* MPEG-2 */
    {
    if(h->aspect_ratio > 1)
      {
      int gcd;
      ret->pixel_width = mpeg2_aspect[h->aspect_ratio].num *
        ret->image_height;
      ret->pixel_height = mpeg2_aspect[h->aspect_ratio].den *
        ret->image_width;
      gcd = get_gcd(ret->pixel_width, ret->pixel_height);
      ret->pixel_width  /= gcd;
      ret->pixel_height /= gcd;
      }
    else
      {
      ret->pixel_width  = 1;
      ret->pixel_height = 1;
      }
    }
  }

gavl_pixelformat_t bgav_mpv_get_pixelformat(bgav_mpv_sequence_header_t * h)
  {
  gavl_pixelformat_t ret;

  ret = GAVL_YUV_420_P;
  
  if(h->mpeg2)
    {
    if(h->ext.chroma_format == 2)
      ret = GAVL_YUV_422_P;
    else if(h->ext.chroma_format == 3)
      ret = GAVL_YUV_422_P;
    }
  return ret;
  }

void bgav_mpv_get_size(bgav_mpv_sequence_header_t * h,
                       gavl_video_format_t * ret)
  {
  ret->image_width = h->horizontal_size_value;
  ret->image_height = h->vertical_size_value;
  
  if(h->mpeg2)
    {
    ret->image_width  += h->ext.horizontal_size_ext << 12;
    ret->image_height += h->ext.vertical_size_ext << 12;
    }

  ret->frame_width  = ((ret->image_width  + 15) / 16) * 16;
  ret->frame_height = ((ret->image_height + 15) / 16) * 16;
  
  }
