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
#include <stdlib.h>

#include <avdec_private.h>
#include <stdio.h>

#include <qt.h>


/*
 typedef struct
   {
   qt_atom_header_t h;

   uint32_t num_entries;
   struct
     {
     uint32_t count;
     uint32_t duration;
     } * entries;
   } qt_stts_t;
*/

int bgav_qt_stts_read(qt_atom_header_t * h,
                      bgav_input_context_t * input, qt_stts_t * ret)
  {
  uint32_t i;

  READ_VERSION_AND_FLAGS;
  memcpy(&ret->h, h, sizeof(*h));
  
  if(!bgav_input_read_32_be(input, &ret->num_entries))
    return 0;

  ret->entries = calloc(ret->num_entries, sizeof(*(ret->entries)));
  
  for(i = 0; i < ret->num_entries; i++)
    {
    if(!bgav_input_read_32_be(input, &ret->entries[i].count) ||
       !bgav_input_read_32_be(input, &ret->entries[i].duration))
      return 0;
    }
  return 1;
  }

void bgav_qt_stts_free(qt_stts_t * c)
  {
  if(c->entries)
    free(c->entries);
  }

void bgav_qt_stts_dump(int indent, qt_stts_t * c)
  {
  int i;
  bgav_diprintf(indent, "%c%c%c%c\n",
                (c->h.fourcc & 0xFF000000) >> 24,
                (c->h.fourcc & 0x00FF0000) >> 16,
                (c->h.fourcc & 0x0000FF00) >> 8,
                (c->h.fourcc & 0x000000FF));
  
  bgav_diprintf(indent+2, "num_entries: %d\n", c->num_entries);
  for(i = 0; i < c->num_entries; i++)
    {
    bgav_diprintf(indent+2, "count: %d duration: %d\n",
            c->entries[i].count, c->entries[i].duration);
    
    }
  bgav_diprintf(indent, "end of stts\n");
  }

int64_t bgav_qt_stts_num_samples(qt_stts_t * c)
  {
  int i;
  int64_t ret = 0;
  for(i = 0; i < c->num_entries; i++)
    ret += c->entries[i].count;
  return ret;
  }
