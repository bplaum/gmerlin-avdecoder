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
#include <ptscache.h>

static int get_min_index(bgav_pts_cache_t * c)
  {
  int i;
  int ret = -1;
  int64_t min;
  for(i = 0; i < PTS_CACHE_SIZE; i++)
    {
    if(c->entries[i].used)
      {
      if((ret < 0) || (c->entries[i].pts < min))
        {
        ret = i;
        min = c->entries[i].pts;
        }
      }
    }
  return ret;
  }

static int get_max_index(bgav_pts_cache_t * c)
  {
  int i;
  int ret = -1;
  int64_t max;
  for(i = 0; i < PTS_CACHE_SIZE; i++)
    {
    if(c->entries[i].used)
      {
      if((ret < 0) || (c->entries[i].pts > max))
        {
        ret = i;
        max = c->entries[i].pts;
        }
      }
    }
  return ret;
  }

void bgav_pts_cache_push(bgav_pts_cache_t * c,
                         bgav_packet_t * p,
                         int * index,
                         bgav_pts_cache_entry_t ** e)
  {
  int i;
  for(i = 0; i < PTS_CACHE_SIZE; i++)
    {
    if(!c->entries[i].used)
      break;
    }
  if(i == PTS_CACHE_SIZE)
    i = get_min_index(c);
  
  c->entries[i].used      = 1;
  c->entries[i].pts       = p->pts;
  c->entries[i].duration  = p->duration;
  c->entries[i].tc        = p->tc;
  
  if(index)
    *index = i;
  if(e)
    *e = &c->entries[i];
  }

void bgav_pts_cache_clear(bgav_pts_cache_t * c)
  {
  int i;
  for(i = 0; i < PTS_CACHE_SIZE; i++)
    c->entries[i].used = 0;
  }

/* Get the smallest timestamp */
int bgav_pts_cache_get_first(bgav_pts_cache_t * c, gavl_video_frame_t * f)
  {
  int i = get_min_index(c);
  
  if(i < 0)
    return 0;
  else
    {
    c->entries[i].used = 0;
    if(f)
      {
      f->duration = c->entries[i].duration;
      f->timecode = c->entries[i].tc;
      f->timestamp = c->entries[i].pts;
      }
    }
  return 1;
  }

int bgav_pts_cache_peek_first(bgav_pts_cache_t * c, gavl_video_frame_t * f)
  {
  int i = get_min_index(c);
  
  if(i < 0)
    return 0;
  else
    {
    if(f)
      {
      f->duration = c->entries[i].duration;
      f->timecode = c->entries[i].tc;
      f->timestamp = c->entries[i].pts;
      }
    }
  return 1;
  }


int64_t bgav_pts_cache_peek_last(bgav_pts_cache_t * c, int * duration)
  {
  int i = get_max_index(c);

  if(i < 0)
    return GAVL_TIME_UNDEFINED;
  else
    {
    *duration = c->entries[i].duration;
    return c->entries[i].pts;
    }
  }
