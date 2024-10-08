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
#include <stdlib.h>
#include <avdec_private.h>

#include <stdio.h>
#include <qt.h>

int bgav_qt_fiel_read(qt_atom_header_t * h, bgav_input_context_t * ctx,
                      qt_fiel_t * ret)
  {
  memcpy(&ret->h, h, sizeof(*h));
  if(!bgav_input_read_data(ctx, &ret->fields, 1) ||
     !bgav_input_read_data(ctx, &ret->detail, 1))
    return 0;
  //  bgav_qt_fiel_dump(ret);
  return 1;
  }

void bgav_qt_fiel_dump(int indent, qt_fiel_t * p)
  {
  gavl_diprintf(indent, "fiel:\n");
  gavl_diprintf(indent+2, "fields: %d\n", p->fields);
  gavl_diprintf(indent+2, "detail: %d\n", p->detail);
  }
