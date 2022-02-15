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

int bgav_qt_moov_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_moov_t * ret)
  {
  qt_atom_header_t ch; /* Child header */
  memcpy(&ret->h, h, sizeof(*h));

  while(input->position < h->start_position + h->size)
    {
    if(!bgav_qt_atom_read_header(input, &ch))
      return 0;
    switch(ch.fourcc)
      {
      case BGAV_MK_FOURCC('c', 'm', 'o', 'v'):
        /* Compressed header */
        return bgav_qt_cmov_read(&ch, input, ret);
        break;
        /* Reference movie */
      case BGAV_MK_FOURCC('r', 'm', 'r', 'a'):
        if(!bgav_qt_rmra_read(&ch, input, &ret->rmra))
          return 0;
        ret->has_rmra = 1;
        break;
      case BGAV_MK_FOURCC('t', 'r', 'a', 'k'):
        ret->num_tracks++;
        ret->tracks =
          realloc(ret->tracks, sizeof(*(ret->tracks))*ret->num_tracks);
        memset(&ret->tracks[ret->num_tracks-1], 0,
               sizeof(*ret->tracks));
        if(!bgav_qt_trak_read(&ch, input, &ret->tracks[ret->num_tracks-1]))
          return 0;
        //        bgav_qt_trak_dump(&ret->tracks[ret->num_tracks-1]);
        break;
      case BGAV_MK_FOURCC('m', 'v', 'h', 'd'):
        if(!bgav_qt_mvhd_read(&ch, input, &ret->mvhd))
          return 0;
        break;
      case BGAV_MK_FOURCC('u', 'd', 't', 'a'):
        if(!bgav_qt_udta_read(&ch, input, &ret->udta))
          return 0;
        ret->has_udta = 1;
        break;
      default:
        bgav_qt_atom_skip_unknown(input, &ch, h->fourcc);
        break;
      }
    }

  

  return 1;
  }

void bgav_qt_moov_free(qt_moov_t * c)
  {
  int i;
  if(c->num_tracks)
    {
    for(i = 0; i < c->num_tracks; i++)
      bgav_qt_trak_free(&c->tracks[i]);
    free(c->tracks);
    }
  bgav_qt_mvhd_free(&c->mvhd);
  bgav_qt_udta_free(&c->udta);
  }

void bgav_qt_moov_dump(int indent, qt_moov_t * c)
  {
  int i;
  bgav_diprintf(indent, "moov\n");
  bgav_qt_mvhd_dump(indent+2, &c->mvhd);

  if(c->has_udta)
    bgav_qt_udta_dump(indent+2, &c->udta);

  for(i = 0; i < c->num_tracks; i++)
    bgav_qt_trak_dump(indent+2, &c->tracks[i]);
  if(c->has_rmra)
    bgav_qt_rmra_dump(indent+2, &c->rmra);
  bgav_diprintf(indent, "end of moov\n");
  
  }

int bgav_qt_is_chapter_track(qt_moov_t * moov, qt_trak_t * trak)
  {
  int i, j, k;
  for(i = 0; i < moov->num_tracks; i++)
    {
    if(trak == &moov->tracks[i])
      continue;

    if(moov->tracks[i].has_tref)
      {
      for(j = 0; j < moov->tracks[i].tref.num_references; j++)
        {
        if(moov->tracks[i].tref.references[j].type ==
           BGAV_MK_FOURCC('c','h','a','p'))
          {
          for(k = 0; k < moov->tracks[i].tref.references[j].num_tracks; k++)
            {
            if(moov->tracks[i].tref.references[j].tracks[k] == trak->tkhd.track_id)
              return 1;
            }
          }
        }
      }
    }
  return 0;
  }

