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



#include <opus.h>
#include <stdlib.h>

#include <config.h>
#include <avdec_private.h>
#include <parser.h>
#include <opus_header.h>


static int get_format(bgav_packet_parser_t * parser)
  {
  int ret = 0;
  bgav_opus_header_t h;
  bgav_input_context_t * input_mem;
  
  input_mem = bgav_input_open_memory(parser->ci->codec_header.buf,
                                     parser->ci->codec_header.len);
  
  if(!bgav_opus_header_read(input_mem, &h))
    goto fail;

  parser->afmt->num_channels = h.channel_count;  
  parser->afmt->samplerate = 48000;
  
  bgav_opus_set_channel_setup(&h, parser->afmt);

  //  if(parser->s->opt->dump_headers)
  //    bgav_opus_header_dump(&h);

  parser->ci->pre_skip = h.pre_skip;
  ret = 1;
  fail:
  
  bgav_input_destroy(input_mem);
  return ret;
  }

static int parse_frame_opus(bgav_packet_parser_t * parser, bgav_packet_t * p)
  {
  p->duration = opus_packet_get_nb_samples(p->buf.buf, p->buf.len, 48000);
  //  fprintf(stderr, "parse_frame_opus:\n");
  //  gavl_packet_dump(p);
  return 1;
  }


void bgav_packet_parser_init_opus(bgav_packet_parser_t * parser)
  {
  get_format(parser);
  parser->parse_frame = parse_frame_opus;
  }
