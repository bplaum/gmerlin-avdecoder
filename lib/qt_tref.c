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
#include <qt.h>

int bgav_qt_tref_read(qt_atom_header_t * h,
                      bgav_input_context_t * input, qt_tref_t * ret)
  {
  int i;
  uint32_t size;

  while(input->position < h->start_position + h->size)
    {
    ret->references = realloc(ret->references,
                              (ret->num_references+1) * sizeof(*ret->references));

    if(!bgav_input_read_32_be(input, &size) ||
       !bgav_input_read_fourcc(input, &ret->references[ret->num_references].type))
      return 0;

    ret->references[ret->num_references].num_tracks = (size - 8)/4;
    ret->references[ret->num_references].tracks =
      malloc(ret->references[ret->num_references].num_tracks *
             sizeof(*ret->references[ret->num_references].tracks));

    for(i = 0; i < ret->references[ret->num_references].num_tracks; i++)
      {
      if(!bgav_input_read_32_be(input, &ret->references[ret->num_references].tracks[i]))
        return 0;
      }
    ret->num_references++;
    }
  return 1;
  }

void bgav_qt_tref_free(qt_tref_t * r)
  {
  int i;
  for(i = 0; i < r->num_references; i++)
    {
    if(r->references[i].tracks)
      free(r->references[i].tracks);
    }
  free(r->references);
  }

void bgav_qt_tref_dump(int indent, qt_tref_t * c)
  {
  int i, j;
  bgav_diprintf(indent, "tref\n");

  for(i = 0; i < c->num_references; i++)
    {
    bgav_diprintf(indent+2, "track reference: ");
    bgav_dump_fourcc(c->references[i].type);
    bgav_dprintf(" (%d tracks)\n", c->references[i].num_tracks);
    for(j = 0; j < c->references[i].num_tracks; j++)
      bgav_diprintf(indent+2, "Track %d\n", c->references[i].tracks[j]);
    }
  bgav_diprintf(indent, "end of tref\n");
  }
