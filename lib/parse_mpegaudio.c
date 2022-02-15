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
#include <mpa_header.h>

#define HEADER_BYTES 4

static int parse_mpa(bgav_audio_parser_t * parser)
  {
  int i;
  bgav_mpa_header_t h;
  
  for(i = 0; i < parser->buf.size - HEADER_BYTES; i++)
    {
    if(bgav_mpa_header_decode(&h, parser->buf.buffer + i))
      {
      if(parser->have_format)
        {
        gavl_audio_format_t fmt;
        memset(&fmt, 0, sizeof(fmt));
        bgav_mpa_header_get_format(&h, &fmt);

        /* Sometimes we have bogus sync codes, which mess up the decoder */
        if((fmt.samplerate != parser->s->data.audio.format->samplerate) ||
           (fmt.num_channels != parser->s->data.audio.format->num_channels))
          continue;
        }
      else
        {
        bgav_mpa_header_get_format(&h, parser->s->data.audio.format);
        parser->have_format = 1;

        if(parser->s->fourcc == BGAV_MK_FOURCC('m', 'p', 'g', 'a'))
          {
          switch(h.layer)
            {
            case 1:
              parser->s->fourcc = BGAV_MK_FOURCC('.','m','p','1');
              break;
            case 2:
              parser->s->fourcc = BGAV_MK_FOURCC('.','m','p','2');
              break;
            case 3:
              parser->s->fourcc = BGAV_MK_FOURCC('.','m','p','3');
              break;
            }
          }
        
        if(!parser->s->container_bitrate)
          parser->s->container_bitrate = h.bitrate;
        
        return PARSER_HAVE_FORMAT;
        }
      bgav_audio_parser_set_frame(parser,
                                  i, h.frame_bytes, h.samples_per_frame);
      return PARSER_HAVE_FRAME;
      }
    }
  return PARSER_NEED_DATA;
  }

#if 0
void cleanup_mpa(bgav_audio_parser_t * parser)
  {
  
  }

void reset_mpa(bgav_audio_parser_t * parser)
  {
  
  }
#endif

void bgav_audio_parser_init_mpeg(bgav_audio_parser_t * parser)
  {
  parser->parse = parse_mpa;
  
  }
