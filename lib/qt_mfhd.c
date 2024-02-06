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
  uint32_t sequence_number;  
  } qt_mfhd_t;
*/
  
int bgav_qt_mfhd_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_mfhd_t * ret)
  {
  READ_VERSION_AND_FLAGS;
  memcpy(&ret->h, h, sizeof(*h));
  return bgav_input_read_32_be(input, &ret->sequence_number);

  }
  
void bgav_qt_mfhd_dump(int indent, qt_mfhd_t * g)
  {
  gavl_diprintf(indent, "mfhd\n");

  gavl_diprintf(indent+2, "version:         %d\n", g->version);
  gavl_diprintf(indent+2, "flags:           %08x\n", g->flags);
  gavl_diprintf(indent+2, "sequence_number: %d\n", g->sequence_number);
  gavl_diprintf(indent, "end of mfhd\n");
  }
