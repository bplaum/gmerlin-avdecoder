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
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

#include "libw32dll/wine/ldt_keeper.h"

#include <win32codec.h>

/* Due to lots of crashes, the win32 codecs are now in a private thread */


#define STATE_NORMAL 0
#define STATE_QUIT   1
#define STATE_ERROR  2

static void * win32_thread(void * data)
  {
  bgav_win32_thread_t * t;
  t = data;

  t->ldt_fs = Setup_LDT_Keeper();
  
  /* Initialize codec */
  if(!t->init(t))
    {
    t->state = STATE_ERROR;
    Restore_LDT_Keeper(t->ldt_fs);
    sem_post(&t->output_ready);
    return NULL;
    }

  /* Tell, that we have initialized */
  sem_post(&t->output_ready);

  Check_FS_Segment();

  
  /* Go into the decode loop */
  
  while(1)
    {
    /* Wait for input semaphore */
    sem_wait(&t->input_ready);

    if(t->state == STATE_NORMAL)
      {
      /* Decode */
      
      if(!t->keyframe_seen)
        {
        if(t->keyframe)
          t->keyframe_seen = 1;
        else
          {
          sem_post(&t->output_ready);
          if(t->video_frame)
            gavl_video_frame_clear(t->video_frame, &t->s->data.video.format);
          continue;
          }
        }

      
      if(!t->decode(t))
        t->state = STATE_ERROR;
      sem_post(&t->output_ready);
      
      }
    else
      break;
    
    }
  
  Restore_LDT_Keeper(t->ldt_fs);
  return NULL;
  }


int bgav_win32_codec_thread_init(bgav_win32_thread_t * t,bgav_stream_t*s)
  {
  t->s = s;

  sem_init(&t->input_ready, 0, 0);
  sem_init(&t->output_ready, 0, 0);

  pthread_create(&t->thread, NULL, win32_thread, t);

  /* Wait until we have the stream format */

  sem_wait(&t->output_ready);

  if(t->state != STATE_NORMAL)
    return 0;
  else
    return 1;
  }

static int decode_common(bgav_win32_thread_t * t,
                          uint8_t * data,
                          int data_len)
  {
  t->data = data;
  t->data_len = data_len;

  sem_post(&t->input_ready);
  sem_wait(&t->output_ready);

  if(t->state != STATE_NORMAL)
    return 0;
  else
    return 1;
  }

int bgav_win32_codec_thread_decode_audio(bgav_win32_thread_t * t,
                                         gavl_audio_frame_t * f,
                                         uint8_t * data,
                                         int data_len)
  {
  t->audio_frame = f;
  return decode_common(t, data, data_len);
  }

int bgav_win32_codec_thread_decode_video(bgav_win32_thread_t * t,
                                         gavl_video_frame_t * f,
                                         uint8_t * data,
                                         int data_len, int keyframe)
  {
  t->video_frame = f;
  t->keyframe = keyframe;
  return decode_common(t, data, data_len);
  }

void bgav_win32_codec_thread_cleanup(bgav_win32_thread_t * t)
  {
  t->state = STATE_QUIT;
  
  pthread_join(t->thread, NULL);
  
  }
