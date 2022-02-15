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

int bgav_qt_pasp_read(qt_atom_header_t * h, bgav_input_context_t * ctx,
                      qt_pasp_t * ret)
  {
  memcpy(&ret->h, h, sizeof(*h));
  if(!bgav_input_read_32_be(ctx, &ret->hSpacing) ||
     !bgav_input_read_32_be(ctx, &ret->vSpacing))
    return 0;
  //  bgav_qt_pasp_dump(ret);
  return 1;
  }

void bgav_qt_pasp_dump(int indent, qt_pasp_t * p)
  {
  bgav_diprintf(indent, "pasp:\n");
  bgav_diprintf(indent+2, "hSpacing: %d\n", p->hSpacing);
  bgav_diprintf(indent+2, "vSpacing: %d\n", p->vSpacing);
  }
