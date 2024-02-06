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
  qt_mfhd_t mfhd;
  
  int num_trafs;
  int trafs_alloc;
  qt_traf_t * traf;
  } qt_moof_t;
#endif

int bgav_qt_moof_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_moof_t * ret)
  {
  qt_atom_header_t ch; /* Child header */
  memcpy(&ret->h, h, sizeof(*h));

  while(input->position < h->start_position + h->size)
    {
    if(!bgav_qt_atom_read_header(input, &ch))
      return 0;

    switch(ch.fourcc)
      {
      case BGAV_MK_FOURCC('m', 'f', 'h', 'd'):
        if(!bgav_qt_mfhd_read(&ch, input, &ret->mfhd))
          return 0;
        break;
      case BGAV_MK_FOURCC('t', 'r', 'a', 'f'):
        if(ret->num_trafs + 1 > ret->trafs_alloc)
          {
          ret->trafs_alloc += 16;
          ret->traf = realloc(ret->traf, ret->trafs_alloc * sizeof(*ret->traf));
          memset(ret->traf + ret->num_trafs, 0,
                 (ret->trafs_alloc - ret->num_trafs) * sizeof(*ret->traf));
          }

        if(!bgav_qt_traf_read(&ch, input, ret->traf + ret->num_trafs))
          return 0;
        ret->num_trafs++;
        break;
      default:
        bgav_qt_atom_skip_unknown(input, &ch, h->fourcc);
        break;
        
      }
    
    }
  return 1;
  }

void bgav_qt_moof_free(qt_moof_t * c)
  {
  int i;
  if(c->traf)
    {
    for(i = 0; i < c->num_trafs; i++)
      bgav_qt_traf_free(&c->traf[i]);
    free(c->traf);
    }

  /* Reinitialize */
  memset(c, 0, sizeof(*c));
  }
 

void bgav_qt_moof_dump(int indent, qt_moof_t * c)
  {
  int i;
  gavl_diprintf(indent, "moof\n");
  bgav_qt_mfhd_dump(indent+2, &c->mfhd);

  for(i = 0; i < c->num_trafs; i++)
    bgav_qt_traf_dump(indent+2, c->traf + i);
  
  gavl_diprintf(indent, "end of moof\n");
  }

