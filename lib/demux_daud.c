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
#include <string.h>
#include <stdio.h>

static int probe_daud(bgav_input_context_t * input)
  {
  char * pos;
  /* There seems to be no way to do proper probing of the stream.
     Therefore, we accept only local files with .dv as extension */

  if(input->location)
    {
    pos = strrchr(input->location, '.');
    if(!pos)
      return 0;
    if(!strcasecmp(pos, ".302"))
      return 1;
    }
  return 0;
  }


static int open_daud(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;

  ctx->tt = bgav_track_table_create(1);
  s = bgav_track_add_audio_stream(ctx->tt->tracks[0], ctx->opt);

  s->fourcc = BGAV_MK_FOURCC('d', 'a', 'u', 'd');
  s->data.audio.format->num_channels = 6;
  s->data.audio.format->samplerate = 96000;
  s->container_bitrate = 3 * 6 * 96000 * 8;
  s->data.audio.block_align = 3 * 6;
  s->data.audio.bits_per_sample = 24;

  bgav_track_set_format(ctx->tt->cur, "D-Cinema audio", NULL);
  
  ctx->tt->cur->data_start = 0;
  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
  
  return 1;
  }

static gavl_source_status_t next_packet_daud(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;
  bgav_packet_t * p;
  uint16_t size;

  if(!bgav_input_read_16_be(ctx->input, &size))
    return GAVL_SOURCE_EOF;
  bgav_input_skip(ctx->input, 2); // Unknown
  
  s = bgav_track_find_stream(ctx, 0);
  if(s)
    {
    p = bgav_stream_get_packet_write(s);
    gavl_packet_alloc(p, size);
    p->buf.len = bgav_input_read_data(ctx->input, p->buf.buf, size);
    if(!p->buf.len)
      return GAVL_SOURCE_EOF;
    bgav_stream_done_packet_write(s, p);
    }
  else
    bgav_input_skip(ctx->input, size);
  
  return GAVL_SOURCE_OK;
  }

static void close_daud(bgav_demuxer_context_t * ctx)
  {
  }

const bgav_demuxer_t bgav_demuxer_daud =
  {
    .probe =       probe_daud,
    .open =        open_daud,
    .next_packet = next_packet_daud,
    .close =       close_daud
  };
