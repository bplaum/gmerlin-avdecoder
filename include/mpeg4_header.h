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

#define MPEG4_CODE_VOS_START  1
#define MPEG4_CODE_VO_START   2
#define MPEG4_CODE_VOL_START  3
#define MPEG4_CODE_VOP_START  4
#define MPEG4_CODE_USER_DATA  5
#define MPEG4_CODE_GOV_START  6

int bgav_mpeg4_get_start_code(const uint8_t * data);

void bgav_mpeg4_remove_packed_flag(uint8_t * data, uint32_t * len, uint32_t * header_len); 


typedef struct
  {
  int random_accessible_vol;         // 1
  int video_object_type_indication;  // 8
  int is_object_layer_identifier;    // 1
  // if (is_object_layer_identifier) {
  int video_object_layer_verid;      // 4
  int video_object_layer_priority;   // 3
  // }
  int aspect_ratio_info;             // 4
  // if (aspect_ratio_info == "extended_PAR") {
  int par_width;                     // 8
  int par_height;                    // 8
  // }
  int vol_control_parameters;        // 1

  // if (vol_control_parameters) {
  int chroma_format;                 // 2
  int low_delay;                     // 1
  int vbv_parameters;                // 1
  // if (vbv_parameters) {
  int first_half_bit_rate;           // 15
  /* int marker_bit; */
  int latter_half_bit_rate;          // 15
  /* int marker_bit; */
  int first_half_vbv_buffer_size;    // 15
  /* int marker_bit; */
  int latter_half_vbv_buffer_size;   // 3
  int first_half_vbv_occupancy;      // 11
  /* int marker_bit; */
  int latter_half_vbv_occupancy;     // 15
  /* int marker_bit; */
  // }
  // }
  int video_object_layer_shape;      // 2
  // if (video_object_layer_shape == "grayscale"
  //    && video_object_layer_verid != '0001')
  int video_object_layer_shape_extension; // 4
  /* int marker_bit; */                        // 1
  int vop_time_increment_resolution;      // 16
  /*  int marker_bit; */                        // 1
  int fixed_vop_rate;                     // 1
  // if (fixed_vop_rate)
  int fixed_vop_time_increment;           // 1-16

  int video_object_layer_width;           // 13
  int video_object_layer_height;           // 13
  
  /* Calculated */
  int time_increment_bits;
  
  } bgav_mpeg4_vol_header_t;

int bgav_mpeg4_vol_header_read(const bgav_options_t * opt,
                               bgav_mpeg4_vol_header_t * ret,
                               const uint8_t * buffer, int len);

void bgav_mpeg4_vol_header_dump(bgav_mpeg4_vol_header_t * h);

void bgav_mpeg4_get_pixel_aspect(bgav_mpeg4_vol_header_t * h,
                                 uint32_t * width, uint32_t * height);

typedef struct
  {
  int coding_type;
  int modulo_time_base;
  int time_increment;
  int vop_coded;
  } bgav_mpeg4_vop_header_t;

int bgav_mpeg4_vop_header_read(const bgav_options_t * opt,
                               bgav_mpeg4_vop_header_t * ret,
                               const uint8_t * buffer, int len,
                               const bgav_mpeg4_vol_header_t * vol);

void bgav_mpeg4_vop_header_dump(bgav_mpeg4_vop_header_t * h);
