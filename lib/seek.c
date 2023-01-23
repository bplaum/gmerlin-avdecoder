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

// #define DUMP_SUPERINDEX    
#include <avdec_private.h>
// #include <parser.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_DOMAIN "seek"

// #define DUMP_ITERATIVE

static void skip_to(bgav_t * b, bgav_track_t * track, int64_t * time, int scale)
  {
  //  fprintf(stderr, "Skip to: %ld\n", *time);
  if(!bgav_track_skipto(track, time, scale))
    b->flags |= BGAV_FLAG_EOF;
  //  fprintf(stderr, "Skipped to: %ld\n", *time);
  }

/* Seek functions with superindex */

static void get_start_end(bgav_stream_t * s, int num,
                          int32_t * start_packet, int32_t * end_packet)
  {
  int i;
  for(i = 0; i < num; i++)
    {
    if(s[i].action == BGAV_STREAM_MUTE)
      continue;
    
    if((s[i].type != GAVL_STREAM_AUDIO) &&
       (s[i].type != GAVL_STREAM_VIDEO))
      continue;
    
    if(*start_packet > s[i].index_position)
      *start_packet = s[i].index_position;
    if(*end_packet < s[i].index_position)
      *end_packet = s[i].index_position;
    }
  }

static void seek_si(bgav_t * b, bgav_demuxer_context_t * ctx,
                    int64_t time, int scale)
  {
  int64_t orig_time;
  uint32_t i, j;
  int32_t start_packet;
  int32_t end_packet;
  bgav_track_t * track;
  bgav_stream_t * s;
  int64_t seek_time;
  
  track = ctx->tt->cur;
  bgav_track_clear(track);
  
  /* Seek the start chunks indices of all streams */
  
  orig_time = time;
  
  for(j = 0; j < track->num_video_streams; j++)
    {
    
    s = bgav_track_get_video_stream(track, j);
    
    if(s->action == BGAV_STREAM_MUTE)
      continue;
    seek_time = time;
    bgav_superindex_seek(ctx->si, s, &seek_time, scale);
    /* Synchronize time to the video stream */
    if(!j)
      time = seek_time;
    }
  for(j = 0; j < track->num_streams; j++)
    {
    if((track->streams[j].action == BGAV_STREAM_MUTE) ||
       (track->streams[j].type == GAVL_STREAM_VIDEO) ||
       (track->streams[j].flags & STREAM_EXTERN))
      continue;
    
    seek_time = time;
    bgav_superindex_seek(ctx->si, &track->streams[j], &seek_time, scale);
    }
  
  /* Find the start and end packet */

  if(ctx->demux_mode == DEMUX_MODE_SI_I)
    {
    start_packet = 0x7FFFFFFF;
    end_packet   = 0x0;

    get_start_end(track->streams, track->num_streams,
                  &start_packet, &end_packet);
    
    /* Do the seek */
    ctx->si->current_position = start_packet;
    bgav_input_seek(ctx->input,
                    ctx->si->entries[ctx->si->current_position].offset,
                    SEEK_SET);

    ctx->flags |= BGAV_DEMUXER_SI_SEEKING;
    for(i = start_packet; i <= end_packet; i++)
      bgav_demuxer_next_packet_interleaved(ctx);

    ctx->flags &= ~BGAV_DEMUXER_SI_SEEKING;
    }
  bgav_track_resync(track);
  skip_to(b, track, &orig_time, scale);
  }

static void seek_sa(bgav_t * b, int64_t * time, int scale)
  {
  int i;
  bgav_stream_t * s;
  for(i = 0; i < b->tt->cur->num_video_streams; i++)
    {
    s = bgav_track_get_video_stream(b->tt->cur, i);
    
    if(s->action != BGAV_STREAM_MUTE)
      {
      bgav_seek_video(b, i,
                      gavl_time_rescale(scale, s->data.video.format->timescale,
                                        *time) - s->stats.pts_start);
      if(s->flags & STREAM_EOF_C)
        {
        b->flags |= BGAV_FLAG_EOF;
        return;
        }
      /*
       *  We align seeking at the first frame of the first video stream
       */

      if(!i)
        {
        *time =
          gavl_time_rescale(s->data.video.format->timescale,
                            scale, s->out_time);
        }
      }
    }
    
  for(i = 0; i < b->tt->cur->num_audio_streams; i++)
    {
    s = bgav_track_get_audio_stream(b->tt->cur, i);

    if(s->action != BGAV_STREAM_MUTE)
      {
      bgav_seek_audio(b, i,
                      gavl_time_rescale(scale, s->data.audio.format->samplerate,
                                        *time) - s->stats.pts_start);
      if(s->flags & STREAM_EOF_C)
        {
        b->flags |= BGAV_FLAG_EOF;
        return;
        }
      }
    }

  for(i = 0; i < b->tt->cur->num_text_streams; i++)
    {
    s = bgav_track_get_text_stream(b->tt->cur, i);

    if(s->action != BGAV_STREAM_MUTE)
      {
      bgav_seek_text(b, i, gavl_time_rescale(scale, s->timescale, *time));
      }
    }

  for(i = 0; i < b->tt->cur->num_overlay_streams; i++)
    {
    s = bgav_track_get_overlay_stream(b->tt->cur, i);

    if(s->action != BGAV_STREAM_MUTE)
      {
      bgav_seek_overlay(b, i, gavl_time_rescale(scale, s->timescale, *time));
      }
    }
  return;
  
  }

static void seek_once(bgav_t * b, int64_t * time, int scale)
  {
  bgav_track_t * track = b->tt->cur;
  int64_t sync_time;

  bgav_track_clear(track);
  b->demuxer->demuxer->seek(b->demuxer, *time, scale);
  /* Re-sync decoders */

  bgav_track_resync(track);
  sync_time = bgav_track_sync_time(track, scale);

  if(*time > sync_time)
    skip_to(b, track, time, scale);
  else
    {
    skip_to(b, track, &sync_time, scale);
    *time = sync_time;
    }
  
  //  return sync_time;
  }

typedef struct
  {
  double percentage;
  int64_t sync_time;
  } seek_tab_t;

static int64_t seek_test(bgav_t * b, int64_t filepos, int scale)
  {
  bgav_track_clear(b->tt->cur);
  bgav_input_seek(b->input, filepos, SEEK_SET);

  if(!b->demuxer->demuxer->post_seek_resync(b->demuxer))
    return GAVL_TIME_UNDEFINED; // EOF

  return bgav_track_sync_time(b->tt->cur, scale);
  }

/* Time can have an arbitrary scale but the zero point must be the same
   as the stream native timestamps */

static int64_t position_from_percentage(bgav_t * b,
                                        int64_t total_bytes,
                                        double percentage)
  {
  int64_t position;
  position = b->tt->cur->data_start + (int64_t)(percentage * (double)total_bytes);
  if(position > b->tt->cur->data_start + total_bytes)
    position = b->tt->cur->data_start + total_bytes;

  if(b->input->block_size)
    {
    int rest = position % b->input->block_size;
    position -= rest;
    }
  return position;
  }

static void seek_generic(bgav_t * b, int64_t * time, int scale)
  {
  int i;
  
  gavl_time_t start_time;
  gavl_time_t end_time;
  
  int64_t position;
  int64_t sync_time;
  
  seek_tab_t tab[2];
  int64_t total_bytes;
  double percentage;
  int num_seeks = 0;
  int64_t offset = gavl_time_scale(scale, GAVL_TIME_SCALE);
  
  if(b->tt->cur->data_end > 0)
    total_bytes = b->tt->cur->data_end - b->tt->cur->data_start;
  else
    total_bytes = b->input->total_bytes - b->tt->cur->data_start;

  start_time = gavl_track_get_display_time_offset(b->tt->cur->info);
  end_time = start_time + gavl_track_get_duration(b->tt->cur->info);
  
  tab[0].sync_time = gavl_time_scale(scale, start_time);
  tab[0].percentage  = 0.0;
  tab[1].sync_time = gavl_time_scale(scale, end_time);
  tab[1].percentage  = 1.0;
  
  for(i = 0; i < 6; i++)
    {
    /* Bisection search */
    percentage = tab[0].percentage +
      (tab[1].percentage - tab[0].percentage) *
      (double)(*time - tab[0].sync_time - offset) / (double)(tab[1].sync_time - tab[0].sync_time);
    
    if(percentage < tab[0].percentage)
      percentage = tab[0].percentage;
    
    position = position_from_percentage(b, total_bytes, percentage);
    sync_time = seek_test(b, position, scale);
    num_seeks++;
    if(sync_time == GAVL_TIME_UNDEFINED)
      break;
    
    if(sync_time < *time)
      {
      /* Return if we are less than 1 second before the seek point */
      if(gavl_time_unscale(scale, *time - sync_time) <= GAVL_TIME_SCALE)
        break;
      
      tab[0].percentage = percentage;
      tab[0].sync_time = sync_time;
      }
    /* Direct hit (unlikely) */
    else if(sync_time == *time)
      break;
    /* Upper boundary */
    else
      {
      tab[1].percentage = percentage;
      tab[1].sync_time = sync_time;
      }
    }

  if(sync_time == tab[1].sync_time)
    {
    /* Go to lower boundary instead */
    position = position_from_percentage(b, total_bytes, tab[0].percentage);
    sync_time = seek_test(b, position, scale);
    num_seeks++;
    }
  
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Seek generic: Seeks: %d, goal: %"PRId64", reached: %"PRId64" diff: %"PRId64,
           num_seeks, *time, sync_time, *time - sync_time);
  
  bgav_track_resync(b->tt->cur);

  if(*time > sync_time)
    skip_to(b, b->tt->cur, time, scale);
  else
    *time = sync_time;
  
  }

void
bgav_seek_scaled(bgav_t * b, int64_t * time, int scale)
  {
  bgav_track_t * track = b->tt->cur;

  if(b->flags & BGAV_FLAG_PAUSED)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "bgav_seek_scaled failed: Decoder paused");
    return;
    }
  
  //  fprintf(stderr, "bgav_seek_scaled: %f\n",
  //          gavl_time_to_seconds(gavl_time_unscale(scale, *time)));
  
  /* Clear EOF */

  bgav_track_clear_eof_d(track);
  b->flags &= ~BGAV_FLAG_EOF;
  
  /*
   * Seek with superindex
   *
   * This must be checked *before* we check for seek_sa because
   * AVIs with mp3 audio will also have b->tt->cur->sample_accurate = 1
   */
  
  if(b->demuxer->si && !(b->demuxer->flags & BGAV_DEMUXER_SI_PRIVATE_FUNCS))
    {
    seek_si(b, b->demuxer, *time, scale);
    }
  /* Seek with sample accuracy */
  else if(b->tt->cur->flags & TRACK_SAMPLE_ACCURATE)
    {
    seek_sa(b, time, scale);    
    }
  /* Seek once */
  else if(b->demuxer->demuxer->seek)
    seek_once(b, time, scale);
  /* Seek iterative */
  else if(b->demuxer->demuxer->post_seek_resync)
    seek_generic(b, time, scale);
  }

void
bgav_seek(bgav_t * b, gavl_time_t * time)
  {
  bgav_seek_scaled(b, time, GAVL_TIME_SCALE);
  }
