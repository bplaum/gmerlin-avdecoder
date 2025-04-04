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



#include <string.h>
#include <stdlib.h>

#include <avdec_private.h>
#include <parser.h>
#include <pngreader.h>

#define LOG_DOMAIN "parse_png"


typedef struct
  {
  int have_format;
  } png_priv_t;

static int get_format(bgav_packet_t * p,
                      gavl_video_format_t * fmt)
  {
  int ret = 1;
  bgav_png_reader_t * png = bgav_png_reader_create();
  
  if(!bgav_png_reader_read_header(png,
                                  p->buf.buf, p->buf.len,
                                  fmt))
    {
    ret = 0;
    }
  bgav_png_reader_destroy(png);
  return ret;
  }

static int parse_frame_png(bgav_packet_parser_t * parser, bgav_packet_t * p)
  {
  png_priv_t * priv = parser->priv;
  
  PACKET_SET_CODING_TYPE(p, GAVL_PACKET_TYPE_I);

  /* Extract format */
  if(!priv->have_format)
    {
    priv->have_format = 1;
    parser->ci->flags &= ~GAVL_COMPRESSION_HAS_P_FRAMES;

    if(!get_format(p, parser->vfmt))
      return 0;
    }
  return 1;
  }


static void cleanup_png(bgav_packet_parser_t * parser)
  {
  png_priv_t * priv = parser->priv;
  free(priv);
  }

void bgav_packet_parser_init_png(bgav_packet_parser_t * parser)
  {
  png_priv_t * priv;
  priv = calloc(1, sizeof(*priv));
  parser->priv        = priv;
  parser->parse_frame = parse_frame_png;
  parser->cleanup     = cleanup_png;
  }
