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

#include <avdec_private.h>
#include <parser.h>
#include <audioparser_priv.h>
#include <flac_header.h>

#define LOG_DOMAIN "parse_flac"


typedef struct
  {
  bgav_flac_streaminfo_t si;
  } flac_priv_t;

static int parse_frame_flac(bgav_audio_parser_t * parser, bgav_packet_t * p)
  {
  bgav_flac_frame_header_t fh;
  flac_priv_t * priv = parser->priv;

  if(p->buf.len < BGAV_FLAC_FRAMEHEADER_MIN)
    return 0;
  bgav_flac_frame_header_read(p->buf.buf, p->buf.len, &priv->si, &fh);
  p->duration = fh.blocksize;

  if((priv->si.total_samples > 0) &&
     (p->pts < priv->si.total_samples) && 
     (p->pts + p->duration > priv->si.total_samples))
    p->duration = priv->si.total_samples - p->pts;
  return 1;
  }

static void cleanup_flac(bgav_audio_parser_t * parser)
  {
  flac_priv_t * priv = parser->priv;
  free(priv);
  }

void bgav_audio_parser_init_flac(bgav_audio_parser_t * parser)
  {
  flac_priv_t * priv;
  
  /* Get stream info */
  if(parser->s->ci->global_header_len != 42)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Corrupted flac header");
    return;
    }

  priv = calloc(1, sizeof(*priv));
  parser->priv = priv;

  
  bgav_flac_streaminfo_read(parser->s->ci->global_header + 8, &priv->si);
  
  parser->parse_frame = parse_frame_flac;
  parser->cleanup = cleanup_flac;
  }
