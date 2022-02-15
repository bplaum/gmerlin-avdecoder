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

#if 0
  uint32_t flags;
  uint32_t fourcc;
  uint32_t data_ref_size;
  uint8_t * data_ref;
#endif

void bgav_qt_rdrf_dump(int indent, qt_rdrf_t * r)
  {
  bgav_diprintf(indent, "rdrf:\n");
  bgav_diprintf(indent+2, "fourcc:        ");
  bgav_dump_fourcc(r->fourcc);
  bgav_dprintf( "\n");
  bgav_diprintf(indent+2, "data_ref_size: %d\n", r->data_ref_size);

  if(r->fourcc == BGAV_MK_FOURCC('u', 'r', 'l', ' '))
    {
    bgav_diprintf(indent+2, "data_ref:      ");
    fwrite(r->data_ref, 1, r->data_ref_size, stderr);
    bgav_dprintf( "\n");
    }
  else
    {
    bgav_diprintf(indent+2, "Unknown data, hexdump follows: ");
    gavl_hexdump(r->data_ref, r->data_ref_size, 16);
    }
  }

int bgav_qt_rdrf_read(qt_atom_header_t * h,
                      bgav_input_context_t * input, qt_rdrf_t * ret)
  {
  if(!bgav_input_read_32_le(input, &ret->flags) ||
     !bgav_input_read_fourcc(input, &ret->fourcc) ||
     !bgav_input_read_fourcc(input, &ret->data_ref_size))
    return 0;

  ret->data_ref = malloc(ret->data_ref_size);
  if(bgav_input_read_data(input, ret->data_ref, ret->data_ref_size) <
     ret->data_ref_size)
    return 0;
  return 1;
  }

void bgav_qt_rdrf_free(qt_rdrf_t * r)
  {
  if(r->data_ref)
    free(r->data_ref);
  }

