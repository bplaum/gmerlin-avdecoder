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

struct bgav_packet_buffer_s
  {
  bgav_packet_t * packets;
  bgav_packet_t * packets_end;
  bgav_packet_pool_t * pp;
  };

bgav_packet_buffer_t * bgav_packet_buffer_create(bgav_packet_pool_t * pp)
  {
  bgav_packet_buffer_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->pp = pp;
  return ret;
  }

void bgav_packet_buffer_destroy(bgav_packet_buffer_t * b)
  {
  bgav_packet_t * tmp;
  while(b->packets)
    {
    tmp = b->packets->next;
    bgav_packet_destroy(b->packets);
    b->packets = tmp;
    }
  free(b);
  }

bgav_packet_t *
bgav_packet_buffer_get_packet_read(bgav_packet_buffer_t* b)
  {
  bgav_packet_t * ret;
  if(b->packets)
    {
    ret = b->packets;
    b->packets = b->packets->next;
    ret->next = NULL;
    return ret;
    }
  else
    return NULL;
  }

bgav_packet_t *
bgav_packet_buffer_peek_packet_read(bgav_packet_buffer_t * b)
  {
  return b->packets;
  }

void bgav_packet_buffer_clear(bgav_packet_buffer_t * b)
  {
  bgav_packet_t * tmp;
  //  fprintf(stderr, "bgav_packet_buffer_clear()...\n");
  while(b->packets)
    {
    tmp = b->packets->next;
    bgav_packet_pool_put(b->pp, b->packets);
    //    fprintf(stderr, "bgav_packet_buffer_clear %p %p\n", b->packets, tmp);

    b->packets = tmp;
    }
  b->packets_end = NULL;
  //  fprintf(stderr, "bgav_packet_buffer_clear()...done\n");
  }

int bgav_packet_buffer_is_empty(bgav_packet_buffer_t * b)
  {
  return !b->packets ? 1 : 0;
  }

void bgav_packet_buffer_append(bgav_packet_buffer_t * b,
                               bgav_packet_t * p)
  {
  p->next = NULL;
  
  if(!b->packets)
    {
    b->packets = p;
    b->packets_end = p;
    }
  else
    {
    b->packets_end->next = p;
    b->packets_end = b->packets_end->next;
    }
  }

