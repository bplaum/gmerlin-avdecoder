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

#include <stdio.h>
#include <avdec_private.h>
#include <string.h>

#define GSM_BLOCK_SIZE 33
#define GSM_FRAME_SIZE 160

static int probe_gsm(bgav_input_context_t * input)
  {
    char * pos;
  /* There seems to be no way to do proper probing of the stream.
     Therefore, we accept only local files with .gsm as extension */

  if(input->filename)
    {
    pos = strrchr(input->filename, '.');
    if(!pos)
      return 0;
    if(!strcmp(pos, ".gsm"))
      return 1;
    }
  return 0;
  }

static int64_t bytes_2_samples(int64_t bytes)
  {
  return (bytes / GSM_BLOCK_SIZE) * GSM_FRAME_SIZE;
  }

static int open_gsm(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * as;
  
  /* Create track */
  ctx->tt = bgav_track_table_create(1);

  as = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
  as->fourcc = BGAV_MK_FOURCC('G', 'S', 'M', ' ');
  as->data.audio.format->samplerate = 8000;
  as->data.audio.format->num_channels = 1;

  
  as->data.audio.block_align = GSM_BLOCK_SIZE;
  
  if(ctx->input->total_bytes)
    {
    as->stats.pts_end = bytes_2_samples(ctx->input->total_bytes);
    if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
      ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
    }

  bgav_track_set_format(ctx->tt->cur, "GSM", NULL);

  ctx->data_start = ctx->input->position;
  ctx->flags |= BGAV_DEMUXER_HAS_DATA_START;
  
  return 1;
  }

static gavl_source_status_t next_packet_gsm(bgav_demuxer_context_t * ctx)
  {
  bgav_packet_t * p;
  bgav_stream_t * s;
  int bytes_read;
  
  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  p = bgav_stream_get_packet_write(s);

  bgav_packet_alloc(p, s->data.audio.block_align);

  p->pts = bytes_2_samples(ctx->input->position);
  PACKET_SET_KEYFRAME(p);
  bytes_read = bgav_input_read_data(ctx->input, p->buf.buf,
                                    s->data.audio.block_align);
  p->buf.len = bytes_read;

  if(bytes_read < s->data.audio.block_align)
    return GAVL_SOURCE_EOF;
  
  bgav_stream_done_packet_write(s, p);
  return GAVL_SOURCE_OK;
  }

static void seek_gsm(bgav_demuxer_context_t * ctx, int64_t time, int scale)
  {
  bgav_stream_t * s;
  int64_t position;
  int64_t sample;
  
  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  
  sample = gavl_time_rescale(scale, s->data.audio.format->samplerate, time); 
  sample /= GSM_FRAME_SIZE;
  position = sample * GSM_BLOCK_SIZE;
  sample *= GSM_FRAME_SIZE;
  
  bgav_input_seek(ctx->input, position, SEEK_SET);

  STREAM_SET_SYNC(s, sample);
  }

static void close_gsm(bgav_demuxer_context_t * ctx)
  {
  return;
  }

const bgav_demuxer_t bgav_demuxer_gsm =
  {
    .probe =       probe_gsm,
    .open =        open_gsm,
    .next_packet = next_packet_gsm,
    .seek =        seek_gsm,
    .close =       close_gsm
  };
