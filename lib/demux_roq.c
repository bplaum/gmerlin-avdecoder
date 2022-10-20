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
#include <string.h>
#include <stdio.h>

#define LOG_DOMAIN "roq"

#define RoQ_INFO           0x1001
#define RoQ_QUAD_CODEBOOK  0x1002
#define RoQ_QUAD_VQ        0x1011
#define RoQ_SOUND_MONO     0x1020
#define RoQ_SOUND_STEREO   0x1021

#define RoQ_CHUNKS_TO_SCAN 30
#define RoQ_AUDIO_SAMPLE_RATE 22050

#define AUDIO_ID 0
#define VIDEO_ID 1

#define PREAMBLE_SIZE 8

typedef struct
  {
  uint16_t id;
  uint32_t size;
  uint16_t arg;
  } chunk_header_t;

static int read_chunk_header(bgav_input_context_t * input, chunk_header_t * h)
  {
  return bgav_input_read_16_le(input, &h->id) &&
    bgav_input_read_32_le(input, &h->size) &&
    bgav_input_read_16_le(input, &h->arg);
  }

static void parse_chunk_header(uint8_t * data, chunk_header_t * h)
  {
  h->id    = GAVL_PTR_2_16LE(data); data+=2;
  h->size  = GAVL_PTR_2_32LE(data); data+=4;
  h->arg   = GAVL_PTR_2_16LE(data);
  }

#if 0
static void dump_chunk_header(chunk_header_t * h)
  {
  bgav_dprintf("Roq chunk header\n");
  bgav_dprintf("  id:   %04x\n", h->id);
  bgav_dprintf("  size: %d\n", h->size);
  bgav_dprintf("  .arg =  %04x\n", h->arg);
  }
#endif

static const uint8_t roq_magic[6] =
  { 0x84, 0x10, 0xff, 0xff, 0xff, 0xff };

static int probe_roq(bgav_input_context_t * input)
  {
  uint8_t data[6];
  if(bgav_input_get_data(input, data, 6) < 6)
    return 0;

  if(memcmp(data, roq_magic, 6))
    return 0;

  return 1;
  }

static int open_roq(bgav_demuxer_context_t * ctx)
  {
  int i;
  chunk_header_t h;
  bgav_stream_t * s;
  uint16_t width = 0, height = 0, framerate;
  int num_channels = 0;
  
  
  /* We can play RoQ files only from seekable sources */
  if(!(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Cannot play Roq files from nonseekable source");
    return 0;
    }

  /* Skip magic */
  bgav_input_skip(ctx->input, 6);

  if(!bgav_input_read_16_le(ctx->input, &framerate))
    return 0;
  
  for(i = 0; i < RoQ_CHUNKS_TO_SCAN; i++)
    {
    if(!read_chunk_header(ctx->input, &h))
      break;

    switch(h.id)
      {
      case RoQ_INFO:
        if(!bgav_input_read_16_le(ctx->input, &width) ||
           !bgav_input_read_16_le(ctx->input, &height))
          return 0;
        bgav_input_skip(ctx->input, 4);
        break;
      case RoQ_QUAD_CODEBOOK:
      case RoQ_QUAD_VQ:
        bgav_input_skip(ctx->input, h.size);
        break;
      case RoQ_SOUND_MONO:
        num_channels = 1;
        bgav_input_skip(ctx->input, h.size);
        break;
      case RoQ_SOUND_STEREO:
        num_channels = 2;
        bgav_input_skip(ctx->input, h.size);
        break;
      default:
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Unknown Roq chunk %04x", h.id);
        return 0;
        break;
      }
    if(width && height && num_channels)
      break;
    }
  /* Seek back */
  bgav_input_seek(ctx->input, 8, SEEK_SET);

  /* Set up track table */
  ctx->tt = bgav_track_table_create(1);

  /* Set up audio stream */

  if(num_channels)
    {
    s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
    s->stream_id = AUDIO_ID;
    s->fourcc = BGAV_MK_FOURCC('R','O','Q','A');
    s->data.audio.format->num_channels = num_channels;
    s->data.audio.format->samplerate = RoQ_AUDIO_SAMPLE_RATE;
    s->data.audio.bits_per_sample = 16;
    s->data.audio.block_align = num_channels * s->data.audio.bits_per_sample;
    }

  if(width && height)
    {
    s = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
    s->stream_id = VIDEO_ID;
    s->fourcc = BGAV_MK_FOURCC('R','O','Q','V');
    s->data.video.format->image_width = width;
    s->data.video.format->image_height = height;

    s->data.video.format->frame_width = width;
    s->data.video.format->frame_height = height;

    s->data.video.format->pixel_width = 1;
    s->data.video.format->pixel_height = 1;
    
    s->data.video.format->timescale = framerate;
    s->data.video.format->frame_duration = 1;
    }

  bgav_track_set_format(ctx->tt->cur, "ID Roq", NULL);
  
  
  ctx->data_start = ctx->input->position;
  ctx->flags |= BGAV_DEMUXER_HAS_DATA_START;
  
  return 1;
  }

static int next_packet_roq(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;
  
  uint8_t preamble[PREAMBLE_SIZE];
  chunk_header_t h;
  int done = 0;
  bgav_packet_t * video_packet = NULL;
  bgav_packet_t * audio_packet = NULL;
  

  while(!done)
    {
    if(bgav_input_read_data(ctx->input, preamble, PREAMBLE_SIZE) < PREAMBLE_SIZE)
      return 0;
    parse_chunk_header(preamble, &h);
    switch(h.id)
      {
      case RoQ_INFO:
        bgav_input_skip(ctx->input, h.size);
        break;
      case RoQ_QUAD_CODEBOOK:
        s = bgav_track_find_stream(ctx, VIDEO_ID);
        if(!s)
          {
          bgav_input_skip(ctx->input, h.size);
          break;
          }
        video_packet = bgav_stream_get_packet_write(s);
        bgav_packet_alloc(video_packet, h.size + PREAMBLE_SIZE);
        video_packet->buf.len = 0;
        
        memcpy(video_packet->buf.buf, preamble, PREAMBLE_SIZE);
        video_packet->buf.len += PREAMBLE_SIZE;
        
        if(bgav_input_read_data(ctx->input,
                                video_packet->buf.buf + video_packet->buf.len,
                                h.size) < h.size)
          return 0;
        
        video_packet->buf.len += h.size;
        break;
      case RoQ_QUAD_VQ:
        s = bgav_track_find_stream(ctx, VIDEO_ID);
        if(!s)
          {
          bgav_input_skip(ctx->input, h.size);
          break;
          }
        if(!video_packet)
          {
          gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN,
                   "No CODEBOOK chunk before VQ chunk");
          video_packet = bgav_stream_get_packet_write(s);
          video_packet->buf.len = 0;
          }
        bgav_packet_alloc(video_packet,
                          video_packet->buf.len + h.size + PREAMBLE_SIZE);

        memcpy(video_packet->buf.buf + video_packet->buf.len, preamble, PREAMBLE_SIZE);
        video_packet->buf.len += PREAMBLE_SIZE;
        
        if(bgav_input_read_data(ctx->input,
                                video_packet->buf.buf + video_packet->buf.len,
                                h.size) < h.size)
          return 0;

        video_packet->buf.len += h.size;
        video_packet->pts = s->in_position;
        bgav_stream_done_packet_write(s, video_packet);
        video_packet = NULL;
        done = 1;
        break;
      case RoQ_SOUND_MONO:
      case RoQ_SOUND_STEREO:
        s = bgav_track_find_stream(ctx, AUDIO_ID);
        if(!s)
          {
          bgav_input_skip(ctx->input, h.size);
          break;
          }
        audio_packet = bgav_stream_get_packet_write(s);

        bgav_packet_alloc(audio_packet, h.size + PREAMBLE_SIZE);
        memcpy(audio_packet->buf.buf, preamble, 8);
        
        if(bgav_input_read_data(ctx->input,
                                audio_packet->buf.buf+PREAMBLE_SIZE,
                                h.size) < h.size)
          return 0;
        audio_packet->buf.len = h.size + PREAMBLE_SIZE;

        bgav_stream_done_packet_write(s, audio_packet);
        done = 1;
        break;
      default:
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Unknown chunk %04x", h.id);
        return 0;
      }
    }
  return 1;
  }

static void close_roq(bgav_demuxer_context_t * ctx)
  {
  }

const bgav_demuxer_t bgav_demuxer_roq =
  {
    .probe =       probe_roq,
    .open =        open_roq,
    .next_packet = next_packet_roq,
    .close =       close_roq
  };
