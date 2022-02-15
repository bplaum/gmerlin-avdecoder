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
#include <stdio.h>

#include <avdec_private.h>
#include <qt.h>


// http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap2/chapter_3_section_7.html#//apple_ref/doc/uid/TP40000939-CH204-BBCGAIFH

int bgav_qt_rmda_read(qt_atom_header_t * h,
                      bgav_input_context_t * input, qt_rmda_t * ret)
  {
  qt_atom_header_t ch;
  memcpy(&ret->h, h, sizeof(*h));

  while(input->position < h->start_position + h->size)
    {
    
    if(!bgav_qt_atom_read_header(input, &ch))
      return 0;

    // bgav_qt_atom_dump_header(0, &ch);
    
    switch(ch.fourcc)
      {
      case BGAV_MK_FOURCC('r', 'd', 'r', 'f'):
        if(!bgav_qt_rdrf_read(&ch, input, &ret->rdrf))
          return 0;
        ret->has_rdrf = 1;
        break;
      /* Data rate atom */
      case BGAV_MK_FOURCC('r', 'm', 'd', 'r'):
      /* CPU Speed atom */
      case BGAV_MK_FOURCC('r', 'm', 'c', 's'):
      /* Version check atom */
      case BGAV_MK_FOURCC('r', 'm', 'v', 'c'):
      /* Component detect atom */
      case BGAV_MK_FOURCC('r', 'm', 'c', 'd'):
      /* Quality atom */
      case BGAV_MK_FOURCC('r', 'm', 'q', 'u'):
        bgav_qt_atom_skip(input, &ch);
        break;
      default:
        bgav_qt_atom_skip_unknown(input, &ch, h->fourcc);
        break;
        
      }
    }
  return 1;
  }

void bgav_qt_rmda_free(qt_rmda_t * r)
  {
  bgav_qt_rdrf_free(&r->rdrf);
  }

void bgav_qt_rmda_dump(int indent, qt_rmda_t * r)
  {
  bgav_diprintf(indent, "rmda\n");
  if(r->has_rdrf)
    bgav_qt_rdrf_dump(indent+2, &r->rdrf);
  }
