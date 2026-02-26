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



#include <avdec_private.h>
#include <nanosoft.h>
#include <cue.h>

#include <stdlib.h>
#include <stdio.h>

#define STREAM_ID 0

#define LOG_DOMAIN "wav"

/* WAV demuxer */

typedef struct
  {
  int packet_size;
  
  int have_info; /* INFO chunk found */
  bgav_RIFFINFO_t * info;
  } wav_priv_t;

static int probe_wav(bgav_input_context_t * input)
  {
  uint8_t test_data[12];
  if(bgav_input_get_data(input, test_data, 12) < 12)
    return 0;
  if((test_data[0] ==  'R') &&
     (test_data[1] ==  'I') &&
     (test_data[2] ==  'F') &&
     (test_data[3] ==  'F') &&
     (test_data[8] ==  'W') &&
     (test_data[9] ==  'A') &&
     (test_data[10] == 'V') &&
     (test_data[11] == 'E'))
    return 1;
  return 0;
  }

static int find_tag(bgav_demuxer_context_t * ctx, uint32_t tag)
  {
  uint32_t fourcc;
  uint32_t size;
  wav_priv_t * priv;
  priv = ctx->priv;
  
  while(1)
    {
    /* We also could get an INFO chunk */
    if(bgav_RIFFINFO_probe(ctx->input))
      {
      priv->info = bgav_RIFFINFO_read(ctx->input);
      continue;
      }
    if(!bgav_input_read_fourcc(ctx->input, &fourcc) ||
       !bgav_input_read_32_le(ctx->input, &size))
      return -1;

    if(fourcc == tag)
      return size;
    
    bgav_input_skip(ctx->input, size);
    }
  return -1;
  }

static int open_wav(bgav_demuxer_context_t * ctx)
  {
  uint8_t * buf;
  
  uint32_t fourcc;
  //  int size;
  uint32_t file_size;
  int format_size;
  bgav_stream_t * s;

  bgav_WAVEFORMAT_t wf;
  wav_priv_t * priv;
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  /* Create track */

  ctx->tt = bgav_track_table_create(1);
  
  /* Recheck the header */
  
  if(!bgav_input_read_fourcc(ctx->input, &fourcc) ||
     (fourcc != BGAV_MK_FOURCC('R', 'I', 'F', 'F')))
    goto fail;
  
  if(!bgav_input_read_32_le(ctx->input, &file_size))
    goto fail;
    
  if(!bgav_input_read_fourcc(ctx->input, &fourcc) ||
     (fourcc != BGAV_MK_FOURCC('W', 'A', 'V', 'E')))
    goto fail;

  /* Search for the format tag */

  format_size = find_tag(ctx, BGAV_MK_FOURCC('f', 'm', 't', ' '));

  if(format_size <= 0)
    goto fail;
  
  s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
  s->stream_id = STREAM_ID;
  
  buf = malloc(format_size);
  if(bgav_input_read_data(ctx->input, buf, format_size) < format_size)
    goto fail;
  
  bgav_WAVEFORMAT_read(&wf, buf, format_size);

  if(bgav_options_get_bool(ctx->opt, BGAV_OPT_DUMP_HEADERS))
    bgav_WAVEFORMAT_dump(&wf);
  
  bgav_WAVEFORMAT_get_format(&wf, s);
  bgav_WAVEFORMAT_free(&wf);
  free(buf);
  s->stats.total_bytes = find_tag(ctx, BGAV_MK_FOURCC('d', 'a', 't', 'a'));

  if(s->stats.total_bytes < 0)
    goto fail;
  ctx->tt->cur->data_start = ctx->input->position;

  /* If we don't have an INFO chunk yet, it could come after the data chunk */

  if(!priv->info &&
     (ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE) &&
     (ctx->input->total_bytes - 12 > ctx->tt->cur->data_start + s->stats.total_bytes))
    {
    bgav_input_seek(ctx->input, ctx->tt->cur->data_start + s->stats.total_bytes, SEEK_SET);
    if(bgav_RIFFINFO_probe(ctx->input))
      {
      priv->info = bgav_RIFFINFO_read(ctx->input);
      }
    bgav_input_seek(ctx->input, ctx->tt->cur->data_start, SEEK_SET);
    }

  /* Convert info to metadata */

  if(priv->info)
    {
    bgav_RIFFINFO_get_metadata(priv->info, ctx->tt->cur->metadata);
    }

  /* Packet size will be at least 1024 bytes */

  if(!s->ci->block_align)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "BlockAlign is zero");
    goto fail;
    }
  
  priv->packet_size = ((1024 + s->ci->block_align - 1) / 
                       s->ci->block_align) * s->ci->block_align;

  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;

  bgav_track_set_format(ctx->tt->cur, "WAV", "audio/wav");
  
  if(s->data.audio.bits_per_sample)
    {
    ctx->flags |= BGAV_DEMUXER_SAMPLE_ACCURATE;
    s->stats.pts_end = s->stats.total_bytes / s->ci->block_align;
    }
  else
    gavl_track_set_duration(ctx->tt->cur->info,
                            ((int64_t)s->stats.total_bytes * (int64_t)GAVL_TIME_SCALE) / 
                            (s->codec_bitrate / 8));
  
  return 1;
  
  fail:
  return 0;
  }

static gavl_source_status_t next_packet_wav(bgav_demuxer_context_t * ctx)
  {
  bgav_packet_t * p;
  bgav_stream_t * s;
  wav_priv_t * priv;
  int bytes_to_read;
  
  priv = ctx->priv;
  
  s = bgav_track_find_stream(ctx, STREAM_ID);
  
  if(!s)
    return GAVL_SOURCE_OK;

  bytes_to_read = priv->packet_size;
  if(ctx->input->position + bytes_to_read >=
     ctx->tt->cur->data_start + s->stats.total_bytes)
    {
    bytes_to_read = ctx->tt->cur->data_start + s->stats.total_bytes -
      ctx->input->position;
    }
  
  if(bytes_to_read <= 0)
    return GAVL_SOURCE_EOF; // EOF
  
  p = bgav_stream_get_packet_write(s);
  
  p->pts =
    ((ctx->input->position - ctx->tt->cur->data_start) * s->data.audio.format->samplerate) /
    (s->codec_bitrate / 8);
  
  gavl_packet_alloc(p, priv->packet_size);
    
  p->buf.len = bgav_input_read_data(ctx->input, p->buf.buf, bytes_to_read);

  PACKET_SET_KEYFRAME(p);
  
  if(!p->buf.len)
    return GAVL_SOURCE_EOF;
  
  bgav_stream_done_packet_write(s, p);
  
  return GAVL_SOURCE_OK;
  }

static void seek_wav(bgav_demuxer_context_t * ctx, int64_t time, int scale)
  {
  int64_t file_position;
  bgav_stream_t * s;
  
  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  
  if(s->data.audio.bits_per_sample)
    {
    file_position = s->ci->block_align * gavl_time_rescale(scale,
                                                                  s->data.audio.format->samplerate,
                                                                  time);
    }
  else
    {
    file_position = (gavl_time_unscale(scale, time) * (s->codec_bitrate / 8)) / scale;
    file_position /= s->ci->block_align;
    file_position *= s->ci->block_align;
    }
  /* Calculate the time before we add the start offset */
  STREAM_SET_SYNC(s, ((int64_t)file_position * s->data.audio.format->samplerate) /
    (s->codec_bitrate / 8));
  
  file_position += ctx->tt->cur->data_start;
  bgav_input_seek(ctx->input, file_position, SEEK_SET);
  }

static void close_wav(bgav_demuxer_context_t * ctx)
  {
  wav_priv_t * priv;
  priv = ctx->priv;
  
  if(priv->info)
    bgav_RIFFINFO_destroy(priv->info);

  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_wav =
  {
    .probe =       probe_wav,
    .open =        open_wav,
    .next_packet = next_packet_wav,
    .seek =        seek_wav,
    .close =       close_wav
  };

