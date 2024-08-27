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

/*
  
typedef struct
  {
  qt_atom_header_t h;

  qt_mdhd_t mdhd;
  qt_hdlr_t hdlr;
  qt_minf_t minf;
  } qt_mdia_t;

*/

int bgav_qt_mdia_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_mdia_t * ret)
  {
  qt_atom_header_t ch; /* Child header */
  memcpy(&ret->h, h, sizeof(*h));

  while(input->position < h->start_position + h->size)
    {
    if(!bgav_qt_atom_read_header(input, &ch))
      return 0;
    switch(ch.fourcc)
      {
      case BGAV_MK_FOURCC('m', 'd', 'h', 'd'):
        if(!bgav_qt_mdhd_read(&ch, input, &ret->mdhd))
          return 0;
        break;
      case BGAV_MK_FOURCC('h', 'd', 'l', 'r'):
        if(!bgav_qt_hdlr_read(&ch, input, &ret->hdlr))
          return 0;
        break;
      case BGAV_MK_FOURCC('m', 'i', 'n', 'f'):
        if(!bgav_qt_minf_read(&ch, input, &ret->minf))
          return 0;
        break;
      default:
        bgav_qt_atom_skip_unknown(input, &ch, h->fourcc);
        break;
      }
    bgav_qt_atom_skip(input, &ch);
    }
  return 1;
  }

void bgav_qt_mdia_free(qt_mdia_t * c)
  {
  bgav_qt_mdhd_free(&c->mdhd);
  bgav_qt_hdlr_free(&c->hdlr);
  bgav_qt_minf_free(&c->minf);
  }

void bgav_qt_mdia_dump(int indent, qt_mdia_t * c)
  {
  gavl_diprintf(indent, "mdia\n");
  bgav_qt_mdhd_dump(indent+2, &c->mdhd);
  bgav_qt_hdlr_dump(indent+2, &c->hdlr);
  bgav_qt_minf_dump(indent+2, &c->minf);
  gavl_diprintf(indent, "end of mdia\n");
  }
