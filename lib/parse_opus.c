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

#include <opus.h>
#include <stdlib.h>

#include <config.h>
#include <avdec_private.h>
#include <parser.h>
#include <audioparser_priv.h>
#include <opus_header.h>

typedef struct
  {
  int64_t pts_offset;
  } opus_priv_t;

static int get_format(bgav_audio_parser_t * parser)
  {
  int ret = 0;
  bgav_opus_header_t h;
  bgav_input_context_t * input_mem;
  opus_priv_t * p = parser->priv;
  
  input_mem = bgav_input_open_memory(parser->s->ci->codec_header.buf,
                                     parser->s->ci->codec_header.len,
                                     parser->s->opt);
  
  if(!bgav_opus_header_read(input_mem, &h))
    goto fail;

  bgav_opus_set_channel_setup(&h, parser->s->data.audio.format);

  if(parser->s->opt->dump_headers)
    bgav_opus_header_dump(&h);

  parser->s->ci->pre_skip = h.pre_skip;
  p->pts_offset = - ((int)parser->s->ci->pre_skip);
  ret = 1;
  fail:
    
  bgav_input_close(input_mem);
  bgav_input_destroy(input_mem);
  return ret;
  }

static int parse_frame_opus(bgav_audio_parser_t * parser, bgav_packet_t * p)
  {
  opus_priv_t * priv = parser->priv;
  int nb_frames = opus_packet_get_nb_frames(p->buf.buf, p->buf.len);

  if(!parser->have_format)
    return 0;
  
  parser->timestamp += priv->pts_offset;
  priv->pts_offset = 0;
  
  if (nb_frames < 1)
    p->duration = nb_frames;
  else
    p->duration =
      opus_packet_get_samples_per_frame(p->buf.buf, 48000) * nb_frames;
  
  return 1;
  }

static void cleanup_opus(bgav_audio_parser_t * parser)
  {
  opus_priv_t * p = parser->priv;
  free(p);
  }

static void reset_opus(bgav_audio_parser_t * parser)
  {
  opus_priv_t * p = parser->priv;
  p->pts_offset = -((int64_t)parser->s->ci->pre_skip);
  }

void bgav_audio_parser_init_opus(bgav_audio_parser_t * parser)
  {
  opus_priv_t * priv = calloc(1, sizeof(*priv));

  parser->priv = priv;
  
  if(get_format(parser))
    parser->have_format = 1;
  
  parser->parse_frame = parse_frame_opus;
  parser->cleanup = cleanup_opus;
  parser->reset = reset_opus;
  }
