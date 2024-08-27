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



#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <avdec_private.h>
#include <parser.h>
#include <a52_header.h>

#define FRAME_SAMPLES 1536

static int find_frame_boundary_a52(bgav_packet_parser_t * parser, int * skip)
  {
  int i;
  bgav_a52_header_t h;
  
  for(i = parser->buf.pos; i < parser->buf.len - BGAV_A52_HEADER_BYTES; i++)
    {
    if(bgav_a52_header_read(&h, parser->buf.buf + i))
      {
      parser->buf.pos = i;
      *skip = h.total_bytes;
      return 1;
      }
    }
  return 0;
  }

static int parse_frame_a52(bgav_packet_parser_t * parser,
                           bgav_packet_t * p)
  {
  if(!(parser->parser_flags & PARSER_HAS_HEADER))
    {
    bgav_a52_header_t h;
    if(!bgav_a52_header_read(&h, p->buf.buf))
      return 0;
    bgav_a52_header_get_format(&h, parser->afmt);
    parser->ci.bitrate = h.bitrate;
    }
  p->duration = FRAME_SAMPLES;
  return 1;
  }

#if 0 
void cleanup_a52(bgav_packet_parser_t * parser)
  {
  
  }

void reset_a52(bgav_packet_parser_t * parser)
  {
  
  }
#endif

void bgav_packet_parser_init_a52(bgav_packet_parser_t * parser)
  {
  parser->find_frame_boundary = find_frame_boundary_a52;
  parser->parse_frame = parse_frame_a52;
  }
