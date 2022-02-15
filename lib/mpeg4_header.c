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

int bgav_mpeg4_vol_header_read(const bgav_options_t * opt,
                               bgav_mpeg4_vol_header_t * ret,
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
  bgav_dprintf("VOL header\n");
  
  bgav_dprintf("  random_accessible_vol:              %d\n",
               h->random_accessible_vol);
  bgav_dprintf("  video_object_type_indication:       %d\n",
               h->video_object_type_indication);
  bgav_dprintf("  is_object_layer_identifier:         %d\n",
               h->is_object_layer_identifier);
  if (h->is_object_layer_identifier)
    {
    bgav_dprintf("  video_object_layer_verid:           %d\n", h->video_object_layer_verid);
    bgav_dprintf("  video_object_layer_priority:        %d\n", h->video_object_layer_priority);
    }
  bgav_dprintf("  aspect_ratio_info:                  %d\n", h->aspect_ratio_info);
  if(h->aspect_ratio_info == 15)
    {
    bgav_dprintf("  par_width:                          %d\n", h->par_width);
    bgav_dprintf("  par_height:                         %d\n", h->par_height);
    }
  bgav_dprintf("  vol_control_parameters:             %d\n", h->vol_control_parameters);

  if (h->vol_control_parameters)
    {
    bgav_dprintf("  chroma_format:                      %d\n", h->chroma_format);
    bgav_dprintf("  low_delay:                          %d\n", h->low_delay);
    bgav_dprintf("  vbv_parameters:                     %d\n", h->vbv_parameters);
    if (h->vbv_parameters)
      {
      bgav_dprintf("  first_half_bit_rate:                %d\n", h->first_half_bit_rate);
      bgav_dprintf("  latter_half_bit_rate:               %d\n", h->latter_half_bit_rate);
      bgav_dprintf("  first_half_vbv_buffer_size:         %d\n", h->first_half_vbv_buffer_size);
      bgav_dprintf("  latter_half_vbv_buffer_size:        %d\n", h->latter_half_vbv_buffer_size);
      bgav_dprintf("  first_half_vbv_occupancy:           %d\n", h->first_half_vbv_occupancy);
      bgav_dprintf("  latter_half_vbv_occupancy:          %d\n", h->latter_half_vbv_occupancy);
      }
    }
  bgav_dprintf("  video_object_layer_shape:           %d\n", h->video_object_layer_shape);
  if ((h->video_object_layer_shape == SHAPE_GRAYSCALE) &&
      (h->video_object_layer_verid != 1))
    {
    bgav_dprintf("  video_object_layer_shape_extension: %d\n", h->video_object_layer_shape_extension);
    }
  bgav_dprintf("  vop_time_increment_resolution:      %d\n", h->vop_time_increment_resolution);
  bgav_dprintf("  fixed_vop_rate:                     %d\n", h->fixed_vop_rate);
  if(h->fixed_vop_rate)
    {
    bgav_dprintf("  fixed_vop_time_increment:           %d\n", h->fixed_vop_time_increment);
    }
  if(h->video_object_layer_shape != SHAPE_BINARY_ONLY)
    {
    if(h->video_object_layer_shape == SHAPE_RECT)
      {
      bgav_dprintf("  video_object_layer_width:           %d\n", h->video_object_layer_width);
      bgav_dprintf("  video_object_layer_height:          %d\n", h->video_object_layer_height);
      }
    }
  
  }
                               
int bgav_mpeg4_vop_header_read(const bgav_options_t * opt,
                               bgav_mpeg4_vop_header_t * ret,
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
      ret->coding_type = BGAV_CODING_TYPE_I;
      break;
    case 1:
    case 3:
      ret->coding_type = BGAV_CODING_TYPE_P;
      break;
    case 2:
      ret->coding_type = BGAV_CODING_TYPE_B;
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
  bgav_dprintf("VOP header\n");

  bgav_dprintf("  coding_type:      %s\n", bgav_coding_type_to_string(h->coding_type));
  bgav_dprintf("  modulo_time_base: %d\n", h->modulo_time_base); 
  bgav_dprintf("  time_increment:   %d\n", h->time_increment);
  bgav_dprintf("  vop_coded:        %d\n", h->vop_coded);
  
  }

static void remove_byte(uint8_t * data, int byte, uint32_t * len, uint32_t * header_end)
  {
  /* Byte if the last one */
  if(byte < *len - 1)
    memmove(data + byte, data + byte + 1, *len - 1 - byte);
  
  (*len)--;

  if(len != header_end)
    (*header_end)--;
  }

void bgav_mpeg4_remove_packed_flag(uint8_t * data, uint32_t * len, uint32_t * header_len)
  {
  const uint8_t * sc2;
  uint8_t * hend = data + *header_len;
  uint8_t * dend = data + *len;
  const uint8_t * pos = data;
  int userdata_size;
  
  while(pos < hend)
    {
    pos = bgav_mpv_find_startcode(pos, dend);
    if(!pos)
      break;
    
    switch(bgav_mpeg4_get_start_code(pos))
      {
      case MPEG4_CODE_USER_DATA:
        pos += 4;
        sc2 = bgav_mpv_find_startcode(pos, dend);
        if(sc2)
          userdata_size = sc2 - pos;
        else
          userdata_size = dend - pos;

        if(userdata_size < 4)
          break;
        
        if(strncasecmp((char*)pos, "divx", 4))
          break;

        if(pos[userdata_size-1] == 'p')
          remove_byte(data, pos - data + userdata_size - 1, len, header_len);
        pos += userdata_size - 1;
        break;
      default:
        pos += 4;
        break;
      }
      
    }
  
  }
