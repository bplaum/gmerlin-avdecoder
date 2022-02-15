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
#include <qt.h>

/*
typedef struct
  {
  qt_atom_header_t h;
  int version;
  uint32_t flags;
  uint32_t creation_time;
  uint32_t modification_time;
  uint32_t time_scale;
  uint32_t duration;
  float preferred_rate;
  float preferred_volume;
  char reserved[10];
  float matrix[9];
  uint32_t preview_time;
  uint32_t preview_duration;
  uint32_t poster_time;
  uint32_t selection_time;
  uint32_t selection_duration;
  uint32_t current_time;
  uint32_t next_track_id;
  
  } qt_mvhd_t;

*/

int bgav_qt_mvhd_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_mvhd_t * ret)
  {
  int i;
  uint32_t i_tmp;
  READ_VERSION_AND_FLAGS;
  memcpy(&ret->h, h, sizeof(*h));

  if(version == 0)
    {
    if(!bgav_input_read_32_be(input, &i_tmp))
      return 0;
    ret->creation_time = i_tmp;
    
    if(!bgav_input_read_32_be(input, &i_tmp))
      return 0;
    ret->modification_time = i_tmp;
    if(!bgav_input_read_32_be(input, &ret->time_scale))
      return 0;
    if(!bgav_input_read_32_be(input, &i_tmp))
      return 0;
    ret->duration = i_tmp;
    }
  else if(version == 1)
    {
    if(!bgav_input_read_64_be(input, &ret->creation_time) ||
       !bgav_input_read_64_be(input, &ret->modification_time) ||
       !bgav_input_read_32_be(input, &ret->time_scale) ||
       !bgav_input_read_64_be(input, &ret->duration))
      return 0;
    }

  if(!bgav_qt_read_fixed32(input, &ret->preferred_rate) ||
     !bgav_qt_read_fixed16(input, &ret->preferred_volume) ||
     !(bgav_input_read_data(input, ret->reserved, 10) == 10))
    return 0;
  
  for(i = 0; i < 9; i++)
    if(!bgav_qt_read_fixed32(input, &ret->matrix[i]))
      return 0;

  return(bgav_input_read_32_be(input, &ret->preview_time) &&
         bgav_input_read_32_be(input, &ret->preview_duration) &&
         bgav_input_read_32_be(input, &ret->poster_time) &&
         bgav_input_read_32_be(input, &ret->selection_time) &&
         bgav_input_read_32_be(input, &ret->selection_duration) &&
         bgav_input_read_32_be(input, &ret->current_time) &&
         bgav_input_read_32_be(input, &ret->next_track_id));
  }

void bgav_qt_mvhd_free(qt_mvhd_t * c)
  {
  
  }

/*
  int version;
  uint32_t flags;
  uint64_t creation_time;
  uint64_t modification_time;
  uint32_t time_scale;
  uint64_t duration;
  float preferred_rate;
  float preferred_volume;
  uint8_t reserved[10];
  float matrix[9];
  uint32_t preview_time;
  uint32_t preview_duration;
  uint32_t poster_time;
  uint32_t selection_time;
  uint32_t selection_duration;
  uint32_t current_time;
  uint32_t next_track_id;
*/

void bgav_qt_mvhd_dump(int indent, qt_mvhd_t * c)
  {
  int i, j;
  
  bgav_diprintf(indent, "mvhd\n");
  bgav_diprintf(indent+2, "version:            %d\n", c->version);
  bgav_diprintf(indent+2, "flags:              %08x\n", c->flags);
  bgav_diprintf(indent+2, "creation_time:      %" PRId64 "\n", c->creation_time);
  bgav_diprintf(indent+2, "modification_time:  %" PRId64 "\n", c->modification_time);
  bgav_diprintf(indent+2, "time_scale:         %d\n", c->time_scale);
  bgav_diprintf(indent+2, "duration:           %" PRId64 "\n", c->duration);
  bgav_diprintf(indent+2, "preferred_rate:     %f\n", c->preferred_rate);
  bgav_diprintf(indent+2, "preferred_volume:   %f\n", c->preferred_volume);
  bgav_diprintf(indent+2, "reserved:           ");
  gavl_hexdump(c->reserved, 10, 10);
  bgav_dprintf("\n");
  bgav_diprintf(indent+2, "Matrix:\n");

  for(i = 0; i < 3; i++)
    {
    bgav_diprintf(indent, "    ");
    for(j = 0; j < 3; j++)
      bgav_dprintf( "%f ", c->matrix[3*i+j]);
    bgav_dprintf( "\n");
    }
  bgav_diprintf(indent+2, "preview_time:       %d\n", c->preview_time);
  bgav_diprintf(indent+2, "preview_duration:   %d\n", c->preview_duration);
  bgav_diprintf(indent+2, "selection_time:     %d\n", c->selection_time);
  bgav_diprintf(indent+2, "selection_duration: %d\n", c->selection_duration);
  bgav_diprintf(indent+2, "current_time:       %d\n", c->current_time);
  bgav_diprintf(indent+2, "next_track_id:      %d\n", c->next_track_id);
  
  bgav_diprintf(indent, "end of mvhd\n");
  }
