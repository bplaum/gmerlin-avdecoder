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
#include <string.h>

#include <config.h>
#include <avdec_private.h>
#include <parser.h>
#include <audioparser_priv.h>
#include <a52_header.h>

#define FRAME_SAMPLES 1536

static int parse_a52(bgav_audio_parser_t * parser)
  {
  int i;
  bgav_a52_header_t h;
  
  for(i = 0; i < parser->buf.size - BGAV_A52_HEADER_BYTES; i++)
    {
    if(bgav_a52_header_read(&h, parser->buf.buffer + i))
      {
      if(!parser->have_format)
        {
        bgav_a52_header_get_format(&h, parser->s->data.audio.format);
        parser->s->codec_bitrate = h.bitrate;
        parser->have_format = 1;
        return PARSER_HAVE_FORMAT;
        }
      bgav_audio_parser_set_frame(parser,
                                  i, h.total_bytes, FRAME_SAMPLES);
      return PARSER_HAVE_FRAME;
      }
    }
  return PARSER_NEED_DATA;

  }

static int parse_frame_a52(bgav_audio_parser_t * parser,
                           bgav_packet_t * p)
  {
  bgav_a52_header_t h;

  if(!parser->have_format)
    {
    if(!bgav_a52_header_read(&h, p->data))
      return 0;
    bgav_a52_header_get_format(&h, parser->s->data.audio.format);
    parser->s->codec_bitrate = h.bitrate;
    parser->have_format = 1;
    }
  p->duration = FRAME_SAMPLES;
  return 1;
  }

#if 0 
void cleanup_a52(bgav_audio_parser_t * parser)
  {
  
  }

void reset_a52(bgav_audio_parser_t * parser)
  {
  
  }
#endif

void bgav_audio_parser_init_a52(bgav_audio_parser_t * parser)
  {
  parser->parse = parse_a52;
  parser->parse_frame = parse_frame_a52;
  }
