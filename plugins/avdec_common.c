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



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <gmerlin/plugin.h>
#include <gmerlin/utils.h>
#include <gmerlin/log.h>
#include <avdec.h>
#include "avdec_common.h"
#include <gavl/metatags.h>

static int bg_avdec_start(void * priv);


static int handle_cmd(void * data, gavl_msg_t * msg)
  {
  avdec_priv * avdec = data;

  bg_avdec_lock(avdec);
  
  switch(msg->NS)
    {
    case GAVL_MSG_NS_SRC:
      switch(msg->ID)
        {
        case GAVL_CMD_SRC_SELECT_TRACK:
          {
          int track = gavl_msg_get_arg_int(msg, 0);
          gavl_dictionary_t * dict;
          //          bg_media_source_stream_t * st;

          //          fprintf(stderr, "Select track %d\n", track);
          
          if(!bgav_select_track(avdec->dec, track))
            {
            
            }
          bg_media_source_cleanup(&avdec->src);
          bg_media_source_init(&avdec->src);
          
          /* Build media source structure */
          if((dict = bgav_get_media_info(avdec->dec)) &&
             (dict = gavl_get_track_nc(dict, track)))
            bg_media_source_set_from_track(&avdec->src, dict);
          }
          break;
          
        case GAVL_CMD_SRC_START:
          bg_avdec_start(data);
          break;
        case GAVL_CMD_SRC_PAUSE:
          bgav_pause(avdec->dec);
          break;
        case GAVL_CMD_SRC_RESUME:
          bgav_resume(avdec->dec);
          break;
        case GAVL_CMD_SRC_SEEK:
          {
//          bg_msg_hub_t * hub;
          int64_t time = gavl_msg_get_arg_long(msg, 0);
          int scale = gavl_msg_get_arg_int(msg, 1);
          
          //    fprintf(stderr, "GAVL_CMD_SRC_SEEK\n");
          
          /* Seek */
          bgav_seek_scaled(avdec->dec, &time, scale);
          }
          break;
        case GAVL_CMD_SRC_SET_VIDEO_SKIP_MODE:
          {
          int stream;
          int mode;
          
          stream = gavl_msg_get_arg_int(msg, 0);
          mode = gavl_msg_get_arg_int(msg, 1);
          bgav_set_video_skip_mode(avdec->dec, stream, mode);
          }
        }
      
      break;
    }
  bg_avdec_unlock(avdec);
  return 1;
  }

bg_media_source_t * bg_avdec_get_src(void * priv)
  {
  avdec_priv * avdec = priv;
  return &avdec->src;
  }

void * bg_avdec_create()
  {
  avdec_priv * ret = calloc(1, sizeof(*ret));
  ret->opt = bgav_options_create();
  
  bg_controllable_init(&ret->ctrl,
                       bg_msg_sink_create(handle_cmd, ret, 1),
                       bg_msg_hub_create(1));

  //  bgav_options_set_msg_callback(ret->opt, handle_evt, ret);
  
  pthread_mutex_init(&ret->mutex, NULL);
  
  return ret;
  }

void bg_avdec_close(void * priv)
  {
  avdec_priv * avdec = priv;
  if(avdec->dec)
    {
    bgav_close(avdec->dec);
    avdec->dec = NULL;
    }
  }

bg_controllable_t *
bg_avdec_get_controllable(void * priv)
  {
  avdec_priv * avdec = priv;
  return &avdec->ctrl;
  }


void bg_avdec_destroy(void * priv)
  {
  avdec_priv * avdec = priv;
  bg_avdec_close(priv);

  if(avdec->dec)
    bgav_close(avdec->dec);
  if(avdec->opt)
    bgav_options_destroy(avdec->opt);
  
  bg_controllable_cleanup(&avdec->ctrl);
  bg_media_source_cleanup(&avdec->src);

  pthread_mutex_destroy(&avdec->mutex);
  
  free(avdec);
  }

gavl_dictionary_t * bg_avdec_get_media_info(void * p)
  {
  avdec_priv * avdec = p;
  
  // fprintf(stderr, "bg_avdec_get_media_info\n");
  //   gavl_dictionary_dump(bgav_get_media_info(avdec->dec), 2);
  
  return bgav_get_media_info(avdec->dec);
  }

void bg_avdec_skip_video(void * priv, int stream, int64_t * time,
                         int scale, int exact)
  {
  avdec_priv * avdec = priv;
  bgav_skip_video(avdec->dec, stream, time, scale, exact);
  }

int bg_avdec_has_still(void * priv,
                       int stream)
  {
  avdec_priv * avdec = priv;
  return bgav_video_has_still(avdec->dec, stream);
  }


int bg_avdec_read_audio(void * priv,
                            gavl_audio_frame_t * frame,
                            int stream,
                            int num_samples)
  {
  avdec_priv * avdec = priv;
  return bgav_read_audio(avdec->dec, frame, stream, num_samples);
  }

static bgav_stream_action_t get_stream_action(bg_stream_action_t action)
  {
  switch(action)
    {
    case BG_STREAM_ACTION_OFF:
      return BGAV_STREAM_MUTE;
      break;
    case BG_STREAM_ACTION_DECODE:
      return BGAV_STREAM_DECODE;
      break;
    case BG_STREAM_ACTION_READRAW:
      return BGAV_STREAM_READRAW;
      break;
    }
  return -1;
  }

static int handle_msg(void * priv, gavl_msg_t * msg)
  {
  bg_media_source_stream_t * st = priv;
  bg_msg_sink_put_copy(bg_msg_hub_get_sink(st->msghub), msg);
  return 1;
  }


static int bg_avdec_start(void * priv)
  {
  int i, num;
  avdec_priv * avdec = priv;
  bg_media_source_stream_t * st;

  if(!avdec->src.track)
    num = 0;
  else
    num = gavl_track_get_num_streams_all(avdec->src.track);
  
  if(!num) // Redirector
    return 1;
  
  for(i = 0; i < num; i++)
    bgav_set_stream_action_all(avdec->dec, i, get_stream_action(avdec->src.streams[i]->action));

  /* Connect msg stream. *must* be done before starting */

  st = bg_media_source_get_stream_by_id(&avdec->src, GAVL_META_STREAM_ID_MSG_PROGRAM);

  switch(st->action)
    {
    case BG_STREAM_ACTION_DECODE:
      st->msghub_priv = bg_msg_hub_create(1);
      st->msghub = st->msghub_priv;
        
      bgav_set_msg_callback_by_id(avdec->dec, GAVL_META_STREAM_ID_MSG_PROGRAM, handle_msg, st);
      
      /* Redirect to the central event sink */
      //      bg_msg_hub_connect_sink(st->msghub, avdec->ctrl.evt_sink);
        
      break;
    case BG_STREAM_ACTION_READRAW:
      break;
    case BG_STREAM_ACTION_OFF:
      break;
    }
  
  if(!bgav_start(avdec->dec))
    return 0;

#if 0  
  fprintf(stderr, "bg_avdec_start\n");
  gavl_dictionary_dump(avdec->src.track, 2);
#endif
  
  num = gavl_track_get_num_audio_streams(avdec->src.track);
  for(i = 0; i < num; i++)
    {
    st = bg_media_source_get_audio_stream(&avdec->src, i);

    switch(st->action)
      {
      case BG_STREAM_ACTION_DECODE:
        st->asrc = bgav_get_audio_source(avdec->dec, i);
        break;
      case BG_STREAM_ACTION_READRAW:
        st->psrc = bgav_get_audio_packet_source(avdec->dec, i);
        break;
      case BG_STREAM_ACTION_OFF:
        break;
      }
    
    }
  num = gavl_track_get_num_video_streams(avdec->src.track);
  for(i = 0; i < num; i++)
    {
    st = bg_media_source_get_video_stream(&avdec->src, i);

    switch(st->action)
      {
      case BG_STREAM_ACTION_DECODE:
        st->vsrc = bgav_get_video_source(avdec->dec, i);
        break;
      case BG_STREAM_ACTION_READRAW:
        st->psrc = bgav_get_video_packet_source(avdec->dec, i);
        break;
      case BG_STREAM_ACTION_OFF:
        break;
      }

    }
  num = gavl_track_get_num_text_streams(avdec->src.track);
  for(i = 0; i < num; i++)
    {
    st = bg_media_source_get_text_stream(&avdec->src, i);

    switch(st->action)
      {
      case BG_STREAM_ACTION_DECODE:
      case BG_STREAM_ACTION_READRAW:
        st->psrc = bgav_get_text_packet_source(avdec->dec, i);
        break;
      case BG_STREAM_ACTION_OFF:
        break;
      }
    }
  num = gavl_track_get_num_overlay_streams(avdec->src.track);
  for(i = 0; i < num; i++)
    {
    st = bg_media_source_get_overlay_stream(&avdec->src, i);

    switch(st->action)
      {
      case BG_STREAM_ACTION_DECODE:
        st->vsrc = bgav_get_overlay_source(avdec->dec, i);
        break;
      case BG_STREAM_ACTION_READRAW:
        st->psrc = bgav_get_overlay_packet_source(avdec->dec, i);
        break;
      case BG_STREAM_ACTION_OFF:
        break;
      }
    }

  /* Set lock functions */

  for(i = 0; i < avdec->src.num_streams; i++)
    {
    st = avdec->src.streams[i];
    
    if(st->asrc)
      gavl_audio_source_set_lock_funcs(st->asrc, bg_avdec_lock, bg_avdec_unlock, avdec);
    else if(st->vsrc)
      gavl_video_source_set_lock_funcs(st->vsrc, bg_avdec_lock, bg_avdec_unlock, avdec);
    else if(st->psrc)
      gavl_packet_source_set_lock_funcs(st->psrc, bg_avdec_lock, bg_avdec_unlock, avdec);
    }
  
#if 0  
  num = gavl_track_get_num_msg_streams(avdec->src.track);
  for(i = 0; i < num; i++)
    {
    }
#endif
  
  return 1;
  }

void
bg_avdec_set_parameter(void * p, const char * name,
                       const gavl_value_t * val)
  {
  avdec_priv * avdec = p;
  bg_avdec_option_set_parameter(avdec->opt, name, val);
  }

gavl_frame_table_t * bg_avdec_get_frame_table(void * priv, int stream)
  {
  avdec_priv * avdec = priv;
  return bgav_get_frame_table(avdec->dec, stream);
  }



const char * bg_avdec_get_disc_name(void * priv)
  {
  avdec_priv * avdec = priv;
  if(avdec->dec)
    return bgav_get_disc_name(avdec->dec);
  return NULL;
  }

void bg_avdec_lock(void * priv)
  {
  avdec_priv * avdec = priv;

  pthread_mutex_lock(&avdec->mutex);
  //  fprintf(stderr, "bg_avdec_lock %ld\n", pthread_self());
  }

void bg_avdec_unlock(void * priv)
  {
  avdec_priv * avdec = priv;
  
  // fprintf(stderr, "bg_avdec_unlock %ld\n", pthread_self());
  pthread_mutex_unlock(&avdec->mutex);
  
  }
