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



#ifndef BGAV_PARSER_H_INCLUDED
#define BGAV_PARSER_H_INCLUDED

#include <gavl/connectors.h>

/* Packet parser */

#define PARSER_HAS_SYNC   (1<<0)
#define PARSER_HAS_HEADER (1<<1)

typedef struct
  {
  int64_t pts;      // PTS of packet
  int64_t position; // Position in the stream
  int64_t size;     // Is decreased by bgav_packet_parser_flush
  
  } packet_info_t;

struct bgav_packet_parser_s
  {
  gavl_dictionary_t * info;
  gavl_dictionary_t * m;
  int packet_timescale;
  gavl_buffer_t buf;
  
  int fourcc;
  int stream_id;
  
  packet_info_t * packets;
  int num_packets;
  int packets_alloc;

  gavl_packet_sink_t * next;
  gavl_packet_sink_t * sink;

  gavl_audio_format_t * afmt;
  gavl_video_format_t * vfmt;
  gavl_compression_info_t ci;
  
  gavl_packet_t in_packet;

  int stream_flags;
  int parser_flags;

  int64_t raw_position;
  
  /* Format specific */

  void * priv;
  void (*cleanup)(struct bgav_packet_parser_s *);
  void (*reset)(struct bgav_packet_parser_s *);

  /* Find the boundary of a frame.
     if found: set buf.pos to the frame start, set *skip to the bytes after pos, where
     we scan for the next frame boundary
     if not found: set buf.pos to the position, where we can re-start the scan
  */
  
  int (*find_frame_boundary)(struct bgav_packet_parser_s *, int * skip);

  /*
   *  
   */
  
  int (*parse_frame)(struct bgav_packet_parser_s *, gavl_packet_t * pkt);
  
  };

gavl_packet_sink_t * bgav_packet_parser_connect(bgav_packet_parser_t * p, gavl_packet_sink_t * dst);
bgav_packet_parser_t * bgav_packet_parser_create(gavl_dictionary_t * stream_info, int stream_flags);
void bgav_packet_parser_destroy(bgav_packet_parser_t * p);

/* Signal EOF, might send some more packets */
void bgav_packet_parser_flush(bgav_packet_parser_t * p);

/* Call after seeking */
void bgav_packet_parser_reset(bgav_packet_parser_t * p);

/* Initialization functions */
void bgav_packet_parser_init_mpeg(bgav_packet_parser_t * parser);
void bgav_packet_parser_init_a52(bgav_packet_parser_t * parser);

#ifdef HAVE_FAAD2
void bgav_packet_parser_init_aac(bgav_packet_parser_t * parser);
#endif

#ifdef HAVE_DCA
void bgav_packet_parser_init_dca(bgav_packet_parser_t * parser);
#endif

#ifdef HAVE_SPEEX
void bgav_packet_parser_init_speex(bgav_packet_parser_t * parser);
#endif

#ifdef HAVE_VORBIS
void bgav_packet_parser_init_vorbis(bgav_packet_parser_t * parser);
#endif

#ifdef HAVE_THEORADEC
void bgav_packet_parser_init_theora(bgav_packet_parser_t * parser);
#endif


#ifdef HAVE_OPUS
void bgav_packet_parser_init_opus(bgav_packet_parser_t * parser);
#endif

void bgav_packet_parser_init_adts(bgav_packet_parser_t * parser);

void bgav_packet_parser_init_flac(bgav_packet_parser_t * parser);

void bgav_packet_parser_init_mpeg12(bgav_packet_parser_t * parser);
void bgav_packet_parser_init_h264(bgav_packet_parser_t * parser);
void bgav_packet_parser_init_mpeg4(bgav_packet_parser_t * parser);
void bgav_packet_parser_init_cavs(bgav_packet_parser_t * parser);
void bgav_packet_parser_init_vc1(bgav_packet_parser_t * parser);
void bgav_packet_parser_init_dirac(bgav_packet_parser_t * parser);
void bgav_packet_parser_init_mjpa(bgav_packet_parser_t * parser);
void bgav_packet_parser_init_dv(bgav_packet_parser_t * parser);
void bgav_packet_parser_init_jpeg(bgav_packet_parser_t * parser);
void bgav_packet_parser_init_png(bgav_packet_parser_t * parser);
void bgav_packet_parser_init_dvdsub(bgav_packet_parser_t * parser);
void bgav_packet_parser_init_vp8(bgav_packet_parser_t * parser);
void bgav_packet_parser_init_vp9(bgav_packet_parser_t * parser);

   
   


#endif // BGAV_PARSER_H_INCLUDED

