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
#include <cavs_header.h>
#include <bitstream.h>

#define FRAME_I  0xb3
#define FRAME_PB 0xb6
#define SEQUENCE 0xb0

int bgav_cavs_get_start_code(const uint8_t * data)
  {
  switch(data[3])
    {
    case SEQUENCE:
      return CAVS_CODE_SEQUENCE;
      break;
    case FRAME_I:
      return CAVS_CODE_PICTURE_I;
      break;
    case FRAME_PB:
      return CAVS_CODE_PICTURE_PB;
      break;
    }
  return 0;
  }

int bgav_cavs_sequence_header_read(const bgav_options_t * opt,
                                   bgav_cavs_sequence_header_t * ret,
                                   const uint8_t * buffer, int len)
  {
  bgav_bitstream_t b;
  int dummy;
  
  buffer+=4;
  len -= 4;
  
  bgav_bitstream_init(&b, buffer, len);
  
  if(!bgav_bitstream_get(&b, &ret->profile_id,           8) ||
     !bgav_bitstream_get(&b, &ret->level_id,             8) ||
     !bgav_bitstream_get(&b, &ret->progressive_sequence, 1) ||
     !bgav_bitstream_get(&b, &ret->horizontal_size,      14) ||
     !bgav_bitstream_get(&b, &ret->vertical_size,        14) ||
     !bgav_bitstream_get(&b, &ret->chromat_fromat,       2) ||
     !bgav_bitstream_get(&b, &ret->sample_precision,     3) ||
     !bgav_bitstream_get(&b, &ret->aspect_ratio,         4) ||
     !bgav_bitstream_get(&b, &ret->frame_rate_code,      4) ||
     !bgav_bitstream_get(&b, &ret->bit_rate_lower,       18) ||
     !bgav_bitstream_get(&b, &dummy,                     1) ||
     !bgav_bitstream_get(&b, &ret->bit_rate_upper,       12) ||
     !bgav_bitstream_get(&b, &ret->low_delay,            1))
    return 0;

  return len - bgav_bitstream_get_bits(&b) / 8;
  }

void bgav_cavs_sequence_header_dump(const bgav_cavs_sequence_header_t * h)
  {
  bgav_dprintf("CAVS Sequence header\n");
  bgav_dprintf("  profile_id:           %d\n", h->profile_id);
  bgav_dprintf("  level_id:             %d\n", h->level_id);
  bgav_dprintf("  progressive_sequence: %d\n", h->progressive_sequence);
  bgav_dprintf("  horizontal_size:      %d\n", h->horizontal_size);
  bgav_dprintf("  vertical_size:        %d\n", h->vertical_size);
  bgav_dprintf("  chromat_fromat:       %d\n", h->chromat_fromat);
  bgav_dprintf("  sample_precision:     %d\n", h->sample_precision);
  bgav_dprintf("  aspect_ratio:         %d\n", h->aspect_ratio);
  bgav_dprintf("  frame_rate_code:      %d\n", h->frame_rate_code);
  bgav_dprintf("  bit_rate_lower:       %d\n", h->bit_rate_lower);
  /* market_bit */
  bgav_dprintf("  bit_rate_upper:       %d\n", h->bit_rate_upper);
  bgav_dprintf("  low_delay:            %d\n", h->low_delay);
  }


int bgav_cavs_picture_header_read(const bgav_options_t * opt,
                                  bgav_cavs_picture_header_t * ret,
                                  const uint8_t * buffer, int len,
                                  const bgav_cavs_sequence_header_t * seq)
  {
  int sc = buffer[3];
  bgav_bitstream_t b;
  
  memset(ret, 0, sizeof(*ret));

  //  fprintf(stderr, "Read picture header\n");
  //  gavl_hexdump(buffer, 16, 16);
  
  bgav_bitstream_init(&b, buffer + 4, len - 4);

  if(!bgav_bitstream_get(&b, &ret->bbv_delay, 16))
    return 0;

  if(sc == FRAME_I)
    {
    ret->coding_type = BGAV_CODING_TYPE_I;
    if(!bgav_bitstream_get(&b, &ret->time_code_flag, 1))
      return 0;
    if(ret->time_code_flag &&
       !bgav_bitstream_get(&b, &ret->time_code, 24))
      return 0;
    }
  else
    {
    if(!bgav_bitstream_get(&b, &ret->picture_coding_type, 2))
      return 0;
    if(ret->picture_coding_type == 1)
      ret->coding_type = BGAV_CODING_TYPE_P;
    else
      ret->coding_type = BGAV_CODING_TYPE_B;
    }

  if(!bgav_bitstream_get(&b, &ret->picture_distance, 8))
    return 0;

  if(seq->low_delay && 
     !bgav_bitstream_get_golomb_ue(&b, &ret->bbv_check_times))
    return 0; 

  if(!bgav_bitstream_get(&b, &ret->progressive_frame, 1))
    return 0;

  if(!ret->progressive_frame)
    {
    if(!bgav_bitstream_get(&b, &ret->picture_structure, 1))
      return 0;
    if(!ret->picture_structure && (sc == FRAME_PB))
      {
      if(!bgav_bitstream_get(&b, &ret->advanced_pred_mode_disable, 1))
        return 0;
      
      }
    }

  if(!bgav_bitstream_get(&b, &ret->top_field_first, 1) ||
     !bgav_bitstream_get(&b, &ret->repeat_first_field, 1))
    return 0;
  
  return len - bgav_bitstream_get_bits(&b) / 8;
  }

void bgav_cavs_picture_header_dump(const bgav_cavs_picture_header_t * h,
                                   const bgav_cavs_sequence_header_t * seq)
  {
  bgav_dprintf("CAVS Picture header\n");
  bgav_dprintf("  coding_type:                %s\n", bgav_coding_type_to_string(h->coding_type));

  bgav_dprintf("  bbv_delay:                  %d\n", h->bbv_delay);       /* I/PB, 16 */

  if(h->coding_type == BGAV_CODING_TYPE_I)
    {
    bgav_dprintf("  time_code_flag:             %d\n", h->time_code_flag);  /* I, 1 */
    if(h->time_code_flag)
      {
      bgav_dprintf("  time_code:                  %d\n", h->time_code);       /* I, 24 */
      }
    }
  else
    {
    bgav_dprintf("  picture_coding_type:        %d\n", h->picture_coding_type); /* PB, 2 */
    }
  bgav_dprintf("  picture_distance:           %d\n", h->picture_distance);    /* I/PB, 8 */
  bgav_dprintf("  bbv_check_times:            %d\n", h->bbv_check_times);     /* I/PB, ue */
  bgav_dprintf("  progressive_frame:          %d\n", h->progressive_frame);   /* I/PB, 1 */
  if(!h->progressive_frame)
    {
    bgav_dprintf("  picture_structure:          %d\n", h->picture_structure);
    if(h->coding_type != BGAV_CODING_TYPE_I)
      {
      bgav_dprintf("  advanced_pred_mode_disable: %d\n", h->advanced_pred_mode_disable );
      }
    }
  bgav_dprintf("  top_field_first:            %d\n", h->top_field_first);
  bgav_dprintf("  repeat_first_field:         %d\n", h->repeat_first_field);
  }

