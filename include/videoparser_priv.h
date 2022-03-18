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

#ifndef BGAV_VIDEOPARSER_PRIV_H_INCLUDED
#define BGAV_VIDEOPARSER_PRIV_H_INCLUDED

#define PARSER_CACHE_MAX 32

#define PARSER_CONTINUE     (PARSER_PRIV+0)
#define PARSER_DISCARD      (PARSER_PRIV+1)

typedef void (*init_func)(bgav_video_parser_t*);

// typedef int (*parse_func)(bgav_video_parser_t*);
typedef void (*cleanup_func)(bgav_video_parser_t*);
typedef void (*reset_func)(bgav_video_parser_t*);
// typedef int (*parse_header_func)(bgav_video_parser_t*);

/* Parse a frame contained in p.
   
   - Detect coding type, keyframe, field pcture, interlace mode,
     repeat modes
   - Extract a global header when it makes sense.

   Return 0 on error, 1 else
*/
   
typedef int (*parse_frame_func)(bgav_video_parser_t*, bgav_packet_t * p, int64_t pts_orig);

/*
 *  Find a frame boundary in the bytebuffer (parser->buf)
 *
 *  If a frame boundary is found:
 *  
 *  - Set parser->pos to the byte offset of the frame start
 *  - Set *skip to the offset *relative* to the frame start,
 *    after which we continue searching the next frame boundary.
 *  - Return 1
 *
 *  If no frame boundary is found:
 *
 *  - Set parser->pos to the position where we continue searching after
 *    new data was added.
 *  - Return 0.
 */

typedef int (*find_frame_boundary_func)(bgav_video_parser_t * p, int * skip);

typedef struct
  {
  int64_t packet_position;
  int     parser_position;
  int     size;
  int64_t pts;
  } packet_t;

/* MPEG-2 Intra slice refresh */
#define PARSER_NO_I_FRAMES (1<<0)
#define PARSER_GEN_PTS     (1<<1)
#define PARSER_INITIALIZED (1<<2)

struct bgav_video_parser_s
  {
  bgav_stream_t * s;

  int raw;
  bgav_bytebuffer_t buf;
  
  int flags;
  int pos;
  
  //  parse_func        parse;
  //  parse_header_func parse_header;
  cleanup_func      cleanup;
  reset_func        reset;
  parse_frame_func  parse_frame;
  find_frame_boundary_func find_frame_boundary;
  
  //  const bgav_options_t * opt;
  
  /* Extradata */
  //  uint8_t * header;
  //  int header_len;
  
  /* Private data for parsers */
  void * priv;
  
  /* Raw byte offset of the start of the parser buffer */
  int64_t raw_position;
  
  /* Timescales */
  
  gavl_video_format_t * format;
  
  /* Packets */
  packet_t * packets;
  int packets_alloc;
  int num_packets;
  
  int low_delay;
  
  int64_t timestamp;

  int eof;
  int have_sync;
  
  int non_b_count;
  int keyframe_count;
  
  bgav_packet_t * out_packet;
  bgav_packet_source_t src;

  /* Packet position from which we can start decoding */
  int64_t start_pos;
  
  };

void bgav_video_parser_init_mpeg12(bgav_video_parser_t * parser);
void bgav_video_parser_init_h264(bgav_video_parser_t * parser);
void bgav_video_parser_init_mpeg4(bgav_video_parser_t * parser);
void bgav_video_parser_init_cavs(bgav_video_parser_t * parser);
void bgav_video_parser_init_vc1(bgav_video_parser_t * parser);
void bgav_video_parser_init_dirac(bgav_video_parser_t * parser);
void bgav_video_parser_init_mjpa(bgav_video_parser_t * parser);
void bgav_video_parser_init_dv(bgav_video_parser_t * parser);
void bgav_video_parser_init_jpeg(bgav_video_parser_t * parser);
void bgav_video_parser_init_png(bgav_video_parser_t * parser);
void bgav_video_parser_init_dvdsub(bgav_video_parser_t * parser);
void bgav_video_parser_init_vp8(bgav_video_parser_t * parser);
void bgav_video_parser_init_vp9(bgav_video_parser_t * parser);

void bgav_video_parser_flush(bgav_video_parser_t * parser, int bytes);

#endif // BGAV_VIDEOPARSER_PRIV_H_INCLUDED

