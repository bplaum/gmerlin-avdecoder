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
#include <mpa_header.h>

#define HEADER_BYTES 4

typedef struct
  {
  bgav_mpa_header_t first_header;
  } mpa_priv_t;
 
#if 0
static int parse_mpa(bgav_audio_parser_t * parser)
  {
  int i;
  bgav_mpa_header_t h;
  
  for(i = 0; i < parser->buf.len - HEADER_BYTES; i++)
    {
    if(bgav_mpa_header_decode(&h, parser->buf.buf + i))
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
#endif

static int scan_header(bgav_packet_parser_t * parser, bgav_mpa_header_t * h)
  {
  int i;
  
  for(i = parser->buf.pos; i < parser->buf.len - 4; i++)
    {
    if(bgav_mpa_header_decode(h, parser->buf.buf + i))
      {
      parser->buf.pos = i;
      return 1;
      }
    }
  return 0;
  }

static int find_frame_boundary_mpa(bgav_packet_parser_t * parser, int * skip)
  {
  bgav_mpa_header_t h;
  mpa_priv_t * priv = parser->priv;

  if(parser->buf.pos + 4 >= parser->buf.len)
    return 0; // Too little data
  
  /* Not synchronized yet */

  if(!priv->first_header.samples_per_frame)
    {
    if(scan_header(parser, &priv->first_header))
      {
      *skip = priv->first_header.frame_bytes;
      return 1;
      }
    else
      return 0;
    }
  
  while(scan_header(parser, &h))
    {
    if(bgav_mpa_header_equal(&h, &priv->first_header))
      {
      *skip = h.frame_bytes;
      return 1;
      }
    else
      {
      /* Misdetected start code, try again */
      parser->buf.pos += 4;
      }
    }
  return 0;
  }

static int parse_frame_mpa(bgav_packet_parser_t * parser, bgav_packet_t * pkt)
  {
  mpa_priv_t * priv = parser->priv;

  if(!(parser->parser_flags & PARSER_HAS_HEADER))
    bgav_mpa_header_get_format(&priv->first_header, parser->afmt);
  pkt->duration = priv->first_header.samples_per_frame;
  return 1;
  }
  
static void cleanup_mpa(bgav_packet_parser_t * parser)
  {
  free(parser->priv);
  }

#if 0
void reset_mpa(bgav_audio_parser_t * parser)
  {
  
  }
#endif

void bgav_packet_parser_init_mpeg(bgav_packet_parser_t * parser)
  {
  mpa_priv_t * priv;

  priv = calloc(1, sizeof(*priv));
  
  parser->priv                = priv;
  parser->cleanup             = cleanup_mpa;
  parser->find_frame_boundary = find_frame_boundary_mpa;
  parser->parse_frame         = parse_frame_mpa;
  }

