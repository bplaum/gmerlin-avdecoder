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



#include <avdec_private.h>

#define VIDEO_STREAM 0


int main(int argc, char ** argv)
  {
  bgav_t * file;
  bgav_options_t * opt;
  int num_tracks, num_streams;
  int i, j;
  gavl_frame_table_t * tab;
  const gavl_video_format_t * format;
  file = bgav_create();
  opt = bgav_get_options(file);
  bgav_options_set_sample_accurate(opt, 1);
  if(!bgav_open(file, argv[1]))
    return -1;

#if 0  
  if(bgav_is_redirector(file))
    {
    fprintf(stderr, "Found redirector\n");
    return 0;
    }
#endif
  
  num_tracks = bgav_num_tracks(file);

  for(i = 0; i < num_tracks; i++)
    {
    bgav_select_track(file, i);

    num_streams = bgav_num_video_streams(file, i);

    for(j = 0; j < num_streams; j++)
      bgav_set_video_stream(file, j, BGAV_STREAM_DECODE);

    if(!bgav_start(file))
      {
      fprintf(stderr, "Starting decoders failed\n");
      return -1;
      }

    for(j = 0; j < num_streams; j++)
      {
      fprintf(stderr, "Track %d, stream %d:\n", i+1, j+1);

      format = bgav_get_video_format(file, j);
      gavl_video_format_dump(format);
      
      tab = bgav_get_frame_table(file, j);
      if(tab)
        {
        gavl_frame_table_dump(tab);
        gavl_frame_table_destroy(tab);
        }
      else
        {
        fprintf(stderr, "No frame table\n");
        }
      }

    }
  
  bgav_close(file);
  return 0;
  }
