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

#include <speex/speex.h>
#include <speex/speex_header.h>

typedef struct
  {
  int packet_samples;
  int64_t pts_offset;
  } speex_priv_t;

static int parse_frame_speex(bgav_audio_parser_t * parser, bgav_packet_t * p)
  {
  speex_priv_t * priv = parser->priv;
  p->duration = priv->packet_samples;

  if(p->pts == 0)
    parser->timestamp += priv->pts_offset;
  //  priv->pts_offset = 0;
  
  return 1;
  }

static void cleanup_speex(bgav_audio_parser_t * parser)
  {
  free(parser->priv);
  }

static void reset_speex(bgav_audio_parser_t * parser)
  {

  }

void bgav_audio_parser_init_speex(bgav_audio_parser_t * parser)
  {
  void *dec_state;
  SpeexHeader *header;
  speex_priv_t * priv;
  int frame_size;
  
  priv = calloc(1, sizeof(*priv));
  parser->priv = priv;

  /* Set functions */
  
  parser->parse_frame = parse_frame_speex;
  parser->cleanup = cleanup_speex;
  parser->reset = reset_speex;
  
  /* Get samples per packet */
  header = speex_packet_to_header((char*)parser->s->ci->global_header,
                                  parser->s->ci->global_header_len);

  if(!header)
    return;
  
  dec_state = speex_decoder_init(speex_mode_list[header->mode]);

  if(!dec_state)
    {
    free(header);
    return;
    }
  
  speex_decoder_ctl(dec_state, SPEEX_GET_FRAME_SIZE, &frame_size);
  speex_decoder_ctl(dec_state, SPEEX_GET_LOOKAHEAD,
                    &parser->s->ci->pre_skip);
  
  priv->packet_samples = header->frames_per_packet * frame_size;

  parser->s->data.audio.format->samplerate = header->rate;
  parser->s->timescale = header->rate;

  parser->s->data.audio.format->num_channels = header->nb_channels;
  gavl_set_channel_setup(parser->s->data.audio.format);

  priv->pts_offset = -parser->s->ci->pre_skip;
  
  speex_decoder_destroy(dec_state);
  free(header);
  
  }
