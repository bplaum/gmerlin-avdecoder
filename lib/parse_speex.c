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

#include <speex/speex.h>
#include <speex/speex_header.h>


static int parse_frame_speex(bgav_packet_parser_t * parser, bgav_packet_t * p)
  {
  if(p->duration < 0)
    p->duration = parser->afmt->samples_per_frame;
  
  return 1;
  }


static void reset_speex(bgav_packet_parser_t * parser)
  {

  }

void bgav_packet_parser_init_speex(bgav_packet_parser_t * parser)
  {
  void *dec_state;
  SpeexHeader *header;
  int frame_size;
  
  /* Set functions */
  
  parser->parse_frame = parse_frame_speex;
  parser->reset = reset_speex;
  
  /* Get samples per packet */
  header = speex_packet_to_header((char*)parser->ci.codec_header.buf,
                                  parser->ci.codec_header.len);

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
                    &parser->ci.pre_skip);
  
  parser->afmt->samplerate = header->rate;
  parser->afmt->samples_per_frame = header->frames_per_packet * frame_size;
  
  parser->afmt->num_channels = header->nb_channels;

  gavl_set_channel_setup(parser->afmt);
  speex_decoder_destroy(dec_state);
  free(header);
  
  }
