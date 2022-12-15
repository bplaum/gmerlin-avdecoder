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

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <avdec_private.h>
#include <parser.h>

#define LOG_DOMAIN "sampleseek"

#if 0
static int file_index_seek(bgav_file_index_t * idx, int64_t time)
  {
  int pos1, pos2, tmp;

  pos1 = 0;
  
  pos2 = idx->num_entries - 1;
  
  /* Binary search */
  while(1)
    {
    tmp = (pos1 + pos2)/2;
    
    if(idx->entries[tmp].pts < time)
      pos1 = tmp;
    else
      pos2 = tmp;
    
    if(pos2 - pos1 <= 4)
      break;
    }
  
  while((idx->entries[pos2].pts > time) && pos2)
    pos2--;

  while(pos2 && (idx->entries[pos2-1].pts == idx->entries[pos2].pts))
    pos2--;
  
  return pos2;
  }
#endif

int bgav_can_seek_sample(bgav_t * bgav)
  {
  return !!(bgav->tt->cur->flags & TRACK_SAMPLE_ACCURATE);
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

#if 0
static void sync_stream(bgav_t * bgav, bgav_stream_t * s)
  {
  if(s->flags & STREAM_EXTERN)
    return;
  
  if(bgav->demuxer->demuxer->resync)
    bgav->demuxer->demuxer->resync(bgav->demuxer, s);
  }
#endif

void bgav_seek_audio(bgav_t * bgav, int stream, int64_t sample)
  {
  int64_t frame_time;
  bgav_stream_t * s;

  if(bgav->flags & BGAV_FLAG_PAUSED)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "bgav_seek_audio failed: Decoder paused");
    return;
    }


  s = bgav_track_get_audio_stream(bgav->tt->cur, stream);

  // fprintf(stderr, "Seek audio: %ld\n", sample);
  
  if(sample >= s->stats.pts_end) /* EOF */
    {
    s->flags |= STREAM_EOF_C;
    return;
    }

  s->flags &= ~(STREAM_EOF_C|STREAM_EOF_D);
  
  bgav_stream_clear(s);

  if(bgav->demuxer->index_mode == INDEX_MODE_PCM)
    {
    bgav->demuxer->demuxer->seek(bgav->demuxer, sample,
                                 s->data.audio.format->samplerate);
    //    return;
    }
  else if(bgav->demuxer->index_mode == INDEX_MODE_SI_SA)
    {
    frame_time = gavl_time_rescale(s->data.audio.format->samplerate,
                                   s->timescale, sample);
    bgav_superindex_seek(bgav->demuxer->si, s,
                         &frame_time,
                         s->timescale);
    
    s->out_time = gavl_time_rescale(s->timescale, s->data.audio.format->samplerate,
                                    STREAM_GET_SYNC(s));
    }
  
  bgav_audio_resync(s);

  bgav_audio_skipto(s, &sample, s->data.audio.format->samplerate);
  
  }

void bgav_seek_video(bgav_t * bgav, int stream, int64_t time)
  {
  bgav_stream_t * s;
  int64_t frame_time;

  if(bgav->flags & BGAV_FLAG_PAUSED)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "bgav_seek_video failed: Decoder paused");
    return;
    }
  
  s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  
  //  fprintf(stderr, "Seek video: %ld\n", time);
  
  if(time >= s->stats.pts_end) /* EOF */
    {
    s->flags |= STREAM_EOF_C;
    return;
    }
  
  s->flags &= ~(STREAM_EOF_C|STREAM_EOF_D);
  
  if(time == s->out_time)
    {
    return;
    }
  if((time > s->out_time) &&
     (bgav_video_keyframe_after(bgav, stream, s->out_time) > time))
    {
    //    fprintf(stderr, "Skip to: %ld\n", time);
    bgav_video_skipto(s, &time, s->data.video.format->timescale);
    //    fprintf(stderr, "Skipped to: %ld %ld\n", time, s->out_time);
    return;
    }

  bgav_stream_clear(s);

  if(bgav->demuxer->index_mode == INDEX_MODE_SI_SA)
    {
    frame_time = time;
    bgav_superindex_seek(bgav->demuxer->si, s, &frame_time, s->timescale);
    s->out_time = bgav->demuxer->si->entries[s->index_position].pts;
    }
    
  bgav_video_resync(s);


  time += s->stats.pts_start;
  
  //  fprintf(stderr, "Skip to: %ld\n", time);
  bgav_video_skipto(s, &time, s->data.video.format->timescale);
  //  fprintf(stderr, "Skipped to: %ld %ld\n", time, s->out_time);
  }

int64_t bgav_video_stream_keyframe_before(bgav_stream_t * s, int64_t time)
  {
  int pos;
  if(s->demuxer->index_mode == INDEX_MODE_SI_SA)
    {
    pos = s->last_index_position;
    while(pos >= s->first_index_position)
      {
      if((s->demuxer->si->entries[pos].stream_id == s->stream_id) &&
         (s->demuxer->si->entries[pos].flags & GAVL_PACKET_KEYFRAME) &&
         (s->demuxer->si->entries[pos].pts < time))
        {
        break;
        }
      pos--;
      }
    if(pos < s->first_index_position)
      return GAVL_TIME_UNDEFINED;
    else
      return s->demuxer->si->entries[pos].pts;
    }
  /* Stupid gcc :( */
  return GAVL_TIME_UNDEFINED;
  }

int64_t bgav_video_keyframe_before(bgav_t * bgav, int stream, int64_t time)
  {
  bgav_stream_t * s;
  s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  return bgav_video_stream_keyframe_before(s, time);
  }

int64_t bgav_video_stream_keyframe_after(bgav_stream_t * s, int64_t time)
  {
  int pos;
  if(s->demuxer->index_mode == INDEX_MODE_SI_SA)
    {
    pos = s->first_index_position;
    while(pos <= s->last_index_position)
      {
      if((s->demuxer->si->entries[pos].stream_id == s->stream_id) &&
         (s->demuxer->si->entries[pos].flags & GAVL_PACKET_KEYFRAME) &&
         (s->demuxer->si->entries[pos].pts > time))
        {
        break;
        }
      pos++;
      }
    if(pos > s->last_index_position)
      return GAVL_TIME_UNDEFINED;
    else
      return s->demuxer->si->entries[pos].pts;
    }
  return GAVL_TIME_UNDEFINED;
  
  }

int64_t bgav_video_keyframe_after(bgav_t * bgav, int stream, int64_t time)
  {
  bgav_stream_t * s;
  
  s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  return bgav_video_stream_keyframe_after(s, time);
  }

static void seek_subtitle(bgav_t * bgav, bgav_stream_t * s, int64_t time)
  {
  bgav_stream_clear(s);

  s->flags &= ~(STREAM_EOF_C|STREAM_EOF_D);
  
  bgav_stream_clear(s);

  if(bgav->demuxer->index_mode == INDEX_MODE_SI_SA)
    {
    bgav_superindex_seek(bgav->demuxer->si, s, &time, s->timescale);
    s->out_time = bgav->demuxer->si->entries[s->index_position].pts;
    }
  }

void bgav_seek_subtitle(bgav_t * bgav, int stream, int64_t time)
  {
  bgav_stream_t * s;
  if(bgav->flags & BGAV_FLAG_PAUSED)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "bgav_seek_subtitle failed: Decoder paused");
    return;
    }

  s = bgav_track_get_subtitle_stream(bgav->tt->cur, stream);
  seek_subtitle(bgav, s, time);
  }

void bgav_seek_text(bgav_t * bgav, int stream, int64_t time)
  {
  bgav_stream_t * s;
  if(bgav->flags & BGAV_FLAG_PAUSED)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "bgav_seek_text failed: Decoder paused");
    return;
    }

  s = bgav_track_get_text_stream(bgav->tt->cur, stream);
  seek_subtitle(bgav, s, time);
  }

void bgav_seek_overlay(bgav_t * bgav, int stream, int64_t time)
  {
  bgav_stream_t * s;
  if(bgav->flags & BGAV_FLAG_PAUSED)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "bgav_seek_overlay failed: Decoder paused");
    return;
    }
  
  s = bgav_track_get_overlay_stream(bgav->tt->cur, stream);
  seek_subtitle(bgav, s, time);
  }

/* Check if the decoder is already sample accurate */

void bgav_check_sample_accurate(bgav_t * b)
  {
  int i;
  int val = 0;
  
  if(b->tt->tracks &&
     gavl_dictionary_get_int(b->tt->tracks[0]->metadata, GAVL_META_SAMPLE_ACCURATE, &val) &&
     val)
    return;

  if(!b->demuxer)
    return;
     
  
  switch(b->demuxer->index_mode)
    {
    case INDEX_MODE_PCM:
    case INDEX_MODE_SI_SA:
      if(!(b->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
        return;
      /* Format is already sample accurate */
      for(i = 0; i < b->tt->num_tracks; i++)
        {
        b->tt->tracks[i]->flags |= TRACK_SAMPLE_ACCURATE;
        gavl_dictionary_set_int(b->tt->tracks[i]->metadata, GAVL_META_SAMPLE_ACCURATE, 1);
        }
      break;
    }
  
  }

int bgav_set_sample_accurate(bgav_t * b)
  {
  int i;
  //  gavl_time_t t;
  //  bgav_stream_t * s;
  int val;
  
  if(!b->demuxer)
    return 0;
  
  if(b->tt->tracks &&
     gavl_dictionary_get_int(b->tt->tracks[0]->metadata, GAVL_META_SAMPLE_ACCURATE, &val) &&
     val)
    return 1;
  
  switch(b->demuxer->index_mode)
    {
    case INDEX_MODE_NONE:
      return 0;
      break;
    case INDEX_MODE_PCM:
    case INDEX_MODE_SI_SA:
      if(!(b->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
        return 0;
      /* Format is already sample accurate */
      for(i = 0; i < b->tt->num_tracks; i++)
        b->tt->tracks[i]->flags |= TRACK_SAMPLE_ACCURATE;
      return 1;
      break;
    case INDEX_MODE_SIMPLE:
    case INDEX_MODE_MIXED:
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Index building unsupported for now");
      return 1;
      break;
    }
  return 0;
  }
