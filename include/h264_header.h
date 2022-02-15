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

/* H.264 stuff */

const uint8_t *
bgav_h264_find_nal_start(const uint8_t * buffer, int len);

/* Returns -1 if the buffer doesn't contain the start of the next
   NAL */

int bgav_h264_get_nal_size(const uint8_t * buffer, int len);

/* NAL unit types */

#define H264_NAL_NON_IDR_SLICE     1
#define H264_NAL_SLICE_PARTITION_A 2
#define H264_NAL_SLICE_PARTITION_B 3
#define H264_NAL_SLICE_PARTITION_C 4
#define H264_NAL_IDR_SLICE         5
#define H264_NAL_SEI               6
#define H264_NAL_SPS               7
#define H264_NAL_PPS               8
#define H264_NAL_ACCESS_UNIT_DEL   9
#define H264_NAL_END_OF_SEQUENCE   10
#define H264_NAL_END_OF_STREAM     11
#define H264_NAL_FILLER_DATA       12


typedef struct
  {
  int ref_idc;
  int unit_type;
  } bgav_h264_nal_header_t;

/* rbsp must have at least the size of in_buffer */
int bgav_h264_decode_nal_header(const uint8_t * in_buffer, int len,
                                bgav_h264_nal_header_t * header);

/* Returns the number of bytes read */
/* RBSP = raw byte sequence payload */
int bgav_h264_decode_nal_rbsp(const uint8_t * in_buffer, int len,
                              uint8_t * ret);

/* VUI (Video Usability Information) */

typedef struct
  {
  int aspect_ratio_info_present_flag;
  // if( aspect_ratio_info_present_flag ) {
  int aspect_ratio_idc;
  // if( aspect_ratio_idc = = Extended_SAR ) {
  int sar_width;
  int sar_height;
  // }
  // }
  int overscan_info_present_flag;
  // if( overscan_info_present_flag )
  int overscan_appropriate_flag;

  int video_signal_type_present_flag;
  // if( video_signal_type_present_flag ) {
  int video_format;
  int video_full_range_flag;
  int colour_description_present_flag;
  // if( colour_description_present_flag ) {
  int colour_primaries;
  int transfer_characteristics;
  int matrix_coefficients;
  // }
  // }
  int chroma_loc_info_present_flag;
  // if( chroma_loc_info_present_flag ) {
  int chroma_sample_loc_type_top_field;
  int chroma_sample_loc_type_bottom_field;
  // }
  int timing_info_present_flag;
  // if( timing_info_present_flag ) {
  int num_units_in_tick;
  int time_scale;
  int fixed_frame_rate_flag;
  // }

  int nal_hrd_parameters_present_flag;
  int vcl_hrd_parameters_present_flag;

  int cpb_removal_delay_length_minus1;
  int dpb_output_delay_length_minus1;
  
  int low_delay_hrd_flag;
  int pic_struct_present_flag;
  int bitstream_restriction_flag;
  // if( bitstream_restriction_flag ) {
  int motion_vectors_over_pic_boundaries_flag;
  int max_bytes_per_pic_denom;
  int max_bits_per_mb_denom;
  int log2_max_mv_length_horizontal;
  int log2_max_mv_length_vertical;
  int num_reorder_frames;
  int max_dec_frame_buffering;
  // }
  
  } bgav_h264_vui_t;

/* Sequence parameter set */

typedef struct
  {
  int profile_idc;
  int constraint_set0_flag;
  int constraint_set1_flag;
  int constraint_set2_flag;
  int constraint_set3_flag;
  int level_idc;
  int seq_parameter_set_id;
  
  int chroma_format_idc;
  // if( chroma_format_idc == 3 )
  int separate_colour_plane_flag;
  int bit_depth_luma_minus8;
  int bit_depth_chroma_minus8;
  int qpprime_y_zero_transform_bypass_flag;
  int seq_scaling_matrix_present_flag;
  
  
  int log2_max_frame_num_minus4;
  int pic_order_cnt_type;

  /* if( pic_order_cnt_type == 0 ) */
  int log2_max_pic_order_cnt_lsb_minus4;

  /* else if(pic_order_cnt_type == 1) { */
  int delta_pic_order_always_zero_flag;
  int offset_for_non_ref_pic;
  int offset_for_top_to_bottom_field;
  int num_ref_frames_in_pic_order_cnt_cycle;
  int * offset_for_ref_frame;
  /* } */

  int num_ref_frames;
  int gaps_in_frame_num_value_allowed_flag;
  int pic_width_in_mbs_minus1;
  int pic_height_in_map_units_minus1;
  int frame_mbs_only_flag;
  /* if( !frame_mbs_only_flag ) */
  int mb_adaptive_frame_field_flag;
  int direct_8x8_inference_flag;
  int frame_cropping_flag;
  /* if( frame_cropping_flag ) { */
  int frame_crop_left_offset;
  int frame_crop_right_offset;
  int frame_crop_top_offset;
  int frame_crop_bottom_offset;
  /* } */
  int vui_parameters_present_flag;

  bgav_h264_vui_t vui;
  
  } bgav_h264_sps_t;

int bgav_h264_sps_parse(const bgav_options_t * opt,
                        bgav_h264_sps_t *,
                        const uint8_t * buffer, int len);

void bgav_h264_sps_free(bgav_h264_sps_t *);

void bgav_h264_sps_dump(bgav_h264_sps_t *);

void bgav_h264_sps_get_image_size(bgav_h264_sps_t * sps,
                                  gavl_video_format_t * format);


#if 0 /* Not needed */

typedef struct
  {
  
  } bgav_h264_pps_t;

int bgav_h264_pps_parse(const bgav_options_t * opt,
                        bgav_h264_pps_t *,
                        const uint8_t * buffer, int len);

#endif

/* SEI stuff */

int bgav_h264_decode_sei_message_header(const uint8_t * data, int len,
                                        int * sei_type, int * sei_size);

typedef struct
  {
  int pic_struct;
  int have_timecode;
  int tc_hours;
  int tc_minutes;
  int tc_seconds;
  int tc_frames;
  int counting_type;
  } bgav_h264_sei_pic_timing_t;

int bgav_h264_decode_sei_pic_timing(const uint8_t * data, int len,
                                    bgav_h264_sps_t * sps,
                                    bgav_h264_sei_pic_timing_t * ret);

typedef struct
  {
  int recovery_frame_cnt;
  int exact_match_flag;
  int broken_link_flag;
  int changing_slice_group_idc;
  } bgav_h264_sei_recovery_point_t;

int bgav_h264_decode_sei_recovery_point(const uint8_t * data, int len,
                                        bgav_h264_sei_recovery_point_t * ret);


/* Slice header */

typedef struct
  {
  int first_mb_in_slice;
  int slice_type;
  int pic_parameter_set_id;
  // if( separate_colour_plane_flag = = 1 )
  int colour_plane_id;
  int frame_num;
  // if( !frame_mbs_only_flag ) {
  int field_pic_flag;
  // if( field_pic_flag )
  int bottom_field_flag;
  // }
  } bgav_h264_slice_header_t;

void bgav_h264_slice_header_parse(const uint8_t * data, int len,
                                  const bgav_h264_sps_t * sps,
                                  bgav_h264_slice_header_t * ret);

void bgav_h264_slice_header_dump(const bgav_h264_sps_t * sps,
                                 const bgav_h264_slice_header_t * ret);
