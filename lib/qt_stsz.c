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
  uint32_t sample_size;
  uint32_t num_entries;
  uint32_t * entries;
  } qt_stsz_t;

*/

int bgav_qt_stsz_read(qt_atom_header_t * h,
                      bgav_input_context_t * input, qt_stsz_t * ret)
  {
  uint32_t i;
  READ_VERSION_AND_FLAGS;
  memcpy(&ret->h, h, sizeof(*h));
  
  if(!bgav_input_read_32_be(input, &ret->sample_size) ||
     !bgav_input_read_32_be(input, &ret->num_entries))
    return 0;
  
  if(!ret->sample_size)
    {
    ret->entries = calloc(ret->num_entries, sizeof(*(ret->entries)));
    for(i = 0; i < ret->num_entries; i++)
      {
      if(!bgav_input_read_32_be(input, &ret->entries[i]))
        return 0;
      }
    }
  return 1;
  }

void bgav_qt_stsz_free(qt_stsz_t * c)
  {
  if(c->entries)
    free(c->entries);
  }

void bgav_qt_stsz_dump(int indent, qt_stsz_t * c)
  {
  int i;
  bgav_diprintf(indent, "stsz\n");

  if(c->sample_size)
    {
    bgav_diprintf(indent+2, "sample size: %d\n", c->sample_size);
    }
  else
    {
    bgav_diprintf(indent+2, "num_entries: %d\n", c->num_entries);
    
    for(i = 0; i < c->num_entries; i++)
      {
      bgav_diprintf(indent+2, "sample size: %d\n", c->entries[i]);
      }
    }
  bgav_diprintf(indent, "end of stsz\n");
  }
