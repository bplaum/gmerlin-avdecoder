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

#define PARSER_CACHE_MAX 16

#define PARSER_HAVE_FRAME    (PARSER_PRIV+0)

typedef void (*init_func)(bgav_audio_parser_t*);

typedef int (*parse_func)(bgav_audio_parser_t*);
typedef void (*cleanup_func)(bgav_audio_parser_t*);
typedef void (*reset_func)(bgav_audio_parser_t*);
// typedef int (*parse_header_func)(bgav_audio_parser_t*);
typedef int (*parse_frame_func)(bgav_audio_parser_t*, bgav_packet_t*);

#define STATE_NEED_START 0
#define STATE_NEED_END   1
#define STATE_HAVE_END   2

typedef struct
  {
  int64_t packet_position;
  int     parser_position;
  int     size;
  int64_t pts;
  } packet_t;

struct bgav_audio_parser_s
  {
  int raw;
  bgav_bytebuffer_t buf;
  
  bgav_stream_t * s;
  
  parse_func        parse;
  cleanup_func      cleanup;
  reset_func        reset;
  //  parse_header_func parse_header;
  parse_frame_func  parse_frame;
  
  /* Private data for parsers */
  void * priv;
  
  /* Raw byte offset of the start of the parser buffer */
  int64_t raw_position;
  
  int have_format;
  
  /* Packets */
  packet_t * packets;
  int packets_alloc;
  int num_packets;
  
  int64_t timestamp;

  int eof;

  /* Set by the parse function */
  int frame_bytes;
  int frame_samples;
  int64_t frame_position;
  
  int64_t frame_pts;
  
  int last_state;
  
  bgav_packet_t * out_packet;

  /* Where to get packets */
  bgav_packet_source_t src;
  
  };

void bgav_audio_parser_init_mpeg(bgav_audio_parser_t * parser);
void bgav_audio_parser_init_a52(bgav_audio_parser_t * parser);

#ifdef HAVE_FAAD2
void bgav_audio_parser_init_aac(bgav_audio_parser_t * parser);
#endif

#ifdef HAVE_DCA
void bgav_audio_parser_init_dca(bgav_audio_parser_t * parser);
#endif

#ifdef HAVE_SPEEX
void bgav_audio_parser_init_speex(bgav_audio_parser_t * parser);
#endif


#ifdef HAVE_VORBIS
void bgav_audio_parser_init_vorbis(bgav_audio_parser_t * parser);
#endif

#ifdef HAVE_OPUS
void bgav_audio_parser_init_opus(bgav_audio_parser_t * parser);
#endif

void bgav_audio_parser_init_adts(bgav_audio_parser_t * parser);

void bgav_audio_parser_init_flac(bgav_audio_parser_t * parser);

// void bgav_audio_parser_init_adts(bgav_audio_parser_t * parser);

void bgav_audio_parser_flush(bgav_audio_parser_t * parser, int bytes);

void bgav_audio_parser_set_frame(bgav_audio_parser_t * parser,
                                 int pos, int len, int samples);
