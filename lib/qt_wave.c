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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <qt.h>

#define LOG_DOMAIN "quicktime.wave"

int bgav_qt_wave_read(qt_atom_header_t * h, bgav_input_context_t * ctx,
                      qt_wave_t * ret)
  {
  int done = 0;
  qt_atom_header_t ch; /* Child header */
  bgav_input_context_t * input_mem;
    
  ret->raw_size = h->size - (ctx->position - h->start_position);
  ret->raw = malloc(ret->raw_size);
  
  if(bgav_input_read_data(ctx, ret->raw, ret->raw_size) < ret->raw_size)
    return 0;
  
  input_mem = bgav_input_open_memory(ret->raw,
                                     ret->raw_size);
  
  while(input_mem->position < ret->raw_size)
    {
    //    data_ptr = ret->raw + input_mem->position;
    
    if(!bgav_qt_atom_read_header(input_mem, &ch))
      return 0;
    switch(ch.fourcc)
      {
      case BGAV_MK_FOURCC('e', 's', 'd', 's'):
        if(!bgav_qt_esds_read(&ch, input_mem, &ret->esds))
          return 0;
        ret->has_esds = 1;
        break;
      case BGAV_MK_FOURCC('f', 'r', 'm', 'a'):
        if(!bgav_qt_frma_read(&ch, input_mem, &ret->frma))
          return 0;
        ret->has_frma = 1;
        break;
      case BGAV_MK_FOURCC('e', 'n', 'd', 'a'):
        if(!bgav_qt_enda_read(&ch, input_mem, &ret->enda))
          return 0;
        ret->has_enda = 1;
        break;
      case 0: /* audio atom Terminator */
        done = 1;
        ret->raw_size -= 8;
        break;
      default:
        /* Workaround for some broken encoders, which write some parts
           of the wave atom in little endian */
        if(ch.size > ret->raw_size)
          {
          gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                   "Skipping remainder of broken wave atom");
          done = 1;
          break;
          }
        /* Append user atom */
        if(!bgav_qt_user_atoms_append(&ch, input_mem, &ret->user))
          return 0;
        break;
      }
    if(done)
      break;
    }
  bgav_input_destroy(input_mem);
  return 1;
  }

void bgav_qt_wave_dump(int indent, qt_wave_t * f)
  {

  bgav_diprintf(indent, "wave\n");
  if(f->has_frma)
    bgav_qt_frma_dump(indent+2, &f->frma);
  if(f->has_enda)
    bgav_qt_enda_dump(indent+2, &f->enda);
  if(f->has_esds)
    bgav_qt_esds_dump(indent+2, &f->esds);

  bgav_qt_user_atoms_dump(indent+2, &f->user);
  }

void bgav_qt_wave_free(qt_wave_t * w)
  {
  if(w->raw)
    free(w->raw);
  
  if(w->has_esds)
    bgav_qt_esds_free(&w->esds);

  bgav_qt_user_atoms_free(&w->user);
  
  }
