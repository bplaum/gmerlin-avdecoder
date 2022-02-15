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

#define SOL_DPCM    1
#define SOL_16BIT   4
#define SOL_STEREO 16


static int probe_sol(bgav_input_context_t * input)
  {
  uint16_t magic;
  uint8_t probe_data[6];

  if(bgav_input_get_data(input, probe_data, 6) < 6)
    return 0;

  magic = GAVL_PTR_2_16LE(&probe_data[0]);

  if((magic != 0x0B8D) && (magic != 0x0C0D) && (magic != 0x0C8D))
    return 0;

  if((probe_data[2] == 'S') &&
     (probe_data[3] == 'O') &&
     (probe_data[4] == 'L') &&
     (probe_data[5] == 0))
    return 1;
  return 0;
  }

static uint32_t get_fourcc_dpcm(int magic, int type, int * bits)
  {
  if(magic == 0x0B8D)
    return BGAV_MK_FOURCC('S','O','L','1');//SOL_DPCM_OLD;
  if(type & SOL_DPCM)
    {
    if(type & SOL_16BIT)
      return BGAV_MK_FOURCC('S','O','L','3');//SOL_DPCM_NEW16;
    else if(magic == 0x0C8D)
      return BGAV_MK_FOURCC('S','O','L','1');//SOL_DPCM_OLD;
    else return BGAV_MK_FOURCC('S','O','L','2');//SOL_DPCM_NEW8;
    }
  return 0;
  }

static uint32_t get_fourcc(int magic, int type, int * bits)
  {
  if(magic == 0x0B8D)
    {
    if (type & SOL_DPCM) return get_fourcc_dpcm(magic, type, bits);
    else
      {
      *bits = 8;
      return
        BGAV_WAVID_2_FOURCC(0x0001);
      }
    }
  if(type & SOL_DPCM)
    {
    if (type & SOL_16BIT) return get_fourcc_dpcm(magic, type, bits);
    else if (magic == 0x0C8D) return get_fourcc_dpcm(magic, type, bits);
    else return get_fourcc_dpcm(magic, type, bits);
    }
  if(type & SOL_16BIT)
    {
    *bits = 16;
    return BGAV_WAVID_2_FOURCC(0x0001);
    }
  *bits = 8;
  return BGAV_WAVID_2_FOURCC(0x0001);
  }

static int sol_channels(int magic, int type)
  {
  if (magic == 0x0B8D || !(type & SOL_STEREO)) return 1;
  return 2;
  }

static int open_sol(bgav_demuxer_context_t * ctx)
  {
  uint16_t magic;
  bgav_stream_t * s;
  uint16_t rate;
  uint8_t type;
  uint32_t size;
  
  if(!bgav_input_read_16_le(ctx->input, &magic))
    return 0;

  bgav_input_skip(ctx->input, 4);

  if(!bgav_input_read_16_le(ctx->input, &rate) ||
     !bgav_input_read_data(ctx->input, &type, 1) ||
     !bgav_input_read_32_le(ctx->input, &size))
    return 0;

  if(magic != 0x0B8D)
    bgav_input_skip(ctx->input, 1);

  
  
  ctx->tt = bgav_track_table_create(1);
  s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);

  s->data.audio.bits_per_sample = 16;
  s->fourcc = get_fourcc(magic, type, &s->data.audio.bits_per_sample);
  s->data.audio.format->samplerate = rate;
  s->data.audio.format->num_channels = sol_channels(magic, type);
  s->stream_id = 0;
  
  bgav_track_set_format(ctx->tt->cur, "Sierra SOL", NULL);
  return 1;
  }

#define MAX_SIZE 4096

static int next_packet_sol(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;
  bgav_packet_t * p;

  s = bgav_track_find_stream(ctx, 0);
  p = bgav_stream_get_packet_write(s);

  bgav_packet_alloc(p, MAX_SIZE);

  p->data_size = bgav_input_read_data(ctx->input, p->data, MAX_SIZE);

  if(!p->data_size)
    return 0;
  bgav_stream_done_packet_write(s, p);


  return 1;
  }


static void close_sol(bgav_demuxer_context_t * ctx)
  {
  }

const bgav_demuxer_t bgav_demuxer_sol =
  {
    .probe =       probe_sol,
    .open =        open_sol,
    .next_packet = next_packet_sol,
    .close =       close_sol
  };
