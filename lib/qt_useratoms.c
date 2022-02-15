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

#include <stdlib.h>


#include <avdec_private.h>
#include <qt.h>

#include <utils.h>

int bgav_qt_user_atoms_append(qt_atom_header_t * h,
                               bgav_input_context_t * ctx,
                               qt_user_atoms_t * ret)
  {
  ret->atoms = realloc(ret->atoms,
                       (ret->num + 1) * sizeof(*ret->atoms));
  ret->atoms[ret->num] = malloc(h->size);
  GAVL_32BE_2_PTR(h->size, ret->atoms[ret->num]);
  GAVL_32BE_2_PTR(h->fourcc, ret->atoms[ret->num]+4);

  if(bgav_input_read_data(ctx, ret->atoms[ret->num]+8,
                          h->size - 8) < h->size - 8)
    return 0;

  ret->num++;
  return 1;
  }

void bgav_qt_user_atoms_free(qt_user_atoms_t * a)
  {
  int i;

  if(!a->atoms)
    return;
  
  for(i = 0; i < a->num; i++)
    {
    if(a->atoms[i])
      free(a->atoms[i]);
    }
  free(a->atoms);
  }

void bgav_qt_user_atoms_dump(int indent, qt_user_atoms_t * a)
  {
  int i;
  int size;
  uint32_t fourcc;
  
  for(i = 0; i < a->num; i++)
    {
    size = GAVL_PTR_2_32BE(a->atoms[i]);
    fourcc = BGAV_PTR_2_FOURCC(a->atoms[i]+4);
    bgav_diprintf(indent, "User atom: ");
    bgav_dump_fourcc(fourcc);
    bgav_dprintf( " (size: %d)\n", size);
    gavl_hexdump(a->atoms[i], size, 16);
    }

  }

uint8_t * bgav_user_atoms_find(qt_user_atoms_t * a, uint32_t fourcc,
                               int * len)
  {
  int i;
  uint32_t fc;
  for(i = 0; i < a->num; i++)
    {
    fc = BGAV_PTR_2_FOURCC(a->atoms[i]+4);
    if(fc == fourcc)
      {
      *len = GAVL_PTR_2_32BE(a->atoms[i]);
      return a->atoms[i];
      }
    }
  return NULL;
  }
