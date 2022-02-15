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

void bgav_bytebuffer_append(bgav_bytebuffer_t * b, bgav_packet_t * p, int padding)
  {
  if(b->size + p->data_size + padding > b->alloc)
    {
    b->alloc = b->size + p->data_size + padding + 1024; 
    b->buffer = realloc(b->buffer, b->alloc);
    }
  memcpy(b->buffer + b->size, p->data, p->data_size);
  b->size += p->data_size;
  if(padding)
    memset(b->buffer + b->size, 0, padding);
  }

void bgav_bytebuffer_append_data(bgav_bytebuffer_t * b, uint8_t * data, int len, int padding)
  {
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

int bgav_bytebuffer_append_read(bgav_bytebuffer_t * b, bgav_input_context_t * input,
                                int len, int padding)
  {
  int ret;
  if(b->size + len + padding > b->alloc)
    {
    b->alloc = b->size + len + padding + 1024; 
    b->buffer = realloc(b->buffer, b->alloc);
    }

  ret = bgav_input_read_data(input, b->buffer + b->size, len);
  b->size += ret;
  if(padding)
    memset(b->buffer + b->size, 0, padding);
  
  return ret;
  }



void bgav_bytebuffer_remove(bgav_bytebuffer_t * b, int bytes)
  {
  if(bytes > b->size)
    bytes = b->size;
  
  if(bytes < b->size)
    memmove(b->buffer, b->buffer + bytes, b->size - bytes);
  b->size -= bytes;
  }

void bgav_bytebuffer_free(bgav_bytebuffer_t * b)
  {
  if(b->buffer)
    free(b->buffer);
  }

void bgav_bytebuffer_flush(bgav_bytebuffer_t * b)
  {
  b->size = 0;
  }
