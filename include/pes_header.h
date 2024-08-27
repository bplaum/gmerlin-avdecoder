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



#ifndef BGAV_PES_HEADER_H_INCLUDED
#define BGAV_PES_HEADER_H_INCLUDED

#define START_CODE_SYSTEM_HEADER 0x000001bb
#define START_CODE_PACK_HEADER   0x000001ba
#define START_CODE_PROGRAM_END   0x000001b9

/* MPEG-1/2 PES packet */

typedef struct
  {
  int64_t pts;
  int64_t dts;
  int stream_id;     /* For private streams: substream_id | (stream_id << 8)   */
  int payload_size;  /* To be read from the input after bgav_pes_header_read() */
  } bgav_pes_header_t;

int bgav_pes_header_read(bgav_input_context_t * input,
                         bgav_pes_header_t * ret);

void bgav_pes_header_dump(bgav_pes_header_t * p);

/* Pack header (doesn't belog here but well....) */

typedef struct
  {
  int64_t scr;
  int mux_rate;
  int version;
  } bgav_pack_header_t;

void bgav_pack_header_dump(bgav_pack_header_t * h);

int bgav_pack_header_read(bgav_input_context_t * input,
                          bgav_pack_header_t * ret);

#endif // BGAV_PES_HEADER_H_INCLUDED

