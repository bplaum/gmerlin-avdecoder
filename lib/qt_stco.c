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
  int num_entries;
  
  uint64_t * entries;
  } qt_stco_t;

*/

int bgav_qt_stco_read(qt_atom_header_t * h,
                      bgav_input_context_t * input, qt_stco_t * ret)
  {
  uint32_t i, tmp;
  READ_VERSION_AND_FLAGS;
  memcpy(&ret->h, h, sizeof(*h));
  
  if(!bgav_input_read_32_be(input, &ret->num_entries))
    return 0;
  
  ret->entries = calloc(ret->num_entries, sizeof(*(ret->entries)));
  for(i = 0; i < ret->num_entries; i++)
    {
    if(!bgav_input_read_32_be(input, &tmp))
      return 0;
    ret->entries[i] = tmp;
    }
  return 1;
  }

int bgav_qt_stco_read_64(qt_atom_header_t * h,
                         bgav_input_context_t * input, qt_stco_t * ret)
  {
  uint32_t i;
  READ_VERSION_AND_FLAGS;
  memcpy(&ret->h, h, sizeof(*h));
  
  if(!bgav_input_read_32_be(input, &ret->num_entries))
    return 0;
  
  ret->entries = calloc(ret->num_entries, sizeof(*(ret->entries)));
  for(i = 0; i < ret->num_entries; i++)
    {
    if(!bgav_input_read_64_be(input, &ret->entries[i]))
      return 0;
    }
  return 1;
  }


void bgav_qt_stco_free(qt_stco_t * c)
  {
  if(c->entries)
    free(c->entries);
  }

void bgav_qt_stco_dump(int indent, qt_stco_t * c)
  {
  int i;
  bgav_diprintf(indent,  "stco\n");
  bgav_diprintf(indent+2,  "num_entries: %d\n", c->num_entries);
  
  for(i = 0; i < c->num_entries; i++)
    {
    bgav_diprintf(indent+2,  "offset: %" PRId64 "\n", c->entries[i]);
    }
  bgav_diprintf(indent,  "end of stco\n");
  }
