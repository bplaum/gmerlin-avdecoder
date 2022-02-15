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
#include <stdio.h>

#include <qt.h>



int bgav_qt_frma_read(qt_atom_header_t * h, bgav_input_context_t * ctx,
                      qt_frma_t * ret)
  {
  int result;
  result = bgav_input_read_fourcc(ctx, &ret->fourcc);

  if(result)
    bgav_qt_atom_skip(ctx, h);
  return result;
  }
  
void bgav_qt_frma_dump(int indent, qt_frma_t * f)
  {
  bgav_diprintf(indent, "frma:\n");
  bgav_diprintf(indent+2, "fourcc: ");
  bgav_dump_fourcc(f->fourcc);
  bgav_dprintf( "\n");
  }
