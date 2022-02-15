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


int bgav_qt_ftab_read(qt_atom_header_t * h, bgav_input_context_t * ctx,
                      qt_ftab_t * ret)
  {
  int i;
  if(!bgav_input_read_16_be(ctx, &ret->num_fonts))
    return 0;

  ret->fonts = calloc(ret->num_fonts, sizeof(*ret->fonts));
  for(i = 0; i < ret->num_fonts; i++)
    {
    if(!bgav_input_read_16_be(ctx, &ret->fonts[i].font_id) ||
       !bgav_input_read_string_pascal(ctx, ret->fonts[i].font_name))
      return 0;
    }
  return 1;
  }

void bgav_qt_ftab_dump(int indent, qt_ftab_t * f)
  {
  int i;
  bgav_diprintf(indent, "ftab:\n");
  bgav_diprintf(indent+2, "Number of fonts: %d\n", f->num_fonts);
  for(i = 0; i < f->num_fonts; i++)
    {
    bgav_diprintf(indent + 2 , "Font %d, ID: %d, .name = %s\n",
                  i+1, f->fonts[i].font_id, f->fonts[i].font_name);
    }
  bgav_diprintf(indent, "end of ftab\n");
  }

void bgav_qt_ftab_free(qt_ftab_t * f)
  {
  if(f->fonts)
    free(f->fonts);
  }

