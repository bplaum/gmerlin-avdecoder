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



#include <string.h>

#include <avdec_private.h>
#include <mpeg4_header.h>
#include <mpv_header.h>
#include <bitstream.h>

static const struct
  {
  int w;
  int h;
  }
pixel_aspect[16] =
  {
    { 0, 0 },
    { 1, 1 },
    { 12, 11 },
    { 10, 11 },
    { 16, 11 },
    { 40, 33 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
  };

void bgav_mpeg4_get_pixel_aspect(bgav_mpeg4_vol_header_t * h,
                                 uint32_t * width, uint32_t * height)
  {
  if(h->aspect_ratio_info == 15)
    {
    *width  = h->par_width;
    *height = h->par_height;
    return;
    }
  if(pixel_aspect[h->aspect_ratio_info].w)
    {
    *width = pixel_aspect[h->aspect_ratio_info].w;
    *height = pixel_aspect[h->aspect_ratio_info].h;
    }
  else
    {
    *width = 1;
    *height = 1;
    }
  }
  
/* log2 ripped from ffmpeg (maybe move to central place?) */

const uint8_t log2_tab[256]={
        0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

static inline int bgav_log2(unsigned int v)
{
    int n = 0;
    if (v & 0xffff0000) {
        v >>= 16;
        n += 16;
    }
    if (v & 0xff00) {
        v >>= 8;
        n += 8;
    }
    n += log2_tab[v];

    return n;
}


int bgav_mpeg4_get_start_code(const uint8_t * data)
  {
  if(data[3] <= 0x1f)
    return MPEG4_CODE_VO_START;
  else if(data[3] <= 0x2f)
    return MPEG4_CODE_VOL_START;
  else if(data[3] == 0xb0)
    return MPEG4_CODE_VOS_START;
  else if(data[3] == 0xb6)
    return MPEG4_CODE_VOP_START;
  else if(data[3] == 0xb2)
    return MPEG4_CODE_USER_DATA;
  else if(data[3] == 0xb3)
    return MPEG4_CODE_GOV_START; // Group of VOPs
  return 0;
  }

#define SHAPE_RECT        0
#define SHAPE_BINARY      1
#define SHAPE_BINARY_ONLY 2
#define SHAPE_GRAYSCALE   3

int bgav_mpeg4_vol_header_read(bgav_mpeg4_vol_header_t * ret,
                               const uint8_t * buffer, int len)
  {
  bgav_bitstream_t b;
  int dummy;

  buffer+=4;
  len -= 4;
  
  bgav_bitstream_init(&b, buffer, len);

  if(!bgav_bitstream_get(&b, &ret->random_accessible_vol, 1) ||
     !bgav_bitstream_get(&b, &ret->video_object_type_indication, 8) ||
     !bgav_bitstream_get(&b, &ret->is_object_layer_identifier, 1))
    return 0;

  if(ret->is_object_layer_identifier)
    {
    if(!bgav_bitstream_get(&b, &ret->video_object_layer_verid, 4) ||
       !bgav_bitstream_get(&b, &ret->video_object_layer_priority, 3))
      return 0;
    }

  if(!bgav_bitstream_get(&b, &ret->aspect_ratio_info, 4))
    return 0;
  
  if(ret->aspect_ratio_info == 15) // extended PAR
    {
    if(!bgav_bitstream_get(&b, &ret->par_width, 8) ||
       !bgav_bitstream_get(&b, &ret->par_height, 8))
      return 0;
    }

  if(!bgav_bitstream_get(&b, &ret->vol_control_parameters, 1))
    return 0;
  if(ret->vol_control_parameters)
    {
    if(!bgav_bitstream_get(&b, &ret->chroma_format, 2) ||
       !bgav_bitstream_get(&b, &ret->low_delay, 1) ||
       !bgav_bitstream_get(&b, &ret->vbv_parameters, 1))
      return 0;

    if(ret->vbv_parameters)
      {
      if(!bgav_bitstream_get(&b, &ret->first_half_bit_rate, 15) ||
         !bgav_bitstream_get(&b, &dummy, 1) || /* int marker_bit; */
         !bgav_bitstream_get(&b, &ret->latter_half_bit_rate, 15) || 
         !bgav_bitstream_get(&b, &dummy, 1) || /* int marker_bit; */
         !bgav_bitstream_get(&b, &ret->first_half_vbv_buffer_size, 15) || 
         !bgav_bitstream_get(&b, &dummy, 1) || /* int marker_bit; */
         !bgav_bitstream_get(&b, &ret->latter_half_vbv_buffer_size, 3) ||
         !bgav_bitstream_get(&b, &ret->first_half_vbv_occupancy, 11) ||
         !bgav_bitstream_get(&b, &dummy, 1) ||  /* int marker_bit; */
         !bgav_bitstream_get(&b, &ret->latter_half_vbv_occupancy, 15) ||
         !bgav_bitstream_get(&b, &dummy, 1)) /* int marker_bit; */
        return 0;
      }
    
    }
  
  if(!bgav_bitstream_get(&b, &ret->video_object_layer_shape, 2))      // 2
    return 0;
  
  if((ret->video_object_layer_shape == SHAPE_GRAYSCALE) && // "grayscale"
     (ret->video_object_layer_verid != 1))
    {
    if(!bgav_bitstream_get(&b, &ret->video_object_layer_shape_extension, 2))      // 2
      return 0;
    }

  if(!bgav_bitstream_get(&b, &dummy, 1) || /* int marker_bit; */
     !bgav_bitstream_get(&b, &ret->vop_time_increment_resolution, 16) ||
     !bgav_bitstream_get(&b, &dummy, 1) || /* int marker_bit; */
     !bgav_bitstream_get(&b, &ret->fixed_vop_rate, 1))
    return 0;

  ret->time_increment_bits = bgav_log2(ret->vop_time_increment_resolution - 1) + 1;

  if(ret->time_increment_bits < 1)
    ret->time_increment_bits = 1;
    
  if(ret->fixed_vop_rate)
    {
    if(!bgav_bitstream_get(&b, &ret->fixed_vop_time_increment,
                           ret->time_increment_bits))
      return 0;
    }
  else
    ret->fixed_vop_time_increment = 1;

  /* Size */
  if(ret->video_object_layer_shape != SHAPE_BINARY_ONLY)
    {
    if(ret->video_object_layer_shape == SHAPE_RECT)
      {
      if(!bgav_bitstream_get(&b, &dummy, 1) || /* int marker_bit; */
         !bgav_bitstream_get(&b, &ret->video_object_layer_width, 13) ||
         !bgav_bitstream_get(&b, &dummy, 1) || /* int marker_bit; */
         !bgav_bitstream_get(&b, &ret->video_object_layer_height, 13) ||
         !bgav_bitstream_get(&b, &dummy, 1)) /* int marker_bit; */
        return 0;
      }
    }
  
  return len - bgav_bitstream_get_bits(&b) / 8;
  }

void bgav_mpeg4_vol_header_dump(bgav_mpeg4_vol_header_t * h)
  {
  gavl_dprintf("VOL header\n");
  
  gavl_dprintf("  random_accessible_vol:              %d\n",
               h->random_accessible_vol);
  gavl_dprintf("  video_object_type_indication:       %d\n",
               h->video_object_type_indication);
  gavl_dprintf("  is_object_layer_identifier:         %d\n",
               h->is_object_layer_identifier);
  if (h->is_object_layer_identifier)
    {
    gavl_dprintf("  video_object_layer_verid:           %d\n", h->video_object_layer_verid);
    gavl_dprintf("  video_object_layer_priority:        %d\n", h->video_object_layer_priority);
    }
  gavl_dprintf("  aspect_ratio_info:                  %d\n", h->aspect_ratio_info);
  if(h->aspect_ratio_info == 15)
    {
    gavl_dprintf("  par_width:                          %d\n", h->par_width);
    gavl_dprintf("  par_height:                         %d\n", h->par_height);
    }
  gavl_dprintf("  vol_control_parameters:             %d\n", h->vol_control_parameters);

  if (h->vol_control_parameters)
    {
    gavl_dprintf("  chroma_format:                      %d\n", h->chroma_format);
    gavl_dprintf("  low_delay:                          %d\n", h->low_delay);
    gavl_dprintf("  vbv_parameters:                     %d\n", h->vbv_parameters);
    if (h->vbv_parameters)
      {
      gavl_dprintf("  first_half_bit_rate:                %d\n", h->first_half_bit_rate);
      gavl_dprintf("  latter_half_bit_rate:               %d\n", h->latter_half_bit_rate);
      gavl_dprintf("  first_half_vbv_buffer_size:         %d\n", h->first_half_vbv_buffer_size);
      gavl_dprintf("  latter_half_vbv_buffer_size:        %d\n", h->latter_half_vbv_buffer_size);
      gavl_dprintf("  first_half_vbv_occupancy:           %d\n", h->first_half_vbv_occupancy);
      gavl_dprintf("  latter_half_vbv_occupancy:          %d\n", h->latter_half_vbv_occupancy);
      }
    }
  gavl_dprintf("  video_object_layer_shape:           %d\n", h->video_object_layer_shape);
  if ((h->video_object_layer_shape == SHAPE_GRAYSCALE) &&
      (h->video_object_layer_verid != 1))
    {
    gavl_dprintf("  video_object_layer_shape_extension: %d\n", h->video_object_layer_shape_extension);
    }
  gavl_dprintf("  vop_time_increment_resolution:      %d\n", h->vop_time_increment_resolution);
  gavl_dprintf("  fixed_vop_rate:                     %d\n", h->fixed_vop_rate);
  if(h->fixed_vop_rate)
    {
    gavl_dprintf("  fixed_vop_time_increment:           %d\n", h->fixed_vop_time_increment);
    }
  if(h->video_object_layer_shape != SHAPE_BINARY_ONLY)
    {
    if(h->video_object_layer_shape == SHAPE_RECT)
      {
      gavl_dprintf("  video_object_layer_width:           %d\n", h->video_object_layer_width);
      gavl_dprintf("  video_object_layer_height:          %d\n", h->video_object_layer_height);
      }
    }
  
  }
                               
int bgav_mpeg4_vop_header_read(bgav_mpeg4_vop_header_t * ret,
                               const uint8_t * buffer, int len,
                               const bgav_mpeg4_vol_header_t * vol)
  {
  int dummy;
  bgav_bitstream_t b;
  
  buffer+=4;
  len -= 4;

  memset(ret, 0, sizeof(*ret));
  
  bgav_bitstream_init(&b, buffer, len);

  if(!bgav_bitstream_get(&b, &dummy, 2))
    return 0;

  switch(dummy)
    {
    case 0:
      ret->coding_type = GAVL_PACKET_TYPE_I;
      break;
    case 1:
    case 3:
      ret->coding_type = GAVL_PACKET_TYPE_P;
      break;
    case 2:
      ret->coding_type = GAVL_PACKET_TYPE_B;
      break;
    }

  while(1)
    {
    if(!bgav_bitstream_get(&b, &dummy, 1))
      return 0;
    if(dummy)
      ret->modulo_time_base++;
    else
      break;
    }
  
  if(!bgav_bitstream_get(&b, &dummy, 1)) /* Marker */
    return 0;

  if(!bgav_bitstream_get(&b, &ret->time_increment, vol->time_increment_bits))
    return 0;

  if(!bgav_bitstream_get(&b, &dummy, 1)) /* Marker */
    return 0;

  if(!bgav_bitstream_get(&b, &ret->vop_coded, 1))
    return 0;

  return len - bgav_bitstream_get_bits(&b) / 8;
  }

void bgav_mpeg4_vop_header_dump(bgav_mpeg4_vop_header_t * h)
  {
  gavl_dprintf("VOP header\n");

  gavl_dprintf("  coding_type:      %s\n", gavl_coding_type_to_string(h->coding_type));
  gavl_dprintf("  modulo_time_base: %d\n", h->modulo_time_base); 
  gavl_dprintf("  time_increment:   %d\n", h->time_increment);
  gavl_dprintf("  vop_coded:        %d\n", h->vop_coded);
  
  }

static void remove_byte(gavl_buffer_t * buf, int byte)
  {
  /* Byte if the last one */
  if(byte < buf->len - 1)
    memmove(buf->buf + byte, buf->buf + byte + 1, buf->len - 1 - byte);

  buf->len--;
  }

int bgav_mpeg4_remove_packed_flag(gavl_buffer_t * buf)
  {
  const uint8_t * sc2;
  uint8_t * end = buf->buf + buf->len;
  const uint8_t * pos = buf->buf;
  int userdata_size;
  
  while(pos < end)
    {
    pos = bgav_mpv_find_startcode(pos, end);
    if(!pos)
      break;
    
    switch(bgav_mpeg4_get_start_code(pos))
      {
      case MPEG4_CODE_USER_DATA:
        pos += 4;
        sc2 = bgav_mpv_find_startcode(pos, end);
        if(sc2)
          userdata_size = sc2 - pos;
        else
          userdata_size = end - pos;

        if(userdata_size < 4)
          break;
        
        if(strncasecmp((char*)pos, "divx", 4))
          break;

        if(pos[userdata_size-1] == 'p')
          {
          remove_byte(buf, pos - buf->buf + userdata_size - 1);
          return 1;
          }
        pos += userdata_size - 1;
        break;
      default:
        pos += 4;
        break;
      }
      
    }
  return 0;
  }

int bgav_mpeg4_vos_header_read(bgav_mpeg4_vos_header_t * ret,
                               const uint8_t * buffer, int len)
  {
  if(len < 5)
    return 0;
  ret->profile_and_level_indication = buffer[4];
  return 5;
  }

void bgav_mpeg4_vos_header_dump(bgav_mpeg4_vos_header_t * vos)
  {
  gavl_dprintf("VOS header\n");
  gavl_dprintf("  profile_and_level_indication: %d\n", vos->profile_and_level_indication);
  }
