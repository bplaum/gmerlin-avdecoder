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

#define NUM_ALLOC 1024

#define LOG_DOMAIN "superindex"

bgav_superindex_t * bgav_superindex_create(int size)
  {
  bgav_superindex_t * ret;
  ret = calloc(1, sizeof(*ret));

  if(size)
    {
    ret->entries_alloc = size;
    ret->entries = calloc(ret->entries_alloc, sizeof(*(ret->entries)));
    }
  return ret;
  }

void bgav_superindex_set_size(bgav_superindex_t * ret, int size)
  {
  if(size > ret->entries_alloc)
    {
    ret->entries_alloc = size;
    ret->entries = realloc(ret->entries, ret->entries_alloc * sizeof(*(ret->entries)));
    memset(ret->entries + ret->num_entries, 0,
           sizeof(*ret->entries) * (ret->entries_alloc - ret->num_entries));
    }
  ret->num_entries = size;
  }

void bgav_superindex_set_sbr(bgav_superindex_t * si, bgav_stream_t * s)
  {
  int i;
  
  s->timescale *= 2;

  if(s->stats.pts_start != GAVL_TIME_UNDEFINED)
    s->stats.pts_start *= 2;

  if(s->stats.pts_end != GAVL_TIME_UNDEFINED)
    s->stats.pts_end *= 2;

  s->data.audio.format->samplerate *= 2;
  
  for(i = 0; i < si->num_entries; i++)
    {
    if(si->entries[i].stream_id != s->stream_id)
      continue;
    
    si->entries[i].pts      *= 2;
    si->entries[i].duration *= 2;
    }
  }

void bgav_superindex_destroy(bgav_superindex_t * idx)
  {
  if(idx->entries)
    free(idx->entries);
  free(idx);
  }

void bgav_superindex_add_packet(bgav_superindex_t * idx,
                                bgav_stream_t * s,
                                int64_t offset,
                                uint32_t size,
                                int stream_id,
                                int64_t timestamp,
                                int keyframe, int duration)
  {
  /* Realloc */
  
  if(idx->num_entries >= idx->entries_alloc)
    {
    idx->entries_alloc += NUM_ALLOC;
    idx->entries = realloc(idx->entries,
                           idx->entries_alloc * sizeof(*idx->entries));
    memset(idx->entries + idx->num_entries, 0,
           NUM_ALLOC * sizeof(*idx->entries));
    }
  /* Set fields */
  idx->entries[idx->num_entries].offset    = offset;
  idx->entries[idx->num_entries].size      = size;
  idx->entries[idx->num_entries].stream_id = stream_id;
  idx->entries[idx->num_entries].pts = timestamp;

  if(keyframe)
    idx->entries[idx->num_entries].flags = GAVL_PACKET_KEYFRAME;
  idx->entries[idx->num_entries].duration   = duration;

  /* Update indices */
  if(s)
    {
    if(s->first_index_position > idx->num_entries)
      s->first_index_position = idx->num_entries;
    if(s->last_index_position < idx->num_entries)
      s->last_index_position = idx->num_entries;
    }
  
  idx->num_entries++;
  }

void bgav_superindex_set_durations(bgav_superindex_t * idx,
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
  idx->entries[s->last_index_position].duration = s->stats.pts_end -
    idx->entries[s->last_index_position].pts;
  }

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

static void fix_b_pyramid(bgav_superindex_t * idx,
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
          (entries[next_ip_frame].type == BGAV_CODING_TYPE_B))
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

void bgav_superindex_set_coding_types(bgav_superindex_t * idx,
                                      bgav_stream_t * s)
  {
  int i;
  int64_t max_time = GAVL_TIME_UNDEFINED;
  int last_coding_type = 0;
  int64_t last_pts = 0;
  int b_pyramid = 0;
  int num_entries = 0;

  if(idx->entries[s->first_index_position].flags & GAVL_PACKET_TYPE_MASK)
    return;
  
  for(i = 0; i < idx->num_entries; i++)
    {
    if(idx->entries[i].stream_id != s->stream_id)
      continue;

    num_entries++;
    
    if(max_time == GAVL_TIME_UNDEFINED)
      {
      if(idx->entries[i].flags & GAVL_PACKET_KEYFRAME)
        idx->entries[i].flags |= BGAV_CODING_TYPE_I;
      else
        idx->entries[i].flags |= BGAV_CODING_TYPE_P;
      max_time = idx->entries[i].pts;
      }
    else if(idx->entries[i].pts > max_time)
      {
      if(idx->entries[i].flags & GAVL_PACKET_KEYFRAME)
        idx->entries[i].flags |= BGAV_CODING_TYPE_I;
      else
        idx->entries[i].flags |= BGAV_CODING_TYPE_P;
      max_time = idx->entries[i].pts;
      }
    else
      {
      idx->entries[i].flags |= BGAV_CODING_TYPE_B;
      if(!b_pyramid &&
         (last_coding_type == BGAV_CODING_TYPE_B) &&
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
    s->flags |= STREAM_B_PYRAMID;
    fix_b_pyramid(idx, s, num_entries);
    }
  
  }

void bgav_superindex_set_stream_stats(bgav_superindex_t * idx,
                                      bgav_stream_t * s)
  {
  int i;
  gavl_stream_stats_init(&s->stats);
  
  for(i = 0; i < idx->num_entries; i++)
    {
    if(idx->entries[i].stream_id != s->stream_id)
      continue;
    
    gavl_stream_stats_update_params(&s->stats,
                                    idx->entries[i].pts,
                                    idx->entries[i].duration,
                                    idx->entries[i].size,
                                    idx->entries[i].flags & 0xFFFF);
    }
  }


void bgav_superindex_seek(bgav_superindex_t * idx,
                          bgav_stream_t * s,
                          int64_t * time, int scale)
  {
  int i;
  int64_t time_scaled;

  if(s->first_index_position >= s->last_index_position)
    return;
  
  time_scaled = gavl_time_rescale(scale, s->timescale, *time);
  
  i = s->last_index_position;

  /* Go to frame before */
  while(i >= s->first_index_position)
    {
    if((idx->entries[i].stream_id == s->stream_id) &&
       (idx->entries[i].pts <= time_scaled))
      {
      break;
      }
    i--;
    }
  
  if(i < s->first_index_position)
    i = s->first_index_position;

  *time = gavl_time_rescale(s->timescale, scale, idx->entries[i].pts);
  
  /* Go to keyframe before */
  while(i >= s->first_index_position)
    {
    if((idx->entries[i].stream_id == s->stream_id) &&
       (idx->entries[i].flags & GAVL_PACKET_KEYFRAME))
      {
      break;
      }
    i--;
    }
  
  if(i < s->first_index_position)
    i = s->first_index_position;

  STREAM_SET_SYNC(s, idx->entries[i].pts);
  
  /* Handle audio preroll */
  if((s->type == GAVL_STREAM_AUDIO) && s->data.audio.preroll)
    {
    while(i >= s->first_index_position)
      {
      if((idx->entries[i].stream_id == s->stream_id) &&
         (idx->entries[i].flags & GAVL_PACKET_KEYFRAME) &&
         (STREAM_GET_SYNC(s) - idx->entries[i].pts >= s->data.audio.preroll))
        {
        break;
        }
      i--;
      }
    }

  if(i < s->first_index_position)
    i = s->first_index_position;

  s->index_position = i;
  STREAM_SET_SYNC(s, idx->entries[i].pts);
  }

void bgav_superindex_dump(bgav_superindex_t * idx)
  {
  int i;
  bgav_dprintf( "superindex %d entries:\n", idx->num_entries);
  for(i = 0; i < idx->num_entries; i++)
    {
    bgav_dprintf( "  No: %6d ID: %d K: %d O: %" PRId64 " T: %" PRId64 " D: %d S: %6d", 
                  i,
                  idx->entries[i].stream_id,
                  !!(idx->entries[i].flags & GAVL_PACKET_KEYFRAME),
                  idx->entries[i].offset,
                  idx->entries[i].pts,
                  idx->entries[i].duration,
                  idx->entries[i].size);
    bgav_dprintf(" PT: %s\n",
                 bgav_coding_type_to_string(idx->entries[i].flags));
    }
  }

