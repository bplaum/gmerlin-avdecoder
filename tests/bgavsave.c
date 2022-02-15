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


#include <stdlib.h>
#include <avdec_private.h>
#include <stdio.h>

#define BUFFER_SIZE 4096

int main(int argc, char ** argv)
  {
  FILE * out;
  uint8_t buffer[BUFFER_SIZE];
  int bytes_read;
  bgav_input_context_t * input;

  input = calloc(1, sizeof(*input));
  
  if(!bgav_input_open(input, argv[1]))
    return -1;
  
  out = fopen(argv[2], "w");
  do
    {
    bytes_read = bgav_input_read_data(input, buffer, BUFFER_SIZE);
    fwrite(buffer, 1, bytes_read, out);
    fprintf(stderr, "Wrote %d bytes\n", bytes_read);
    } while(bytes_read == BUFFER_SIZE);

  fclose(out);
  return 0;
  }
