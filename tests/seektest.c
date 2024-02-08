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

#include <avdec.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gavl/log.h>

static int track = 0;
static int audio_stream = 0;
static int video_stream = 0;

static int64_t audio_position = -1;
static int64_t video_position = -1;

static int sample_accurate = 0;

bgav_options_t * opt;

static bgav_t * open_common(const char * filename)
  {
  bgav_t * b;
  bgav_options_t * opt1;
  int num_tracks;
  
  b = bgav_create();
  opt1 = bgav_get_options(b);

  bgav_options_copy(opt1, opt);
  
  if(!bgav_open(b, filename))
    goto fail;
  
  if(sample_accurate &&
     !bgav_can_seek_sample(b))
    {
    fprintf(stderr, "%s supports no sample accuracy\n", filename);
    goto fail;
    }

  num_tracks = bgav_num_tracks(b);

  if(track >= num_tracks)
    {
    fprintf(stderr, "requested track %d, but file has just %d tracks\n",
            track+1, num_tracks);
    goto fail;
    }
  if(track < 0)
    {
    fprintf(stderr, "Negative track\n");
    goto fail;
    }
  bgav_select_track(b, track);
  return b;
  
  fail:

  if(b)
    bgav_close(b);
  return NULL;
  }

static bgav_t * open_audio(const char * filename)
  {
  bgav_t * b = open_common(filename);
  
  if(!b)
    goto fail;

  bgav_set_audio_stream(b, audio_stream, BGAV_STREAM_DECODE);
  if(!bgav_start(b))
    goto fail;

  return b;
  
  fail:

  if(b)
    bgav_close(b);
  return NULL;
  }

static bgav_t * open_video(const char * filename)
  {
  bgav_t * b = open_common(filename);
  
  if(!b)
    goto fail;

  bgav_set_video_stream(b, video_stream, BGAV_STREAM_DECODE);
  if(!bgav_start(b))
    goto fail;

  return b;
  
  fail:

  if(b)
    bgav_close(b);
  return NULL;
  
  }

#define SAMPLES_TO_READ (10*1024)

static int test_audio(const char * filename)
  {
  const gavl_audio_format_t * format_orig;
  gavl_audio_format_t format;
  
  gavl_audio_frame_t * f1 = NULL, *f2 = NULL;
  int result;
  int64_t samples_to_skip;
  int64_t samples_to_read;
  bgav_t * b = open_audio(filename);

  int ret = 0;
  
  if(!b)
    goto fail;
  
  format_orig = bgav_get_audio_format(b, audio_stream);
  gavl_audio_format_copy(&format, format_orig);
  format.samples_per_frame = SAMPLES_TO_READ;
  
  f1 = gavl_audio_frame_create(&format);
  f2 = gavl_audio_frame_create(&format);
  
  /* Skip to the requested sample */
  
  samples_to_skip = audio_position -
    bgav_audio_start_time(b, audio_stream);

  if(samples_to_skip < 0)
    {
    fprintf(stderr, "Audio position must be larger than %"PRId64"\n",
            bgav_audio_start_time(b, audio_stream));
    goto fail;
    }

  if(samples_to_skip)
    fprintf(stderr, "Skipping %"PRId64" samples\n", samples_to_skip);
  
  while(samples_to_skip)
    {
    samples_to_read = SAMPLES_TO_READ;
    if(samples_to_read > samples_to_skip)
      samples_to_read = samples_to_skip;
    
    result = bgav_read_audio(b, f1, audio_stream, samples_to_read);
    
    if(!result)
      {
      fprintf(stderr, "EOF while skipping to sample position\n");
      goto fail;
      }
    samples_to_skip -= result;
    }
  /* Read first frame */
  fprintf(stderr, "Decoding frame 1\n");
  result = bgav_read_audio(b, f1, audio_stream, SAMPLES_TO_READ);
  fprintf(stderr, "Decoded frame 1: %d samples, pts: %" PRId64 "\n",
          result, f1->timestamp);
  
  /* Close */
  bgav_close(b);

  /* Reopen and seek */
  b = open_audio(filename);

  if(!b)
    goto fail;

  samples_to_skip =  audio_position;

  bgav_seek_scaled(b, &samples_to_skip, format.samplerate);
  
  fprintf(stderr, "Decoding frame 2\n");
  result = bgav_read_audio(b, f2, audio_stream, SAMPLES_TO_READ);
  fprintf(stderr, "Decoded frame 2: %d samples, pts: %" PRId64 "\n",
          result, f2->timestamp);

  fprintf(stderr, "Testing if audio timestamps are equal:   ");

  if(f1->timestamp == f2->timestamp)
    fprintf(stderr, "[  ok  ]\n");
  else
    fprintf(stderr, "[ fail ]\n");
  
  fprintf(stderr, "Testing if audio frames are equal:       ");
  
  if(gavl_audio_frames_equal(&format, f1, f2))
    fprintf(stderr, "[  ok  ]\n");
  else
    {
    fprintf(stderr, "[ fail ]\n");
    fprintf(stderr, "Saving frames...");
    gavl_audio_frame_plot(&format, f1, "frame_1");
    gavl_audio_frame_plot(&format, f2, "frame_2");
    fprintf(stderr, "Done\n");
    }
  fail:
  if(b)
    bgav_close(b);
  if(f1)
    gavl_audio_frame_destroy(f1);
  if(f2)
    gavl_audio_frame_destroy(f2);

  return ret;
  
  }

static int test_video(const char * filename)
  {
  const gavl_video_format_t * format_orig;
  gavl_video_format_t format;
  
  gavl_video_frame_t * f1, *f2;
  int result;
  int64_t frames_to_skip;
  int64_t video_time;
  
  bgav_t * b = open_video(filename);
  int ret = 0;

  if(!b)
    goto fail;
  
  format_orig = bgav_get_video_format(b, video_stream);
  gavl_video_format_copy(&format, format_orig);
  
  f1 = gavl_video_frame_create(&format);
  f2 = gavl_video_frame_create(&format);
  
  /* Skip to the requested sample */
  
  frames_to_skip =  video_position;
  
  while(frames_to_skip)
    {
    result = bgav_read_video(b, f1, video_stream);
    if(!result)
      {
      fprintf(stderr, "EOF while skipping to frame position\n");
      goto fail;
      }
    frames_to_skip--;
    }
  
  /* Read first frame */
  result = bgav_read_video(b, f1, video_stream);
  fprintf(stderr, "Decoded frame 1: pts: %" PRId64 ", duration: %" PRId64 "\n",
          f1->timestamp, f1->duration);
  
  /* Close */
  bgav_close(b);

  /* Reopen and seek */
  b = open_video(filename);

  if(!b)
    goto fail;

  video_time = f1->timestamp;
  
  bgav_seek_scaled(b, &video_time, format.timescale);

  /* Read second frame */
  result = bgav_read_video(b, f2, video_stream);

  if(!result)
    {
    fprintf(stderr, "EOF after seeking to frame position\n");
    goto fail;
    }
  
  fprintf(stderr, "Decoded frame 2: pts: %" PRId64 ", duration: %" PRId64 "\n",
          f2->timestamp, f2->duration);

  fprintf(stderr, "Testing if video timestamps are equal:   ");

  if(f1->timestamp == f2->timestamp)
    fprintf(stderr, "[  ok  ]\n");
  else
    fprintf(stderr, "[ fail ]\n");
  
  fprintf(stderr, "Testing if video are equal:              ");

  if(gavl_video_frames_equal(&format, f1, f2))
    fprintf(stderr, "[  ok  ]\n");
  else
    {
    fprintf(stderr, "[ fail ]\n");
    }
  fail:
  if(b)
    bgav_close(b);
  if(f1)
    gavl_video_frame_destroy(f1);
  if(f2)
    gavl_video_frame_destroy(f2);
  
  return ret;
  }

#if 0
static void test_subtitle(bgav_t * b, int stream)
  {
  
  }
#endif

int main(int argc, char ** argv)
  {
  int i;
  char * filename = NULL;
  
  opt = bgav_options_create();

  /* Disable dynamic range control, which destroys bit-exactness */
  bgav_options_set_audio_dynrange(opt, 0);
  
  i = 1;
  while(i < argc)
    {
    if(!strcmp(argv[i], "-atime"))
      {
      audio_position = strtoll(argv[i+1], NULL, 10);
      i++;
      }
    else if(!strcmp(argv[i], "-vpos"))
      {
      video_position = strtoll(argv[i+1], NULL, 10);
      i++;
      }
    else if(!strcmp(argv[i], "-sa"))
      {
      bgav_options_set_sample_accurate(opt, 1);
      sample_accurate = 1;
      }
    else if(!strcmp(argv[i], "-as"))
      {
      audio_stream = atoi(argv[i+1]);
      i++;
      }
    else if(!strcmp(argv[i], "-vs"))
      {
      video_stream = atoi(argv[i+1]);
      i++;
      }
    else if(!strcmp(argv[i], "-t"))
      {
      track = atoi(argv[i+1])-1;
      i++;
      }
    else if(!strcmp(argv[i], "-v"))
      {
      int level_int;
      level_int = atoi(argv[i+1]);

      gavl_set_log_verbose(level_int);
      i++;
      }
    else
      {
      filename = argv[i];
      }
    i++;
    }

  if(!filename)
    {
    fprintf(stderr, "Usage: %s [options] filename", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "-atime <sample>  Audio position\n");
    fprintf(stderr, "-vpos  <number>  Video frame number\n");
    fprintf(stderr, "-sa             Sample accurate\n");
    fprintf(stderr, "-as <stream>    Select audio stream\n");
    fprintf(stderr, "-vs <stream>    Select video stream\n");
    fprintf(stderr, "-t <track>      Select track\n");
    fprintf(stderr, "-v <level>      Verbosity level\n");
    return 1;
    }

  if(audio_position >= 0)
    test_audio(filename);

  if(video_position >= 0)
    test_video(filename);

  bgav_options_destroy(opt);

  return 0;
  }
