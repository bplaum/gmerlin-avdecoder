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

#if 0
typedef struct
  {
  qt_tfhd_t tfhd;
  
  int num_truns;
  int truns_alloc;
  qt_trun_t * trun;
  } qt_traf_t;
#endif

int bgav_qt_traf_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_traf_t * ret)
  {
  qt_atom_header_t ch; /* Child header */
  memcpy(&ret->h, h, sizeof(*h));

  while(input->position < h->start_position + h->size)
    {
    if(!bgav_qt_atom_read_header(input, &ch))
      return 0;

    switch(ch.fourcc)
      {
      case BGAV_MK_FOURCC('t', 'f', 'h', 'd'):
        if(!bgav_qt_tfhd_read(&ch, input, &ret->tfhd))
          return 0;
        break;
      case BGAV_MK_FOURCC('t', 'f', 'd', 't'):
        if(!bgav_qt_tfdt_read(&ch, input, &ret->tfdt))
          return 0;
        ret->have_tfdt = 1;
        break;
      case BGAV_MK_FOURCC('t', 'r', 'u', 'n'):
        if(ret->num_truns + 1 > ret->truns_alloc)
          {
          ret->truns_alloc += 16;
          ret->trun = realloc(ret->trun, ret->truns_alloc * sizeof(*ret->trun));
          memset(ret->trun + ret->num_truns, 0,
                 (ret->truns_alloc - ret->num_truns) * sizeof(*ret->trun));
          }

        if(!bgav_qt_trun_read(&ch, input, ret->trun + ret->num_truns))
          return 0;
        ret->num_truns++;
        break;
      default:
        bgav_qt_atom_skip_unknown(input, &ch, h->fourcc);
        break;
        
      }
    
    }
  return 1;
  }

void bgav_qt_traf_free(qt_traf_t * c)
  {
  int i;
  if(c->trun)
    {
    for(i = 0; i < c->num_truns; i++)
      bgav_qt_trun_free(&c->trun[i]);
    free(c->trun);
    }
  }
 

void bgav_qt_traf_dump(int indent, qt_traf_t * c)
  {
  int i;
  gavl_diprintf(indent, "traf\n");
  bgav_qt_tfhd_dump(indent+2, &c->tfhd);
 
  if(c->have_tfdt)
    bgav_qt_tfdt_dump(indent+2, &c->tfdt);

  for(i = 0; i < c->num_truns; i++)
    bgav_qt_trun_dump(indent+2, c->trun + i);
  
  gavl_diprintf(indent, "end of traf\n");
  
  }
