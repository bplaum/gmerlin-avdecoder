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
#include <stdlib.h>
#include <string.h>

void bgav_bytebuffer_append_packet(gavl_buffer_t * b, bgav_packet_t * p, int padding)
  {
  gavl_buffer_append_data_pad(b, p->data, p->data_size, padding);
  }

#if 0
void bgav_bytebuffer_append_data(gavl_buffer_t * b, uint8_t * data, int len, int padding)
  {
  gavl_buffer_append_data_pad(b, data, len, padding);

  if(b->size + len + padding > b->alloc)
    {
    b->alloc = b->size + len + padding + 1024; 
    b->buffer = realloc(b->buffer, b->alloc);
    }
  memcpy(b->buffer + b->size, data, len);
  b->size += len;

  if(padding)
    memset(b->buffer + b->size, 0, padding);
  }
#endif

int bgav_bytebuffer_append_read(gavl_buffer_t * b, bgav_input_context_t * input,
                                int len, int padding)
  {
  int ret;

  gavl_buffer_alloc(b, b->len + len + padding);

  ret = bgav_input_read_data(input, b->buf + b->len, len);
  b->len += ret;

  if(padding)
    memset(b->buf + b->len, 0, padding);
  
  return ret;
  }


#if 0
void bgav_bytebuffer_remove(gavl_buffer_t * b, int bytes)
  {
  if(bytes > b->size)
    bytes = b->size;
  
  if(bytes < b->size)
    memmove(b->buffer, b->buffer + bytes, b->size - bytes);
  b->size -= bytes;
  }

void bgav_bytebuffer_free(gavl_buffer_t * b)
  {
  if(b->buffer)
    free(b->buffer);
  }

void bgav_bytebuffer_flush(gavl_buffer_t * b)
  {
  b->size = 0;
  }
#endif
