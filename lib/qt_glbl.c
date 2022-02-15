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
#include <string.h>
#include <stdlib.h>

#include <qt.h>


int bgav_qt_glbl_read(qt_atom_header_t * h, bgav_input_context_t * ctx,
                      qt_glbl_t * ret)
  {
  ret->size = h->size - 8;
  ret->data = malloc(ret->size);
  if(bgav_input_read_data(ctx, ret->data, ret->size) < ret->size)
    return 0;
  return 1;
  }

void bgav_qt_glbl_dump(int indent, qt_glbl_t * f)
  {
  bgav_diprintf(indent, "glbl:\n");
  bgav_diprintf(indent+2, "Size: %d\n", f->size);
  gavl_hexdump(f->data, f->size, 16);
  }

void bgav_qt_glbl_free(qt_glbl_t * r)
  {
  if(r->data) free(r->data);
  }
