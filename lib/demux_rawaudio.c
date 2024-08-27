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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gavl/metatags.h>

#define LOG_DOMAIN "rawaudio"
#define STREAM_ID 0
#define SAMPLES_PER_FRAME 512

static int probe_rawaudio(bgav_input_context_t * input)
  {
  const char * var = NULL;

  if(!gavl_metadata_get_src(&input->m, GAVL_META_SRC, 0, &var, NULL) || !var)
    return 0;
  
  if((!strncasecmp(var, "audio/L16", 9) ||
      !strncasecmp(var, "audio/L8", 8)))
    return 1; 

  return 0;
  }

static int open_rawaudio(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;
  const char * mimetype = NULL;
  const char * var;

  if(!gavl_metadata_get_src(&ctx->input->m, GAVL_META_SRC, 0, &mimetype, NULL) || !mimetype)
    return 0;
  
  /* Add stream */
  
  ctx->tt = bgav_track_table_create(1);

  s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
  s->stream_id = STREAM_ID;
  
  if(!(var = strstr(mimetype, "rate=")))
    return 0; // Required
  
  s->data.audio.format->samplerate = atoi(var + 5);

  if(!(var = strstr(mimetype, "channels=")))
    s->data.audio.format->num_channels = 1;
  else
    s->data.audio.format->num_channels = atoi(var + 9);

  if(!strncasecmp(mimetype, "audio/L16", 9))
    {
    s->data.audio.bits_per_sample = 16;
    s->data.audio.format->sample_format = GAVL_SAMPLE_S16;
    s->data.audio.block_align = 2 * s->data.audio.format->num_channels;
    s->fourcc = BGAV_MK_FOURCC('t', 'w', 'o', 's');
    }
  else if(!strncasecmp(mimetype, "audio/L8", 8))
    {
    s->data.audio.bits_per_sample = 8;
    s->data.audio.format->sample_format = GAVL_SAMPLE_U8;
    s->data.audio.block_align = s->data.audio.format->num_channels;
    s->fourcc = BGAV_WAVID_2_FOURCC(0x01);
    }

  s->data.audio.format->interleave_mode = GAVL_INTERLEAVE_ALL;
  
  switch(s->data.audio.format->num_channels)
    {
    case 1:
      s->data.audio.format->channel_locations[0] = GAVL_CHID_FRONT_CENTER;
      break;
    case 2:
      s->data.audio.format->channel_locations[0] = GAVL_CHID_FRONT_LEFT;
      s->data.audio.format->channel_locations[1] = GAVL_CHID_FRONT_RIGHT;
      break;
    case 3:
      s->data.audio.format->channel_locations[0] = GAVL_CHID_FRONT_LEFT;
      s->data.audio.format->channel_locations[1] = GAVL_CHID_FRONT_RIGHT;
      s->data.audio.format->channel_locations[2] = GAVL_CHID_FRONT_CENTER;
      break;
    case 4: /* Note: 4 channels can also be "left center right surround" but we
               believe, that quad is more common */
      s->data.audio.format->channel_locations[0] = GAVL_CHID_FRONT_LEFT;
      s->data.audio.format->channel_locations[1] = GAVL_CHID_FRONT_RIGHT;
      s->data.audio.format->channel_locations[2] = GAVL_CHID_REAR_LEFT;
      s->data.audio.format->channel_locations[3] = GAVL_CHID_REAR_RIGHT;
      break;
    case 5:
      s->data.audio.format->channel_locations[0] = GAVL_CHID_FRONT_LEFT;
      s->data.audio.format->channel_locations[1] = GAVL_CHID_FRONT_RIGHT;
      s->data.audio.format->channel_locations[2] = GAVL_CHID_FRONT_CENTER;
      s->data.audio.format->channel_locations[3] = GAVL_CHID_SIDE_LEFT;
      s->data.audio.format->channel_locations[4] = GAVL_CHID_SIDE_RIGHT;
      break;
    case 6:
      s->data.audio.format->channel_locations[0] = GAVL_CHID_FRONT_LEFT;
      s->data.audio.format->channel_locations[1] = GAVL_CHID_FRONT_CENTER_LEFT;
      s->data.audio.format->channel_locations[2] = GAVL_CHID_FRONT_CENTER;
      s->data.audio.format->channel_locations[3] = GAVL_CHID_FRONT_RIGHT;
      s->data.audio.format->channel_locations[4] = GAVL_CHID_FRONT_CENTER_RIGHT;
      s->data.audio.format->channel_locations[5] = GAVL_CHID_REAR_CENTER;
      break;
    }

  if((ctx->input->total_bytes > 0))
    {
    s->stats.pts_start = 0;
    s->stats.pts_end     = ctx->input->total_bytes / s->data.audio.block_align;
    s->stats.total_bytes = ctx->input->total_bytes;

    if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
      ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
    }

  ctx->tt->cur->data_start = 0;
  ctx->flags |= BGAV_DEMUXER_SAMPLE_ACCURATE;

  
  bgav_track_set_format(ctx->tt->cur, "LPCM", mimetype);
  return 1;
  }

static gavl_source_status_t next_packet_rawaudio(bgav_demuxer_context_t * ctx)
  {
  bgav_packet_t * p;
  bgav_stream_t * s;

  int bytes_to_read;
  
  //  priv = ctx->priv;
  
  s = bgav_track_find_stream(ctx, STREAM_ID);
  
  if(!s)
    return GAVL_SOURCE_OK;
  
  bytes_to_read = SAMPLES_PER_FRAME * s->data.audio.block_align;
  if(ctx->input->total_bytes &&
     (ctx->input->position + bytes_to_read >= ctx->input->total_bytes))
    bytes_to_read = ctx->input->total_bytes - ctx->input->position;
  
  if(bytes_to_read <= 0)
    return GAVL_SOURCE_EOF; // EOF
  
  p = bgav_stream_get_packet_write(s);
  
  p->pts = ctx->input->position / s->data.audio.block_align;

  gavl_packet_alloc(p, bytes_to_read);
  p->buf.len = bgav_input_read_data(ctx->input, p->buf.buf, bytes_to_read);
  p->duration = p->buf.len/s->data.audio.block_align;
  
  PACKET_SET_KEYFRAME(p);
  
  if(!p->buf.len)
    return GAVL_SOURCE_EOF;
  
  bgav_stream_done_packet_write(s, p);
  return GAVL_SOURCE_OK;
  }

static void seek_rawaudio(bgav_demuxer_context_t * ctx, int64_t time, int scale)
  {
  int64_t file_position;
  bgav_stream_t * s;
  
  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);

  if(s->data.audio.bits_per_sample)
    {
    file_position = s->data.audio.block_align * gavl_time_rescale(scale,
                                                                  s->data.audio.format->samplerate,
                                                                  time);
    }
  else
    {
    file_position = (gavl_time_unscale(scale, time) * (s->codec_bitrate / 8)) / scale;
    file_position /= s->data.audio.block_align;
    file_position *= s->data.audio.block_align;
    }
  /* Calculate the time before we add the start offset */
  STREAM_SET_SYNC(s, (int64_t)file_position / s->data.audio.block_align);
  
  bgav_input_seek(ctx->input, file_position, SEEK_SET);
  }

static void close_rawaudio(bgav_demuxer_context_t * ctx)
  {
  }


const bgav_demuxer_t bgav_demuxer_rawaudio =
  {
    .probe =       probe_rawaudio,
    .open =        open_rawaudio,
    .next_packet = next_packet_rawaudio,
    .seek =        seek_rawaudio,
    .close =       close_rawaudio
  };
