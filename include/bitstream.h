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

/* bitstream.c */

typedef struct
  {
  const uint8_t * pos;
  const uint8_t * end;
  int bit_cache;
  uint32_t c;
  } bgav_bitstream_t;

void bgav_bitstream_init(bgav_bitstream_t * b, const uint8_t * pos, 
                         int len);

int bgav_bitstream_get(bgav_bitstream_t * b, int * ret,  int bits);
int bgav_bitstream_get_long(bgav_bitstream_t * b, int64_t * ret,  int bits);
int bgav_bitstream_get_bits(bgav_bitstream_t * b);

int bgav_bitstream_peek(bgav_bitstream_t * b, int * ret, int bits);
int bgav_bitstream_skip(bgav_bitstream_t * b, int bits);


/* Special parsing functions */

int bgav_bitstream_get_golomb_ue(bgav_bitstream_t * b, int * ret);
int bgav_bitstream_get_golomb_se(bgav_bitstream_t * b, int * ret);
int bgav_bitstream_decode012(bgav_bitstream_t * b, int * ret);
int bgav_bitstream_get_unary(bgav_bitstream_t * b, int stop,
                             int len, int * ret);
