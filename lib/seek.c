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



// #define DUMP_SUPERINDEX    
#include <avdec_private.h>
// #include <parser.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_DOMAIN "seek"

// #define DUMP_ITERATIVE

static int skip_to(bgav_t * b, bgav_track_t * track, int64_t * time, int scale)
  {
  //  fprintf(stderr, "Skip to: %ld\n", *time);
  if(!bgav_track_skipto(track, time, scale))
    {
    b->flags |= BGAV_FLAG_EOF;
    return 0;
    }
  return 1;
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

static int seek_si(bgav_t * b, bgav_demuxer_context_t * ctx,
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
      return 0;
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
    if((s->type == GAVL_STREAM_AUDIO) && s->data.audio.sync_samples)
      time_scaled -= s->data.audio.sync_samples;

    if((s->stats.pts_start != GAVL_TIME_UNDEFINED) &&
       (time_scaled < s->stats.pts_start))
      time_scaled = s->stats.pts_start;
    
    s->index_position = gavl_packet_index_seek(b->demuxer->si, s->stream_id, time_scaled);
    
    if(s->index_position < 0)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Seeking failed");
      return 0;
      }
    
    }
  
  /* Find the start packet */

  if(!(ctx->flags & BGAV_DEMUXER_NONINTERLEAVED))
    {
    ctx->index_position = get_start(track->streams, track->num_streams);
    bgav_input_seek(b->input, ctx->si->entries[ctx->index_position].position, SEEK_SET);
    }

  for(j = 0; j < track->num_streams; j++)
    {
    s = track->streams[j];

    if((s->action == BGAV_STREAM_MUTE) ||
       (s->flags & STREAM_EXTERN))
      continue;

    if(!(ctx->flags & BGAV_DEMUXER_NONINTERLEAVED))
      s->index_position = gavl_packet_index_get_next_keyframe(ctx->si, s->stream_id, ctx->index_position);
    
    STREAM_SET_SYNC(s, b->demuxer->si->entries[s->index_position].pts);
    }
  
  bgav_track_resync(track);
  return skip_to(b, track, &orig_time, scale);
  }


static int seek_once(bgav_t * b, int64_t * time, int scale)
  {
  bgav_track_t * track = b->tt->cur;
  int64_t sync_time;

  bgav_track_clear(track);
  b->demuxer->demuxer->seek(b->demuxer, *time, scale);
  /* Re-sync decoders */

  bgav_track_resync(track);
  sync_time = bgav_track_sync_time(track, scale);

  if(*time > sync_time)
    return skip_to(b, track, time, scale);
  else
    {
    if(!skip_to(b, track, &sync_time, scale))
      return 0;
    *time = sync_time;
    return 1;
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

static int seek_generic(bgav_t * b, int64_t * time, int scale)
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
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Failed to re-sync during iterative seek");
      return 0;
      }
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
    return skip_to(b, b->tt->cur, time, scale);
  
  *time = sync_time;
  return 1;
  }

static int seek_input(bgav_t * b, int64_t * time, int scale)
  {
  gavl_time_t seek_time;
  //  gavl_time_t time_offset = gavl_track_get_display_time_offset(b->tt->cur->info);
  
  bgav_track_clear(b->tt->cur);

  seek_time = gavl_time_unscale(scale, *time);

  if(!bgav_input_seek_time(b->input, seek_time))
    return 0;
  bgav_track_resync(b->tt->cur);
  //  skip_to(b, b->tt->cur, time, scale);
  return 1;
  }

int bgav_ensure_index(bgav_t * b)
  {
  if(b->demuxer->si)
    return 1;
  
  if(b->demuxer->index_mode == INDEX_MODE_SIMPLE)
    {
    /* Build packet index */
    const char * location = NULL;

    if(!gavl_metadata_get_src(&b->input->m, GAVL_META_SRC, 0, NULL, &location) |
       !location)
      return 0;
    
    if((b->demuxer->si = bgav_get_packet_index(location)))
      {
      //      gavl_dprintf("Built packet index:\n");
      //      gavl_packet_index_dump(b->demuxer->si);
      return 1;
      }
    else
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Building packet index failed");
      return 0;
      }
    }
  gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Cannot build packet index (unsupported file format)");
  return 0;
  }

int
bgav_seek_scaled(bgav_t * b, int64_t * time, int scale)
  {
  bgav_track_t * track = b->tt->cur;

  if(b->flags & BGAV_FLAG_PAUSED)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "bgav_seek_scaled failed: Decoder paused");
    return 0;
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

  if(bgav_options_get_bool(&b->opt, BGAV_OPT_SAMPLE_ACCURATE) &&
     !(b->demuxer->flags & BGAV_DEMUXER_SAMPLE_ACCURATE))
    {
    if(!bgav_ensure_index(b))
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Sample accurate seeking not supported for format");
    }
    
  if(b->demuxer->si)
    return seek_si(b, b->demuxer, *time, scale);
  else if(b->input->flags & BGAV_INPUT_CAN_SEEK_TIME)
    {
    return seek_input(b, time, scale);
    }
  /* Seek once */
  else if(b->demuxer->demuxer->seek)
    return seek_once(b, time, scale);
  /* Seek iterative */
  else if(b->demuxer->demuxer->post_seek_resync)
    return seek_generic(b, time, scale);
  /* Build seek index */
  else if((b->demuxer->index_mode == INDEX_MODE_SIMPLE) &&
          (b->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    {
    if(!bgav_ensure_index(b) || !b->demuxer->si)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Building packet index failed");
      return 0;
      }
    return seek_si(b, b->demuxer, *time, scale);
    }
  
  gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Don't know how to seek");
  return 0;
  }

int
bgav_seek_to_video_frame(bgav_t * b, int stream, int frame)
  {
  int64_t pts;

  /* Frame -> Time */
  bgav_stream_t * s = bgav_track_get_video_stream(b->tt->cur, stream);
  if(s->data.video.format->framerate_mode == GAVL_FRAMERATE_CONSTANT)
    pts = s->stats.pts_start + frame * s->data.video.format->frame_duration;
  else if(!(s->ci->flags & GAVL_COMPRESSION_HAS_B_FRAMES))
    {
    pts = gavl_packet_index_packet_number_to_pts(b->demuxer->si,
                                                 s->stream_id,
                                                 frame);
    if(pts == GAVL_TIME_UNDEFINED)
      return 0;
    }
  else /* B-frames and nonconstant framerate */
    {
    if(!s->data.video.frame_table)
      {
      if(!bgav_ensure_index(b))
        return 0;
      
      s->data.video.frame_table = gavl_packet_index_create(0);
      
      gavl_packet_index_extract_stream(b->demuxer->si,
                                       s->data.video.frame_table,
                                       s->stream_id);
      gavl_packet_index_sort_by_pts(s->data.video.frame_table);
      }

    if((frame < 0) || (frame >= s->data.video.frame_table->num_entries))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Frame index %d out of range (must be between zero and %"PRId64")",
               frame, bgav_get_num_video_frames(b, stream));
      return 0;
      }
    pts = s->data.video.frame_table->entries[frame].pts;
    }
  return bgav_seek_scaled(b, &pts, s->data.video.format->timescale);
  }

void
bgav_seek(bgav_t * b, gavl_time_t * time)
  {
  bgav_seek_scaled(b, time, GAVL_TIME_SCALE);
  }
