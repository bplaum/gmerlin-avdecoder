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

#include <stdlib.h>
#include <stdio.h>
#include <gavl/state.h>

#define LOG_DOMAIN "core"

static bgav_input_context_t * create_input(bgav_t * b)
  {
  bgav_input_context_t * ret;
   
  ret = bgav_input_create(b, NULL);
  ret->b = b;
  return ret;
  }

int bgav_pause(bgav_t * bgav)
  {
  /* Close seekable network connection */

  if(bgav->input->input->pause)
    bgav->input->input->pause(bgav->input);

  return 1;
  }

int bgav_resume(bgav_t * bgav)
  {
  /* Resume seekable network connection */
  
  if(bgav->input->input->resume)
    bgav->input->input->resume(bgav->input);
  
  return 1;
  
  }


static bgav_demuxer_context_t *
create_demuxer(bgav_t * b, const bgav_demuxer_t * demuxer)
  {
  bgav_demuxer_context_t * ret;
  
  ret = bgav_demuxer_create(b, demuxer, NULL);
  return ret;
  }

/* After calling this function, the stream array must not be changed anymore */
   
int bgav_init(bgav_t * ret)
  {
  int i, j;
  int is_redirector = 0;
  const bgav_demuxer_t * demuxer = NULL;
  const bgav_redirector_t * redirector = NULL;
  
  bgav_subtitle_reader_context_t * subreader, * subreaders;
  
  /*
   *  If the input already has it's track table,
   *  we can stop here
   */
  if(ret->input->tt)
    {
    ret->tt = ret->input->tt;
    bgav_track_table_ref(ret->tt);

    ret->demuxer = ret->input->demuxer;

    if(ret->demuxer)
      {
      ret->demuxer->tt = ret->input->tt;
      bgav_track_table_ref(ret->demuxer->tt);
      }
    if(ret->tt->num_tracks > 1)
      {
      bgav_track_table_remove_unsupported(ret->tt);
      bgav_track_table_merge_metadata(ret->tt, &ret->input->m);
      goto done;
      }
    }
  /*
   *  Autodetect the format and fire up the demuxer
   */
  if(!ret->input->demuxer)
    {
    /* First, we try the redirector, because we never need to
       skip bytes for them. */

    if((redirector = bgav_redirector_probe(ret->input)))
      {
      if(!(ret->tt = redirector->parse(ret->input)))
        goto fail;
      else
        {
        is_redirector = 1;
        bgav_track_table_merge_metadata(ret->tt, &ret->input->m);
        goto done;
        }
      }
    /* Check for ID3V2 tags here, they can be prepended to
       many different file types */
    
    if(bgav_id3v2_probe(ret->input))
      ret->input->id3v2 = bgav_id3v2_read(ret->input);
    
    demuxer = bgav_demuxer_probe(ret->input);
    
    if(demuxer)
      {
      ret->demuxer = create_demuxer(ret, demuxer);
      if(!bgav_demuxer_start(ret->demuxer))
        goto fail;
      
      if(bgav_is_redirector(ret))
        {
        bgav_track_table_merge_metadata(ret->tt, &ret->input->m);
        goto done;
        }
      }
    if(!ret->demuxer)
      goto fail;
    }
  else
    {
    ret->demuxer = ret->input->demuxer;
    bgav_track_table_merge_metadata(ret->tt, &ret->input->m);
    }
  
  ret->tt = ret->demuxer->tt;

  if(ret->tt)
    {
    bgav_track_table_ref(ret->tt);
    bgav_track_table_remove_unsupported(ret->tt);
    bgav_track_table_merge_metadata(ret->tt, &ret->input->m);
    
    /* Check for subtitle file */
    
    if(ret->tt->cur && ret->opt.seek_subtitles &&
       (ret->tt->cur->num_video_streams >= 1))
      {
      subreaders = bgav_subtitle_reader_open(ret->input);
      
      subreader = subreaders;
      while(subreader)
        {
        bgav_track_attach_subtitle_reader(ret->tt->cur,
                                          &ret->opt, subreader);
        subreader = subreader->next;
        }
      }
    }
  
  if(!ret->input->tt)
    {
    ret->input->tt = ret->tt;
    bgav_track_table_ref(ret->input->tt);
    }

  done:
  
  if(is_redirector)
    return 1;
  
  /* Add message streams */
  
  bgav_track_table_create_message_streams(ret->tt, &ret->opt);

  bgav_check_sample_accurate(ret);
  
  if(bgav_can_seek(ret) || 
     bgav_can_pause(ret))
    {
    for(i = 0; i < ret->tt->num_tracks; i++)
      {
      if(bgav_can_seek(ret))
        gavl_dictionary_set_int(ret->tt->tracks[i]->metadata, GAVL_META_CAN_SEEK, 1);
      if(bgav_can_pause(ret))
        gavl_dictionary_set_int(ret->tt->tracks[i]->metadata, GAVL_META_CAN_PAUSE, 1);
      }
    }

  /* Get compression infos */

  for(i = 0; i < ret->tt->num_tracks; i++)
    {
    bgav_stream_t * s;
    
    bgav_select_track(ret, i);
    
    for(j = 0; j < ret->tt->cur->num_audio_streams; j++)
      {
      s = bgav_track_get_audio_stream(ret->tt->cur, j);
      bgav_get_audio_compression_info(ret, j, NULL);
      gavl_stream_set_compression_info(s->info, s->ci);
      }
    for(j = 0; j < ret->tt->cur->num_video_streams; j++)
      {
      s = bgav_track_get_video_stream(ret->tt->cur, j);
      bgav_get_video_compression_info(ret, j, NULL);
      gavl_stream_set_compression_info(s->info, s->ci);
      }
    for(j = 0; j < ret->tt->cur->num_overlay_streams; j++)
      {
      s = bgav_track_get_overlay_stream(ret->tt->cur, j);
      bgav_get_overlay_compression_info(ret, j, NULL);
      gavl_stream_set_compression_info(s->info, s->ci);
      }
    
    }
  
  return 1;
    
  fail:

  if(!demuxer && !redirector)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Cannot detect stream type");
    }
  
  if(ret->demuxer)
    {
    bgav_demuxer_destroy(ret->demuxer);
    ret->demuxer = NULL;
    }
  return 0;
  }

int bgav_num_tracks(bgav_t * b)
  {
  if(b->tt)
    return b->tt->num_tracks;
  else
    return 0;
  }

bgav_t * bgav_create()
  {
  bgav_t * ret;
  bgav_translation_init();
  ret = calloc(1, sizeof(*ret));

  bgav_options_set_defaults(&ret->opt);
  
  return ret;
  }

int bgav_open(bgav_t * ret, const char * location)
  {
  bgav_codecs_init(&ret->opt);
  ret->input = create_input(ret);

  /* Create global metadata */
  
  if(!bgav_input_open(ret->input, location))
    goto fail;
  if(!bgav_init(ret))
    goto fail;

  ret->location = gavl_strdup(location);

  if(ret->demuxer &&
     (ret->demuxer->flags & BGAV_DEMUXER_SEEK_ITERATIVE) &&
     (ret->input->flags & BGAV_INPUT_SEEK_SLOW))
    ret->demuxer->flags &= ~BGAV_DEMUXER_CAN_SEEK;
  
  /* Check for file index */
  if((ret->opt.sample_accurate == 1) ||
     ((ret->opt.sample_accurate == 2) &&
      ret->demuxer &&
      !(ret->demuxer->flags & BGAV_DEMUXER_CAN_SEEK)))
    bgav_set_sample_accurate(ret);
  
  bgav_track_table_compute_info(ret->tt);
  
  
  return 1;
  fail:

  if(ret->input)
    {
    bgav_input_close(ret->input);
    free(ret->input);
    ret->input = NULL;
    }
  return 0;
  }

void bgav_close(bgav_t * b)
  {
  if(b->location)
    free(b->location);
  
  if(b->flags & BGAV_FLAG_IS_RUNNING)
    {
    bgav_track_stop(b->tt->cur);
    b->flags &= ~BGAV_FLAG_IS_RUNNING;
    }
  if(b->tt)
    bgav_track_table_unref(b->tt);
  
  if(b->demuxer)
    bgav_demuxer_destroy(b->demuxer);

  if(b->input)
    {
    bgav_input_close(b->input);
    free(b->input);
    }

  bgav_options_free(&b->opt);

  gavl_dictionary_free(&b->state);
  
  free(b);
  }


gavl_dictionary_t * bgav_get_media_info(bgav_t * bgav)
  {
  return &bgav->tt->info;
  }

void bgav_dump(bgav_t * bgav)
  {
  bgav_track_dump(bgav, bgav->tt->cur);
  }

gavl_time_t bgav_get_duration(bgav_t * bgav, int track)
  {
  return gavl_track_get_duration(bgav->tt->tracks[track]->info);
  }

const char * bgav_get_track_name(bgav_t * b, int track)
  {
  return gavl_dictionary_get_string(b->tt->tracks[track]->metadata, GAVL_META_LABEL);
  }

void bgav_stop(bgav_t * b)
  {
  bgav_track_stop(b->tt->cur);
  b->flags &= ~BGAV_FLAG_IS_RUNNING;
  }

static void set_stream_demuxer(bgav_stream_t * s,
                               bgav_demuxer_context_t * demuxer)
  {
  if(s->flags & STREAM_HAS_SUBREADER)
    {
    s->src.data = s->data.subtitle.subreader;
    s->src.get_func = bgav_subtitle_reader_read_packet;
    s->src.peek_func = bgav_subtitle_reader_peek_packet;
    }
  else
    {
    s->demuxer = demuxer;
    s->src.data = s;
    s->src.get_func = bgav_demuxer_get_packet_read;
    s->src.peek_func = bgav_demuxer_peek_packet_read;
    }

  }

static void set_stream_demuxers(bgav_track_t * t,
                                bgav_demuxer_context_t * demuxer)
  {
  int i;
  /* We must first set the demuxer of *all* streams
     before we initialize the parsers/decoders */
  
  for(i = 0; i < t->num_streams; i++)
    set_stream_demuxer(&t->streams[i], demuxer);
  }

int bgav_select_track(bgav_t * b, int track)
  {
  int was_running = 0;
  int reset_input = 0;
  int64_t data_start = -1;
  int i;
  
  if((track < 0) || (track >= b->tt->num_tracks))
    return 0;

  b->flags &= ~BGAV_FLAG_EOF;

  if(bgav_is_redirector(b))
    {
    if((track < 0) || (track >= b->tt->num_tracks))
      return 0;
    
    b->tt->cur = b->tt->tracks[track];
    return 1;
    }
  
  if(b->flags & BGAV_FLAG_IS_RUNNING)
    {
    bgav_track_stop(b->tt->cur);
    bgav_track_clear_eof_d(b->tt->cur);
    b->flags &= ~BGAV_FLAG_IS_RUNNING;
    was_running = 1;
    }
  
  if(b->input->input->select_track)
    {
    /* Input switches track, recreate the demuxer */

    bgav_demuxer_stop(b->demuxer);
    bgav_track_table_select_track(b->tt, track);

    /* Clear buffer */
    gavl_buffer_reset(&b->input->buf);
    
    if(!b->input->input->select_track(b->input, track))
      return 0;
    bgav_demuxer_start(b->demuxer);
    goto done;
    }

  if(b->demuxer)
    {
    if(b->demuxer->flags & BGAV_DEMUXER_HAS_DATA_START)
      {
      if(b->demuxer->data_start < b->input->position)
        {
        /* Some demuxers produce packets during initialization */
        // bgav_track_clear(b->tt->cur);
        
        if(b->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
          bgav_input_seek(b->input, b->demuxer->data_start, SEEK_SET);
        else
          {
          data_start = b->demuxer->data_start;
          reset_input = 1;
          //        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
          //                 "Cannot reset track when on a nonseekable source");
          //        return 0;
          }
        }
      else if(b->demuxer->data_start > b->input->position)
        bgav_input_skip(b->input, b->demuxer->data_start - b->input->position);
      
      }
    else if(b->demuxer->si)
      {
      b->demuxer->si->current_position = 0;
      
      if(b->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
        bgav_input_seek(b->input, b->demuxer->si->entries[0].offset, SEEK_SET);
      else
        {
        data_start = b->demuxer->si->entries[0].offset;
        reset_input = 1;
        //        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
        //                 "Cannot reset track when on a nonseekable source");
        //        return 0;
        }
      
      }
    
    // Enable this for inputs, which read sector based but have
    // no track selection

    //if(b->input->input->seek_sector)
    //  bgav_input_seek_sector(b->input, 0);

    if(!reset_input)
      {
      if(b->demuxer->si && was_running)
        {
        b->demuxer->si->current_position = 0;
        if(b->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
          bgav_input_seek(b->input, b->demuxer->si->entries[0].offset,
                          SEEK_SET);
        else
          {
          data_start = b->demuxer->si->entries[0].offset;
          reset_input = 1;
          //        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
          //                 "Cannot reset track when on a nonseekable source");
          //        return 0;
          }
        }
      }
    
    if(b->demuxer->demuxer->select_track)
      {
      if(b->tt->cur)
        bgav_track_clear(b->tt->cur);
      
      /* Demuxer switches track */
      bgav_track_table_select_track(b->tt, track);
      
      if(!b->demuxer->demuxer->select_track(b->demuxer, track))
        reset_input = 1;
      }
    
    if(reset_input)
      {
      /* Reset input. This will be done by closing and reopening the input */
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Reopening input due to track reset");

      
      if(!bgav_input_reopen(b->input))
        return 0;

      if(data_start >= 0)
        {
        bgav_input_skip(b->input, data_start);
        }
      }
    }
  
  /* If we have a file index for this track, switch to
     file index mode */

  if((b->tt->cur->flags & TRACK_HAS_FILE_INDEX) && !b->demuxer->si)
    {
    b->demuxer->demux_mode = DEMUX_MODE_FI;

    /* Index positions are -1 by default for the superindex */
    for(i = 0; i < b->tt->cur->num_streams; i++)
      b->tt->cur->streams[i].index_position = 0;
    }

  
  
  done:

  set_stream_demuxers(b->tt->cur, b->demuxer);
  
  return 1;
  }

int bgav_start(bgav_t * b)
  {
  /* Create buffers */
  //  bgav_input_buffer(b->input);

  if(b->demuxer)
    {
    if(!bgav_track_start(b->tt->cur, b->demuxer))
      return 0;
    }
  
  bgav_track_compute_info(b->tt->cur);

  b->flags |= BGAV_FLAG_IS_RUNNING;
  
  return 1;
  }

int bgav_can_seek(bgav_t * b)
  {
  return !!(b->demuxer) && !!(b->demuxer->flags & BGAV_DEMUXER_CAN_SEEK);
  }

const bgav_metadata_t * bgav_get_metadata(bgav_t*b, int track)
  {
  return b->tt->tracks[track]->metadata;
  }

const char * bgav_get_description(bgav_t * b)
  {
  return gavl_dictionary_get_string(b->tt->tracks[0]->metadata,
                           GAVL_META_FORMAT);
  }

bgav_options_t * bgav_get_options(bgav_t * b)
  {
  return &b->opt;
  }

const char * bgav_get_disc_name(bgav_t * bgav)
  {
  const gavl_dictionary_t * m;

  m = gavl_track_get_metadata(&bgav->tt->info);
  return gavl_dictionary_get_string(m, GAVL_META_DISK_NAME);
  }

const gavl_dictionary_t * bgav_get_edl(bgav_t * bgav)
  {
  return gavl_dictionary_get_dictionary(&bgav->tt->info, GAVL_META_EDL);
  }

int bgav_can_pause(bgav_t * bgav)
  {
  //  if(bgav->tt && (bgav->tt->cur->duration != GAVL_TIME_UNDEFINED))
  //    return 1;
  if(bgav->input && (bgav->input->flags & BGAV_INPUT_CAN_PAUSE))
    return 1;
  return 0;
  }

int bgav_set_stream_action_all(bgav_t * bgav, int idx, bgav_stream_action_t action)
  {
  if(!bgav->tt || !bgav->tt->cur || (idx < 0) || (idx >= bgav->tt->cur->num_streams))
    return 0;

  bgav->tt->cur->streams[idx].action = action;
  return 1;
  }

/* State */

static void state_foreach_func(void * priv, const char * var,
                               const gavl_value_t * val)
  {
  gavl_msg_t msg;
  bgav_stream_t * s = priv;
  
  gavl_msg_init(&msg);

  fprintf(stderr, "state_foreach_func %s\n", var);
  gavl_value_dump(val, 2);
  
  gavl_msg_set_state(&msg, GAVL_MSG_STATE_CHANGED, 1, GAVL_STATE_CTX_SRC, var, val);
  
  s->data.msg.msg_callback(s->data.msg.msg_callback_data, &msg);
  gavl_msg_free(&msg);

  }

void bgav_send_state(bgav_t * b)
  {
  bgav_stream_t * s;
  if(!b->tt || !(s = bgav_track_find_stream_all(b->tt->cur, GAVL_META_STREAM_ID_MSG_PROGRAM)) ||
     !s->data.msg.msg_callback)
    return;
  
  gavl_dictionary_foreach(&b->state, state_foreach_func, s);
  
  }
