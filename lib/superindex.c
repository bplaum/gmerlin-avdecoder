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
#include <string.h>

#include <avdec_private.h>
#include <stdio.h>

#define LOG_DOMAIN "superindex"

void gavl_packet_index_set_durations(gavl_packet_index_t * idx,
                                   bgav_stream_t * s)
  {
  int i;
  int last_pos;
  if(idx->entries[s->first_index_position].duration)
    return;
  
  /* Special case if there is only one chunk */
  if(s->first_index_position == s->last_index_position)
    {
    idx->entries[s->first_index_position].duration = bgav_stream_get_duration(s);
    return;
    }
  
  i = s->first_index_position+1;
  while(idx->entries[i].stream_id != s->stream_id)
    i++;
  
  last_pos = s->first_index_position;
  
  while(i <= s->last_index_position)
    {
    if(idx->entries[i].stream_id == s->stream_id)
      {
      idx->entries[last_pos].duration = idx->entries[i].pts - idx->entries[last_pos].pts;
      last_pos = i;
      }
    i++;
    }
  if((idx->entries[s->last_index_position].duration <= 0) &&
     (s->stats.pts_end > idx->entries[s->last_index_position].pts))
    idx->entries[s->last_index_position].duration = s->stats.pts_end -
      idx->entries[s->last_index_position].pts;
  }

#if 0
typedef struct
  {
  int index;
  int64_t pts;
  int duration;
  int type;
  int done;
  } fix_b_entries;

static int find_min(fix_b_entries * e, int start, int end)
  {
  int i, ret = -1;
  int64_t min_pts = 0;

  for(i = start; i < end; i++)
    {
    if(!e[i].done)
      {
      if((ret == -1) || (e[i].pts < min_pts))
        {
        ret = i;
        min_pts = e[i].pts;
        }
      }
    }
  return ret;
  }

static void fix_b_pyramid(gavl_packet_index_t * idx,
                          bgav_stream_t * s, int num_entries)
  {
  int i, index, min_index;
  int next_ip_frame;
  fix_b_entries * entries;
  int64_t pts;
  
  /* Set up array */
  entries = malloc(num_entries * sizeof(*entries));
  index = 0;
  for(i = 0; i  < idx->num_entries; i++)
    {
    if(idx->entries[i].stream_id == s->stream_id)
      {
      entries[index].index = i;
      entries[index].pts = idx->entries[i].pts;
      entries[index].duration = idx->entries[i].duration;
      entries[index].type = idx->entries[i].flags & 0xff;
      entries[index].done = 0;
      index++;
      }
    }
  
  /* Get timestamps from durations */

  pts = entries[0].pts;
  pts += entries[0].duration;
  entries[0].done = 1;
  
  index = 1;
  
  while(1)
    {
    next_ip_frame = index+1;

    while((next_ip_frame < num_entries) &&
          (entries[next_ip_frame].type == GAVL_PACKET_TYPE_B))
      {
      next_ip_frame++;
      }
    
    /* index         -> ipframe before b-frames */
    /* next_ip_frame -> ipframe after b-frames  */

    if(next_ip_frame == index + 1)
      {
      entries[index].pts = pts;
      pts += entries[index].duration;
      }
    else
      {
      for(i = index; i < next_ip_frame; i++)
        {
        min_index = find_min(entries, index, next_ip_frame);
        if(min_index < 0)
          break;
        entries[min_index].pts = pts;
        entries[min_index].done = 1;
        pts += entries[min_index].duration;
        }
      }
    
    if(next_ip_frame >= num_entries)
      break;
    index = next_ip_frame;
    }

  /* Copy fixed timestamps back */
  for(i = 0; i < num_entries; i++)
    idx->entries[entries[i].index].pts = entries[i].pts;
  
  free(entries);
  }

void gavl_packet_index_set_coding_types(gavl_packet_index_t * idx,
                                        bgav_stream_t * s)
  {
  int i;
  int64_t max_time = GAVL_TIME_UNDEFINED;
  int last_coding_type = 0;
  int64_t last_pts = 0;
  int b_pyramid = 0;
  int num_entries = 0;
  
  for(i = 0; i < idx->num_entries; i++)
    {
    if(idx->entries[i].stream_id != s->stream_id)
      continue;
    
    if(idx->entries[i].flags & GAVL_PACKET_TYPE_MASK)
      return;
    
    num_entries++;
    
    if(max_time == GAVL_TIME_UNDEFINED)
      {
      if(idx->entries[i].flags & GAVL_PACKET_KEYFRAME)
        idx->entries[i].flags |= GAVL_PACKET_TYPE_I;
      else
        idx->entries[i].flags |= GAVL_PACKET_TYPE_P;
      max_time = idx->entries[i].pts;
      }
    else if(idx->entries[i].pts > max_time)
      {
      if(idx->entries[i].flags & GAVL_PACKET_KEYFRAME)
        idx->entries[i].flags |= GAVL_PACKET_TYPE_I;
      else
        idx->entries[i].flags |= GAVL_PACKET_TYPE_P;
      max_time = idx->entries[i].pts;
      }
    else
      {
      idx->entries[i].flags |= GAVL_PACKET_TYPE_B;
      if(!b_pyramid &&
         (last_coding_type == GAVL_PACKET_TYPE_B) &&
         (idx->entries[i].pts < last_pts))
        {
        b_pyramid = 1;
        }
      }
    
    last_pts = idx->entries[i].pts;
    last_coding_type = idx->entries[i].flags & 0xff;
    }
  
  if(b_pyramid)
    {
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
             "Detected B-pyramid, fixing possibly broken timestamps");
    fix_b_pyramid(idx, s, num_entries);
    }
  
  }
#endif
