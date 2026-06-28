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



#include <avdec.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int track = 0;
int stream = 0;

int main(int argc, char ** argv)
  {
  bgav_t * file;
  int arg_index;
  
  int64_t count;
  int64_t first_pts = GAVL_TIME_UNDEFINED;
  bgav_stream_action_t action = BGAV_STREAM_DECODE;
  
  if(argc == 1)
    {
    fprintf(stderr,
            "Usage: count_samples [-vs stream] [-pkt] [-t track] <location>\n");
    return 0;
    }
  file = bgav_create();

  arg_index = 1;
  
  while(arg_index < argc - 1)
    {
    if(!strcmp(argv[arg_index], "-vs"))
      {
      stream = strtol(argv[arg_index+1], NULL, 10);
      arg_index+=2;
      }
    else if(!strcmp(argv[arg_index], "-t"))
      {
      track = strtol(argv[arg_index+1], NULL, 10);
      arg_index+=2;
      }
    else if(!strcmp(argv[arg_index], "-pkt"))
      {
      action = BGAV_STREAM_READRAW;
      arg_index++;
      }
    else
      arg_index++;
    }

  if(!bgav_open(file, argv[argc-1]))
    {
    fprintf(stderr, "Could not open file %s\n",
            argv[argc-1]);
    bgav_close(file);
    return -1;
    }
  
  if(track < 0 || track >= bgav_num_tracks(file))
    {
    fprintf(stderr,
            "No such track %d\n", track);
    return -1;
    }

  bgav_select_track(file, track);
  
  if(stream < 0 || stream >= bgav_num_video_streams(file, track))
    {
    fprintf(stderr,
            "No such stream %d\n", stream);
    return -1;
    }

  bgav_set_video_stream(file, stream, action);

  if(!bgav_start(file))
    {
    fprintf(stderr, "Starting decoders failed\n");
    return -1;
    }
  else
    fprintf(stderr, "Starting decoders done\n");
  
  count = 0;


  if(action == BGAV_STREAM_READRAW)
    {
    gavl_packet_source_t * psrc;
    gavl_packet_t * pkt = NULL;
    psrc = bgav_get_video_packet_source(file, stream);
    
    while(gavl_packet_source_read_packet(psrc, &pkt) == GAVL_SOURCE_OK)
      {
      if(first_pts == GAVL_TIME_UNDEFINED)
        first_pts = pkt->pts;
      pkt = NULL;
      count++;
      }
    }
  else
    {
    gavl_video_source_t * vsrc;
    gavl_video_frame_t * frame = NULL;
    vsrc = bgav_get_video_source(file, stream);

    while(gavl_video_source_read_frame(vsrc, &frame) == GAVL_SOURCE_OK)
      {
      if(first_pts == GAVL_TIME_UNDEFINED)
        first_pts = frame->timestamp;
      frame = NULL;
      count++;
      }
    }
  
  fprintf(stderr, "Track %d stream %d contains %"PRId64" frames\n",
          track+1, stream+1, count);
  
  bgav_close(file);
  return 0;
  }
