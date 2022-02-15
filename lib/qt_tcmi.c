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

int bgav_qt_tcmi_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_tcmi_t * ret)
  {
  READ_VERSION_AND_FLAGS;
  if(!bgav_input_read_16_be(input, &ret->font) ||
     !bgav_input_read_16_be(input, &ret->face) ||
     !bgav_input_read_16_be(input, &ret->size))
    return 0;
  bgav_input_skip(input, 2);
  
  if(!bgav_input_read_16_be(input, &ret->txtcolor[0]) ||
     !bgav_input_read_16_be(input, &ret->txtcolor[1]) ||
     !bgav_input_read_16_be(input, &ret->txtcolor[2]) ||
     !bgav_input_read_16_be(input, &ret->bgcolor[0]) ||
     !bgav_input_read_16_be(input, &ret->bgcolor[1]) ||
     !bgav_input_read_16_be(input, &ret->bgcolor[2]) ||
     !bgav_input_read_string_pascal(input, ret->fontname))
    return 0;
  return 1;
  }

void bgav_qt_tcmi_free(qt_tcmi_t * g)
  {

  }

void bgav_qt_tcmi_dump(int indent, qt_tcmi_t * tcmi)
  {
  bgav_diprintf(indent, "tcmi:\n");
  bgav_diprintf(indent+2, "version %d\n", tcmi->version);
  bgav_diprintf(indent+2, "flags %d\n", tcmi->flags);
  bgav_diprintf(indent+2, "font %d\n", tcmi->font);
  bgav_diprintf(indent+2, "face %d\n", tcmi->face);
  bgav_diprintf(indent+2, "size %d\n", tcmi->size);
  bgav_diprintf(indent+2, "txtcolor %d %d %d\n",
                tcmi->txtcolor[0], tcmi->txtcolor[1], tcmi->txtcolor[2]);
  bgav_diprintf(indent+2, "bgcolor %d %d %d\n",
                tcmi->bgcolor[0], tcmi->bgcolor[1], tcmi->bgcolor[2]);
  bgav_diprintf(indent+2, "fontname %s\n", tcmi->fontname);
  bgav_diprintf(indent, "end of tcmi\n");
  }
