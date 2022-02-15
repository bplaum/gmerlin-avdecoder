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

#include <avdec_private.h>
#include <qt.h>

int bgav_qt_gmin_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_gmin_t * ret)
  {
  READ_VERSION_AND_FLAGS;
  return bgav_input_read_16_be(input, &ret->graphics_mode) &&
    bgav_input_read_16_be(input, &ret->opcolor[0]) &&
    bgav_input_read_16_be(input, &ret->opcolor[1]) &&
    bgav_input_read_16_be(input, &ret->opcolor[2]) &&
    bgav_input_read_16_be(input, &ret->balance) &&
    bgav_input_read_16_be(input, &ret->reserved);
  }

void bgav_qt_gmin_free(qt_gmin_t * g)
  {

  }

void bgav_qt_gmin_dump(int indent, qt_gmin_t * g)
  {
  bgav_diprintf(indent, "gmin\n");

  bgav_diprintf(indent+2, "version:       %d\n", g->version);
  bgav_diprintf(indent+2, "flags:         %08x\n", g->flags);
  bgav_diprintf(indent+2, "graphics_mode: %d\n", g->graphics_mode);
  bgav_diprintf(indent+2, "opcolors:      %d %d %d\n",
               g->opcolor[0], g->opcolor[1], g->opcolor[2]);
  bgav_diprintf(indent+2, "balance:       %d\n", g->balance);
  bgav_diprintf(indent+2, "reserved:      %d\n", g->reserved);
  bgav_diprintf(indent, "end of gmin\n");
  }
