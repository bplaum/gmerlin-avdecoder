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


static int probe_shorten(bgav_input_context_t * input)
  {
  uint32_t fourcc;
  if(!bgav_input_get_fourcc(input, &fourcc))
    return 0;
  if(fourcc == BGAV_MK_FOURCC('a','j','k','g'))
    return 1;
  return 0;
  }

static int open_shorten(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;
  ctx->tt = bgav_track_table_create(1);

  s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
  s->fourcc = BGAV_MK_FOURCC('.','s','h','n');
  s->stream_id = 0;

  bgav_track_set_format(ctx->tt->cur, "Shorten", NULL);
  return 1;
  }

#define DATA_SIZE 4096

static int next_packet_shorten(bgav_demuxer_context_t * ctx)
  {
  bgav_packet_t * p;
  bgav_stream_t * s;
  s = bgav_track_find_stream(ctx, 0);
  if(!s)
    return 1;

  p = bgav_stream_get_packet_write(s);
  bgav_packet_alloc(p, DATA_SIZE);
  p->data_size = bgav_input_read_data(ctx->input, p->data, DATA_SIZE);
  if(!p->data_size)
    return 0;

  bgav_stream_done_packet_write(s, p);
  return 1;
  }

static void close_shorten(bgav_demuxer_context_t * ctx)
  {
  }

const bgav_demuxer_t bgav_demuxer_shorten =
  {
    .probe =       probe_shorten,
    .open =        open_shorten,
    .next_packet = next_packet_shorten,
    .close =       close_shorten
  };

  
