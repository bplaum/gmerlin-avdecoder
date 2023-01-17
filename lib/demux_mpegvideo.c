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

#include <avdec_private.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <parser.h>

#define LOG_DOMAIN "mpegvideo"

/* Trivial demuxer for MPEG-1/2 video streams */

#define BUFFER_LEN 1024

#define MPEG12_SEQUENCE_HEADER 

typedef struct
  {
  bgav_packet_parser_t * parser;
  int eof;
  } mpegvideo_priv_t;

static int detect_type(bgav_input_context_t * input)
  {
  uint32_t header_32;
  uint64_t header_64;
  if(!bgav_input_get_32_be(input, &header_32))
    return 0;

  /* MPEG-1/2 Video */
  if(header_32 == 0x000001b3)
    return BGAV_MK_FOURCC('m', 'p', 'g', 'v');
  else if(header_32 == 0x000001b0)
    return BGAV_MK_FOURCC('C', 'A', 'V', 'S');
  else if(header_32 == 0x0000010f)
    return BGAV_MK_FOURCC('V', 'C', '-', '1');
  
  /* H.264 */
  if(input->location && gavl_string_ends_with(input->location, ".h264"))
    return BGAV_MK_FOURCC('H', '2', '6', '4');
  
  /* MPEG-4 */
  if(!bgav_input_get_64_be(input, &header_64))
    return 0;

  /* Check for video_object_start_code followed by
     video_object_layer_start_code */
#if 0
  fprintf(stderr, "Test header: %016lx %016lx\n",
          header_64, header_64 & 0xFFFFFFE0FFFFFFF0LL);
#endif
  if((header_64 & 0xFFFFFFE0FFFFFFF0LL) == 0x0000010000000120LL)
    return BGAV_MK_FOURCC('m', 'p', '4', 'v');
  
  return 0;
  }

static int probe_mpegvideo(bgav_input_context_t * input)
  {
  return detect_type(input) ? 1 : 0;
  }

static gavl_source_status_t next_packet_mpegvideo(bgav_demuxer_context_t * ctx)
  {
  int ret;
  bgav_packet_t * p;
  bgav_stream_t * s;

  int bytes_to_read;
  
  s = bgav_track_get_video_stream(ctx->tt->cur, 0);

  /* Sample accurate: Read data and output as many
     packets as available */
  
  bytes_to_read = BUFFER_LEN;
  
  p = bgav_stream_get_packet_write(s);

  bgav_packet_alloc(p, bytes_to_read);
  p->position = ctx->input->position;
  p->buf.len = bgav_input_read_data(ctx->input, p->buf.buf,
                                    bytes_to_read);

  ret = p->buf.len ? GAVL_SOURCE_OK : GAVL_SOURCE_EOF;
  bgav_stream_done_packet_write(s, p);
  return ret;
  }

static int open_mpegvideo(bgav_demuxer_context_t * ctx)
  {
  mpegvideo_priv_t * priv;
  bgav_stream_t * s;
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
  
  /* Create track */

  ctx->tt = bgav_track_table_create(1);
  
  s = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
  s->index_mode = INDEX_MODE_SIMPLE;
  /*
   *  We just set the fourcc, everything else will
   *  be set by the parser
   */

  s->fourcc = detect_type(ctx->input);

  s->flags |= (STREAM_RAW_PACKETS);
  s->ci->flags |= GAVL_COMPRESSION_HAS_B_FRAMES;
  
  ctx->tt->cur->data_start = ctx->input->position;
  /* TODO: Fix this for non MPEG-1/2 streams */
  bgav_track_set_format(ctx->tt->cur, "Elementary video stream",
                        "video/mpeg");
  
  ctx->index_mode = INDEX_MODE_MIXED;
  bgav_stream_set_parse_full(s);
  
  
  return 1;

  }

static void close_mpegvideo(bgav_demuxer_context_t * ctx)
  {
  mpegvideo_priv_t * priv;
  priv = ctx->priv;
  free(priv);
  }


static int select_track_mpegvideo(bgav_demuxer_context_t * ctx, int track)
  {
#if 0
  mpegvideo_priv_t * priv;
  priv = ctx->priv;
  bgav_video_parser_reset(priv->parser, GAVL_TIME_UNDEFINED, 0);
  priv->eof = 0;
#endif
  return 1;
  }


const bgav_demuxer_t bgav_demuxer_mpegvideo =
  {
    .probe =       probe_mpegvideo,
    .open =        open_mpegvideo,
    .next_packet = next_packet_mpegvideo,
    //    .seek =        seek_mpegvideo,
    .close =       close_mpegvideo,
    .select_track =      select_track_mpegvideo
  };

