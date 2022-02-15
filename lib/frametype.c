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

#include <string.h>
#include <stdlib.h>


#define LOG_DOMAIN "frametypedetector"
#define MAX_PACKETS 16

struct bgav_frametype_detector_s
  {
  int initialized;
  int64_t max_pts;
  bgav_packet_source_t src;
  bgav_packet_t * packets[MAX_PACKETS];
  int num_packets;
  bgav_stream_t * s;
  };

static void set_packet_type(bgav_frametype_detector_t * fd,
                           bgav_packet_t * p)
  {
  int coding_type;
  
  if(PACKET_GET_KEYFRAME(p))
    {
    coding_type = BGAV_CODING_TYPE_I;
    fd->max_pts = p->pts;
    }
  else if(p->pts < fd->max_pts)
    {
    coding_type = BGAV_CODING_TYPE_B;
    }
  else
    {
    coding_type = BGAV_CODING_TYPE_P;
    fd->max_pts = p->pts;
    }
  PACKET_SET_CODING_TYPE(p, coding_type);
  }

static int put_packet(bgav_frametype_detector_t * fd,
                      bgav_packet_t * p)
  {
  if(fd->num_packets == MAX_PACKETS)
    return 0;
  fd->packets[fd->num_packets] = p;
  fd->num_packets++;
  return 1;
  }

static bgav_packet_t * get_packet(bgav_frametype_detector_t * fd)
  {
  bgav_packet_t * ret;
  if(!fd->num_packets)
    return NULL;

  ret = fd->packets[0];
  fd->num_packets--;
  if(fd->num_packets)
    memmove(&fd->packets[0],
            &fd->packets[1],
            fd->num_packets * sizeof(fd->packets[0]));
  return ret;
  }

static int resync(bgav_frametype_detector_t * fd)
  {
  bgav_packet_t * p;
  if(fd->max_pts != GAVL_TIME_UNDEFINED)
    return 1;
  
  /* Skip all frames until the first keyframe */
  while(1)
    {
    p = NULL;

    if(fd->src.get_func(fd->src.data, &p) != GAVL_SOURCE_OK)
      return 0;
    if(PACKET_GET_KEYFRAME(p))
      break;
    bgav_packet_pool_put(fd->s->pp, p);
    }
  
  set_packet_type(fd, p);
  put_packet(fd, p);
  return 1;
  }

static int initialize(bgav_frametype_detector_t * fd)
  {
  bgav_packet_t * p = NULL;
  
  resync(fd);

  while(fd->num_packets < 3)
    {
    p = NULL;
    
    if(fd->src.get_func(fd->src.data, &p) != GAVL_SOURCE_OK)
      return 0;
    set_packet_type(fd, p);
    put_packet(fd, p);
    
    if(PACKET_GET_CODING_TYPE(p) == BGAV_CODING_TYPE_B)
      {
      fd->s->ci->flags |= GAVL_COMPRESSION_HAS_B_FRAMES;
      break;
      }
    }
  
  //  fd->s->flags &= ~STREAM_NEED_FRAMETYPES;
  fd->initialized = 1;
  return 1;
  }

static gavl_source_status_t peek_func(void * pt1, bgav_packet_t ** ret,
                                      int force)
  {
  gavl_source_status_t st;
  bgav_packet_t * p;
  bgav_frametype_detector_t * fd = pt1;
  
  if(!fd->initialized)
    {
    if(!force)
      return GAVL_SOURCE_AGAIN;
    
    if(!initialize(fd))
      return GAVL_SOURCE_EOF;
    }
  
  if(fd->num_packets)
    {
    if(ret)
      *ret = fd->packets[0];
    return GAVL_SOURCE_OK;
    }
  
  if(!force)
    {
    if((st = fd->src.peek_func(fd->src.data, &p, 0)) != GAVL_SOURCE_OK)
      return st;
    }
  p = NULL;
  
  if((st = fd->src.get_func(fd->src.data, &p)) != GAVL_SOURCE_OK)
    return st;
  
  set_packet_type(fd, p);
  put_packet(fd, p);

  if(ret)
    *ret = fd->packets[0];
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t
get_func(void * pt1, bgav_packet_t ** ret)
  {
  bgav_packet_t * p;
  gavl_source_status_t st;
  bgav_frametype_detector_t * fd = pt1;

  if(!fd->initialized)
    {
    if(!initialize(fd))
      return GAVL_SOURCE_EOF;
    }
  
  while(!(*ret = get_packet(fd)))
    {
    p = NULL;

    if((st = fd->src.get_func(fd->src.data, &p)) != GAVL_SOURCE_OK)
      return st;
    set_packet_type(fd, p);
    put_packet(fd, p);
    }
  return GAVL_SOURCE_OK;
  }

bgav_frametype_detector_t *
bgav_frametype_detector_create(bgav_stream_t * s)
  {
  bgav_frametype_detector_t * ret = calloc(1, sizeof(*ret));
  ret->s = s;

  bgav_packet_source_copy(&ret->src, &s->src);

  s->src.get_func = get_func;
  s->src.peek_func = peek_func;
  s->src.data = ret;
  return ret;
  }

void bgav_frametype_detector_destroy(bgav_frametype_detector_t * fd)
  {
  int i;
  for(i = 0; i < fd->num_packets; i++)
    bgav_packet_pool_put(fd->s->pp, fd->packets[i]);
  
  free(fd);
  }

void bgav_frametype_detector_reset(bgav_frametype_detector_t * fd)
  {
  int i;
  fd->max_pts = GAVL_TIME_UNDEFINED;
  for(i = 0; i < fd->num_packets; i++)
    bgav_packet_pool_put(fd->s->pp, fd->packets[i]);
  fd->num_packets = 0;
  }
