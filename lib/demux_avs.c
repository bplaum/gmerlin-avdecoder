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

#define LOG_DOMAIN "avs"

#define AUDIO_ID 0
#define VIDEO_ID 1

#define AVS_VIDEO     0x01
#define AVS_AUDIO     0x02
#define AVS_PALETTE   0x03
#define AVS_GAME_DATA 0x04

typedef struct
  {
  uint8_t type;
  uint32_t size;
  uint8_t frequency_divisor;
  uint8_t data_packing;
  } audio_header_t;

static int read_audio_header(bgav_input_context_t * input,
                             audio_header_t * ah)
  {
  return
    bgav_input_read_data(input, &ah->type, 1) &&
    bgav_input_read_24_le(input, &ah->size) &&
    bgav_input_read_data(input, &ah->frequency_divisor, 1) &&
    bgav_input_read_data(input, &ah->data_packing, 1);
  }
#if 0
static void dump_audio_header(audio_header_t * ah)
  {
  gavl_dprintf("AVS audio header\n");
  gavl_dprintf("  .type =              %d\n", ah->type);
  gavl_dprintf("  size:              %d\n", ah->size);
  gavl_dprintf("  frequency_divisor: %d\n", ah->frequency_divisor);
  gavl_dprintf("  data_packing:      %d\n", ah->data_packing);
  }
#endif
static const uint8_t avs_sig[10] =
  { 0x77, 0x57, 0x10, 0x00, 0x3e, 0x01, 0xc6, 0x00, 0x08, 0x00 };

typedef struct
  {
  int audio_bytes_remaining;
  int need_audio_format;
  } avs_priv_t;

static int probe_avs(bgav_input_context_t * input)
  {
  uint8_t data[10];
  if(bgav_input_get_data(input, data, 10) < 10)
    return 0;

  if(!memcmp(data, avs_sig, 10))
    return 1;
  
  return 0;
  }

static gavl_source_status_t next_packet_avs(bgav_demuxer_context_t * ctx)
  {
  audio_header_t ah;
  uint8_t  block_header[4];
  uint16_t frame_size;
  uint16_t block_size;
  uint16_t block_type;
  int64_t frame_start;
  int bytes_to_read;
  bgav_stream_t * as;
  bgav_stream_t * vs;
  
  avs_priv_t * priv;
  priv = ctx->priv;
  
  /* We process an entire frame */

  frame_start = ctx->input->position;
  
  /* Check if we have data left */
  if(!bgav_input_read_16_le(ctx->input, &frame_size) ||
     !frame_size)
    return GAVL_SOURCE_EOF;
  
  /* Actual frame size */
  if(!bgav_input_read_16_le(ctx->input, &frame_size))
    return GAVL_SOURCE_EOF;

  if(priv->need_audio_format)
    {
    as = bgav_track_find_stream_all(ctx->tt->cur, AUDIO_ID);
    vs = bgav_track_find_stream_all(ctx->tt->cur, VIDEO_ID);
    }
  else
    {
    as = bgav_track_find_stream(ctx, AUDIO_ID);
    vs = bgav_track_find_stream(ctx, VIDEO_ID);
    }
  
  while(ctx->input->position - frame_start < frame_size)
    {
    if(bgav_input_read_data(ctx->input, block_header, 4) < 4)
      return GAVL_SOURCE_EOF;
    block_type = GAVL_PTR_2_16LE(&block_header[0]);
    block_size = GAVL_PTR_2_16LE(&block_header[2]);
    switch(block_type >> 8)
      {
      case 0x01: /* Video data */

        if(!vs || priv->need_audio_format)
          {
          bgav_input_skip(ctx->input, block_size - 4);
          break;
          }
        if(!vs->packet)
          {
          vs->packet =
            bgav_stream_get_packet_write(vs);
          vs->packet->buf.len = 0;
          }

        gavl_packet_alloc(vs->packet, vs->packet->buf.len + block_size);

        memcpy(vs->packet->buf.buf + vs->packet->buf.len, block_header, 4);
        vs->packet->buf.len += 4;
        
        if(bgav_input_read_data(ctx->input,
                                vs->packet->buf.buf + vs->packet->buf.len,
                                block_size - 4) < block_size - 4)
          return GAVL_SOURCE_EOF;
        vs->packet->buf.len += (block_size - 4);
        vs->packet->pts = vs->in_position;
        bgav_stream_done_packet_write(vs, vs->packet);
        vs->packet = NULL;
        break;
      case 0x03: /* Palette data */

        if(!vs || priv->need_audio_format)
          {
          bgav_input_skip(ctx->input, block_size - 4);
          break;
          }
        if(vs->packet)
          {
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                   "2 Palette blocks without intermediate video block");
          return GAVL_SOURCE_EOF;
          }
        vs->packet =
          bgav_stream_get_packet_write(vs);
        vs->packet->buf.len = 0;
        
        gavl_packet_alloc(vs->packet, block_size);
        
        memcpy(vs->packet->buf.buf + vs->packet->buf.len, block_header, 4);
        vs->packet->buf.len += 4;

        if(bgav_input_read_data(ctx->input,
                                vs->packet->buf.buf + vs->packet->buf.len,
                                block_size - 4) < block_size - 4)
          return GAVL_SOURCE_EOF;
        vs->packet->buf.len += block_size - 4;
        break;
      case 0x02: /* Audio data */

        if(priv->need_audio_format)
          {
          /* Initialize audio stream */
          as = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
          as->stream_id = AUDIO_ID;
          as->fourcc = BGAV_WAVID_2_FOURCC(0x0001);
          as->data.audio.bits_per_sample = 8;
          as->data.audio.format->num_channels = 1;
          }
        if(!as)
          {
          bgav_input_skip(ctx->input, block_size - 4);
          break;
          }
        block_size -= 4;
        
        while(block_size)
          {
          /* Read audio header */
          if(!as->packet)
            {
            if(block_size < 6)
              {
              bgav_input_skip(ctx->input, block_size);
              break;
              }
            read_audio_header(ctx->input, &ah);
            block_size -= 6;
            
            if(!as->data.audio.format->samplerate)
              as->data.audio.format->samplerate =
                1000000 / (256 - ah.frequency_divisor);
            else
              {
              as->packet =
                bgav_stream_get_packet_write(as);
              as->packet->buf.len = 0;
              }
            //            dump_audio_header(&ah);
            priv->audio_bytes_remaining = ah.size - 2;
            }
          /* Read data */
          bytes_to_read = priv->audio_bytes_remaining;
          if(bytes_to_read > block_size)
            bytes_to_read = block_size;

          if(as->packet)
            {
            gavl_packet_alloc(as->packet,
                              as->packet->buf.len + bytes_to_read);
            if(bgav_input_read_data(ctx->input,
                                    as->packet->buf.buf + as->packet->buf.len,
                                    bytes_to_read) < bytes_to_read)
              return GAVL_SOURCE_EOF;
            
            as->packet->buf.len += bytes_to_read;
            }
          else
            bgav_input_skip(ctx->input, bytes_to_read);
          
          priv->audio_bytes_remaining -= bytes_to_read;
          block_size -= bytes_to_read;
          
          if(!priv->audio_bytes_remaining && as->packet)
            {
            bgav_stream_done_packet_write(as, as->packet);
            as->packet = NULL;
            }
          }
        break;
      case 0x04: /* Game data */
        bgav_input_skip(ctx->input, block_size - 4);
        break;
      default:
        bgav_input_skip(ctx->input, block_size - 4);
        break;
      }
    }
  
  return GAVL_SOURCE_OK;
  }

static int open_avs(bgav_demuxer_context_t * ctx)
  {
  uint8_t header[16];
  bgav_stream_t * s;
  avs_priv_t * priv;
  
  if(bgav_input_read_data(ctx->input, header, 16) < 16)
    return 0;

  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
  
  ctx->tt = bgav_track_table_create(1);
  
  /* Initialize video stream */
  s = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
  s->stream_id = VIDEO_ID;
  s->fourcc = BGAV_MK_FOURCC('A','V','S',' ');
  s->data.video.format->image_width = 318;
  s->data.video.format->frame_width = 318;

  s->data.video.format->image_height = 198;
  s->data.video.format->frame_height = 198;

  s->data.video.format->pixel_width  = 1;
  s->data.video.format->pixel_height = 1;

  s->data.video.format->timescale = GAVL_PTR_2_16LE(&header[10]);
  s->data.video.format->frame_duration = 1;
  s->data.video.depth = 8;
  
  s->stats.pts_end = GAVL_PTR_2_32LE(&header[12]);
  
  ctx->tt->cur->data_start = ctx->input->position;
  
#if 1
  priv->need_audio_format = 1;
  if(!next_packet_avs(ctx))
    return 0;
  priv->need_audio_format = 0;
#endif
  return 1;
  }


static void close_avs(bgav_demuxer_context_t * ctx)
  {
  avs_priv_t * priv;
  priv = ctx->priv;
  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_avs =
  {
    .probe =       probe_avs,
    .open =        open_avs,
    .next_packet = next_packet_avs,
    .close =       close_avs
  };
