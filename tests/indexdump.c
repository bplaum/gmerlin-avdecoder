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

static void index_callback(void * data, float perc)
  {
  fprintf(stdout, "Building index %.2f %% completed\r",
          perc * 100.0);
  fflush(stdout);
  }

int main(int argc, char ** argv)
  {
  bgav_t * b;
  bgav_options_t * opt;

  if(argc < 2)
    {
    fprintf(stderr, "Usage: %s <file>\n", argv[0]);
    return -1;
    }
  
  b = bgav_create();
  opt = bgav_get_options(b);
  bgav_options_set_sample_accurate(opt, 1);
  bgav_options_set_index_callback(opt, index_callback, NULL);
  
  if(!bgav_open(b, argv[1]))
    return -1;
  fprintf(stderr, "\n");
  if(b->demuxer->si)
    bgav_superindex_dump(b->demuxer->si);
  bgav_file_index_dump(b);

  bgav_close(b);
  return 0;
  }
