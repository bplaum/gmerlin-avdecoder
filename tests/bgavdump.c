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



// #include <avdec.h>
#include <avdec_private.h>
#include <locale.h>

#include <utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <gavl/metatags.h>

#ifndef _WIN32
#include <termios.h> /* Ask passwords */
#endif

#define TRACK 12

/* Callback based reading: We do a simple stdio mapping here */

#ifdef _WIN32
#define BGAV_FSEEK(a,b,c) fseeko64(a,b,c)
#define BGAV_FTELL(a) ftello64(a)
#else
#ifdef HAVE_FSEEKO
#define BGAV_FSEEK fseeko
#else
#define BGAV_FSEEK fseek
#endif

#ifdef HAVE_FTELLO
#define BGAV_FTELL ftello
#else
#define BGAV_FSEEK ftell
#endif

#endif

static void index_callback(void * data, float perc)
  {
  fprintf(stdout, "Building index %.2f %% completed\r",
          perc * 100.0);
  fflush(stdout);
  }

static int read_callback(void * priv, uint8_t * data, int len)
  {
  FILE * f = (FILE*)priv;
  return fread(data, 1, len, f);
  }

static int64_t seek_callback(void * priv, int64_t pos, int whence)
  {
  FILE * f = (FILE*)priv;
  
  BGAV_FSEEK(f, pos, whence);
  return BGAV_FTELL(f);
  }

/* Taken from the Unix programmer FAQ: http://www.faqs.org/faqs/unix-faq/programmer/faq/ */

#ifndef _WIN32
static struct termios stored_settings;
static void echo_off(void)
  {
  struct termios new_settings;
  tcgetattr(0,&stored_settings);
  new_settings = stored_settings;
  new_settings.c_lflag &= (~ECHO);
  tcsetattr(0,TCSANOW,&new_settings);
  return;
  }

static void echo_on(void)
  {
  tcsetattr(0,TCSANOW,&stored_settings);
  return;
  }

/* Ok this is not so clean. But the library allows arbitrary string lengths */

char buf[1024];

static int user_pass_func(void * data, const char * resource, char ** user, char ** pass)
  {
  char * pos;
  fprintf(stderr, "Enter authentication for \"%s\"\n", resource);
  fprintf(stderr, "Username: ");

  fgets(buf, 1024, stdin);

  pos = strchr(buf, '\n');
  if(pos)
    *pos = '\0';
  if(buf[0] == '\0')
    return 0;
  *user = gavl_strndup(buf, NULL);
    
  fprintf(stderr, "Password: ");
  echo_off();
  fgets(buf, 1024, stdin);
  echo_on();
  fprintf(stderr, "\n");

  pos = strchr(buf, '\n');
  if(pos)
    *pos = '\0';
  if(buf[0] == '\0')
    return 0;
  *pass = gavl_strndup(buf, NULL);
  return 1;
  }
#endif

/* Configuration data */

static int connect_timeout   = 5000;
static int read_timeout      = 5000;
static int network_bandwidth = 524300; /* 524.3 Kbps (Cable/DSL) */
static int seek_subtitles    = 2;
static int frames_to_read    = 10;
static int do_audio          = 1;
static int do_video          = 1;
static int sample_accurate = 0;
static int vdpau = 0;
static int64_t audio_seek = -1;
static int64_t video_seek = -1;
static gavl_time_t global_seek = -1;
static int dump_ci = 0;
static int follow_redir= 0;


static void print_usage()
  {
  fprintf(stderr, "Usage: bgavdump [options] location\n");
  fprintf(stderr, "       bgavdump -L\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "-s                    Switch to sample accurate mode\n");
  fprintf(stderr, "-aseek <sample>       Seek to audio sample\n");
  fprintf(stderr, "-vseek <time>         Seek to video time\n");
  fprintf(stderr, "-seek <microseconds>  Do a global seek to a time\n");
  fprintf(stderr, "-na                   Disable audio\n");
  fprintf(stderr, "-nv                   Disable video\n");
  fprintf(stderr, "-nf <number>          Number of A/V frames to read\n");
  fprintf(stderr, "-v <level>            Verbosity level (0..4)\n");
  fprintf(stderr, "-ci                   Dump compression info\n");
  fprintf(stderr, "-vdpau                Try to use vdpau\n");
  fprintf(stderr, "-dh                   Dump headers of the file\n");
  fprintf(stderr, "-di                   Dump indices of the file\n");
  fprintf(stderr, "-dp                   Dump packets\n");
  fprintf(stderr, "-L                    List all demultiplexers and codecs\n");
  fprintf(stderr, "-follow               Follow redirections (e.g. in m3u files)\n");
  fprintf(stderr, "-t <num>              Dump track <num> (default: Dump all)\n");

  }

static void list_all()
  {
  bgav_inputs_dump();
  bgav_redirectors_dump();
  bgav_formats_dump();
  bgav_codecs_dump();
  //  bgav_subreaders_dump();
  }

static void dump_track(bgav_t * file, int track)
  {
  int num_audio_streams;
  int num_video_streams;
  int num_text_streams;
  int num_overlay_streams;

  gavl_audio_frame_t * af;
  gavl_video_frame_t * vf;
  const gavl_audio_format_t * audio_format;
  const gavl_video_format_t * video_format;

  gavl_compression_info_t ci;

  gavl_audio_source_t * asrc;
  gavl_video_source_t * vsrc;

  int i, j;
  char * sub_text = NULL;
  int sub_text_alloc = 0;
  gavl_time_t sub_time;
  gavl_time_t sub_duration;
  gavl_video_frame_t * ovl;
  
  bgav_select_track(file, track);
  if(sample_accurate && !bgav_can_seek_sample(file))
    {
    fprintf(stderr, "Sample accurate access not possible for track %d\n", track+1);
    return;
    }
  fprintf(stderr, "===================================\n");
  fprintf(stderr, "============ Track %3d ============\n", track+1);
  fprintf(stderr, "===================================\n");
    
  num_audio_streams = bgav_num_audio_streams(file, track);
  num_video_streams = bgav_num_video_streams(file, track);
  num_text_streams = bgav_num_text_streams(file, track);
  num_overlay_streams = bgav_num_overlay_streams(file, track);
    
  if(do_audio)
    {
    for(i = 0; i < num_audio_streams; i++)
      {
      if(dump_ci)
        {
        if(bgav_get_audio_compression_info(file, i, &ci))
          {
          fprintf(stderr, "Audio stream %d ", i+1);
          gavl_compression_info_dump(&ci);
          gavl_compression_info_free(&ci);
          audio_format = bgav_get_audio_format(file, i);
          fprintf(stderr, "Format (exported when reading compressed)\n");
          gavl_audio_format_dump(audio_format);
          }
        else
          fprintf(stderr, "Audio stream %d cannot output compressed packets\n",
                  i+1);
        }
      bgav_set_audio_stream(file, i, BGAV_STREAM_DECODE);
      }
    }
  if(do_video)
    {
    for(i = 0; i < num_video_streams; i++)
      {
      if(dump_ci)
        {
        if(bgav_get_video_compression_info(file, i, &ci))
          {
          fprintf(stderr, "Video stream %d ", i+1);
          gavl_compression_info_dump(&ci);
          gavl_compression_info_free(&ci);
          video_format = bgav_get_video_format(file, i);
          fprintf(stderr, "Format (exported when reading compressed)\n");
          gavl_video_format_dump(video_format);
          }
        else
          fprintf(stderr, "Video stream %d cannot output compressed packets\n",
                  i+1);
        }
      bgav_set_video_stream(file, i, BGAV_STREAM_DECODE);
      }
    }
  for(i = 0; i < num_text_streams; i++)
    {
    bgav_set_text_stream(file, i, BGAV_STREAM_DECODE);
    }
  for(i = 0; i < num_overlay_streams; i++)
    {
    bgav_set_overlay_stream(file, i, BGAV_STREAM_DECODE);

    if(dump_ci)
      {
      if(bgav_get_overlay_compression_info(file, i, &ci))
        {
        fprintf(stderr, "Subtitle stream %d ", i+1);
        gavl_compression_info_dump(&ci);
        gavl_compression_info_free(&ci);
        video_format = bgav_get_overlay_format(file, i);
        fprintf(stderr, "Format (exported when reading compressed)\n");
        gavl_video_format_dump(video_format);
        }
      else
        fprintf(stderr, "Subtitle stream %d cannot output compressed packets\n",
                i+1);
      }

    }


  fprintf(stderr, "Starting decoders...\n");
  if(!bgav_start(file))
    {
    fprintf(stderr, "Starting decoders failed\n");
#ifndef TRACK  
    continue;
#else
    return;
#endif
    }
  else
    fprintf(stderr, "Starting decoders done\n");

  if(global_seek >= 0)
    {
    if(bgav_can_seek(file))
      {
      fprintf(stderr, "Doing global seek to %"PRId64"...\n", global_seek);
      bgav_seek(file, &global_seek);
      fprintf(stderr, "Time after seek: %"PRId64"\n", global_seek);
      }
    else
      {
      fprintf(stderr, "Global seek requested for non-seekable file\n");
      return;
      }
    }
    
  fprintf(stderr, "Dumping file contents...\n");
  bgav_dump(file);
  fprintf(stderr, "End of file contents\n");

  /* Try to get some frames from each stream */
  if(do_audio)
    {
    for(i = 0; i < num_audio_streams; i++)
      {
      if(!(asrc = bgav_get_audio_source(file, i)))
        break;
        
      gavl_audio_source_set_dst(asrc, 0, NULL);
        
      audio_format = bgav_get_audio_format(file, i);
      // af = gavl_audio_frame_create(audio_format);
      if(sample_accurate && (audio_seek >= 0))
        bgav_seek_audio(file, i, audio_seek);

      for(j = 0; j < frames_to_read; j++)
        {
        fprintf(stderr, "Reading frame from audio stream %d...", i+1);
          
        af = NULL;
        if(gavl_audio_source_read_frame(asrc, &af) == GAVL_SOURCE_OK)
          fprintf(stderr, "Done, timestamp: %"PRId64" [%"PRId64"], Samples: %d\n",
                  af->timestamp, gavl_time_unscale(audio_format->samplerate, af->timestamp), af->valid_samples);
        else
          {
          fprintf(stderr, "Failed\n");
          break;
          }
        }
      }
    }

  if(do_video)
    {
    for(i = 0; i < num_video_streams; i++)
      {
      video_format = bgav_get_video_format(file, i);
#if 1
      if(!(vsrc = bgav_get_video_source(file, i)))
        break;
        
      gavl_video_source_set_dst(vsrc, 0, NULL);
        
      if(sample_accurate)
        {
        if(video_seek >= 0)
          bgav_seek_video(file, i, video_seek);
        }
      for(j = 0; j < frames_to_read; j++)
        {
        fprintf(stderr, "Reading frame from video stream %d...", i+1);
        vf = NULL;
        if(gavl_video_source_read_frame(vsrc, &vf) == GAVL_SOURCE_OK)
          {
          fprintf(stderr, "Done, timestamp: %"PRId64" [%"PRId64"]", vf->timestamp, gavl_time_unscale(video_format->timescale, vf->timestamp));

          if(vf->duration > 0)
            fprintf(stderr, ", Duration: %"PRId64, vf->duration);
          else
            fprintf(stderr, ", Duration: Unknown");

          if(gavl_interlace_mode_is_mixed(video_format->interlace_mode))
            {
            fprintf(stderr, ", IL: ");
            switch(vf->interlace_mode)
              {
              case GAVL_INTERLACE_TOP_FIRST:
                fprintf(stderr, "T");
                break;
              case GAVL_INTERLACE_BOTTOM_FIRST:
                fprintf(stderr, "B");
                break;
              case GAVL_INTERLACE_NONE:
                fprintf(stderr, "P");
                break;
              default:
                fprintf(stderr, "?");
                break;
                  
              }
            }
            
          if(vf->timecode != GAVL_TIMECODE_UNDEFINED)
            {
            fprintf(stderr, ", Timecode: ");
            gavl_timecode_dump(&video_format->timecode_format,
                               vf->timecode);
            }
          fprintf(stderr, "\n");
  
          // fprintf(stderr, "First 16 bytes of first plane follow\n");
          // bgav_hexdump(vf->planes[0] + vf->strides[0] * 20, 16, 16);
          }
        else
          {
          fprintf(stderr, "Failed\n");
          break;
          }
        }
#endif
      }
    }

  for(i = 0; i < num_text_streams; i++)
    {
    int timescale = bgav_get_text_timescale(file, i);
        
    fprintf(stderr, "Reading text subtitle from stream %d...", i+1);
        
    if(bgav_read_subtitle_text(file, &sub_text, &sub_text_alloc,
                               &sub_time, &sub_duration, i + num_overlay_streams))
      {
      fprintf(stderr, "Done\nstart: %f, duration: %f\n%s\n",
              gavl_time_to_seconds(gavl_time_unscale(timescale,
                                                     sub_time)),
              gavl_time_to_seconds(gavl_time_unscale(timescale, sub_duration)),
              sub_text);
      }
    else
      fprintf(stderr, "Failed\n");
    }
    
  for(i = 0; i < num_overlay_streams; i++)
    {
    video_format = bgav_get_overlay_format(file, i);
      
    fprintf(stderr, "Reading overlay subtitle from stream %d...", i+1);
    ovl = gavl_video_frame_create(video_format);
    if(bgav_read_subtitle_overlay(file, ovl, i))
      {
      fprintf(stderr, "Done\nsrc_rect: ");
      gavl_rectangle_i_dump(&ovl->src_rect);
      fprintf(stderr, "\ndst_coords: %d,%d\n", ovl->dst_x, ovl->dst_y);
      fprintf(stderr, "Time: %" PRId64 " -> %" PRId64 "\n",
              ovl->timestamp,
              ovl->timestamp+ovl->duration);
      }
    else
      {
      fprintf(stderr, "Failed\n");
      }
    gavl_video_frame_destroy(ovl);
    }

  if(sub_text)
    free(sub_text);
  
  }

int main(int argc, char ** argv)
  {
  FILE * cb_file = NULL;

  
  int i;
  int num_tracks;
  int track = -1;
  int arg_index;
  
  bgav_t * file;

  const gavl_dictionary_t * edl;

  bgav_options_t * opt;

  setlocale(LC_MESSAGES, "");
  setlocale(LC_CTYPE, "");
  
  if(argc == 1)
    {
    print_usage();
    return 0;
    }
  
  file = bgav_create();
  opt = bgav_get_options(file);

  /* Never delete cache entries */
  bgav_options_set_cache_size(opt, INT_MAX);
  
  arg_index = 1;
  while(arg_index < argc - 1)
    {
    if(!strcmp(argv[arg_index], "-L"))
      {
      list_all();
      return 0;
      }
    if(!strcmp(argv[arg_index], "-s"))
      {
      sample_accurate = 1;
      bgav_options_set_index_callback(opt, index_callback, NULL);
      arg_index++;
      }
    else if(!strcmp(argv[arg_index], "-vdpau"))
      {
      vdpau = 1;
      arg_index++;
      }
    else if(!strcmp(argv[arg_index], "-na"))
      {
      do_audio = 0;
      arg_index++;
      }
    else if(!strcmp(argv[arg_index], "-nv"))
      {
      do_video = 0;
      arg_index++;
      }
    else if(!strcmp(argv[arg_index], "-nf"))
      {
      frames_to_read = strtoll(argv[arg_index+1], NULL, 10);
      arg_index+=2;
      }
    else if(!strcmp(argv[arg_index], "-aseek"))
      {
      audio_seek = strtoll(argv[arg_index+1], NULL, 10);
      arg_index+=2;
      }
    else if(!strcmp(argv[arg_index], "-vseek"))
      {
      video_seek = strtoll(argv[arg_index+1], NULL, 10);
      arg_index+=2;
      }
    else if(!strcmp(argv[arg_index], "-seek"))
      {
      global_seek = strtoll(argv[arg_index+1], NULL, 10);
      arg_index+=2;
      }
    else if(!strcmp(argv[arg_index], "-t"))
      {
      track = atoi(argv[arg_index+1]);
      arg_index+=2;
      }
    else if(!strcmp(argv[arg_index], "-ci"))
      {
      dump_ci = 1;
      arg_index++;
      }
    else if(!strcmp(argv[arg_index], "-dh"))
      {
      bgav_options_set_dump_headers(opt, 1);
      arg_index++;
      }
    else if(!strcmp(argv[arg_index], "-dp"))
      {
      bgav_options_set_dump_packets(opt, 1);
      arg_index++;
      }
    else if(!strcmp(argv[arg_index], "-di"))
      {
      bgav_options_set_dump_indices(opt, 1);
      arg_index++;
      }
    else if(!strcmp(argv[arg_index], "-v"))
      {
      gavl_set_log_verbose(strtol(argv[arg_index+1], NULL, 10));
      arg_index+=2;
      }
    else if(!strcmp(argv[arg_index], "-follow"))
      {
      follow_redir = 1;
      arg_index++;
      }
    else
      arg_index++;
    }
  
  /* Configure */

  bgav_options_set_connect_timeout(opt,   connect_timeout);
  bgav_options_set_read_timeout(opt,      read_timeout);
  bgav_options_set_network_bandwidth(opt, network_bandwidth);
  bgav_options_set_seek_subtitles(opt, seek_subtitles);
  bgav_options_set_http_shoutcast_metadata(opt, 1);

  
  bgav_options_set_seek_subtitles(opt, 1);

  if(sample_accurate)
    bgav_options_set_sample_accurate(opt, 1);

  bgav_options_set_vdpau(opt, vdpau);
  
#ifndef _WIN32 
  bgav_options_set_user_pass_callback(opt, user_pass_func, NULL);
#endif
  
  if(!strncmp(argv[argc-1], "vcd://", 6))
    {
    if(!bgav_open_vcd(file, argv[argc-1] + 6))
      {
      fprintf(stderr, "Could not open VCD Device %s\n",
              argv[argc-1] + 6);
      return -1;
      }
    }
#if 0
  else if(!strncmp(argv[argc-1], "dvd://", 6))
    {
    if(!bgav_open_dvd(file, argv[argc-1] + 6))
      {
      fprintf(stderr, "Could not open DVD Device %s\n",
              argv[argc-1] + 6);
      return -1;
      }
    }
  else if(!strncmp(argv[argc-1], "dvb://", 6))
    {
    if(!bgav_open_dvb(file, argv[argc-1] + 6))
      {
      fprintf(stderr, "Could not open DVB Device %s\n",
              argv[argc-1] + 6);
      return -1;
      }
    }
#endif
  else if(!strncmp(argv[argc-1], "cb://", 5))
    {
    cb_file = fopen(argv[argc-1] + 5, "r");
    if(!cb_file)
      {
      fprintf(stderr, "Could not open file %s via callbacks\n",
              argv[argc-1] + 5);
      return -1;
      }
    if(!bgav_open_callbacks(file,
                            read_callback,
                            seek_callback,
                            cb_file,
                            argv[argc-1] + 5, NULL, 0))
      {
      fprintf(stderr, "Could not open file %s via callbacks\n",
              argv[argc-1] + 5);
      return -1;
      }
    }
  else 
    {
    if(!follow_redir)
      {
      if(!bgav_open(file, argv[argc-1]))
        {
        fprintf(stderr, "Could not open file %s\n",
                argv[argc-1]);
        bgav_close(file);
        return -1;
        }
      }
    else
      {
      const char * var;
      const gavl_dictionary_t * dict;
      char * str = gavl_strdup(argv[argc-1]);
      
      while(1)
        {
        if(!bgav_open(file, str))
          {
          fprintf(stderr, "Could not open location %s\n", str);
          bgav_close(file);
          return -1;
          }
        /* Check for redirection */

        if((dict = bgav_get_media_info(file)) &&
           (dict = gavl_get_track(dict, 0)) &&
           (dict = gavl_track_get_metadata(dict)) &&
           (var = gavl_dictionary_get_string(dict, GAVL_META_CLASS)) &&
           !strcmp(var, GAVL_META_CLASS_LOCATION) && gavl_metadata_get_src(dict, GAVL_META_SRC, 0,
                                   NULL, &var))
          {
          fprintf(stderr, "Got redirection to %s\n", var);
          str = gavl_strrep(str, var);
          bgav_close(file);
          file = bgav_create();
          }
        else
          {
          free(str);
          break;
          }
        }
      
      }
    
    }
  edl = bgav_get_edl(file);
  if(edl)
    {
    fprintf(stderr, "Found EDL\n");
    gavl_dictionary_dump(edl, 2);
    }
  num_tracks = bgav_num_tracks(file);

  if(track < 0)
    {
    for(i = 0; i < num_tracks; i++)
      dump_track(file, i);
    }
  else
    {
    if((track >= 0) && (track < num_tracks))
      dump_track(file, track);
    else
      fprintf(stderr, "No such track %d\n", track);
    }
  
  bgav_close(file);

  if(cb_file)
    fclose(cb_file);
  
  return -1;
  }
