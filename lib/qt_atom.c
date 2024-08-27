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




#include <avdec_private.h>

#include <qt.h>

#include <stdio.h>

#define LOG_DOMAIN "qt_atom"

/*
  
  uint64_t size;
  uint64_t start_position; 
  uint32_t fourcc;
  uint8_t version;
  uint32_t flags;
*/



int bgav_qt_atom_read_header(bgav_input_context_t * input,
                             qt_atom_header_t * h)
  {
  uint32_t tmp_32;
  
  h->start_position = input->position;
  
  if(!bgav_input_read_32_be(input, &tmp_32))
    return 0;

  h->size = tmp_32;

  if(!bgav_input_read_fourcc(input, &h->fourcc))
    return 0;
  
  if(tmp_32 == 1) /* 64 bit atom */
    {
    if(!bgav_input_read_64_be(input, &h->size))
      return 0;
    }
  return 1;
  }

void bgav_qt_atom_skip(bgav_input_context_t * input,
                       qt_atom_header_t * h)
  {
  int64_t bytes_to_skip = h->size - (input->position - h->start_position);
  if(bytes_to_skip > 0)
    bgav_input_skip(input, bytes_to_skip);
  }

void bgav_qt_atom_skip_dump(bgav_input_context_t * input,
                             qt_atom_header_t * h)
  {
  int64_t bytes_to_skip = h->size - (input->position - h->start_position);
  if(bytes_to_skip > 0)
    bgav_input_skip_dump(input, bytes_to_skip);
  }

void bgav_qt_atom_skip_unknown(bgav_input_context_t * input,
                               qt_atom_header_t * h, uint32_t parent)
  {
  char tmp_string[128];
  if(!parent)
    gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN,
             "Unknown atom [%c%c%c%c] at toplevel",
             (h->fourcc & 0xFF000000) >> 24,
             (h->fourcc & 0x00FF0000) >> 16,
             (h->fourcc & 0x0000FF00) >> 8,
             (h->fourcc & 0x000000FF));
  else
    {
    sprintf(tmp_string, "%" PRId64, h->size);
    gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN,
             "Unknown atom inside [%c%c%c%c] (fourcc: [%c%c%c%c], size: %s)",
             (parent & 0xFF000000) >> 24,
             (parent & 0x00FF0000) >> 16,
             (parent & 0x0000FF00) >> 8,
             (parent & 0x000000FF),
             (h->fourcc & 0xFF000000) >> 24,
             (h->fourcc & 0x00FF0000) >> 16,
             (h->fourcc & 0x0000FF00) >> 8,
             (h->fourcc & 0x000000FF), tmp_string);
    }
  bgav_qt_atom_skip(input, h);
  }



void bgav_qt_atom_dump_header(int indent, qt_atom_header_t * h)
  {
  gavl_diprintf(indent, "Size:           %" PRId64 "\n", h->size);
  gavl_diprintf(indent, "Start Position: %" PRId64 "\n", h->start_position);
  gavl_diprintf(indent, "Fourcc:         ");
  bgav_dump_fourcc(h->fourcc);
  gavl_dprintf("\n");
  }
