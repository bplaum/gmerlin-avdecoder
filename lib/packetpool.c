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



#include <stdlib.h>
#include <string.h>

#include <avdec_private.h>

// #define DEBUG_PP


struct bgav_packet_pool_s
  {
  int num_packets;
  int packets_alloc;
  
  bgav_packet_t ** packets;
  };

bgav_packet_pool_t * bgav_packet_pool_create()
  {
  bgav_packet_pool_t * ret;
  ret = calloc(1, sizeof(*ret));
  return ret;
  }

bgav_packet_t * bgav_packet_pool_get(bgav_packet_pool_t * pp)
  {
  if(!pp->num_packets)
    return gavl_packet_create();
  else
    {
    bgav_packet_t * ret = pp->packets[pp->num_packets-1];
    pp->packets[pp->num_packets-1] = NULL;
    gavl_packet_reset(ret);
    pp->num_packets--;
    return ret;
    }
  }

void bgav_packet_pool_put(bgav_packet_pool_t * pp,
                          bgav_packet_t * p)
  {
  if(pp->num_packets == pp->packets_alloc)
    {
    pp->packets_alloc += 16;

    pp->packets = realloc(pp->packets, pp->packets_alloc * sizeof(*pp->packets));
    memset(pp->packets + pp->num_packets, 0,
           (pp->packets_alloc - pp->num_packets) * sizeof(*pp->packets));
    }

  pp->packets[pp->num_packets] = p;
  pp->num_packets++;
  }

void bgav_packet_pool_destroy(bgav_packet_pool_t * pp)
  {
  int i;
  for(i = 0; i < pp->num_packets; i++)
    gavl_packet_destroy(pp->packets[i]);
  
  if(pp->packets)
    free(pp->packets);
  
  free(pp);
  }
