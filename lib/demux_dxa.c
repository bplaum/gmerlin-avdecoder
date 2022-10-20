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


#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#include <avdec_private.h>
#include <nanosoft.h>


#define LOG_DOMAIN "dxa"

#define DXA_EXTRA_SIZE 9

#define AUDIO_ID 0
#define VIDEO_ID 1

static int probe_dxa(bgav_input_context_t * input)
  {
  uint8_t probe_buffer[4];
  if(bgav_input_get_data(input, probe_buffer, 4) < 4)
    return 0;

  if((probe_buffer[0] == 'D') &&
     (probe_buffer[1] == 'E') &&
     (probe_buffer[2] == 'X') &&
     (probe_buffer[3] == 'A'))
    return 1;
  
  return 0;
  }

typedef struct
  {
  int frames;
  int current_frame;
  
  uint32_t audio_position;
  uint32_t video_position;
  
  uint32_t audio_start;
  uint32_t video_start;
  uint32_t audio_end;
  
  uint32_t audio_block_size;
  } dxa_t;

static int open_dxa(bgav_demuxer_context_t * ctx)
  {
  dxa_t * priv;
  uint8_t flags;
  int32_t fps;
  uint16_t w, h;
  int num, den;
  uint16_t frames;
  uint32_t fourcc;
  uint32_t size;
  uint8_t * buf;
  bgav_WAVEFORMAT_t wf;
    
  bgav_stream_t * as = NULL;
  bgav_stream_t * vs = NULL;

  if(!(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Cannot decode from nonseekable source");
    return 0;
    }
  
  /* Skip singature */
  bgav_input_skip(ctx->input, 4);
  
  /* Read video header */
  if(!bgav_input_read_data(ctx->input, &flags, 1) ||
     !bgav_input_read_16_be(ctx->input, &frames) ||
     !bgav_input_read_32_be(ctx->input, (uint32_t*)(&fps)) ||
     !bgav_input_read_16_be(ctx->input, &w) ||
     !bgav_input_read_16_be(ctx->input, &h))
    return 0;
  
  /* Sanity checks */
  if(!frames)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "File contains zero frames");
    return 0;
    }
  
  /* Create private data */
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  /* Initialize video stream */

  ctx->tt = bgav_track_table_create(1);
  vs = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
  vs->stream_id = VIDEO_ID;
  
  if(fps > 0)
    {
    den = 1000;
    num = fps;
    }
  else if (fps < 0)
    {
    den = 100000;
    num = -fps;
    }
  else
    {
    den = 10;
    num = 1;
    }

  /* flags & 0x80 means that image is interlaced,
   * flags & 0x40 means that image has double height
   * either way set true height
   */

  if(flags & 0xC0)
    h >>= 1;
  
  vs->data.video.format->image_width = w;
  vs->data.video.format->frame_width = w;

  vs->data.video.format->image_height = h;
  vs->data.video.format->frame_height = h;

  vs->data.video.format->pixel_width  = 1;
  vs->data.video.format->pixel_height = 1;
  
  
  vs->fourcc = BGAV_MK_FOURCC('D', 'X', 'A', ' ');

  vs->data.video.format->timescale      = den;
  vs->data.video.format->frame_duration = num;
  
  priv->frames = frames;

  vs->stats.pts_end =
    (int64_t)(priv->frames) * 
    vs->data.video.format->frame_duration;
    
  /* Check for audio stream */

  if(!bgav_input_read_fourcc(ctx->input, &fourcc))
    return 0;

  if(fourcc == BGAV_MK_FOURCC('W', 'A', 'V', 'E'))
    {
    
    
    if(!bgav_input_read_32_be(ctx->input, &size))
      return 0;
    priv->video_start = ctx->input->position + size;

    /* "RIFF....WAVEfmt " */
    bgav_input_skip(ctx->input, 16);

    if(!bgav_input_read_32_le(ctx->input, &size))
      return 0;

    buf = malloc(size);
    if(bgav_input_read_data(ctx->input, buf, size) < size)
      {
      free(buf);
      return 0;
      }

    as = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
    bgav_WAVEFORMAT_read(&wf, buf, size);
    bgav_WAVEFORMAT_get_format(&wf, as);
    free(buf);

    as->stream_id = AUDIO_ID;
    
    while(1)
      {
      if(!bgav_input_read_fourcc(ctx->input, &fourcc) ||
         !bgav_input_read_32_le(ctx->input, &size))
        return 0;
      if(fourcc == BGAV_MK_FOURCC('d', 'a', 't', 'a'))
        break;
      bgav_input_skip(ctx->input, size);
      }
    priv->audio_start     = ctx->input->position;
    priv->audio_end       = ctx->input->position + size;
    
    priv->audio_block_size      =
      ((4096+as->data.audio.block_align-1)/as->data.audio.block_align)*as->data.audio.block_align;
    }
  else
    priv->video_start    = ctx->input->position;
  
  priv->audio_position = priv->audio_start;
  priv->video_position = priv->video_start;

  bgav_track_set_format(ctx->tt->cur, "DXA", NULL);
  
  return 1;
  }

static int next_packet_dxa(bgav_demuxer_context_t * ctx)
  {
  dxa_t * priv;
  bgav_stream_t * s = NULL;
  bgav_packet_t * p = NULL;
  
  priv = ctx->priv;
  
  if((priv->audio_position >= priv->audio_end) &&
     (priv->current_frame >= priv->frames))
    return 0;

  /* Try audio stream */
  if(ctx->request_stream->type == GAVL_STREAM_AUDIO)
    {
    if(priv->audio_position < priv->audio_end)
      s = ctx->request_stream;
    }
  
  /* Try video stream */
  if(!s)
    {
    bgav_stream_t * vs;
    
    if(ctx->tt->cur->num_video_streams &&
       (vs = bgav_track_get_video_stream(ctx->tt->cur, 0)) &&
       (vs->action != BGAV_STREAM_MUTE) && 
       (priv->current_frame < priv->frames))
      s = vs;
    }
  
  if(!s)
    return 0;

  /* Read audio packet */
  if(s->type == GAVL_STREAM_AUDIO)
    {
    int bytes_to_read;
    bytes_to_read = priv->audio_block_size;
    if(bytes_to_read > priv->audio_end - priv->audio_position)
      bytes_to_read = priv->audio_end - priv->audio_position;
    
    p = bgav_stream_get_packet_write(s);
    bgav_packet_alloc(p, bytes_to_read);

    bgav_input_seek(ctx->input, priv->audio_position, SEEK_SET);
    p->buf.len = bgav_input_read_data(ctx->input, p->buf.buf, bytes_to_read);
    if(p->buf.len < bytes_to_read)
      return 0;
    
    bgav_stream_done_packet_write(s, p);

    priv->audio_position = ctx->input->position;
    }
  /* Read video packet */
  else
    {
    uint32_t size;
    uint8_t buf[DXA_EXTRA_SIZE], pal[768+4];
    uint8_t * ptr;
    int pal_size = 0;
    uint32_t fourcc;
    
    bgav_input_seek(ctx->input, priv->video_position, SEEK_SET);

    while(1)
      {
      if(!bgav_input_get_fourcc(ctx->input, &fourcc))
        return 0;

      switch(fourcc)
        {
        case BGAV_MK_FOURCC('N','U','L','L'):
          p = bgav_stream_get_packet_write(s);
          bgav_packet_alloc(p, 4 + pal_size);
          if(pal_size)
            memcpy(p->buf.buf, pal, pal_size);
          bgav_input_read_data(ctx->input, p->buf.buf + pal_size, 4);
          p->buf.len = 4 + pal_size;
          p->pts = s->data.video.format->frame_duration * priv->current_frame;
          
          bgav_stream_done_packet_write(s, p);
          
          priv->current_frame++;
          break;
        case BGAV_MK_FOURCC('C','M','A','P'):
          pal_size = 768+4;
          if(bgav_input_read_data(ctx->input, pal, pal_size) < pal_size)
            return 0;
          break;
        case BGAV_MK_FOURCC('F','R','A','M'):
          if(bgav_input_read_data(ctx->input, buf, DXA_EXTRA_SIZE) < DXA_EXTRA_SIZE)
            return 0;
          ptr = &buf[5];
          size = GAVL_PTR_2_32BE(ptr);

          p = bgav_stream_get_packet_write(s);

          bgav_packet_alloc(p, size + DXA_EXTRA_SIZE + pal_size);

          if(pal_size)
            memcpy(p->buf.buf, pal, pal_size);
          memcpy(p->buf.buf + pal_size, buf, DXA_EXTRA_SIZE);
          if(bgav_input_read_data(ctx->input, p->buf.buf + pal_size + DXA_EXTRA_SIZE, size) < size)
            return 0;
          
          p->buf.len = size + DXA_EXTRA_SIZE + pal_size;
          p->pts = s->data.video.format->frame_duration * priv->current_frame;
          
          bgav_stream_done_packet_write(s, p);

          priv->current_frame++;
          break;
        }
      if(p)
        break;
      }
    
    priv->video_position = ctx->input->position;
    }
  
  return 1;
  }

static int select_track_dxa(bgav_demuxer_context_t * ctx, int track)
  {
  dxa_t * priv;
  priv = ctx->priv;
  priv->audio_position = priv->audio_start;
  priv->video_position = priv->video_start;
  priv->current_frame = 0;
  
  return 1;
  }

static void close_dxa(bgav_demuxer_context_t * ctx)
  {
  dxa_t * priv = ctx->priv;
  if(priv)
    free(priv);
  }

const bgav_demuxer_t bgav_demuxer_dxa =
  {
    .probe =        probe_dxa,
    .open =         open_dxa,
    .select_track = select_track_dxa,
    .next_packet =  next_packet_dxa,
    .close =        close_dxa
  };
