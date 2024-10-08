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
#include <adts_header.h>

#define HEADER_BYTES 4

static int find_frame_boundary_adts(bgav_packet_parser_t * parser, int * skip)
  {
  bgav_adts_header_t h;
  int i;
  
  for(i = parser->buf.pos; i < parser->buf.len - HEADER_BYTES; i++)
    {
    if(bgav_adts_header_read(parser->buf.buf + i, &h))
      {
      parser->buf.pos = i;
      *skip = h.frame_bytes;
      return 1;
      }
    }
  
  parser->buf.pos = parser->buf.len - HEADER_BYTES;
  return 0;
  }

static int parse_frame_adts(bgav_packet_parser_t * parser, gavl_packet_t * p)
  {
  bgav_adts_header_t h;

  if(!bgav_adts_header_read(p->buf.buf, &h))
    return 0;

  if(!(parser->parser_flags & PARSER_HAS_HEADER))
    bgav_adts_header_get_format(&h, parser->afmt);
  
  p->duration = parser->afmt->samples_per_frame * h.num_blocks;
  return 1;
  }

void bgav_packet_parser_init_adts(bgav_packet_parser_t * parser)
  {
  parser->parse_frame         = parse_frame_adts;
  parser->find_frame_boundary = find_frame_boundary_adts;
  
  }
