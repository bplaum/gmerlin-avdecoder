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

static int get_start(bgav_stream_t ** s, int num)
  {
  int i;
  int ret = -1;
  for(i = 0; i < num; i++)
    {
    if(s[i]->action == BGAV_STREAM_MUTE)
      continue;
    
    if((s[i]->type != GAVL_STREAM_AUDIO) &&
       (s[i]->type != GAVL_STREAM_VIDEO))
      continue;
    
    if((ret < 0) || (ret > s[i]->index_position))
      ret = s[i]->index_position;
    }
  return ret;
  }

static void seek_si(bgav_t * b, bgav_demuxer_context_t * ctx,
                    int64_t time, int scale)
  {
  int64_t orig_time;
  uint32_t j;
  bgav_track_t * track;
  bgav_stream_t * s;
  int64_t time_scaled;
  int sample_scale;
    
  track = ctx->tt->cur;
  bgav_track_clear(track);
  
  /* Seek the start chunks indices of all streams */
  
  orig_time = time;
  
  for(j = 0; j < track->num_video_streams; j++)
    {
    s = bgav_track_get_video_stream(track, j);
    
    if(s->action == BGAV_STREAM_MUTE)
      continue;

    gavl_dictionary_get_int(s->m, GAVL_META_STREAM_SAMPLE_TIMESCALE, &sample_scale);
    
    time_scaled = gavl_time_rescale(scale, sample_scale, time);
    
    s->index_position = gavl_packet_index_seek(b->demuxer->si, s->stream_id, time_scaled);
    s->index_position = gavl_packet_index_get_keyframe_before(b->demuxer->si, s->stream_id, s->index_position);

    if(s->index_position < 0)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Seeking failed");
      return;
      }
    
    time = gavl_time_rescale(sample_scale, scale, b->demuxer->si->entries[s->index_position].pts);
    
    }
  for(j = 0; j < track->num_streams; j++)
    {
    s = track->streams[j];
    
    if((s->action == BGAV_STREAM_MUTE) ||
       (s->type == GAVL_STREAM_VIDEO) ||
       (s->flags & STREAM_EXTERN))
      continue;
    
    gavl_dictionary_get_int(s->m, GAVL_META_STREAM_SAMPLE_TIMESCALE, &sample_scale);
    time_scaled = gavl_time_rescale(scale, sample_scale, time);
    
    /* Handle preroll */
    if((s->type == GAVL_STREAM_AUDIO) && s->data.audio.preroll)
      time_scaled -= s->data.audio.preroll;
    
    s->index_position = gavl_packet_index_seek(b->demuxer->si, s->stream_id, time_scaled);

    if(s->index_position < 0)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Seeking failed");
      return;
      }
    
    }
  
  /* Find the start packet */

  if(!(ctx->flags & BGAV_DEMUXER_NONINTERLEAVED))
    {
    ctx->index_position = get_start(track->streams, track->num_streams);
    
    bgav_input_seek(b->input, ctx->si->entries[ctx->index_position].position, SEEK_SET);

    for(j = 0; j < track->num_streams; j++)
      {
      s = track->streams[j];

      if((s->action == BGAV_STREAM_MUTE) ||
         (s->flags & STREAM_EXTERN))
        continue;

      s->index_position = gavl_packet_index_get_next_keyframe(ctx->si, s->stream_id, ctx->index_position);
      STREAM_SET_SYNC(s, b->demuxer->si->entries[s->index_position].pts);
      }
    }
  
  bgav_track_resync(track);
  skip_to(b, track, &orig_time, scale);
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

  start_time = gavl_track_get_start_time(b->tt->cur->info);
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

#if 0
static void build_seek_index(bgav_t * b)
  {
  int i;
  gavl_packet_t * p;
  bgav_stream_t * s;

  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Building seek index");
  
  bgav_track_clear(b->tt->cur);
  bgav_input_seek(b->input, b->tt->cur->data_start, SEEK_SET);
  bgav_track_resync(b->tt->cur);

  /* TODO: This gets a bit more complicated when we build seek indices for files with more than one A/V stream */
  for(i = 0; i < b->tt->cur->num_streams; i++)
    {
    s = b->tt->cur->streams[i];
    if((s->type == GAVL_STREAM_AUDIO) || (s->type == GAVL_STREAM_VIDEO))
      break;
    }
  
  while(1)
    {
    p = NULL;
    if(bgav_stream_get_packet_read(s, &p) != GAVL_SOURCE_OK)
      break;
    gavl_seek_index_append_packet(&s->index, p, s->ci->flags);
    }
  b->demuxer->flags |= BGAV_DEMUXER_HAS_SEEK_INDEX;
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Built seek index: %d entries", s->index.num_entries);
  gavl_seek_index_dump(&s->index);
  
  }
#endif

#if 0
static void seek_with_index(bgav_t * b, int64_t * time, int scale)
  {
  int i;
  int64_t file_pos = -1;
  int stream_scale;

  if(b->tt->cur->num_audio_streams + b->tt->cur->num_video_streams > 1)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Seeking with seek index and more than one A/V stream is not implemented");
    return;
    }
  
  if(!(b->demuxer->flags & BGAV_DEMUXER_HAS_SEEK_INDEX))
    build_seek_index(b);

  bgav_track_clear(b->tt->cur);
  
    
  /* Get right file position */
  for(i = 0; i < b->tt->cur->num_streams; i++)
    {
    bgav_stream_t * s = b->tt->cur->streams[i];
    
    stream_scale = -1;
    if(((s->type != GAVL_STREAM_AUDIO) && (s->type != GAVL_STREAM_VIDEO)) ||
       !gavl_dictionary_get_int(s->m, GAVL_META_STREAM_SAMPLE_TIMESCALE, &stream_scale) ||
       (stream_scale <= 0))
      {
      s->index_position = -1;
      continue;
      }
    else
      {
      s->index_position = gavl_seek_index_seek(&s->index,
                                               gavl_time_rescale(scale, stream_scale, *time));

      if((file_pos < 0) || (file_pos > s->index.entries[s->index_position].position))
        file_pos = s->index.entries[s->index_position].position;
      }
    }

  /* Seek to file position */
  bgav_input_seek(b->input, file_pos, SEEK_SET);
  
  /* Resync streams */

  for(i = 0; i < b->tt->cur->num_streams; i++)
    {
    bgav_stream_t * s = b->tt->cur->streams[i];
    
    if(s->index_position >= 0)
      {
      STREAM_SET_SYNC(s, s->index.entries[s->index_position].pts);
      }
    }
  
  bgav_track_resync(b->tt->cur);
  skip_to(b, b->tt->cur, time, scale);
  }
#endif

static void seek_input(bgav_t * b, int64_t * time, int scale)
  {
  gavl_time_t seek_time;
  //  gavl_time_t time_offset = gavl_track_get_display_time_offset(b->tt->cur->info);
  
  bgav_track_clear(b->tt->cur);

  seek_time = gavl_time_unscale(scale, *time);
  bgav_input_seek_time(b->input, seek_time);
  bgav_track_resync(b->tt->cur);
  //  skip_to(b, b->tt->cur, time, scale);
  
  }

static int ensure_index(bgav_t * b)
  {
  
  if((b->demuxer->index_mode == INDEX_MODE_SIMPLE) && !b->demuxer->si)
    {
    /* TODO: Build packet index */
    const char * location = NULL;

    if(!gavl_metadata_get_src(&b->input->m, GAVL_META_SRC, 0, NULL, &location) |
       !location)
      return 0;
    
    if((b->demuxer->si = bgav_get_packet_index(location)))
      return 1;
    
    }

  return 0;
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

  if(b->opt.sample_accurate && !(b->demuxer->flags & BGAV_DEMUXER_SAMPLE_ACCURATE))
    {
    if(!ensure_index(b))
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Sample accurate seeking not supported for format");
      }
    }
    
  if(b->demuxer->si)
    seek_si(b, b->demuxer, *time, scale);
  else if(b->input->flags & BGAV_INPUT_CAN_SEEK_TIME)
    {
    seek_input(b, time, scale);
    }
  /* Seek once */
  else if(b->demuxer->demuxer->seek)
    seek_once(b, time, scale);
  /* Seek iterative */
  else if(b->demuxer->demuxer->post_seek_resync)
    seek_generic(b, time, scale);
  /* Build seek index */
  else if((b->demuxer->index_mode == INDEX_MODE_SIMPLE) &&
          (b->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    {
    if(!ensure_index(b) || !b->demuxer->si)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Building packet index failed");
      return;
      }
    seek_si(b, b->demuxer, *time, scale);
    }
  }

#if 0
void
bgav_seek_scaled(bgav_t * b, int64_t * time, int scale)
  {
  bgav_seek_scaled_unit(b, time, scale, GAVL_SRC_SEEK_PTS);
  }
#endif

void
bgav_seek(bgav_t * b, gavl_time_t * time)
  {
  bgav_seek_scaled(b, time, GAVL_TIME_SCALE);
  }
