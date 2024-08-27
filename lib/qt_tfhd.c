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
#include <qt.h>

/*
#define TFHD_BASE_DATA_OFFSET_PRESENT         0x000001
#define TFHD_SAMPLE_DESCRIPTION_INDEX_PRESENT 0x000002
#define TFHD_DEFAULT_SAMPLE_DURATION_PRESENT  0x000008
#define TFHD_DEFAULT_SAMPLE_SIZE_PRESENT      0x000010
#define TFHD_DEFAULT_SAMPLE_FLAGS_PRESENT     0x000020
#define TFHD_DURATION_IS_EMPTY                0x010000

typedef struct
  {
  qt_atom_header_t h;
  int version;
  uint32_t flags;
  
  uint32_t track_ID;
// all the following are optional fields
  uint64_t base_data_offset;
  uint32_t sample_description_index;
  uint32_t default_sample_duration;
  uint32_t default_sample_size;
  uint32_t default_sample_flags;
  } qt_tfhd_t;
*/
  
int bgav_qt_tfhd_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_tfhd_t * ret)
  {
  READ_VERSION_AND_FLAGS;
  memcpy(&ret->h, h, sizeof(*h));
  if(!bgav_input_read_32_be(input, &ret->track_ID))
    return 0;
  
  if((ret->flags & TFHD_BASE_DATA_OFFSET_PRESENT) &&
     !bgav_input_read_64_be(input, &ret->base_data_offset))
    return 0;

  if((ret->flags & TFHD_SAMPLE_DESCRIPTION_INDEX_PRESENT) &&
     !bgav_input_read_32_be(input, &ret->sample_description_index))
    return 0;

  if((ret->flags & TFHD_DEFAULT_SAMPLE_DURATION_PRESENT) &&
     !bgav_input_read_32_be(input, &ret->default_sample_duration))
    return 0;

  if((ret->flags & TFHD_DEFAULT_SAMPLE_SIZE_PRESENT) &&
     !bgav_input_read_32_be(input, &ret->default_sample_size))
    return 0;

  if((ret->flags & TFHD_DEFAULT_SAMPLE_FLAGS_PRESENT) &&
     !bgav_input_read_32_be(input, &ret->default_sample_flags))
    return 0;

  return 1;
  }

void bgav_qt_tfhd_dump(int indent, qt_tfhd_t * g)
  {
  gavl_diprintf(indent, "tfhd\n");
  gavl_diprintf(indent+2, "version:                  %d\n", g->version);
  gavl_diprintf(indent+2, "flags:                    %08x\n", g->flags);
  gavl_diprintf(indent+2, "track_ID:                 %d\n", g->track_ID);
  
  if(g->flags & TFHD_BASE_DATA_OFFSET_PRESENT)
    gavl_diprintf(indent+2, "base_data_offset:         %"PRId64"\n", g->base_data_offset);

  if(g->flags & TFHD_SAMPLE_DESCRIPTION_INDEX_PRESENT)
    gavl_diprintf(indent+2, "sample_description_index: %d\n", g->sample_description_index);

  if(g->flags & TFHD_DEFAULT_SAMPLE_DURATION_PRESENT)
    gavl_diprintf(indent+2, "default_sample_duration:  %d\n", g->default_sample_duration);

  if(g->flags & TFHD_DEFAULT_SAMPLE_SIZE_PRESENT)
    gavl_diprintf(indent+2, "default_sample_size:      %d\n", g->default_sample_size);

  if(g->flags & TFHD_DEFAULT_SAMPLE_FLAGS_PRESENT)
    gavl_diprintf(indent+2, "default_sample_flags:     %d\n", g->default_sample_flags);
  
  gavl_diprintf(indent, "end of tfhd\n");
  }
