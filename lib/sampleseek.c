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



#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <avdec_private.h>
#include <parser.h>

#define LOG_DOMAIN "sampleseek"

int bgav_can_seek_sample(bgav_t * bgav)
  {
  if(bgav->demuxer->flags & BGAV_DEMUXER_SAMPLE_ACCURATE)
    return 1;

  if((bgav->input->flags & BGAV_INPUT_CAN_SEEK_BYTE) &&
     (bgav->demuxer->index_mode = INDEX_MODE_SIMPLE))
    return 1;

  return 0;
  
    
  }

int64_t bgav_audio_duration(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s = bgav_track_get_audio_stream(bgav->tt->cur, stream);
  return bgav_stream_get_duration(s);
  }

int64_t bgav_video_duration(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  return bgav_stream_get_duration(s);
  }

int64_t bgav_subtitle_duration(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s = bgav_track_get_subtitle_stream(bgav->tt->cur, stream);
  return bgav_stream_get_duration(s);
  }

int64_t bgav_text_duration(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s = bgav_track_get_text_stream(bgav->tt->cur, stream);
  return bgav_stream_get_duration(s);
  }

int64_t bgav_overlay_duration(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s = bgav_track_get_overlay_stream(bgav->tt->cur, stream);
  return bgav_stream_get_duration(s);
  }

int64_t bgav_audio_start_time(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s = bgav_track_get_audio_stream(bgav->tt->cur, stream);
  return s->stats.pts_start;
  }

int64_t bgav_video_start_time(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  return s->stats.pts_start;
  }

static int64_t bgav_video_stream_keyframe_before(bgav_stream_t * s, int64_t time)
  {
  int pos;
  if(!s->demuxer->si)
    return GAVL_TIME_UNDEFINED;
  
  pos = s->demuxer->si->num_entries -1;
  while(pos >= 0)
    {
    if((s->demuxer->si->entries[pos].stream_id == s->stream_id) &&
       (s->demuxer->si->entries[pos].flags & GAVL_PACKET_KEYFRAME) &&
       (s->demuxer->si->entries[pos].pts < time))
      {
      break;
      }
    pos--;
    }
  if(pos < 0)
    return GAVL_TIME_UNDEFINED;
  else
    return s->demuxer->si->entries[pos].pts;
  }

int64_t bgav_video_keyframe_before(bgav_t * bgav, int stream, int64_t time)
  {
  bgav_stream_t * s;
  s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  return bgav_video_stream_keyframe_before(s, time);
  }

static int64_t bgav_video_stream_keyframe_after(bgav_stream_t * s, int64_t time)
  {
  int pos;

  if(!s->demuxer->si)
    return GAVL_TIME_UNDEFINED;

  pos = 0;
  while(pos < s->demuxer->si->num_entries)
    {
    if((s->demuxer->si->entries[pos].stream_id == s->stream_id) &&
       (s->demuxer->si->entries[pos].flags & GAVL_PACKET_KEYFRAME) &&
       (s->demuxer->si->entries[pos].pts > time))
      {
      break;
      }
    pos++;
    }
  if(pos >= s->demuxer->si->num_entries)
    return GAVL_TIME_UNDEFINED;
  else
    return s->demuxer->si->entries[pos].pts;
    
  }

int64_t bgav_video_keyframe_after(bgav_t * bgav, int stream, int64_t time)
  {
  bgav_stream_t * s;
  
  s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  return bgav_video_stream_keyframe_after(s, time);
  }

