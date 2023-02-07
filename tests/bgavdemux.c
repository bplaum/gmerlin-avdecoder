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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


#include <avdec.h>

static int dump_packets = 0;

typedef struct
  {
  int active;
  int separate;
  gavl_compression_info_t info;

  int64_t time;
  int timescale;

  FILE * out;
  
  const char * ext;
  int frame;
  } stream_t;

static char filename_base[1024];
static char filename_buf[1128];

static int init_audio_stream(bgav_t * b, int index, stream_t * ret)
  {
  if(!bgav_get_audio_compression_info(b, index,
                                      &ret->info))
    {
    fprintf(stderr, "Audio stream %d doesn't support compressed output\n",
            index+1);
    return 0;
    }
  fprintf(stderr, "Audio stream %d compression\n", index+1);
  gavl_compression_info_dump(&ret->info);
  
  /* Get the file extension */
  ret->ext = gavl_compression_get_extension(ret->info.id, &ret->separate);

  if(!ret->ext)
    {
    fprintf(stderr, "Audio stream %d has no supported raw format\n",
            index+1);
    return 0;
    }

  /* Open file */
  sprintf(filename_buf, "%s_audio_%02d.%s",
          filename_base, index+1, ret->ext);
  ret->out = fopen(filename_buf, "wb");
  
  if(!ret->out)
    {
    fprintf(stderr, "Opening output file %s failed: %s\n",
            filename_buf, strerror(errno));
    return 0;
    }

  
  ret->active = 1;
  return 1;
  }

static int init_video_stream(bgav_t * b, int index, stream_t * ret)
  {
  if(!bgav_get_video_compression_info(b, index,
                                      &ret->info))
    {
    fprintf(stderr, "Video stream %d doesn't support compressed output\n",
            index+1);
    return 0;
    }
  fprintf(stderr, "Video stream %d compression\n", index+1);
  gavl_compression_info_dump(&ret->info);

  /* Get the file extension */
  ret->ext = gavl_compression_get_extension(ret->info.id, &ret->separate);

  if(!ret->ext)
    {
    fprintf(stderr, "Video stream %d has no supported raw format\n",
            index+1);
    return 0;
    }

  if(!ret->separate)
    {
    /* Open file */
    sprintf(filename_buf, "%s_video_%02d.%s",
            filename_base, index+1, ret->ext);
    ret->out = fopen(filename_buf, "wb");

    if(!ret->out)
      {
      fprintf(stderr, "Opening output file %s failed: %s\n",
              filename_buf, strerror(errno));
      return 0;
      }
    }
  
  ret->active = 1;
  return 1;
  }

static int write_audio(bgav_t * b, int index, stream_t * s, gavl_packet_t * p)
  {
  /* Get packet */
  if(!bgav_read_audio_packet(b, index, p))
    return 0;

  if(dump_packets)
    {
    fprintf(stderr, "Audio ");
    gavl_packet_dump(p);
    }
  
  if(fwrite(p->buf.buf, 1, p->buf.len, s->out) < p->buf.len)
    {
    fprintf(stderr, "Writing data failed: %s\n",
            strerror(errno));
    return 0;
    } 
  s->time += p->duration;
  return 1;
  }

static int write_video(bgav_t * b, int index, stream_t * s, gavl_packet_t * p)
  {
  /* Get packet */
  if(!bgav_read_video_packet(b, index, p))
    return 0;

  if(dump_packets)
    {
    fprintf(stderr, "Video ");
    gavl_packet_dump(p);
    }
  
  s->frame++;
  
  if(s->separate)
    {
    /* Open file */
    sprintf(filename_buf, "%s_video_%02d_%06d.%s",
            filename_base, index+1, s->frame, s->ext);
    s->out = fopen(filename_buf, "wb");
    if(!s->out)
      {
      fprintf(stderr, "Opening output file %s failed: %s\n",
              filename_buf, strerror(errno));
      return 0;
      }
    }
  

  /* Write data */
  
  if(fwrite(p->buf.buf, 1, p->buf.len, s->out) < p->buf.len)
    {
    fprintf(stderr, "Writing data failed: %s\n",
            strerror(errno));
    return 0;
    } 
  
  if(s->separate)
    {
    /* Close file */
    fclose(s->out);
    s->out = NULL;
    }
  s->time += p->duration;
  
  return 1;
  }

int main(int argc, char ** argv)
  {
  bgav_t * file;

  int num_audio_streams;
  int num_video_streams;

  stream_t * audio_streams;
  stream_t * video_streams;
  
  int audio_stream = 0;
  int video_stream = 0;
  int track = -1;
  int arg_index;
  int i;
  int total_streams = 0;
  char * pos;
  gavl_packet_t p;
  int do_audio, min_index;
  
  gavl_time_t test_time;
  gavl_time_t min_time;
  
  file = bgav_create();
  
  if(argc == 1)
    {
    fprintf(stderr, "Usage: bgavdemux [-dp] [-t track] [-as <num>] [-vs <num>] <location>\n");
    
    return 0;
    }

  arg_index = 1;
  while(arg_index < argc - 1)
    {
    if(!strcmp(argv[arg_index], "-t"))
      {
      arg_index++;
      track = atoi(argv[arg_index]);
      arg_index++;
      }
    else if(!strcmp(argv[arg_index], "-as"))
      {
      arg_index++;
      audio_stream = atoi(argv[arg_index]);
      arg_index++;
      }
    else if(!strcmp(argv[arg_index], "-vs"))
      {
      arg_index++;
      video_stream = atoi(argv[arg_index]);
      arg_index++;
      }
    else if(!strcmp(argv[arg_index], "-dp"))
      {
      dump_packets = 1;
      arg_index++;
      }
    }
  
  if(!strncmp(argv[argc-1], "vcd://", 6))
    {
    if(!bgav_open_vcd(file, argv[argc-1] + 5))
      {
      fprintf(stderr, "Could not open VCD Device %s\n",
              argv[argc-1] + 5);
      return -1;
      }
    }
  else if(!strncmp(argv[argc-1], "dvd://", 6))
    {
#if 0    
    if(!bgav_open_dvd(file, argv[argc-1] + 5))
      {
      fprintf(stderr, "Could not open DVD Device %s\n",
              argv[argc-1] + 5);
      return -1;
      }
#endif
    }
  else if(!strncmp(argv[argc-1], "dvb://", 6))
    {
#if 0    
    if(!bgav_open_dvb(file, argv[argc-1] + 6))
      {
      fprintf(stderr, "Could not open DVB Device %s\n",
              argv[argc-1] + 6);
      return -1;
      }
#endif
    }
  else if(!bgav_open(file, argv[argc-1]))
    {
    fprintf(stderr, "Could not open file %s\n",
            argv[argc-1]);
    bgav_close(file);
    return -1;
    }

  /* Create output filename base */
  pos = strrchr(argv[argc-1], '/');
  if(pos)
    pos++;
  else
    pos = argv[argc-1];

  strcpy(filename_base, pos);

  pos = strrchr(filename_base, '.');
  if(pos)
    *pos = '\0';

  fprintf(stderr, "Filename base: %s\n", filename_base);
  
  /* Check track */
  
  if(track < 0)
    track = 0;

  /* Select track */
  bgav_select_track(file, track);

  /* Decide which streams to demultiplex */
  num_audio_streams = bgav_num_audio_streams(file, track);
  num_video_streams = bgav_num_video_streams(file, track);
  
  audio_streams = calloc(num_audio_streams, sizeof(*audio_streams));
  video_streams = calloc(num_video_streams, sizeof(*audio_streams));

  if(!audio_stream && !video_stream)
    {
    for(i = 0; i < num_audio_streams; i++)
      {
      if(init_audio_stream(file, i, &audio_streams[i]))
        total_streams++;
      }
    for(i = 0; i < num_video_streams; i++)
      {
      if(init_video_stream(file, i, &video_streams[i]))
        total_streams++;
      }
    }
  else if(audio_stream > 0)
    {
    if(init_audio_stream(file, audio_stream-1, &audio_streams[audio_stream-1]))
      total_streams++;
    }
  else if(video_stream > 0)
    {
    if(init_video_stream(file, video_stream-1, &video_streams[video_stream-1]))
      total_streams++;
    }

  if(!total_streams)
    {
    fprintf(stderr, "No streams to demultiplex\n");
    return 0;
    }
  /* Set up streams and start decoder */
  for(i = 0; i < num_audio_streams; i++)
    {
    if(audio_streams[i].active)
      bgav_set_audio_stream(file, i, BGAV_STREAM_READRAW);
    }
  for(i = 0; i < num_video_streams; i++)
    {
    if(video_streams[i].active)
      bgav_set_video_stream(file, i, BGAV_STREAM_READRAW);
    }
  if(!bgav_start(file))
    {
    fprintf(stderr, "Starting decoder failed\n");
    return -1;
    }

  /* Main loop */

  for(i = 0; i < num_audio_streams; i++)
    {
    if(audio_streams[i].active)
      {
      const gavl_audio_format_t * fmt;
      fmt = bgav_get_audio_format(file, i);
      audio_streams[i].timescale = fmt->samplerate;
      fprintf(stderr, "Audio stream %d, format:\n", i+1);
      gavl_audio_format_dump(fmt);
      }
    }
  for(i = 0; i < num_video_streams; i++)
    {
    if(video_streams[i].active)
      {
      const gavl_video_format_t * fmt;
      fmt = bgav_get_video_format(file, i);
      video_streams[i].timescale = fmt->timescale;
      fprintf(stderr, "Video stream %d, format:\n", i+1);
      gavl_video_format_dump(fmt);
      }
    }

  memset(&p, 0, sizeof(p));
  
  while(total_streams)
    {
    /* Get the stream with the smallest time */
    min_time = GAVL_TIME_UNDEFINED;
    min_index = -1;
    
    do_audio = 0;
    
    for(i = 0; i < num_audio_streams; i++)
      {
      if(audio_streams[i].active)
        {
        test_time = gavl_time_unscale(audio_streams[i].timescale,
                                      audio_streams[i].time);
        
        if((min_index < 0) || (test_time < min_time))
          {
          min_index = i;
          do_audio = 1;
          min_time = test_time;
          }
        }
      }
    for(i = 0; i < num_video_streams; i++)
      {
      if(video_streams[i].active)
        {
        test_time = gavl_time_unscale(video_streams[i].timescale,
                                      video_streams[i].time);
        
        if((min_index < 0) || (test_time < min_time))
          {
          min_index = i;
          do_audio = 0;
          min_time = test_time;
          }
        }
      }
    
    /* Read packet */

    if(do_audio)
      {
      if(!write_audio(file, min_index, &audio_streams[min_index], &p))
        {
        audio_streams[min_index].active = 0;
        total_streams--;
        }
      }
    else
      {
      if(!write_video(file, min_index, &video_streams[min_index], &p))
        {
        video_streams[min_index].active = 0;
        total_streams--;
        }
      
      }
    }

  /* Clean up */
  
  return 0;
  }
