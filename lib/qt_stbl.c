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

#include <qt.h>


/*

typedef struct
  {
  qt_atom_header_t h;

  qt_stts_t stts;
  qt_stss_t stss;
  qt_stsd_t stsd;
  qt_stsz_t stsz;
  qt_stsc_t stsc;
  qt_stco_t stco;
  } qt_stbl_t;
*/

int bgav_qt_stbl_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_stbl_t * ret, qt_minf_t * minf)
  {
  qt_atom_header_t ch; /* Child header */
  memcpy(&ret->h, h, sizeof(*h));

  while(input->position < h->start_position + h->size)
    {
    if(!bgav_qt_atom_read_header(input, &ch))
      return 0;
    switch(ch.fourcc)
      {
      case BGAV_MK_FOURCC('s', 't', 't', 's'):
        if(!bgav_qt_stts_read(&ch, input, &ret->stts))
          return 0;
        break;
      case BGAV_MK_FOURCC('c', 't', 't', 's'):
        if(!bgav_qt_stts_read(&ch, input, &ret->ctts))
          return 0;
        // bgav_qt_ctts_shift(&ret->ctts);
        ret->has_ctts = 1;
        break;
      case BGAV_MK_FOURCC('s', 't', 's', 's'):
        if(!bgav_qt_stss_read(&ch, input, &ret->stss))
          return 0;
        ret->has_stss = 1;
        break;
      case BGAV_MK_FOURCC('s', 't', 'p', 's'):
        if(!bgav_qt_stss_read(&ch, input, &ret->stps))
          return 0;
        ret->has_stps = 1;
        break;
      case BGAV_MK_FOURCC('s', 't', 's', 'd'):
        /* Read stsd */
        if(!bgav_qt_stsd_read(&ch, input, &ret->stsd))
          return 0;
        break;
      case BGAV_MK_FOURCC('s', 't', 's', 'z'):
        if(!bgav_qt_stsz_read(&ch, input, &ret->stsz))
          return 0;
        break;
      case BGAV_MK_FOURCC('s', 't', 's', 'c'):
        if(!bgav_qt_stsc_read(&ch, input, &ret->stsc))
          return 0;
        break;
      case BGAV_MK_FOURCC('s', 't', 'c', 'o'):
        if(!bgav_qt_stco_read(&ch, input, &ret->stco))
          return 0;
        break;
      case BGAV_MK_FOURCC('c', 'o', '6', '4'):
        if(!bgav_qt_stco_read_64(&ch, input, &ret->stco))
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

void bgav_qt_stbl_free(qt_stbl_t * c)
  {
  bgav_qt_stts_free(&c->ctts);
  bgav_qt_stts_free(&c->stts);
  bgav_qt_stss_free(&c->stss);
  bgav_qt_stss_free(&c->stps);
  bgav_qt_stsd_free(&c->stsd);
  bgav_qt_stsz_free(&c->stsz);
  bgav_qt_stsc_free(&c->stsc);
  bgav_qt_stco_free(&c->stco);
  }

void bgav_qt_stbl_dump(int indent, qt_stbl_t * c)
  {
  bgav_diprintf(indent, "stbl\n");
  bgav_qt_stsd_dump(indent+2, &c->stsd);
  bgav_qt_stts_dump(indent+2, &c->stts);
  if(c->stss.num_entries)
    bgav_qt_stss_dump(indent+2, &c->stss);
  bgav_qt_stsz_dump(indent+2, &c->stsz);
  bgav_qt_stsc_dump(indent+2, &c->stsc);
  bgav_qt_stco_dump(indent+2, &c->stco);
  if(c->has_ctts)
    bgav_qt_stts_dump(indent+2, &c->ctts);
  if(c->has_stps)
    bgav_qt_stss_dump(indent+2, &c->stps);
    
  bgav_diprintf(indent, "end of stbl\n");
  }
