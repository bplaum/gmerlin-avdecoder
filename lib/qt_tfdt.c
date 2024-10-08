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

int bgav_qt_tfdt_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_tfdt_t * ret)
  {
  uint32_t t32;
  READ_VERSION_AND_FLAGS;
  memcpy(&ret->h, h, sizeof(*h));

  if(ret->version == 1)
    return bgav_input_read_64_be(input, &ret->decode_time);
  else
    {
    if(!bgav_input_read_32_be(input, &t32))
      return 0;
    ret->decode_time = t32;
    }
  return 1;
  }

void bgav_qt_tfdt_dump(int indent, qt_tfdt_t * g)
  {
  gavl_diprintf(indent, "tfdt\n");
  gavl_diprintf(indent+2, "version:     %d\n", g->version);
  gavl_diprintf(indent+2, "flags:       %08x\n", g->flags);
  gavl_diprintf(indent+2, "decode_time: %"PRId64"\n", g->decode_time);
  gavl_diprintf(indent, "end of tfdt\n");
  }

