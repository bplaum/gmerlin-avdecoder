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

#include <a52dec/a52.h>

/* A52 demuxer */

#define FRAME_SAMPLES 1536 /* 6 * 256 */
#define SYNC_BYTES    3840 /* Maximum possible length of a frame */

typedef struct
  {
  int samplerate;

  int64_t data_size;
  } a52_priv_t;

static int probe_a52(bgav_input_context_t * input)
  {
  int dummy_flags;
  int dummy_srate;
  int dummy_brate;
  
  uint8_t test_data[7];
  if(bgav_input_get_data(input, test_data, 7) < 7)
    return 0;
  
  return !!a52_syncinfo(test_data, &dummy_flags, &dummy_srate, &dummy_brate);
  }

static int open_a52(bgav_demuxer_context_t * ctx)
  {
  int dummy_flags;
  int bitrate;
  uint8_t test_data[7];
  a52_priv_t * priv;
  bgav_stream_t * s;
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  /* Recheck header */
  
  if(bgav_input_get_data(ctx->input, test_data, 7) < 7)
    return 0;

  if(!a52_syncinfo(test_data, &dummy_flags, &priv->samplerate,
                   &bitrate))
    goto fail;
  
  /* Create track */

  ctx->tt = bgav_track_table_create(1);
  
  s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
  s->container_bitrate = bitrate;
    
  /* We just set the fourcc, everything else will be set by the decoder */

  s->fourcc = BGAV_MK_FOURCC('.', 'a', 'c', '3');
  
  ctx->tt->cur->data_start = ctx->input->position;
  
  if(ctx->input->total_bytes)
    priv->data_size = ctx->input->total_bytes - ctx->tt->cur->data_start;
  
  /* Packet size will be at least 1024 bytes */
  
  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;

  gavl_track_set_duration(ctx->tt->cur->info,
                          ((int64_t)priv->data_size * (int64_t)GAVL_TIME_SCALE) / 
                          (s->container_bitrate / 8));

  ctx->index_mode = INDEX_MODE_SIMPLE;

  bgav_track_set_format(ctx->tt->cur, "AC3", NULL);
  
  return 1;
  
  fail:
  return 0;
  }

static gavl_source_status_t next_packet_a52(bgav_demuxer_context_t * ctx)
  {
  int dummy_brate, dummy_srate, dummy_flags;
  bgav_packet_t * p;
  bgav_stream_t * s;
  uint8_t test_data[7];
  int packet_size, i;
    
  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  
  p = bgav_stream_get_packet_write(s);
  
  for(i = 0; i < SYNC_BYTES; i++)
    {
    if(bgav_input_get_data(ctx->input, test_data, 7) < 7)
      return GAVL_SOURCE_EOF;
    
    packet_size = a52_syncinfo(test_data, &dummy_flags, &dummy_srate, &dummy_brate);
    if(packet_size)
      break;
    bgav_input_skip(ctx->input, 1);
    }

  if(!packet_size)
    return GAVL_SOURCE_EOF;

  p->duration = FRAME_SAMPLES;
  PACKET_SET_KEYFRAME(p);
  p->position = ctx->input->position;
  
  gavl_packet_alloc(p, packet_size);

  p->buf.len = bgav_input_read_data(ctx->input, p->buf.buf, packet_size);
  
  if(p->buf.len < packet_size)
    return GAVL_SOURCE_EOF;
  
  bgav_stream_done_packet_write(s, p);

  return GAVL_SOURCE_OK;
  }

static void seek_a52(bgav_demuxer_context_t * ctx, int64_t time, int scale)
  {
  int64_t file_position;
  a52_priv_t * priv;
  int64_t t;
  bgav_stream_t * s;

  priv = ctx->priv;

  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  
  file_position = (time * (s->container_bitrate / 8)) /
    scale;

  /* Calculate the time before we add the start offset */
  t = ((int64_t)file_position * scale) /
    (s->container_bitrate / 8);

  STREAM_SET_SYNC(s, gavl_time_rescale(scale, priv->samplerate, t));
  
  file_position += ctx->tt->cur->data_start;
  bgav_input_seek(ctx->input, file_position, SEEK_SET);
  }

static void close_a52(bgav_demuxer_context_t * ctx)
  {
  a52_priv_t * priv;
  priv = ctx->priv;
  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_a52 =
  {
    .probe =       probe_a52,
    .open =        open_a52,
    .next_packet = next_packet_a52,
    .seek =        seek_a52,
    .close =       close_a52
  };

