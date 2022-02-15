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

// #define DEBUG_PP


struct bgav_packet_pool_s
  {
  bgav_packet_t * packets;
  };

bgav_packet_pool_t * bgav_packet_pool_create()
  {
  bgav_packet_pool_t * ret;
  ret = calloc(1, sizeof(*ret));
  return ret;
  }

bgav_packet_t * bgav_packet_pool_get(bgav_packet_pool_t * pp)
  {
  bgav_packet_t * ret;
  if(pp->packets)
    {
    ret = pp->packets;
    pp->packets = pp->packets->next;
    }
  else
    ret = bgav_packet_create();

  ret->next = NULL;
  bgav_packet_reset(ret);
  return ret;
  }

void bgav_packet_pool_put(bgav_packet_pool_t * pp,
                          bgav_packet_t * p)
  {
#ifdef DEBUG_PP
  bgav_packet_t * tmp = pp->packets;
  while(tmp)
    {
    if(tmp == p)
      {
      fprintf(stderr, "Error: Duplicate packet %p\n", p);
      }
    tmp = tmp->next;
    }
#endif

  p->next = pp->packets;
  pp->packets = p;
  }

void bgav_packet_pool_destroy(bgav_packet_pool_t * pp)
  {
  bgav_packet_t * tmp;
  while(pp->packets)
    {
    tmp = pp->packets->next;
    bgav_packet_destroy(pp->packets);
    pp->packets = tmp;
    }
  free(pp);
  }
