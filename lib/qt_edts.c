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

int bgav_qt_edts_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_edts_t * ret)
  {
  qt_atom_header_t ch; /* Child header */
  
  while(input->position < h->start_position + h->size)
    {
    if(!bgav_qt_atom_read_header(input, &ch))
      return 0;
    switch(ch.fourcc)
      {
      case BGAV_MK_FOURCC('e', 'l', 's', 't'):
        if(!bgav_qt_elst_read(&ch, input, &ret->elst))
          return 0;
        ret->has_elst = 1;
        break;
      default:
        bgav_qt_atom_skip_unknown(input, &ch, h->fourcc);
        break;
      }
    }
  return 1;

  }

void bgav_qt_edts_free(qt_edts_t * g)
  {
  if(g->has_elst)
    bgav_qt_elst_free(&g->elst);

  }

void bgav_qt_edts_dump(int indent, qt_edts_t * g)
  {
  bgav_diprintf(indent, "edts\n");
  if(g->has_elst)
    bgav_qt_elst_dump(indent+2, &g->elst);
  bgav_diprintf(indent, "end of edts\n");
  }
