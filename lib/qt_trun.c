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
#include <stdlib.h>

#include <avdec_private.h>
#include <qt.h>

/* 

typedef struct
  {
  uint32_t sample_duration;
  uint32_t sample_size;
  uint32_t sample_flags;
  uint32_t sample_composition_time_offset;
  } qt_trun_sample_t;

#define TRUN_DATA_OFFSET_PRESENT                     0x000001 
#define TRUN_FIRST_SAMPLE_FLAGS_PRESENT              0x000004 
#define TRUN_SAMPLE_DURATION_PRESENT                 0x000100 
#define TRUN_SAMPLE_SIZE_PRESENT                     0x000200 
#define TRUN_SAMPLE_FLAGS_PRESENT                    0x000400 
#define TRUN_SAMPLE_COMPOSITION_TIME_OFFSETS_PRESENT 0x000800 

typedef struct
  {
  qt_atom_header_t h;
  int version;
  uint32_t flags;

  uint32_t sample_count;
  int32_t data_offset;
  uint32_t first_sample_flags;
  
  qt_trun_sample_t * samples;
  } qt_trun_t;
*/
  
int bgav_qt_trun_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_trun_t * ret)
  {
  int i;
  READ_VERSION_AND_FLAGS;
  memcpy(&ret->h, h, sizeof(*h));
  if(!bgav_input_read_32_be(input, &ret->sample_count))
    return 0;

  if((ret->flags & TRUN_DATA_OFFSET_PRESENT) &&
     !bgav_input_read_32_be(input, (uint32_t*)(&ret->data_offset)))
    return 0;

  if((ret->flags & TRUN_FIRST_SAMPLE_FLAGS_PRESENT) &&
     !bgav_input_read_32_be(input, &ret->first_sample_flags))
    return 0;

  if(!(ret->flags & (TRUN_SAMPLE_DURATION_PRESENT |
                     TRUN_SAMPLE_SIZE_PRESENT |
                     TRUN_SAMPLE_FLAGS_PRESENT |
                     TRUN_SAMPLE_COMPOSITION_TIME_OFFSETS_PRESENT)))
    return 1;
  
  ret->samples = calloc(ret->sample_count, sizeof(*ret->samples));
  
  for(i = 0; i < ret->sample_count; i++)
    {
    if((ret->flags & TRUN_SAMPLE_DURATION_PRESENT) &&
       !bgav_input_read_32_be(input, &ret->samples[i].sample_duration))
      return 0;

    if((ret->flags & TRUN_SAMPLE_SIZE_PRESENT) &&
       !bgav_input_read_32_be(input, &ret->samples[i].sample_size))
      return 0;

    if((ret->flags & TRUN_SAMPLE_FLAGS_PRESENT) &&
       !bgav_input_read_32_be(input, &ret->samples[i].sample_flags))
      return 0;

    if((ret->flags & TRUN_SAMPLE_COMPOSITION_TIME_OFFSETS_PRESENT) &&
       !bgav_input_read_32_be(input, &ret->samples[i].sample_composition_time_offset))
      return 0;
    }
  return 1;
  }

void bgav_qt_trun_dump(int indent, qt_trun_t * g)
  {
  int i;

  gavl_diprintf(indent, "trun\n");
  gavl_diprintf(indent+2, "version:                  %d\n", g->version);
  gavl_diprintf(indent+2, "flags:                    %08x\n", g->flags);
  gavl_diprintf(indent+2, "sample_count:             %d\n", g->sample_count);

  if(g->flags & TRUN_DATA_OFFSET_PRESENT)
    gavl_diprintf(indent+2, "data_offset:              %d\n", g->data_offset);
  if(g->flags & TRUN_FIRST_SAMPLE_FLAGS_PRESENT)
    gavl_diprintf(indent+2, "first_sample_flags:       %08x\n", g->first_sample_flags);

  if(g->samples)
    {
    for(i = 0; i < g->sample_count; i++)
      {
      gavl_diprintf(indent+2, "sample %d\n", i);
      
      if(g->flags & TRUN_SAMPLE_DURATION_PRESENT)
        gavl_diprintf(indent+4, "sample_duration                %d\n", g->samples[i].sample_duration);

      if(g->flags & TRUN_SAMPLE_SIZE_PRESENT)
        gavl_diprintf(indent+4, "sample_size                    %d\n", g->samples[i].sample_size);

      if(g->flags & TRUN_SAMPLE_FLAGS_PRESENT)
        gavl_diprintf(indent+4, "sample_flags                   %d\n", g->samples[i].sample_flags);
        
      if(g->flags & TRUN_SAMPLE_COMPOSITION_TIME_OFFSETS_PRESENT)
        gavl_diprintf(indent+4, "sample_composition_time_offset %d\n",
                      g->samples[i].sample_composition_time_offset);
      }
    }
  
  gavl_diprintf(indent, "end of trun\n");
  }

void bgav_qt_trun_free(qt_trun_t * c)
  {
  if(c->samples)
    free(c->samples);
  }
