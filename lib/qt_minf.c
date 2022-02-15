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

int bgav_qt_minf_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_minf_t * ret)
  {
  qt_atom_header_t ch; /* Child header */
  memcpy(&ret->h, h, sizeof(*h));

  while(input->position < h->start_position + h->size)
    {
    if(!bgav_qt_atom_read_header(input, &ch))
      return 0;
    switch(ch.fourcc)
      {
      case BGAV_MK_FOURCC('h', 'd', 'l', 'r'):
        if(!bgav_qt_hdlr_read(&ch, input, &ret->hdlr))
          return 0;
        break;
      case BGAV_MK_FOURCC('d', 'i', 'n', 'f'):
        if(!bgav_qt_dinf_read(&ch, input, &ret->dinf))
          return 0;
        ret->has_dinf = 1;
        break;
      case BGAV_MK_FOURCC('g', 'm', 'h', 'd'):
        if(!bgav_qt_gmhd_read(&ch, input, &ret->gmhd))
          return 0;
        ret->has_gmhd = 1;
        break;
      case BGAV_MK_FOURCC('n', 'm', 'h', 'd'):
        if(!bgav_qt_nmhd_read(&ch, input, &ret->nmhd))
          return 0;
        ret->has_nmhd = 1;
        break;
      case BGAV_MK_FOURCC('s', 't', 'b', 'l'):
        if(!bgav_qt_stbl_read(&ch, input, &ret->stbl, ret))
          return 0;
        break;
      case BGAV_MK_FOURCC('v', 'm', 'h', 'd'):
        ret->has_vmhd = 1;
        bgav_qt_atom_skip(input, &ch);
        break;
      case BGAV_MK_FOURCC('s', 'm', 'h', 'd'):
        ret->has_smhd = 1;
        bgav_qt_atom_skip(input, &ch);
        break;
      default:
        bgav_qt_atom_skip_unknown(input, &ch, h->fourcc);
      }
    }
  return 1;
  }

void bgav_qt_minf_free(qt_minf_t * h)
  {
  //  bgav_qt_vmhd_free(&h->vmhd);
  //  bgav_qt_dinf_free(&h->dinf);
  bgav_qt_hdlr_free(&h->hdlr);
  bgav_qt_stbl_free(&h->stbl);
  
  if(h->has_dinf)
    bgav_qt_dinf_free(&h->dinf);

  if(h->has_gmhd)
    bgav_qt_gmhd_free(&h->gmhd);
  }

void bgav_qt_minf_dump(int indent, qt_minf_t * h)
  {
  bgav_diprintf(indent, "minf\n");
  bgav_qt_hdlr_dump(indent+2, &h->hdlr);
  bgav_qt_stbl_dump(indent+2, &h->stbl);

  if(h->has_dinf)
    bgav_qt_dinf_dump(indent+2, &h->dinf);

  if(h->has_gmhd)
    bgav_qt_gmhd_dump(indent+2, &h->gmhd);
  if(h->has_nmhd)
    bgav_qt_nmhd_dump(indent+2, &h->nmhd);
  
  bgav_diprintf(indent, "end of minf\n");
  }
