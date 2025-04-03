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
#include <vorbis_comment.h>

#include <vorbis/codec.h>

#define LOG_DOMAIN "parse_vorbis"

typedef struct
  {
  vorbis_info vi;
  vorbis_comment vc;
  long last_blocksize;
  } vorbis_priv_t;

static int parse_frame_vorbis(bgav_packet_parser_t * parser, bgav_packet_t * p)
  {
  ogg_packet op;

  long blocksize;
  vorbis_priv_t * priv = parser->priv;

  memset(&op, 0, sizeof(op));
  
  op.bytes = p->buf.len;
  op.packet = p->buf.buf;

  blocksize = vorbis_packet_blocksize(&priv->vi, &op);

  if(priv->last_blocksize)
    p->duration = (priv->last_blocksize + blocksize) / 4;
  else
    p->duration = 0;

  //  fprintf(stderr, "Parse vorbis: %"PRId64"\n", p->duration);
  
  priv->last_blocksize = blocksize;

  //  gavl_packet_dump(p);
  
  return 1;
  }

static void cleanup_vorbis(bgav_packet_parser_t * parser)
  {
  vorbis_priv_t * priv = parser->priv;
  vorbis_comment_clear(&priv->vc);
  vorbis_info_clear(&priv->vi);
  free(priv);
  }

static void reset_vorbis(bgav_packet_parser_t * parser)
  {
  vorbis_priv_t * priv = parser->priv;
  priv->last_blocksize = 0;
  }

void bgav_packet_parser_init_vorbis(bgav_packet_parser_t * parser)
  {
  vorbis_priv_t * priv;
  ogg_packet op;
  int i;
  int len;
  
  priv = calloc(1, sizeof(*priv));
  parser->priv = priv;
  /* Get extradata and initialize codec */
  vorbis_info_init(&priv->vi);
  vorbis_comment_init(&priv->vc);

  memset(&op, 0, sizeof(op));

  if(!parser->ci->codec_header.len)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "No extradata found");
    return;
    }

  
  op.b_o_s = 1;

  for(i = 0; i < 3; i++)
    {
    op.packet =
      gavl_extract_xiph_header(&parser->ci->codec_header,
                               i, &len);
    
    if(!op.packet)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Truncated vorbis header %d", i+1);
      return;
      }
    
    if(i)
      op.b_o_s = 0;
    
    op.bytes = len;

    if(vorbis_synthesis_headerin(&priv->vi, &priv->vc,
                                 &op) < 0)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Packet %d is not a vorbis header", i+1);
      gavl_hexdump(op.packet, op.bytes, 16);
      return;
      }
    op.packetno++;
    }

  parser->afmt->samplerate = priv->vi.rate;
  parser->afmt->num_channels = priv->vi.channels;
  bgav_vorbis_set_channel_setup(parser->afmt);
  
  parser->parse_frame = parse_frame_vorbis;
  parser->cleanup = cleanup_vorbis;
  parser->reset = reset_vorbis;
  }
