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

#include <avdec_private.h>
#include <parser.h>
#include <flac_header.h>

#define LOG_DOMAIN "parse_flac"


typedef struct
  {
  bgav_flac_streaminfo_t si;
  } flac_priv_t;

  /* Find the boundary of a frame.
     if found: set buf.pos to the frame start, set *skip to the bytes after pos, where
     we scan for the next frame boundary
     if not found: set buf.pos to the position, where we can re-start the scan
  */
  
static int find_frame_boundary_flac(struct bgav_packet_parser_s * parser, int * skip)
  {
  bgav_flac_frame_header_t h;
  int i;
  flac_priv_t * priv = parser->priv;
  
  for(i = parser->buf.pos; i < parser->buf.len - BGAV_FLAC_FRAMEHEADER_MAX; i++)
    {
    if(bgav_flac_frame_header_read(parser->buf.buf + i, parser->buf.len - i,
                                   &priv->si, &h))
      {
      parser->buf.pos = i;
      *skip = BGAV_FLAC_FRAMEHEADER_MIN;
      return 1;
      }
    //    else if()
    }
  
  parser->buf.pos = parser->buf.len - BGAV_FLAC_FRAMEHEADER_MAX;
  return 0;
  }


static int parse_frame_flac(bgav_packet_parser_t * parser, bgav_packet_t * p)
  {
  bgav_flac_frame_header_t fh;
  flac_priv_t * priv = parser->priv;

  if(p->buf.len < BGAV_FLAC_FRAMEHEADER_MIN)
    return 0;
  bgav_flac_frame_header_read(p->buf.buf, p->buf.len, &priv->si, &fh);
  p->duration = fh.blocksize;

#if 0
  if((priv->si.total_samples > 0) &&
     (p->pts < priv->si.total_samples) && 
     (p->pts + p->duration > priv->si.total_samples))
    p->duration = priv->si.total_samples - p->pts;
#endif
  return 1;
  }

static void cleanup_flac(bgav_packet_parser_t * parser)
  {
  flac_priv_t * priv = parser->priv;
  free(priv);
  }

void bgav_packet_parser_init_flac(bgav_packet_parser_t * parser)
  {
  flac_priv_t * priv;
  
  /* Get stream info */
  if(parser->ci->codec_header.len != 42)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Corrupted flac header, expected 42 bytes, got %d", parser->ci->codec_header.len);
    gavl_hexdump(parser->ci->codec_header.buf, parser->ci->codec_header.len, 16);
    return;
    }

  priv = calloc(1, sizeof(*priv));
  parser->priv = priv;
  
  bgav_flac_streaminfo_read(parser->ci->codec_header.buf + 8, &priv->si);
  bgav_flac_streaminfo_init_stream(&priv->si, parser->info);
  
  parser->parse_frame         = parse_frame_flac;
  parser->find_frame_boundary = find_frame_boundary_flac;
  parser->cleanup             = cleanup_flac;
  }
