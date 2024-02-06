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

#define AUDIO_ID 0
#define VIDEO_ID 1


static const uint8_t signature[8] = { 0x00, 0x0a, 'S', 'M','J','P','E','G' };

static int probe_smjpeg(bgav_input_context_t * input)
  {
  uint8_t probe_data[8];
  if(bgav_input_get_data(input, probe_data, 8) < 8)
    return 0;
  if(!memcmp(probe_data, signature, 8))
    return 1;
  else
    return 0;
  }

static int open_smjpeg(bgav_demuxer_context_t * ctx)
  {
  int done = 0;
  uint32_t tmp_32;
  uint16_t tmp_16;
  uint8_t tmp_8;
  uint32_t fourcc, length;
  bgav_stream_t * s;
  /* Skip signature and version */
  
  bgav_input_skip(ctx->input, 12);

  /* Create track table */
  ctx->tt = bgav_track_table_create(1);
  
  
  /* Duration in milliseconds */
  if(!bgav_input_read_32_be(ctx->input, &tmp_32))
    return 0;

  gavl_track_set_duration(ctx->tt->cur->info, gavl_time_unscale(1000, tmp_32));
  
  while(!done)
    {
    if(!bgav_input_read_fourcc(ctx->input, &fourcc))
      return 0;
    switch(fourcc)
      {
      case BGAV_MK_FOURCC('_','T','X','T'):
        if(!bgav_input_read_32_be(ctx->input, &length))
          return 0;
        bgav_input_skip(ctx->input, length);
        break;
      case BGAV_MK_FOURCC('_','S','N','D'):
        if(!bgav_input_read_32_be(ctx->input, &length))
          return 0;

        s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
        s->stream_id = AUDIO_ID;
        s->timescale = 1000;
        
        if(!bgav_input_read_16_be(ctx->input, &tmp_16))
          return 0;
        s->data.audio.format->samplerate = tmp_16;

        if(!bgav_input_read_data(ctx->input, &tmp_8, 1))
          return 0;
        s->data.audio.bits_per_sample = tmp_8;

        if(!bgav_input_read_data(ctx->input, &tmp_8, 1))
          return 0;
        s->data.audio.format->num_channels = tmp_8;

        if(!bgav_input_read_fourcc(ctx->input, &fourcc))
          return 0;
        
        if(fourcc == BGAV_MK_FOURCC('N','O','N','E'))
          s->fourcc = BGAV_WAVID_2_FOURCC(0x0001);
        else if(fourcc == BGAV_MK_FOURCC('A','P','C','M'))
          s->fourcc = BGAV_MK_FOURCC('S','M','J','A');
        if(length > 8)
          bgav_input_skip(ctx->input, length - 8);
        break;
      case BGAV_MK_FOURCC('_','V','I','D'):
        if(!bgav_input_read_32_be(ctx->input, &length))
          return 0;

        s = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
        s->stream_id = VIDEO_ID;

        bgav_input_skip(ctx->input, 4); // Number of frames

        if(!bgav_input_read_16_be(ctx->input, &tmp_16))
          return 0;

        s->data.video.format->image_width = tmp_16;
        s->data.video.format->frame_width = tmp_16;

        if(!bgav_input_read_16_be(ctx->input, &tmp_16))
          return 0;

        s->data.video.format->image_height = tmp_16;
        s->data.video.format->frame_height = tmp_16;
        s->data.video.format->pixel_width  = 1;
        s->data.video.format->pixel_height = 1;
        s->data.video.format->timescale = 1000;
        s->data.video.format->framerate_mode = GAVL_FRAMERATE_VARIABLE;
        s->flags |= STREAM_NO_DURATIONS;
        
        if(!bgav_input_read_fourcc(ctx->input, &fourcc))
          return 0;

        s->fourcc = fourcc;
        
        if(length > 12)
          bgav_input_skip(ctx->input, length - 12);
        
        break;
      case BGAV_MK_FOURCC('H','E','N','D'):
        done = 1;
        break;
      }
    }
  ctx->tt->cur->data_start = ctx->input->position;
  return 1;
  }

static gavl_source_status_t next_packet_smjpeg(bgav_demuxer_context_t * ctx)
  {
  uint32_t fourcc, length, timestamp;
  bgav_stream_t * s;
  bgav_packet_t * p;
  
  if(!bgav_input_read_fourcc(ctx->input, &fourcc) ||
     (fourcc == BGAV_MK_FOURCC('D','O','N','E')))
    return GAVL_SOURCE_EOF;
  
  if(!bgav_input_read_32_be(ctx->input, &timestamp) ||
     !bgav_input_read_32_be(ctx->input, &length))
    return GAVL_SOURCE_EOF;
  
  if(fourcc == BGAV_MK_FOURCC('s','n','d','D'))
    s = bgav_track_find_stream(ctx, AUDIO_ID);
  else if(fourcc == BGAV_MK_FOURCC('v','i','d','D'))
    s = bgav_track_find_stream(ctx, VIDEO_ID);
  else
    return GAVL_SOURCE_EOF;
  
  if(!s)
    bgav_input_skip(ctx->input, length);
  else
    {
    p = bgav_stream_get_packet_write(s);
    
    gavl_packet_alloc(p, length);
    if(bgav_input_read_data(ctx->input, p->buf.buf, length) < length)
      return GAVL_SOURCE_EOF;

    p->buf.len = length;
    p->pts = timestamp;
    bgav_stream_done_packet_write(s, p);
    }
  return GAVL_SOURCE_OK;
  }

static void close_smjpeg(bgav_demuxer_context_t * ctx)
  {

  }

const bgav_demuxer_t bgav_demuxer_smjpeg =
  {
    .probe =       probe_smjpeg,
    .open =        open_smjpeg,
    .next_packet = next_packet_smjpeg,
    .close =       close_smjpeg
  };
