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
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char ** argv)
  {
  bgav_yml_node_t * n;
  bgav_input_context_t * input;
  bgav_options_t opt;
  bgav_options_set_defaults(&opt);

  input = bgav_input_create(NULL, &opt);
  
  if(!bgav_input_open(input, argv[1]))
    {
    fprintf(stderr, "Cannot open file %s\n",
            argv[1]);
    return -1;
    }
  
  
  n = bgav_yml_parse(input);
  if(!n)
    {
    fprintf(stderr, "Cannot parse file %s\n",
            argv[1]);
    return -1;
    }
  bgav_yml_dump(n);
  bgav_yml_free(n);
  
  bgav_input_close(input);
  
  return 0;
  }
