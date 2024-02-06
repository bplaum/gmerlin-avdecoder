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

#include <gavl/trackinfo.h>

#include <parser.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#define LOG_DOMAIN "track"

void bgav_track_set_format(bgav_track_t * track, const char * format, const char * mimetype)
  {
  gavl_dictionary_t * src =
    gavl_dictionary_get_dictionary_create(track->metadata, GAVL_META_SRC);
  
  if(!src)
    return;
  
  if(!gavl_dictionary_get_string(src, GAVL_META_FORMAT) && format)
    gavl_dictionary_set_string(src, GAVL_META_FORMAT, format);
  
  if(!gavl_dictionary_get_string(src, GAVL_META_MIMETYPE) && mimetype)
    gavl_dictionary_set_string(src, GAVL_META_MIMETYPE, mimetype);
  }

bgav_stream_t * bgav_track_get_stream(bgav_track_t * track, gavl_stream_type_t type,
                                      int stream)
  {
  int i;
  int idx = 0;

  for(i = 0; i < track->num_streams; i++)
    {
    if(track->streams[i]->type == type)
      {
      if((type == GAVL_STREAM_MSG))
        {
        if(track->streams[i]->stream_id == stream)
          return track->streams[i];
        else
          idx++;
        }
      else if(idx == stream)
        return track->streams[i];
      else
        idx++;
      }
    }
  return NULL;
  }

int bgav_track_set_stream_action(bgav_track_t * track, gavl_stream_type_t type, int stream,
                                 bgav_stream_action_t action)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_stream(track, type, stream)))
    return 0;
  s->action = action;
  return 1;
  }

bgav_stream_t * bgav_track_get_audio_stream(bgav_track_t * track, int stream)
  {
  return bgav_track_get_stream(track, GAVL_STREAM_AUDIO, stream);
  }

bgav_stream_t * bgav_track_get_video_stream(bgav_track_t * track, int stream)
  {
  return bgav_track_get_stream(track, GAVL_STREAM_VIDEO, stream);
  }

bgav_stream_t * bgav_track_get_text_stream(bgav_track_t * track, int stream)
  {
  return bgav_track_get_stream(track, GAVL_STREAM_TEXT, stream);
  }

bgav_stream_t * bgav_track_get_overlay_stream(bgav_track_t * track, int stream)
  {
  return bgav_track_get_stream(track, GAVL_STREAM_OVERLAY, stream);
  }

#if 1
bgav_stream_t * bgav_track_get_msg_stream_by_id(bgav_track_t * track, int id)
  {
  return bgav_track_get_stream(track, GAVL_STREAM_MSG, id);
  }
#endif

bgav_stream_t * bgav_track_get_subtitle_stream(bgav_track_t * t, int index)
  {
  /* First come the overlay streams, then the text streams */
  
  if(index >= t->num_overlay_streams)
    return bgav_track_get_text_stream(t, index - t->num_overlay_streams);
  else
    return bgav_track_get_overlay_stream(t, index);
  }

static bgav_stream_t * append_stream(bgav_track_t * t)
  {
  bgav_stream_t * ret;
  if(t->streams_alloc < t->num_streams + 1)
    {
    t->streams_alloc += 16;
    t->streams = realloc(t->streams, t->streams_alloc * sizeof(*(t->streams)));
    memset(t->streams + t->num_streams, 0,
           sizeof(*t->streams) * (t->streams_alloc - t->num_streams));
    }
  t->streams[t->num_streams] = calloc(1, sizeof(*t->streams[t->num_streams]));
  ret = t->streams[t->num_streams];
  
  t->num_streams++;
  return ret;
  }

bgav_stream_t *
bgav_track_add_audio_stream(bgav_track_t * t, const bgav_options_t * opt)
  {
  bgav_stream_t * ret = append_stream(t);
  t->num_audio_streams++;
  
  bgav_stream_init(ret, opt);

  // ret->data.audio.bits_per_sample = 16;
  ret->type = GAVL_STREAM_AUDIO;
  ret->track = t;
  
  ret->info_ext = gavl_track_append_audio_stream(t->info);
  ret->info = &ret->in_info;
  gavl_dictionary_copy(ret->info, ret->info_ext);
  
  ret->data.audio.format = gavl_stream_get_audio_format_nc(ret->info);
  ret->m = gavl_stream_get_metadata_nc(ret->info);

  bgav_stream_create_packet_buffer(ret);
  
  return ret;
  }

bgav_stream_t *
bgav_track_add_msg_stream(bgav_track_t * t, const bgav_options_t * opt, int id)
  {
  bgav_stream_t * ret = append_stream(t);
  bgav_stream_init(ret, opt);

  ret->type = GAVL_STREAM_MSG;
  ret->track = t;
  ret->stream_id = id;
  ret->action = BGAV_STREAM_DECODE;
  ret->flags |= STREAM_EXTERN;

  ret->info_ext = gavl_track_append_msg_stream(t->info, id);
  ret->info = &ret->in_info;
  gavl_dictionary_copy(ret->info, ret->info_ext);

  ret->m = gavl_stream_get_metadata_nc(ret->info);

  bgav_stream_create_packet_buffer(ret);
  
  return ret;
  }

bgav_stream_t *
bgav_track_add_video_stream(bgav_track_t * t, const bgav_options_t * opt)
  {
  bgav_stream_t * ret = append_stream(t);
  t->num_video_streams++;
  bgav_stream_init(ret, opt);
  ret->type = GAVL_STREAM_VIDEO;
  ret->opt = opt;
  ret->track = t;

  ret->info_ext = gavl_track_append_video_stream(t->info);
  ret->info = &ret->in_info;
  gavl_dictionary_copy(ret->info, ret->info_ext);
  
  ret->data.video.format = gavl_stream_get_video_format_nc(ret->info);
  ret->m = gavl_stream_get_metadata_nc(ret->info);
  
  ret->data.video.format->interlace_mode = GAVL_INTERLACE_UNKNOWN;
  ret->data.video.format->framerate_mode = GAVL_FRAMERATE_UNKNOWN;
  
  ret->ci->flags = GAVL_COMPRESSION_HAS_P_FRAMES;

  bgav_stream_create_packet_buffer(ret);
  
  return ret;
  }

static bgav_stream_t * add_text_stream(bgav_track_t * t,
                                       const bgav_options_t * opt,
                                       const char * charset)
  {
  bgav_stream_t * ret = append_stream(t);
  t->num_text_streams++;
  bgav_stream_init(ret, opt);

  ret->flags |= STREAM_DISCONT;

  ret->type = GAVL_STREAM_TEXT;
  if(charset)
    ret->data.subtitle.charset =
      gavl_strdup(charset);
  else
    ret->data.subtitle.charset =
      gavl_strdup(ret->opt->default_subtitle_encoding);

  ret->info_ext = gavl_track_append_text_stream(t->info);
  ret->info = &ret->in_info;
  gavl_dictionary_copy(ret->info, ret->info_ext);

  ret->data.subtitle.video.format = gavl_stream_get_video_format_nc(ret->info);
  ret->m = gavl_stream_get_metadata_nc(ret->info);
  ret->track = t;
  
  bgav_stream_create_packet_buffer(ret);
  
  return ret;
  }

static bgav_stream_t * add_overlay_stream(bgav_track_t * t,
                                          const bgav_options_t * opt)
  {
  bgav_stream_t * ret = append_stream(t);
  
  t->num_overlay_streams++;
  bgav_stream_init(ret, opt);
 
  ret->flags |= STREAM_DISCONT;
  ret->src_flags |= GAVL_SOURCE_SRC_DISCONTINUOUS;

  ret->type = GAVL_STREAM_OVERLAY;
  ret->track = t;

  ret->info_ext = gavl_track_append_overlay_stream(t->info);
  ret->info = &ret->in_info;
  gavl_dictionary_copy(ret->info, ret->info_ext);


  ret->data.subtitle.video.format = gavl_stream_get_video_format_nc(ret->info);
  ret->m = gavl_stream_get_metadata_nc(ret->info);
  
  bgav_stream_create_packet_buffer(ret);

  return ret;
  }

bgav_stream_t *
bgav_track_add_text_stream(bgav_track_t * t, const bgav_options_t * opt,
                           const char * charset)
  {
  return add_text_stream(t, opt, charset);
  }

bgav_stream_t *
bgav_track_add_overlay_stream(bgav_track_t * t, const bgav_options_t * opt)
  {
  return add_overlay_stream(t, opt);
  }

#if 0
bgav_stream_t *
bgav_track_attach_subtitle_reader(bgav_track_t * t,
                                  const bgav_options_t * opt,
                                  bgav_subtitle_reader_context_t * r)
  {
  bgav_stream_t * ret;

  if(r->reader->type == GAVL_STREAM_TEXT)
    ret = add_text_stream(t, opt, NULL, r);
  else
    ret = add_overlay_stream(t, opt, r);

  if(r->reader->setup_stream)
    r->reader->setup_stream(ret);

  /* Check, if info is a valid language tag */
  if(r->info)
    {
    if(bgav_lang_name(r->info))
      gavl_dictionary_set_string(ret->m, GAVL_META_LANGUAGE, r->info);
    else
      gavl_dictionary_set_string(ret->m, GAVL_META_LABEL, r->info);
    }
  
  return ret;
  }
#endif

bgav_stream_t *
bgav_track_find_stream_all(bgav_track_t * t, int stream_id)
  {
  int i;
  
  for(i = 0; i < t->num_streams; i++)
    {
    if((t->streams[i]->stream_id == stream_id) &&
       !(t->streams[i]->flags & STREAM_EXTERN))
      return t->streams[i];
    }
  return NULL;
  }

static bgav_stream_t * find_stream_by_id(bgav_stream_t ** s,
                                         int num, int id)
  {
  int i;
  for(i = 0; i < num; i++)
    {
    if(s[i]->stream_id == id)
      return s[i];
    }
  return NULL;
  }

bgav_stream_t * bgav_track_find_stream(bgav_demuxer_context_t * ctx,
                                       int stream_id)
  {
  bgav_track_t * t;
  bgav_stream_t * ret = NULL;
  
  t = ctx->tt->cur;
  
  ret = find_stream_by_id(t->streams, t->num_streams, stream_id);
  
  if(ret && (ret->action != BGAV_STREAM_MUTE) &&
     !(ret->flags & STREAM_EOF_D))
    return ret;
  return NULL;
  }

#define FREE(ptr) if(ptr){free(ptr);ptr=NULL;}
  
void bgav_track_stop(bgav_track_t * t)
  {
  int i;
  //  fprintf(stderr, "Stop track\n");
  for(i = 0; i < t->num_streams; i++)
    bgav_stream_stop(t->streams[i]);
  }

#if 0
void bgav_track_init(bgav_track_t * t)
  {
  int i;
  //  fprintf(stderr, "Stop track\n");
  for(i = 0; i < t->num_streams; i++)
    bgav_stream_init(&t->streams[i]);
  }
#endif



int bgav_track_start(bgav_track_t * t, bgav_demuxer_context_t * demuxer)
  {
  int i;
  int num_active_audio_streams = 0;
  int num_active_video_streams = 0;

  bgav_stream_t * s;

  //  fprintf(stderr, "Start track\n");
  
  for(i = 0; i < t->num_audio_streams; i++)
    {
    s = bgav_track_get_audio_stream(t, i);
    if(s->action == BGAV_STREAM_MUTE)
      continue;
    num_active_audio_streams++;

    if(!bgav_stream_start(s))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Starting audio decoder for stream %d failed", i+1);
      return 0;
      }
    }
  for(i = 0; i < t->num_video_streams; i++)
    {
    s = bgav_track_get_video_stream(t, i);
    if(s->action == BGAV_STREAM_MUTE)
      continue;
    num_active_video_streams++;
    if(!bgav_stream_start(s))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Starting video decoder for stream %d failed", i+1);
      //      gavl_dictionary_dump(s->info, 2);
      bgav_stream_dump(s);
      bgav_video_dump(s);
      return 0;
      }
    }

  for(i = 0; i < t->num_text_streams + t->num_overlay_streams; i++)
    {
    s = bgav_track_get_subtitle_stream(t, i);
    if(s->action == BGAV_STREAM_MUTE)
      continue;
    
    if(!bgav_stream_start(s))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Starting subtitle decoder for stream %d failed", i+1);
      //      gavl_dictionary_dump(s->info, 2);
      bgav_stream_dump(s);
      bgav_video_dump(s);
      return 0;
      }
    }

  if(!num_active_audio_streams && !num_active_video_streams)
    demuxer->flags |= BGAV_DEMUXER_PEEK_FORCES_READ;
  else
    demuxer->flags &= ~BGAV_DEMUXER_PEEK_FORCES_READ;
  
  return 1;
  }


void bgav_track_dump(bgav_track_t * t)
  {
  int i;
  //  const char * description;
  
  //  description = bgav_get_description(b);
  
  //  gavl_dprintf( "Format:   %s\n", (description ? description : 
  //                                   "Not specified"));
  
  gavl_diprintf(2, "Metadata\n");
  gavl_dictionary_dump(t->metadata, 4);
  gavl_dprintf("\n");

  gavl_diprintf(2, "Start position: %"PRId64"\n", t->data_start);
  gavl_diprintf(2, "End position:   %"PRId64"\n", t->data_end);
  
  for(i = 0; i < t->num_streams; i++)
    {
    bgav_stream_dump(t->streams[i]);
    
    switch(t->streams[i]->type)
      {
      case GAVL_STREAM_AUDIO:
        bgav_audio_dump(t->streams[i]);
        break;
      case GAVL_STREAM_VIDEO:
        bgav_video_dump(t->streams[i]);
        break;
      case GAVL_STREAM_TEXT:
        bgav_subtitle_dump(t->streams[i]);
        break;
      case GAVL_STREAM_OVERLAY:
        bgav_subtitle_dump(t->streams[i]);
        break;
      case GAVL_STREAM_MSG:
      case GAVL_STREAM_NONE:
        break;
      }
    }
  
  }

void bgav_track_free(bgav_track_t * t)
  {
  int i;
  
  if(t->streams)
    {
    for(i = 0; i < t->num_streams; i++)
      {
      bgav_stream_free(t->streams[i]);
      free(t->streams[i]);
      }
    free(t->streams);
    }
  }

static void remove_stream_abs(bgav_track_t * t, int idx)
  {
  gavl_track_delete_stream(t->info, idx);
  
  /* Streams are sometimes also removed for other reasons */
  bgav_stream_free(t->streams[idx]);
  free(t->streams[idx]);
  if(idx < t->num_streams - 1)
    {
    memmove(&t->streams[idx],
            &t->streams[idx+1],
            sizeof(*t->streams) * (t->num_streams - 1 - idx));
    }
  t->num_streams--;
  }


static void remove_stream(bgav_track_t * t, gavl_stream_type_t type, int idx)
  {
  /* Relative to absolute */
  idx = gavl_track_stream_idx_to_abs(t->info, type, idx);
  remove_stream_abs(t, idx);
  }

void bgav_track_remove_stream(bgav_track_t * track, int stream)
  {
  remove_stream_abs(track, stream);
  
  switch(track->streams[stream]->type)
    {
    case GAVL_STREAM_AUDIO:
      track->num_audio_streams--;
      break;
    case GAVL_STREAM_VIDEO:
      track->num_video_streams--;
      break;
    case GAVL_STREAM_TEXT:
      track->num_text_streams--;
      break;
    case GAVL_STREAM_OVERLAY:
      track->num_overlay_streams--;
      break;
    case GAVL_STREAM_MSG:
    case GAVL_STREAM_NONE:
      break;
    } 
  }

void bgav_track_remove_audio_stream(bgav_track_t * track, int stream)
  {
  remove_stream(track, GAVL_STREAM_AUDIO, stream);
  track->num_audio_streams--;
  }

void bgav_track_remove_video_stream(bgav_track_t * track, int stream)
  {
  remove_stream(track, GAVL_STREAM_VIDEO, stream);
  track->num_video_streams--;
  }

void bgav_track_remove_text_stream(bgav_track_t * track, int stream)
  {
  remove_stream(track, GAVL_STREAM_TEXT, stream);
  track->num_text_streams--;
  }

void bgav_track_remove_overlay_stream(bgav_track_t * track, int stream)
  {
  remove_stream(track, GAVL_STREAM_OVERLAY, stream);
  track->num_overlay_streams--;
  }

void bgav_track_remove_unsupported(bgav_track_t * track)
  {
  int i;
  bgav_stream_t * s;

  i = 0;
  while(i < track->num_audio_streams)
    {
    s = bgav_track_get_audio_stream(track, i);
    if(!bgav_find_audio_decoder(s->fourcc))
      {
      if(!(s->fourcc & 0xffff0000))
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                 "No audio decoder found for WAVId 0x%04x",
                 s->fourcc);
      else
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                 "No audio decoder found for fourcc %c%c%c%c (0x%08x)",
                 (s->fourcc & 0xFF000000) >> 24,
                 (s->fourcc & 0x00FF0000) >> 16,
                 (s->fourcc & 0x0000FF00) >> 8,
                 (s->fourcc & 0x000000FF),
                 s->fourcc);
      bgav_track_remove_audio_stream(track, i);
      }
    else
      i++;
    }
  i = 0;
  while(i < track->num_video_streams)
    {
    s = bgav_track_get_video_stream(track, i);

    if(!bgav_find_video_decoder(s->fourcc))
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "No video decoder found for fourcc %c%c%c%c (0x%08x)",
               (s->fourcc & 0xFF000000) >> 24,
               (s->fourcc & 0x00FF0000) >> 16,
               (s->fourcc & 0x0000FF00) >> 8,
               (s->fourcc & 0x000000FF),
               s->fourcc);
      bgav_track_remove_video_stream(track, i);
      }
    else
      i++;
    }
  }

int bgav_track_foreach(bgav_track_t * t,
                     int (*action)(void * priv, bgav_stream_t * s), void * priv)
  {
  if(!bgav_streams_foreach(t->streams,   t->num_streams,   action, priv))
    return 0;
  return 1;
  }

#if 1
static int reset_index_positions(void * priv, bgav_stream_t * s)
  {
  s->first_index_position = INT_MAX;
  s->last_index_position = 0;
  s->index_position = 0;
  return 1;
  }

void bgav_track_reset_index_positions(bgav_track_t * t)
  {
  bgav_track_foreach(t, reset_index_positions, NULL);
  return;
  }
#endif

static int clear_stream(void * priv, bgav_stream_t * s)
  {
  bgav_stream_clear(s);
  return 1;
  }

void bgav_track_clear(bgav_track_t * track)
  {
  bgav_track_foreach(track, clear_stream, NULL);
  }

static int init_stream_read(void * priv, bgav_stream_t * s)
  {
  bgav_stream_init_read(s);
  return 1;
  }


void bgav_track_init_read(bgav_track_t * track)
  {
  int i;
  /* Set all streams to read mode */
  for(i = 0; i < track->num_streams; i++)
    {
    switch(track->streams[i]->type)
      {
      case GAVL_STREAM_AUDIO:
      case GAVL_STREAM_VIDEO:
      case GAVL_STREAM_OVERLAY:
      case GAVL_STREAM_TEXT:
        track->streams[i]->action = BGAV_STREAM_INIT;
        break;
      case GAVL_STREAM_MSG:
      case GAVL_STREAM_NONE:
        break;
      }
    }
    
  bgav_track_foreach(track, init_stream_read, NULL);

  /* Set all streams back to mute mode */
  for(i = 0; i < track->num_streams; i++)
    {
    switch(track->streams[i]->type)
      {
      case GAVL_STREAM_AUDIO:
      case GAVL_STREAM_VIDEO:
      case GAVL_STREAM_OVERLAY:
      case GAVL_STREAM_TEXT:
        track->streams[i]->action = BGAV_STREAM_MUTE;
        break;
      case GAVL_STREAM_MSG:
      case GAVL_STREAM_NONE:
        break;
      }
    }
  }

void bgav_track_resync(bgav_track_t * track)
  {
  int i;
  bgav_stream_t * s;
  for(i = 0; i < track->num_audio_streams; i++)
    {
    s = bgav_track_get_audio_stream(track, i);
    
    if(s->action == BGAV_STREAM_MUTE)
      continue;
    
    bgav_audio_resync(s);
    }
  for(i = 0; i < track->num_video_streams; i++)
    {
    s = bgav_track_get_video_stream(track, i);
    
    if(s->action == BGAV_STREAM_MUTE)
      continue;
    bgav_video_resync(s);
    }
  }

int bgav_track_skipto(bgav_track_t * track, int64_t * time, int scale)
  {
  int i;
  bgav_stream_t * s;
  int64_t t;
  
  for(i = 0; i < track->num_video_streams; i++)
    {
    t = *time;
    s = bgav_track_get_video_stream(track, i);
    
    if(!bgav_stream_skipto(s, &t, scale))
      {
      return 0;
      }
    if(!i)
      *time = t;
    }
  for(i = 0; i < track->num_audio_streams; i++)
    {
    s = bgav_track_get_audio_stream(track, i);

    if(!bgav_stream_skipto(s, time, scale))
      {
      return 0;
      }
    }
  return 1;
  }



int bgav_track_has_sync(bgav_track_t * t)
  {
  int i;
  bgav_stream_t * s;
  
  for(i = 0; i < t->num_audio_streams; i++)
    {
    s = bgav_track_get_audio_stream(t, i);
    if((s->action != BGAV_STREAM_MUTE) &&
       !STREAM_HAS_SYNC(s))
      return 0;
    }
  for(i = 0; i < t->num_video_streams; i++)
    {
    s = bgav_track_get_video_stream(t, i);
    if(!STREAM_IS_STILL(s) && (s->action != BGAV_STREAM_MUTE) &&
       !STREAM_HAS_SYNC(s))
      return 0;
    }
  return 1;
  }

static int mute_stream(void * priv, bgav_stream_t * s)
  {
  if(s->type == GAVL_STREAM_MSG)
    return 1;
  
  s->action = BGAV_STREAM_MUTE;
  return 1;
  }

void bgav_track_mute(bgav_track_t * t)
  {
  bgav_track_foreach(t, mute_stream, NULL);
  }

static int check_sync_time(bgav_stream_t * s, int64_t * t)
  {
  int64_t tt;
  gavl_packet_t * p = NULL;

  int scale = 0;
  
  if((s->action == BGAV_STREAM_MUTE) ||
     (s->flags & STREAM_DISCONT))
    return 1;
  
  if(bgav_stream_peek_packet_read(s, &p) != GAVL_SOURCE_OK)
    return 0;

  if(!gavl_dictionary_get_int(s->m, GAVL_META_STREAM_SAMPLE_TIMESCALE, &scale) ||
     (scale <= 0))
    return 1;
  
  tt = gavl_time_unscale(scale, p->pts);
  if(tt > *t)
    *t = tt;
  
  return 1;
  }

int64_t bgav_track_sync_time(bgav_track_t * t, int scale)
  {
  int64_t ret = GAVL_TIME_UNDEFINED;
  bgav_stream_t * s;
  int i;
  
  for(i = 0; i < t->num_audio_streams; i++)
    {
    s = bgav_track_get_audio_stream(t, i);

    if(!check_sync_time(s, &ret))
      return GAVL_TIME_UNDEFINED;
    }
  for(i = 0; i < t->num_video_streams; i++)
    {
    s = bgav_track_get_video_stream(t, i);
    if(!check_sync_time(s, &ret))
      return GAVL_TIME_UNDEFINED;
    }
  return gavl_time_scale(scale, ret);
  }

static int check_out_time(bgav_stream_t * s, int64_t * t, int scale,
                          int stream_scale)
  {
  int64_t tt;
  if((s->action == BGAV_STREAM_MUTE) ||
     STREAM_IS_STILL(s))
    return 1;
  
  if(!STREAM_HAS_SYNC(s))
    return 0;

  tt = gavl_time_rescale(s->timescale, scale, STREAM_GET_SYNC(s));
  if(tt > *t)
    *t = tt;
  return 1;
  }


int64_t bgav_track_out_time(bgav_track_t * t, int scale)
  {
  int64_t ret = GAVL_TIME_UNDEFINED;
  bgav_stream_t * s;
  int i;
  
  for(i = 0; i < t->num_audio_streams; i++)
    {
    s = bgav_track_get_audio_stream(t, i);
    if(!check_out_time(s, &ret, scale, s->data.audio.format->samplerate))
      return GAVL_TIME_UNDEFINED;
    }
  for(i = 0; i < t->num_video_streams; i++)
    {
    s = bgav_track_get_video_stream(t, i);
    if(!check_out_time(s, &ret, scale, s->data.video.format->timescale))
      return GAVL_TIME_UNDEFINED;
    }
  return ret;
  }

static int set_eof_d(void * priv, bgav_stream_t * s)
  {
  s->flags |= STREAM_EOF_D;
  bgav_stream_flush(s);
  return 1;
  }

static int clear_eof_d(void * priv, bgav_stream_t * s)
  {
  s->flags &= ~STREAM_EOF_D;
  return 1;
  }

static int has_eof_d(void * priv, bgav_stream_t * s)
  {
  if((s->action != BGAV_STREAM_MUTE) && !(s->flags & STREAM_EOF_D))
    return 0;
  return 1;
  }


void bgav_track_set_eof_d(bgav_track_t * t)
  {
  bgav_track_foreach(t, set_eof_d, NULL);
  }

void bgav_track_clear_eof_d(bgav_track_t * t)
  {
  bgav_track_foreach(t, clear_eof_d, NULL);
  }

int bgav_track_eof_d(bgav_track_t * t)
  {
  return bgav_track_foreach(t, has_eof_d, NULL);
  }


void bgav_track_compute_info(bgav_track_t * t)
  {
  int i;  
  bgav_stream_t * s;

  for(i = 0; i < t->num_audio_streams; i++)
    {
    s = bgav_track_get_audio_stream(t, i);
    gavl_stream_stats_apply_audio(&s->stats, s->data.audio.format,
                                  s->ci, s->m);

    if(!s->timescale)
      s->timescale = s->data.audio.format->samplerate;
    
    gavl_dictionary_set_int(s->m, GAVL_META_STREAM_PACKET_TIMESCALE, s->timescale);

    if(s->ci->flags & GAVL_COMPRESSION_SBR)
      gavl_dictionary_set_int(s->m, GAVL_META_STREAM_SAMPLE_TIMESCALE, s->data.audio.format->samplerate/2);
    else
      gavl_dictionary_set_int(s->m, GAVL_META_STREAM_SAMPLE_TIMESCALE, s->data.audio.format->samplerate);
    }
  for(i = 0; i < t->num_video_streams; i++)
    {
    s = bgav_track_get_video_stream(t, i);
    gavl_stream_stats_apply_video(&s->stats, s->data.video.format,
                                  s->ci, s->m);
    gavl_dictionary_set_int(s->m, GAVL_META_STREAM_PACKET_TIMESCALE, s->timescale);
    gavl_dictionary_set_int(s->m, GAVL_META_STREAM_SAMPLE_TIMESCALE, s->data.video.format->timescale);
    }
  
  for(i = 0; i < t->num_text_streams; i++)
    {
    s = bgav_track_get_text_stream(t, i);
    gavl_stream_stats_apply_subtitle(&s->stats, s->m);

    gavl_dictionary_set_int(s->m, GAVL_META_STREAM_PACKET_TIMESCALE, s->timescale);
    gavl_dictionary_set_int(s->m, GAVL_META_STREAM_SAMPLE_TIMESCALE, s->timescale);
    }

  for(i = 0; i < t->num_overlay_streams; i++)
    {
    s = bgav_track_get_overlay_stream(t, i);

    gavl_stream_stats_apply_subtitle(&s->stats, s->m);
    
    gavl_dictionary_set_int(s->m, GAVL_META_STREAM_PACKET_TIMESCALE, s->timescale);
    gavl_dictionary_set_int(s->m, GAVL_META_STREAM_SAMPLE_TIMESCALE, s->data.video.format->timescale);
    }

  /* Set src array */

  //  mimetype = gavl_dictionary_get_string(t->m, GAVL_META_MIMETYPE);
  
  gavl_track_finalize(t->info);

  //  fprintf(stderr, "Computed track info:\n");
  //  gavl_dictionary_dump(t->info, 2);

  }

void bgav_track_export_infos(bgav_track_t * t)
  {
  int i;
  for(i = 0; i < t->num_streams; i++)
    {
    gavl_dictionary_reset(t->streams[i]->info_ext);
    gavl_dictionary_copy(t->streams[i]->info_ext, &t->streams[i]->in_info);
    gavl_stream_set_stats(t->streams[i]->info_ext, &t->streams[i]->stats);
    gavl_stream_set_compression_info(t->streams[i]->info_ext, t->streams[i]->ci);
    }
  }

int bgav_track_num_media_streams(bgav_track_t * t)
  {
  int ret = 0;
  int i;
  for(i = 0; i < t->num_streams; i++)
    {
    if((t->streams[i]->type == GAVL_STREAM_AUDIO) ||
       (t->streams[i]->type == GAVL_STREAM_VIDEO) ||
       (t->streams[i]->type == GAVL_STREAM_TEXT) ||
       (t->streams[i]->type == GAVL_STREAM_OVERLAY))
      ret++;
    }
  return ret;
  }

static const char * track_priv = "$avdec";

const gavl_dictionary_t * bgav_track_get_priv(const bgav_track_t * t)
  {
  return gavl_dictionary_get_dictionary(t->info, track_priv);
  }

gavl_dictionary_t * bgav_track_get_priv_nc(bgav_track_t * t)
  {
  return gavl_dictionary_get_dictionary_create(t->info, track_priv);
  }
