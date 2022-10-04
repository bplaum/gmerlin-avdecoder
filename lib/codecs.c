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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <config.h>
#include <codecs.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <utils.h>

// #define ENABLE_DEBUG

static void codecs_lock();
static void codecs_unlock();


/* Simple codec registry */

/*
 *  Since we link all codecs statically,
 *  this stays quite simple
 */

static bgav_audio_decoder_t * audio_decoders = NULL;
static bgav_video_decoder_t * video_decoders = NULL;

static int codecs_initialized = 0;

static int num_audio_codecs = 0;
static int num_video_codecs = 0;

static pthread_mutex_t codec_mutex = PTHREAD_MUTEX_INITIALIZER;

static void codecs_lock()
  {
  pthread_mutex_lock(&codec_mutex);
  }

static void codecs_unlock()
  {
  pthread_mutex_unlock(&codec_mutex);
  }


void bgav_codecs_dump()
  {
  bgav_audio_decoder_t * ad;
  bgav_video_decoder_t * vd;
  int i;
  bgav_codecs_init(NULL);
  
  /* Print */
  ad = audio_decoders;

  bgav_dprintf("<h2>Audio codecs</h2>\n");

  bgav_dprintf("<ul>\n");
  for(i = 0; i < num_audio_codecs; i++)
    {
    bgav_dprintf("<li>%s\n", ad->name);
    ad = ad->next;
    }
  bgav_dprintf("</ul>\n");
  
  bgav_dprintf("<h2>Video codecs</h2>\n");
  bgav_dprintf("<ul>\n");
  vd = video_decoders;
  for(i = 0; i < num_video_codecs; i++)
    {
    bgav_dprintf("<li>%s\n", vd->name);
    vd = vd->next;
    }
  bgav_dprintf("</ul>\n");
  
  }


void bgav_codecs_init(bgav_options_t * opt)
  {
  codecs_lock();
  if(codecs_initialized)
    {
    codecs_unlock();
    return;
    }
  codecs_initialized = 1;

#ifdef HAVE_MAD
  bgav_init_audio_decoders_mad();
#endif

#ifdef HAVE_V4L2
  bgav_init_video_decoders_v4l2();
#endif
  
  /* ffmpeg codecs should be initialized BEFORE any DLL codecs */
#ifdef HAVE_AVCODEC
  bgav_init_audio_decoders_ffmpeg(opt);
  bgav_init_video_decoders_ffmpeg(opt);
#endif
#ifdef HAVE_VORBIS
  bgav_init_audio_decoders_vorbis();
#endif

#ifdef HAVE_LIBA52
  bgav_init_audio_decoders_a52();
#endif

#ifdef HAVE_DCA
  bgav_init_audio_decoders_dca();
#endif


#ifdef HAVE_LIBPNG
  bgav_init_video_decoders_png();
#endif

#ifdef HAVE_OPENJPEG
  bgav_init_video_decoders_openjpeg();
#endif

#ifdef HAVE_LIBTIFF
  bgav_init_video_decoders_tiff();
#endif

#ifdef HAVE_THEORADEC
  bgav_init_video_decoders_theora();
#endif

#ifdef HAVE_SCHROEDINGER
  bgav_init_video_decoders_schroedinger();
#endif

#ifdef HAVE_SPEEX
  bgav_init_audio_decoders_speex();
#endif


#ifdef HAVE_FLAC
  bgav_init_audio_decoders_flac();
#endif

#ifdef HAVE_OPUS
  bgav_init_audio_decoders_opus();
#endif
  
  bgav_init_audio_decoders_pcm();
#ifdef HAVE_LIBGSM
  bgav_init_audio_decoders_gsm();
#endif
  bgav_init_video_decoders_aviraw();
  bgav_init_video_decoders_qtraw();
  bgav_init_video_decoders_yuv();
  bgav_init_video_decoders_y4m();
  bgav_init_video_decoders_tga();
  bgav_init_video_decoders_rtjpeg();
  bgav_init_video_decoders_dvdsub();
  
  codecs_unlock();
  
  }

void bgav_audio_decoder_register(bgav_audio_decoder_t * dec)
  {
  bgav_audio_decoder_t * before;
  if(!audio_decoders)
    audio_decoders = dec;
  else
    {
    before = audio_decoders;
    while(before->next)
      before = before->next;
    before->next = dec;
    }
  dec->next = NULL;
  num_audio_codecs++;
  }

void bgav_video_decoder_register(bgav_video_decoder_t * dec)
  {
  bgav_video_decoder_t * before;
  if(!video_decoders)
    video_decoders = dec;
  else
    {
    before = video_decoders;
    while(before->next)
      before = before->next;
    before->next = dec;
    }
  dec->next = NULL;
  num_video_codecs++;
  }

bgav_audio_decoder_t * bgav_find_audio_decoder(uint32_t fourcc)
  {
  bgav_audio_decoder_t * cur;
  int i;
  codecs_lock();
  cur = audio_decoders;
  //  if(!codecs_initialized)
  //    bgav_codecs_init();

  
  while(cur)
    {
    i = 0;
    while(cur->fourccs[i])
      {
      if(cur->fourccs[i] == fourcc)
        {
        codecs_unlock();
        return cur;
        }
      else
        i++;
      }
    cur = cur->next;
    }
  codecs_unlock();
  return NULL;
  }

bgav_video_decoder_t * bgav_find_video_decoder(uint32_t fourcc)
  {
  bgav_video_decoder_t * cur;
  int i;
  codecs_lock();

  //  if(!codecs_initialized)
  //    bgav_codecs_init();
  
  cur = video_decoders;

  while(cur)
    {
    i = 0;
    while(cur->fourccs[i])
      {
      if(cur->fourccs[i] == fourcc)
        {
        codecs_unlock();
        return cur;
        }
      else
        i++;
      }
    cur = cur->next;
    }
  codecs_unlock();
  return NULL;
  }

gavl_codec_id_t * bgav_supported_audio_compressions()
  {
  int i, num;
  gavl_codec_id_t id;
  uint32_t fourcc;
  gavl_codec_id_t * ret;
  int ret_num;
  
  bgav_codecs_init(NULL);

  num = gavl_num_compressions();
  ret = calloc(num, sizeof(*ret));

  ret_num = 0;
  
  for(i = 0; i < num; i++)
    {
    id = gavl_get_compression(i);
    fourcc = bgav_compression_id_2_fourcc(id);
    if(bgav_find_audio_decoder(fourcc))
      {
      ret[ret_num] = id;
      ret_num++;
      }
    }
  ret[ret_num] = GAVL_CODEC_ID_NONE;
  return ret;
  }

gavl_codec_id_t * bgav_supported_video_compressions()
  {
  int i, num;
  gavl_codec_id_t id;
  uint32_t fourcc;
  gavl_codec_id_t * ret;
  int ret_num;
  bgav_codecs_init(NULL);
  num = gavl_num_compressions();
  ret = calloc(num, sizeof(*ret));

  ret_num = 0;
  
  for(i = 0; i < num; i++)
    {
    id = gavl_get_compression(i);
    fourcc = bgav_compression_id_2_fourcc(id);
    if(bgav_find_video_decoder(fourcc))
      {
      ret[ret_num] = id;
      ret_num++;
      }
    }
  ret[ret_num] = GAVL_CODEC_ID_NONE;
  return ret;
  }


