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

#include <avdec_private.h>
#include <mpv_header.h>
#include <h264_header.h>

#include <bitstream.h>

#include <gavl/log.h>
#define LOG_DOMAIN "h264"

static const struct
  {
  int pixel_width;
  int pixel_height;
  }
pixel_aspect[] =
  {
    {1, 1}, /* Unspecified */
    {1, 1},
    {12, 11},
    {10, 11},
    {16, 11},
    {40, 33},
    {24, 11},
    {20, 11},
    {32, 11},
    {80, 33},
    {18, 11},
    {15, 11},
    {64, 33},
    {160,99},
    {4, 3},
    {3, 2},
    {2, 1},
  };

static void get_pixel_size(const bgav_h264_vui_t * v, uint32_t * w, uint32_t * h)
  {
  if(v->aspect_ratio_info_present_flag)
    {
    if(v->aspect_ratio_idc < 17)
      {
      *w = pixel_aspect[v->aspect_ratio_idc].pixel_width;
      *h = pixel_aspect[v->aspect_ratio_idc].pixel_height;
      }
    else if(v->aspect_ratio_idc == 255)
      {
      *w = v->sar_width;
      *h = v->sar_height;
      }
    else
      {
      *w = 1;
      *h = 1;
      }
    }
  else
    {
    *w = 1;
    *h = 1;
    }
  }

/* */

const uint8_t *
bgav_h264_find_nal_start(const uint8_t * buffer, int len)
  {
  const uint8_t * ptr;

  /* We pass len - 1 to ensure that we can read the NAL header too */
  ptr = bgav_mpv_find_startcode(buffer, buffer + len - 1);
  
  if(!ptr)
    return NULL;
  
  /* Get zero byte before actual startcode */
  if((ptr > buffer) && (*(ptr-1) == 0x00))
    ptr--;
  
  return ptr;
  }

int bgav_h264_get_nal_size(const uint8_t * buffer, int len)
  {
  const uint8_t * end;

  if(len < 4)
    return -1;
  
  end = bgav_h264_find_nal_start(buffer + 4, len - 4);
  if(end)
    return (end - buffer);
  else
    return -1;
  }

int bgav_h264_decode_nal_header(const uint8_t * in_buffer, int len,
                                bgav_h264_nal_header_t * header)
  {
  const uint8_t * pos = in_buffer;

  memset(header, 0, sizeof(*header));
  
  while(*pos == 0x00)
    pos++;
  pos++; // 0x01
  header->ref_idc       = pos[0] >> 5;
  header->unit_type = pos[0] & 0x1f;
  return pos - in_buffer + 1;
  }

#if 0
int bgav_h264_nal_header_dump(const bgav_h264_nal_header_t * header)
  {
  
  }
#endif

int bgav_h264_decode_nal_rbsp(const uint8_t * in_buffer, int len,
                              uint8_t * ret)
  {
  const uint8_t * src = in_buffer;
  const uint8_t * end = in_buffer + len;
  uint8_t * dst = ret;
  //  int i;

  while(src < end)
    {
    /* 1 : 2^22 */
    if(BGAV_UNLIKELY((src < end - 3) &&
                     (src[0] == 0x00) &&
                     (src[1] == 0x00) &&
                     (src[2] == 0x03)))
      {
      dst[0] = src[0];
      dst[1] = src[1];
      
      dst += 2;
      src += 3;
      }
    else
      {
      dst[0] = src[0];
      src++;
      dst++;
      }
    }
  return dst - ret;
  }

/* SPS stuff */

static void get_hrd_parameters(bgav_bitstream_t * b,
                               bgav_h264_vui_t * vui)
  {
  int dummy, i;
  int cpb_cnt_minus1;
  
  bgav_bitstream_get_golomb_ue(b, &cpb_cnt_minus1);

  bgav_bitstream_get(b, &dummy, 4); // bit_rate_scale
  bgav_bitstream_get(b, &dummy, 4); // cpb_size_scale
  
  for(i = 0; i <= cpb_cnt_minus1; i++ )
    {
    bgav_bitstream_get_golomb_ue(b, &dummy); // bit_rate_value_minus1[ SchedSelIdx ]
    bgav_bitstream_get_golomb_ue(b, &dummy); // cpb_size_value_minus1[ SchedSelIdx ]
    bgav_bitstream_get(b, &dummy, 1); // cbr_flag[ SchedSelIdx ]
    }
  bgav_bitstream_get(b, &dummy, 5); // initial_cpb_removal_delay_length_minus1
  bgav_bitstream_get(b, &vui->cpb_removal_delay_length_minus1, 5); 
  bgav_bitstream_get(b, &vui->dpb_output_delay_length_minus1, 5); 
  bgav_bitstream_get(b, &dummy, 5); // time_offset_length
  }

static void vui_parse(bgav_bitstream_t * b, bgav_h264_vui_t * vui)
  {
  bgav_bitstream_get(b, &vui->aspect_ratio_info_present_flag, 1);
  if(vui->aspect_ratio_info_present_flag)
    {
    bgav_bitstream_get(b, &vui->aspect_ratio_idc, 8);
    if(vui->aspect_ratio_idc == 255) // Extended_SAR
      {
      bgav_bitstream_get(b, &vui->sar_width, 16);
      bgav_bitstream_get(b, &vui->sar_height, 16);
      }
    }

  bgav_bitstream_get(b, &vui->overscan_info_present_flag, 1);
  if(vui->overscan_info_present_flag)
    bgav_bitstream_get(b, &vui->overscan_appropriate_flag, 1);

  bgav_bitstream_get(b, &vui->video_signal_type_present_flag, 1);
  if(vui->video_signal_type_present_flag)
    {
    bgav_bitstream_get(b, &vui->video_format, 3);
    bgav_bitstream_get(b, &vui->video_full_range_flag, 1);
    bgav_bitstream_get(b, &vui->colour_description_present_flag, 1);
    if(vui->colour_description_present_flag)
      {
      bgav_bitstream_get(b, &vui->colour_primaries, 8);
      bgav_bitstream_get(b, &vui->transfer_characteristics, 8);
      bgav_bitstream_get(b, &vui->matrix_coefficients, 8);
      }
    }

  bgav_bitstream_get(b, &vui->chroma_loc_info_present_flag, 1);
  if(vui->chroma_loc_info_present_flag)
    {
    bgav_bitstream_get_golomb_ue(b, &vui->chroma_sample_loc_type_top_field);
    bgav_bitstream_get_golomb_ue(b, &vui->chroma_sample_loc_type_bottom_field);
    }

  bgav_bitstream_get(b, &vui->timing_info_present_flag, 1);
  if(vui->timing_info_present_flag)
    {
    bgav_bitstream_get(b, &vui->num_units_in_tick, 32);
    bgav_bitstream_get(b, &vui->time_scale, 32);
    bgav_bitstream_get(b, &vui->fixed_frame_rate_flag, 1);
    }

  bgav_bitstream_get(b, &vui->nal_hrd_parameters_present_flag, 1);
  if(vui->nal_hrd_parameters_present_flag)
    get_hrd_parameters(b, vui);

  bgav_bitstream_get(b, &vui->vcl_hrd_parameters_present_flag, 1);
  if(vui->vcl_hrd_parameters_present_flag)
    get_hrd_parameters(b, vui);

  if(vui->nal_hrd_parameters_present_flag || vui->vcl_hrd_parameters_present_flag)
    bgav_bitstream_get(b, &vui->low_delay_hrd_flag, 1);

  bgav_bitstream_get(b, &vui->pic_struct_present_flag, 1);
  bgav_bitstream_get(b, &vui->bitstream_restriction_flag, 1);
  
  if(vui->bitstream_restriction_flag )
    {
    bgav_bitstream_get(b, &vui->motion_vectors_over_pic_boundaries_flag, 1);

    bgav_bitstream_get_golomb_ue(b, &vui->max_bytes_per_pic_denom);
    bgav_bitstream_get_golomb_ue(b, &vui->max_bits_per_mb_denom);
    bgav_bitstream_get_golomb_ue(b, &vui->log2_max_mv_length_horizontal);
    bgav_bitstream_get_golomb_ue(b, &vui->log2_max_mv_length_vertical);
    bgav_bitstream_get_golomb_ue(b, &vui->num_reorder_frames);
    bgav_bitstream_get_golomb_ue(b, &vui->max_dec_frame_buffering);
    }
  
  }

static void vui_dump(bgav_h264_vui_t * vui)
  {
  gavl_dprintf("    aspect_ratio_info_present_flag:        %d\n",
               vui->aspect_ratio_info_present_flag);
  if(vui->aspect_ratio_info_present_flag )
    {
    gavl_dprintf("    aspect_ratio_idc:                      %d\n",
                 vui->aspect_ratio_idc );
    if( vui->aspect_ratio_idc == 255 )
      {
      gavl_dprintf("    sar_width:                             %d\n",
                   vui->sar_width );
      gavl_dprintf("    sar_height:                            %d\n",
                   vui->sar_height );
      }
    }
  gavl_dprintf("    overscan_info_present_flag:            %d\n",
               vui->overscan_info_present_flag );
  if( vui->overscan_info_present_flag )
    gavl_dprintf("    overscan_appropriate_flag:           %d\n",
                 vui->overscan_appropriate_flag );

  gavl_dprintf("    video_signal_type_present_flag:        %d\n",
               vui->video_signal_type_present_flag );
  if( vui->video_signal_type_present_flag )
    {
    gavl_dprintf("    video_format:                          %d\n",
                 vui->video_format );
    gavl_dprintf("    video_full_range_flag:                 %d\n",
                 vui->video_full_range_flag );
    gavl_dprintf("    colour_description_present_flag:       %d\n",
                 vui->colour_description_present_flag );
    if( vui->colour_description_present_flag )
      {
      gavl_dprintf("    colour_primaries:                  %d\n",
                   vui->colour_primaries );
      gavl_dprintf("    transfer_characteristics:          %d\n",
                   vui->transfer_characteristics );
      gavl_dprintf("    matrix_coefficients:               %d\n",
                   vui->matrix_coefficients );
      }
    }
  gavl_dprintf("    chroma_loc_info_present_flag:          %d\n",
               vui->chroma_loc_info_present_flag );
  if( vui->chroma_loc_info_present_flag )
    {
    gavl_dprintf("    chroma_sample_loc_type_top_field:    %d\n",
                 vui->chroma_sample_loc_type_top_field  );
    gavl_dprintf("    chroma_sample_loc_type_bottom_field: %d\n",
                 vui->chroma_sample_loc_type_bottom_field );
    }
  gavl_dprintf("    timing_info_present_flag:              %d\n",
               vui->timing_info_present_flag );
  if( vui->timing_info_present_flag )
    {
    gavl_dprintf("    num_units_in_tick:                     %d\n",
                 vui->num_units_in_tick );
    gavl_dprintf("    time_scale:                            %d\n",
                 vui->time_scale );
    gavl_dprintf("    fixed_frame_rate_flag:                 %d\n",
                 vui->fixed_frame_rate_flag );
    }
  gavl_dprintf("    nal_hrd_present_flag:                  %d\n",
               vui->nal_hrd_parameters_present_flag );
  gavl_dprintf("    vcl_hrd_present_flag:                  %d\n",
               vui->vcl_hrd_parameters_present_flag );

  if(vui->nal_hrd_parameters_present_flag || vui->vcl_hrd_parameters_present_flag)
    {
    gavl_dprintf("    low_delay_hrd_flag:                    %d\n",
                 vui->low_delay_hrd_flag);
    }
  gavl_dprintf("    pic_struct_present_flag:               %d\n",
               vui->pic_struct_present_flag );

  gavl_dprintf("    bitstream_restriction_flag:            %d\n",
               vui->bitstream_restriction_flag );

  if(vui->bitstream_restriction_flag)
    {
    gavl_dprintf("    motion_vectors_over_pic_boundaries_flag: %d\n",
                 vui->motion_vectors_over_pic_boundaries_flag );
    gavl_dprintf("    max_bytes_per_pic_denom:               %d\n",
                 vui->max_bytes_per_pic_denom);
    gavl_dprintf("    max_bits_per_mb_denom:                 %d\n",
                 vui->max_bits_per_mb_denom);
    gavl_dprintf("    log2_max_mv_length_horizontal:         %d\n",
                 vui->log2_max_mv_length_horizontal);
    gavl_dprintf("    log2_max_mv_length_vertical:           %d\n",
                 vui->log2_max_mv_length_vertical);
    gavl_dprintf("    num_reorder_frames:                    %d\n",
                 vui->num_reorder_frames);
    gavl_dprintf("    max_dec_frame_buffering:               %d\n",
                 vui->max_dec_frame_buffering);
    
    }
  }

// Taken from the standard and libavcodec/

static void skip_scaling_list(bgav_bitstream_t * b, int num)
  {
  int i, dummy;

#if 1
  int next_scale = 8;
  int last_scale = 8;
#endif
  
  for(i = 0; i < num; i++)
    {
#if 1
    if(next_scale)
      {
      int delta_scale;
      if(!bgav_bitstream_get_golomb_se(b, &delta_scale))
        {
        //        fprintf(stderr, "EOF: %d\n", i);
        return;
        }
      //      fprintf(stderr, "Delta scale: %d\n", delta_scale);

      next_scale = ( last_scale + delta_scale) & 0xff;
      }
    if(!i && !next_scale)
      break;
    
    dummy = ( next_scale == 0 ) ? last_scale : next_scale;
    last_scale = dummy;
#else
    bgav_bitstream_get_golomb_se(b, &dummy);
#endif
    }
  }

int bgav_h264_sps_parse(bgav_h264_sps_t * sps,
                        const uint8_t * buffer, int len)
  {
  int i;
  bgav_bitstream_t b;
  int dummy;
  //  fprintf(stderr, "Parsing SPS %d bytes\n", len);
  //  gavl_hexdump(buffer, len, 16);

  bgav_bitstream_init(&b, buffer, len);

  bgav_bitstream_get(&b, &sps->profile_idc, 8);
  bgav_bitstream_get(&b, &sps->constraint_set0_flag, 1);
  bgav_bitstream_get(&b, &sps->constraint_set1_flag, 1);
  bgav_bitstream_get(&b, &sps->constraint_set2_flag, 1);
  bgav_bitstream_get(&b, &sps->constraint_set3_flag, 1);
  bgav_bitstream_get(&b, &sps->constraint_set4_flag, 1);
  bgav_bitstream_get(&b, &sps->constraint_set5_flag, 1);
  
  bgav_bitstream_get(&b, &dummy, 2); /* reserved_zero_4bits */
  bgav_bitstream_get(&b, &sps->level_idc, 8); /* level_idc */

  bgav_bitstream_get_golomb_ue(&b, &sps->seq_parameter_set_id);

  /* ffmpeg has just (sps->profile_idc >= 100) */
  if(sps->profile_idc == 100 ||
     sps->profile_idc == 110 ||
     sps->profile_idc == 122 ||
     sps->profile_idc == 244 ||
     sps->profile_idc == 44 ||
     sps->profile_idc == 83 ||
     sps->profile_idc == 86 ) 
    {
    bgav_bitstream_get_golomb_ue(&b, &sps->chroma_format_idc);
    if(sps->chroma_format_idc == 3)
      bgav_bitstream_get(&b, &sps->separate_colour_plane_flag, 1);
    bgav_bitstream_get_golomb_ue(&b, &sps->bit_depth_luma_minus8);
    bgav_bitstream_get_golomb_ue(&b, &sps->bit_depth_chroma_minus8);

    bgav_bitstream_get(&b, &sps->qpprime_y_zero_transform_bypass_flag, 1);
    bgav_bitstream_get(&b, &sps->seq_scaling_matrix_present_flag, 1);
    
    if(sps->seq_scaling_matrix_present_flag)
      {
      int imax = 8;
      if(sps->chroma_format_idc == 3)
        imax = 12;
      
      for(i = 0; i < imax; i++)
        {
        if(!bgav_bitstream_get(&b, &dummy, 1))
          {
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "EOF while skipping SPS scaling list %d", i);
          return 0;
          }

        if(dummy)
          {
          if(i < 6)
            skip_scaling_list(&b, 16);
          else
            skip_scaling_list(&b, 64);
          }
        }
      }
    }
  
  bgav_bitstream_get_golomb_ue(&b, &sps->log2_max_frame_num_minus4);
  bgav_bitstream_get_golomb_ue(&b, &sps->pic_order_cnt_type);

  if(!sps->pic_order_cnt_type)
    bgav_bitstream_get_golomb_ue(&b, &sps->log2_max_pic_order_cnt_lsb_minus4);
  else if(sps->pic_order_cnt_type == 1)
    {
    bgav_bitstream_get(&b, &sps->delta_pic_order_always_zero_flag, 1);

    bgav_bitstream_get_golomb_se(&b, &sps->offset_for_non_ref_pic);  
    bgav_bitstream_get_golomb_se(&b, &sps->offset_for_top_to_bottom_field); 
    bgav_bitstream_get_golomb_ue(&b, &sps->num_ref_frames_in_pic_order_cnt_cycle);

    sps->offset_for_ref_frame =
      malloc(sizeof(*sps->offset_for_ref_frame) *
             sps->num_ref_frames_in_pic_order_cnt_cycle);
    for(i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++)
      {
      bgav_bitstream_get_golomb_se(&b, &sps->offset_for_ref_frame[i]);
      }
    }
  bgav_bitstream_get_golomb_ue(&b, &sps->num_ref_frames);
  bgav_bitstream_get(&b, &sps->gaps_in_frame_num_value_allowed_flag, 1);

  bgav_bitstream_get_golomb_ue(&b, &sps->pic_width_in_mbs_minus1);
  bgav_bitstream_get_golomb_ue(&b, &sps->pic_height_in_map_units_minus1);

  bgav_bitstream_get(&b, &sps->frame_mbs_only_flag, 1);

  if(!sps->frame_mbs_only_flag)
    bgav_bitstream_get(&b, &sps->mb_adaptive_frame_field_flag, 1);

  bgav_bitstream_get(&b, &sps->direct_8x8_inference_flag, 1);
  bgav_bitstream_get(&b, &sps->frame_cropping_flag, 1);
  if(sps->frame_cropping_flag)
    {
    bgav_bitstream_get_golomb_ue(&b, &sps->frame_crop_left_offset);
    bgav_bitstream_get_golomb_ue(&b, &sps->frame_crop_right_offset);
    bgav_bitstream_get_golomb_ue(&b, &sps->frame_crop_top_offset);
    bgav_bitstream_get_golomb_ue(&b, &sps->frame_crop_bottom_offset);
    }
  bgav_bitstream_get(&b, &sps->vui_parameters_present_flag, 1);

  if(sps->vui_parameters_present_flag)
    vui_parse(&b, &sps->vui);
  
  return 1;
  }

void bgav_h264_sps_free(bgav_h264_sps_t * sps)
  {
  if(sps->offset_for_ref_frame)
    free(sps->offset_for_ref_frame);
  }

void bgav_h264_sps_dump(bgav_h264_sps_t * sps)
  {
  int i;
  gavl_dprintf("SPS:\n");
  gavl_dprintf("  profile_idc:                             %d\n", sps->profile_idc);
  gavl_dprintf("  constraint_set0_flag:                    %d\n", sps->constraint_set0_flag);
  gavl_dprintf("  constraint_set1_flag:                    %d\n", sps->constraint_set1_flag);
  gavl_dprintf("  constraint_set2_flag:                    %d\n", sps->constraint_set2_flag);
  gavl_dprintf("  constraint_set3_flag:                    %d\n", sps->constraint_set3_flag);
  gavl_dprintf("  level_idc:                               %d\n", sps->level_idc);
  gavl_dprintf("  seq_parameter_set_id:                    %d\n", sps->seq_parameter_set_id);

  if(sps->profile_idc == 100 ||
     sps->profile_idc == 110 ||
     sps->profile_idc == 122 ||
     sps->profile_idc == 244 ||
     sps->profile_idc == 44 ||
     sps->profile_idc == 83 ||
     sps->profile_idc == 86 ) 
    {
    gavl_dprintf("  chroma_format_idc:                       %d\n", sps->chroma_format_idc);
    if(sps->chroma_format_idc == 3)
      gavl_dprintf("  separate_colour_plane_flag:              %d\n", sps->separate_colour_plane_flag);

    gavl_dprintf("  bit_depth_luma_minus8:                   %d\n", sps->bit_depth_luma_minus8);
    gavl_dprintf("  bit_depth_chroma_minus8:                 %d\n", sps->bit_depth_chroma_minus8);
    gavl_dprintf("  qpprime_y_zero_transform_bypass_flag:    %d\n", sps->qpprime_y_zero_transform_bypass_flag);
    gavl_dprintf("  seq_scaling_matrix_present_flag:         %d\n", sps->seq_scaling_matrix_present_flag);
    }
  
  gavl_dprintf("  log2_max_frame_num_minus4:               %d\n", sps->log2_max_frame_num_minus4);
  gavl_dprintf("  pic_order_cnt_type:                      %d\n", sps->pic_order_cnt_type);

  if( sps->pic_order_cnt_type == 0 )
    gavl_dprintf("  log2_max_pic_order_cnt_lsb_minus4:       %d\n", sps->log2_max_pic_order_cnt_lsb_minus4);
  else if(sps->pic_order_cnt_type == 1)
    {
    gavl_dprintf("  delta_pic_order_always_zero_flag:      %d\n", sps->delta_pic_order_always_zero_flag);
    gavl_dprintf("  offset_for_non_ref_pic:                %d\n", sps->offset_for_non_ref_pic);
    gavl_dprintf("  offset_for_top_to_bottom_field:        %d\n", sps->offset_for_top_to_bottom_field);
    gavl_dprintf("  num_ref_frames_in_pic_order_cnt_cycle: %d\n", sps->num_ref_frames_in_pic_order_cnt_cycle);

    for(i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++)
      {
      gavl_dprintf("  offset_for_ref_frame[%d]:              %d\n", i, sps->offset_for_ref_frame[i]);
      }
    }

  gavl_dprintf("  num_ref_frames:                          %d\n", sps->num_ref_frames);
  gavl_dprintf("  gaps_in_frame_num_value_allowed_flag:    %d\n", sps->gaps_in_frame_num_value_allowed_flag);
  gavl_dprintf("  pic_width_in_mbs_minus1:                 %d\n", sps->pic_width_in_mbs_minus1);
  gavl_dprintf("  pic_height_in_map_units_minus1:          %d\n", sps->pic_height_in_map_units_minus1);
  gavl_dprintf("  frame_mbs_only_flag:                     %d\n", sps->frame_mbs_only_flag);
  
  if( !sps->frame_mbs_only_flag )
    gavl_dprintf("  mb_adaptive_frame_field_flag:            %d\n", sps->mb_adaptive_frame_field_flag);
  gavl_dprintf("  direct_8x8_inference_flag:               %d\n", sps->direct_8x8_inference_flag);
  gavl_dprintf("  frame_cropping_flag:                     %d\n", sps->frame_cropping_flag);
  if( sps->frame_cropping_flag )
    {
    gavl_dprintf("  frame_crop_left_offset:                  %d\n", sps->frame_crop_left_offset);
    gavl_dprintf("  frame_crop_right_offset:                 %d\n", sps->frame_crop_right_offset);
    gavl_dprintf("  frame_crop_top_offset:                   %d\n", sps->frame_crop_top_offset);
    gavl_dprintf("  frame_crop_bottom_offset:                %d\n", sps->frame_crop_bottom_offset);
    }
  gavl_dprintf("  vui_parameters_present_flag:             %d\n", sps->vui_parameters_present_flag);

  if(sps->vui_parameters_present_flag)
    vui_dump(&sps->vui);
  
  }

void bgav_h264_sps_get_image_size(const bgav_h264_sps_t * sps,
                                  gavl_video_format_t * format)
  {
  int crop_right, crop_bottom, width, height;

  width  = 16 * (sps->pic_width_in_mbs_minus1 + 1);
  height = 16 * (sps->pic_height_in_map_units_minus1 + 1) *
    (2 - sps->frame_mbs_only_flag);

  crop_right  = sps->frame_crop_right_offset;
  crop_bottom = sps->frame_crop_bottom_offset;

  if(crop_right)
    {
    if(crop_right > 7)
      crop_right = 7;
    width -= 2 * crop_right;
    }
  if(crop_bottom)
    {
    if(sps->frame_mbs_only_flag)
      {
      if(crop_bottom > 7)
        crop_bottom = 7;
      height -= 2 * crop_bottom;
      }
    else
      {
      if(crop_bottom > 3)
        crop_bottom = 3;
      height -= 4 * crop_bottom;
      }
    }

  format->image_width  = width;
  format->image_height = height;

  format->frame_width  = ((width + 15)/16)*16;
  format->frame_height = ((height + 15)/16)*16;
  
  get_pixel_size(&sps->vui, &format->pixel_width, &format->pixel_height);

#if 0  
  fprintf(stderr, "bgav_h264_sps_get_image_size\n");
  bgav_h264_sps_dump(sps);
  fprintf(stderr, "--\n");
  gavl_video_format_dump(format);
  fprintf(stderr, "--\n");
#endif
  }


// http://blog.comrite.com/2019/06/05/h-264-profile-level-id-packetization-mode-nal/

static char * check_level_1b(const bgav_h264_sps_t * sps)
  {
  if((sps->level_idc == 11) && sps->constraint_set3_flag)
    return gavl_strdup("1b");
  else
    return NULL;
  }

void bgav_h264_sps_get_profile_level(const bgav_h264_sps_t * sps,
                                     gavl_dictionary_t * m)
  {
  const char * profile = NULL;
  char * level = NULL;
  
  switch(sps->profile_idc)
    {
    case 44:
      profile = GAVL_META_H264_PROFILE_CAVLC_444_INTRA;
      break;
    case 66:
      if(sps->constraint_set1_flag)
        profile = GAVL_META_H264_PROFILE_CONSTRAINED_BASELINE;
      else
        profile = GAVL_META_H264_PROFILE_BASELINE;

      level = check_level_1b(sps);

      break;
    case 77:
      profile = GAVL_META_H264_PROFILE_MAIN;
      level = check_level_1b(sps);
      break;
    case 88:
      profile = GAVL_META_H264_PROFILE_EXTENDED;
      break;
    case 100:
      if(sps->constraint_set4_flag)
        {
        if(sps->constraint_set5_flag)
          profile = GAVL_META_H264_PROFILE_CONSTRAINED_HIGH;
        else
          profile = GAVL_META_H264_PROFILE_PROGRESSIVE_HIGH;
        }
      else
        profile = GAVL_META_H264_PROFILE_HIGH;
      break;
    case 110:
      if(sps->constraint_set3_flag)
        profile = GAVL_META_H264_PROFILE_HIGH_10;
      else
        profile = GAVL_META_H264_PROFILE_HIGH_10;
      break;
    case 122:
      if(sps->constraint_set3_flag)
        profile = GAVL_META_H264_PROFILE_HIGH_422_INTRA;
      else
        profile = GAVL_META_H264_PROFILE_HIGH_422;
      break;
    case 244:
      if(sps->constraint_set3_flag)
        profile = GAVL_META_H264_PROFILE_HIGH_444_INTRA;
      else
        profile = GAVL_META_H264_PROFILE_HIGH_444_PREDICTIVE;
      break;
    /* scalable */
    case 83:
      if(sps->constraint_set5_flag)
        profile = GAVL_META_H264_PROFILE_SCALABLE_CONSTRAINED_BASELINE;
      else
        profile = GAVL_META_H264_PROFILE_SCALABLE_BASELINE;
        
      break;
    case 86:
      if(sps->constraint_set5_flag)
        profile = GAVL_META_H264_PROFILE_SCALABLE_CONSTRAINED_HIGH;
      else if(sps->constraint_set3_flag)
        profile = GAVL_META_H264_PROFILE_SCALABLE_HIGH_INTRA;
      else
        profile = GAVL_META_H264_PROFILE_SCALABLE_HIGH;
      break;
    /* Stereo/Multiview */
    case 128:
      profile = GAVL_META_H264_PROFILE_STEREO_HIGH;
      break;
    case 118:
      profile = GAVL_META_H264_PROFILE_MULTIVIEW_HIGH;
      break;
    case 138:
      profile = GAVL_META_H264_PROFILE_MULTIVIEW_DEPTH_HIGH;
      break;
    }
  
  if(profile)
    gavl_dictionary_set_string(m, GAVL_META_PROFILE, profile);
  else  
    gavl_dictionary_set_string_nocopy(m, GAVL_META_PROFILE,
                                      gavl_sprintf("Unknown (%d)", sps->profile_idc));
  
  if(!level)
    {
    if(sps->level_idc % 10)
      level = gavl_sprintf("%.1f", (double)sps->level_idc/10.0);
    else
      level = gavl_sprintf("%d", sps->level_idc/10);
    }
  
  gavl_dictionary_set_string_nocopy(m, GAVL_META_LEVEL, level);
  }

int bgav_h264_decode_sei_message_header(const uint8_t * data, int len,
                                        int * sei_type, int * sei_size)
  {
  const uint8_t * ptr = data;
  *sei_type = 0;
  *sei_size = 0;

  while(*ptr == 0xff)
    {
    *sei_type += 0xff;
    ptr++;
    }
  *sei_type += *ptr;
  ptr++;

  while(*ptr == 0xff)
    {
    *sei_size += 0xff;
    ptr++;
    }
  *sei_size += *ptr;
  ptr++;

  return ptr - data;
  
  }

int bgav_h264_decode_sei_recovery_point(const uint8_t * data, int len,
                                        bgav_h264_sei_recovery_point_t * ret)
  {
  bgav_bitstream_t b;
  bgav_bitstream_init(&b, data, len);
  
  if(!bgav_bitstream_get_golomb_ue(&b, &ret->recovery_frame_cnt) ||
     !bgav_bitstream_get(&b, &ret->exact_match_flag, 1) ||
     !bgav_bitstream_get(&b, &ret->broken_link_flag, 1) ||
     !bgav_bitstream_get(&b, &ret->changing_slice_group_idc, 2))
    return 0;
     
  return 1;
  }

int bgav_h264_decode_sei_pic_timing(const uint8_t * data, int len,
                                    bgav_h264_sps_t * sps,
                                    bgav_h264_sei_pic_timing_t * ret)
  {
  int dummy;
  bgav_bitstream_t b;
  int full_timestamp_flag;
  ret->pic_struct = -1;
  bgav_bitstream_init(&b, data, len);
  
  if(sps->vui.nal_hrd_parameters_present_flag ||
     sps->vui.vcl_hrd_parameters_present_flag)
    {
    bgav_bitstream_get(&b, &dummy, sps->vui.cpb_removal_delay_length_minus1+1);
    bgav_bitstream_get(&b, &dummy, sps->vui.dpb_output_delay_length_minus1+1);
    }
  if(sps->vui.pic_struct_present_flag)
    bgav_bitstream_get(&b, &ret->pic_struct, 4);

  if(!bgav_bitstream_get(&b, &dummy, 1)) // clock_timestamp_flag[0]
    return 0;
  if(dummy)
    {
    ret->have_timecode = 1;
    if(!bgav_bitstream_get(&b, &dummy, 2) || // ct_type
       !bgav_bitstream_get(&b, &dummy, 1) || // nuit_field_based_flag
       !bgav_bitstream_get(&b, &ret->counting_type, 5) ||
       !bgav_bitstream_get(&b, &full_timestamp_flag, 1) || 
       !bgav_bitstream_get(&b, &dummy, 1) || // discontinuity_flag
       !bgav_bitstream_get(&b, &dummy, 1) || // cnt_dropped_flag
       !bgav_bitstream_get(&b, &ret->tc_frames, 8)) // n_frames
      return 0;

    if(full_timestamp_flag)
      {
      if(!bgav_bitstream_get(&b, &ret->tc_seconds, 6) || // seconds
         !bgav_bitstream_get(&b, &ret->tc_minutes, 6) || // minutes
         !bgav_bitstream_get(&b, &ret->tc_hours, 5)) // hours
        return 0;
      }
    else
      {
      ret->tc_minutes = 0;
      ret->tc_seconds = 0;
      ret->tc_hours   = 0;
      
      if(!bgav_bitstream_get(&b, &dummy, 1)) // seconds_flag
        return 0;

      if(dummy)
        {
        if(!bgav_bitstream_get(&b, &ret->tc_seconds, 6)) // seconds
          return 0;

        if(!bgav_bitstream_get(&b, &dummy, 1)) // minutes_flag
          return 0;

        if(dummy)
          {
          if(!bgav_bitstream_get(&b, &ret->tc_minutes, 6)) // minutes
            return 0;
          
          if(!bgav_bitstream_get(&b, &dummy, 1)) // hours_flag
            return 0;

          if(dummy)
            {
            if(!bgav_bitstream_get(&b, &ret->tc_hours, 5)) // hours
              return 0;
            }
          }
        }
      }
    }
  else
    ret->have_timecode = 0;
      
  return 1;
  }

void bgav_h264_slice_header_parse(const uint8_t * data, int len,
                                  const bgav_h264_sps_t * sps,
                                  bgav_h264_slice_header_t * ret)
  {
  bgav_bitstream_t b;
  bgav_bitstream_init(&b, data, len);

  memset(ret, 0, sizeof(*ret));

  bgav_bitstream_get_golomb_ue(&b, &ret->first_mb_in_slice);
  bgav_bitstream_get_golomb_ue(&b, &ret->slice_type);
  bgav_bitstream_get_golomb_ue(&b, &ret->pic_parameter_set_id);

  if(sps->separate_colour_plane_flag)
    bgav_bitstream_get(&b, &ret->colour_plane_id, 2);

  bgav_bitstream_get(&b, &ret->frame_num, sps->log2_max_frame_num_minus4+4);

  if(!sps->frame_mbs_only_flag)
    {
    bgav_bitstream_get(&b, &ret->field_pic_flag, 1);
    if(ret->field_pic_flag)
      bgav_bitstream_get(&b, &ret->bottom_field_flag, 1);
    }
  }

void bgav_h264_slice_header_dump(const bgav_h264_sps_t * sps,
                                 const bgav_h264_slice_header_t * ret)
  {
  gavl_dprintf("Slice header\n");
  gavl_dprintf("  first_mb_in_slice:    %d\n", ret->first_mb_in_slice);
  gavl_dprintf("  slice_type:           %d\n", ret->slice_type);
  gavl_dprintf("  pic_parameter_set_id: %d\n", ret->pic_parameter_set_id);
  if(sps->separate_colour_plane_flag)
    gavl_dprintf("  colour_plane_id:      %d\n", ret->colour_plane_id);
  gavl_dprintf("  frame_num:            %d\n", ret->frame_num);

  //  if(!sps->frame_mbs_only_flag)
  //    {
    gavl_dprintf("  field_pic_flag:       %d\n", ret->field_pic_flag);
    if(ret->field_pic_flag)
      gavl_dprintf("  bottom_field_flag:    %d\n", ret->bottom_field_flag);
    //    }
  }

