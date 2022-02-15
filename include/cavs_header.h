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

#define CAVS_CODE_SEQUENCE   1
#define CAVS_CODE_PICTURE_I  2
#define CAVS_CODE_PICTURE_PB 3

int bgav_cavs_get_start_code(const uint8_t * data);

typedef struct
  {
  int profile_id;
  int level_id;
  int progressive_sequence;
  int horizontal_size;
  int vertical_size;
  int chromat_fromat;
  int sample_precision;
  int aspect_ratio;
  int frame_rate_code;
  int bit_rate_lower;
  /* int market_bit */
  int bit_rate_upper;
  int low_delay;
  
  } bgav_cavs_sequence_header_t;

int bgav_cavs_sequence_header_read(const bgav_options_t * opt,
                                   bgav_cavs_sequence_header_t * ret,
                                   const uint8_t * buffer, int len);

void bgav_cavs_sequence_header_dump(const bgav_cavs_sequence_header_t * h);
                               
typedef struct
  {
  int coding_type;

  int bbv_delay;       /* I/PB, 16 */
  int time_code_flag;  /* I, 1 */
  // if(time_code_flag) {
  int time_code;       /* I, 24 */
  // } else {
  int picture_coding_type; /* PB, 2 */
  // }
  int picture_distance;    /* I/PB, 8 */
  int bbv_check_times;     /* I/PB, ue */
  int progressive_frame;   /* I/PB, 1 */
  // if(!progressive_frame) {
  int picture_structure;
  // if(!coding_type != I) {
  int advanced_pred_mode_disable;
  // }}
  int top_field_first;
  int repeat_first_field;
  
  } bgav_cavs_picture_header_t;

int bgav_cavs_picture_header_read(const bgav_options_t * opt,
                                  bgav_cavs_picture_header_t * ret,
                                  const uint8_t * buffer, int len,
                                  const bgav_cavs_sequence_header_t * seq);

void bgav_cavs_picture_header_dump(const bgav_cavs_picture_header_t * h,
                                   const bgav_cavs_sequence_header_t * seq);
