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



#ifndef BGAV_A52_HEADER_H_INCLUDED
#define BGAV_A52_HEADER_H_INCLUDED

typedef struct
  {
  int total_bytes;
  int samplerate;
  int bitrate;

  int acmod;
  int lfe;
  int dolby;

  float cmixlev;
  float smixlev;
  
  } bgav_a52_header_t;

#define BGAV_A52_HEADER_BYTES 7

int bgav_a52_header_read(bgav_a52_header_t * ret, uint8_t * buf);

void bgav_a52_header_dump(bgav_a52_header_t * h);
void bgav_a52_header_get_format(const bgav_a52_header_t * h,
                                gavl_audio_format_t * format);

#endif // BGAV_A52_HEADER_H_INCLUDED

