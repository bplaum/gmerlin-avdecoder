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

static void seek_iterative(bgav_t * b, int64_t * time, int scale)
  {
  int num_seek = 0;
  int num_resync = 0;
  bgav_track_t * track = b->tt->cur;
  
  int64_t seek_time;
  int64_t sync_time;
  int64_t out_time           = GAVL_TIME_UNDEFINED;
  
  int64_t seek_time_lower    = GAVL_TIME_UNDEFINED;

  int64_t sync_time_upper    = GAVL_TIME_UNDEFINED;
  int64_t sync_time_lower    = GAVL_TIME_UNDEFINED;

  int64_t out_time_lower    = GAVL_TIME_UNDEFINED;
  
  int64_t one_second = gavl_time_scale(scale, GAVL_TIME_SCALE);
  int final_seek = 0;
  
  seek_time = *time;
#ifdef DUMP_ITERATIVE
  bgav_dprintf("****** Seek iterative %"PRId64", %d ***********\n",
               *time, scale);
#endif
  while(1)
    {
#ifdef DUMP_ITERATIVE
    bgav_dprintf("Seek time: %"PRId64"\n", seek_time);
#endif
    bgav_track_clear(track);
    b->demuxer->demuxer->seek(b->demuxer, seek_time, scale);
    num_seek++;
    
    sync_time = bgav_track_sync_time(track, scale);

    if(sync_time == GAVL_TIME_UNDEFINED)
      {
      b->flags |= BGAV_FLAG_EOF;
      return;
      }
    
#ifdef DUMP_ITERATIVE
    bgav_dprintf("Sync time: %"PRId64"\n", sync_time);
#endif    
    // diff_time = *time - sync_time;

    if(sync_time > *time) /* Sync time too late */
      {
      /* Exit if we are already at position zero */
      if(seek_time == 0)
        {
        bgav_track_resync(track);
        num_resync++;
        break;
        }
      
#ifdef DUMP_ITERATIVE
      //      fprintf(stderr, "Sync time too late\n");
#endif
      if((sync_time_upper == GAVL_TIME_UNDEFINED) ||
         (sync_time_upper > sync_time))
        {
        //        seek_time_upper = seek_time;
        sync_time_upper = sync_time;
        }
      /* If we were too early before, exit here */
      if(sync_time_lower != GAVL_TIME_UNDEFINED)
        {
        seek_time = seek_time_lower;
        out_time = out_time_lower;
        final_seek = 1;
        break;
        }
#if 0
      /* If we cannot go before the target time, exit as well */
      if((sync_time_lower == GAVL_TIME_UNDEFINED) &&
         (sync_time == sync_time_upper))
        {
        bgav_track_resync(track);
        num_resync++;
        break;
        }
#endif
      /* Go backward */
      seek_time -= ((3*(sync_time - *time))/2 + one_second);
      if(seek_time < 0)
        seek_time = 0;
      continue;
      }
    /* Sync time too early, but already been there: Exit */
    else if((sync_time_lower != GAVL_TIME_UNDEFINED) &&
            (sync_time == sync_time_lower))
      {
      bgav_track_resync(track);
      num_resync++;
      break;
      }
    else /* Sync time too early, get out time */
      {
      bgav_track_resync(track);
      num_resync++;

      out_time = bgav_track_out_time(track, scale);

#ifdef DUMP_ITERATIVE
      bgav_dprintf("Out time: %"PRId64"\n", out_time);
#endif
      if(out_time > *time) /* Out time too late */
        {
#ifdef DUMP_ITERATIVE
        bgav_dprintf("Out time too late\n");
#endif
        seek_time -= ((3*(out_time - *time))/2 + one_second);
        continue;
        }
      else if(out_time < *time)
        {
#ifdef DUMP_ITERATIVE
        bgav_dprintf("Out time too early\n");
#endif
        /* If difference is less than half a second, exit here */
        if(*time - out_time < one_second / 2)
          break;
        
        /* Remember position and go a bit forward */
        if((out_time_lower == GAVL_TIME_UNDEFINED) ||
           (out_time_lower < out_time))
          {
          seek_time_lower = seek_time;
          out_time_lower  = out_time;
          sync_time_lower = sync_time;
          
          seek_time += (*time - out_time)/2;
          continue;
          }
        /* Have been better before */
        else if(out_time <= out_time_lower)
          {
          seek_time = seek_time_lower;
          out_time = out_time_lower;
          final_seek = 1;
          break;
          }
        }
      else
        break;
      }
    }

  if(final_seek)
    {
    bgav_track_clear(track);
#ifdef DUMP_ITERATIVE
    bgav_dprintf("Final seek %"PRId64"\n", seek_time);
#endif
    b->demuxer->demuxer->seek(b->demuxer, seek_time, scale);
    num_seek++;
    bgav_track_resync(track);
    num_resync++;
    }

  if(*time > out_time)
    skip_to(b, track, time, scale);
  else
    {
    skip_to(b, track, &out_time, scale);
    *time = out_time;
    }

#ifdef DUMP_ITERATIVE
  bgav_dprintf("Seeks: %d, resyncs: %d\n", num_seek, num_resync);
#endif  
  }

typedef struct
  {
  int64_t time;
  int scale;
  } seek_subreader_t;

static int seek_subreader(void * data, bgav_stream_t * s)
  {
  seek_subreader_t * d = data;

  if((s->flags & STREAM_HAS_SUBREADER) &&
     (s->action != BGAV_STREAM_MUTE))
    {
    /* Clear EOF state */
    s->flags &= ~(STREAM_EOF_C|STREAM_EOF_D);
    bgav_subtitle_reader_seek(s, d->time, d->scale);
    }
  return 1;
  }

void
bgav_seek_scaled(bgav_t * b, int64_t * time, int scale)
  {
  seek_subreader_t ss;
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

  if(!(b->demuxer->flags & BGAV_DEMUXER_SUBREAD_ONLY))
    {
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
    else if(!(b->demuxer->flags & BGAV_DEMUXER_SEEK_ITERATIVE))
      {
      seek_once(b, time, scale);
      }
    /* Seek iterative */
    else
      {
      seek_iterative(b, time, scale);
      }
    }
  
  ss.time = *time;
  ss.scale = scale;
  
  bgav_streams_foreach(track->streams, track->num_streams, seek_subreader, &ss);
  }

void
bgav_seek(bgav_t * b, gavl_time_t * time)
  {
  bgav_seek_scaled(b, time, GAVL_TIME_SCALE);
  }
