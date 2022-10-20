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
#include <stdio.h>
#include <string.h>

#include <cue.h>

#define LOG_DOMAIN "wavpack"

typedef struct
  {
  uint32_t fourcc;
  uint32_t block_size;
  uint16_t version;
  uint8_t  block_index_u8;
  uint8_t  total_samples_u8;
  uint32_t total_samples;
  uint32_t block_index;
  uint32_t block_samples;
  uint32_t flags;
  uint32_t crc;
  } wvpk_header_t;

typedef struct
  {
  int64_t pts;
  } wvpk_priv_t;

#define HEADER_SIZE 32

static const int wv_rates[16] = {
     6000,  8000,  9600, 11025, 12000, 16000, 22050, 24000,
    32000, 44100, 48000, 64000, 88200, 96000, 192000, -1
};

#define WV_MONO   (1<<2)
#define WV_HYBRID (1<<3)
#define WV_JOINT  (1<<4)
#define WV_CROSSD (1<<5)
#define WV_HSHAPE (1<<6)
#define WV_FLOAT  (1<<7)
#define WV_INT32  (1<<8)
#define WV_HBR    (1<<9)
#define WV_HBAL   (1<<10)
#define WV_MCINIT (1<<11) // First block in sequence
#define WV_MCEND  (1<<12) // Last block in sequence

// #define WV_EXTRA_SIZE 12 /* Bytes from the header, which must be
//                            passed to the decoder */

static void parse_header(wvpk_header_t * ret, uint8_t * data)
  {
  ret->fourcc           = BGAV_PTR_2_FOURCC(data); data+=4;
  ret->block_size       = GAVL_PTR_2_32LE(data); data+=4;
  ret->version          = GAVL_PTR_2_16LE(data); data+=2;
  ret->block_index_u8   = *data; data++;
  ret->total_samples_u8 = *data; data++;
  ret->total_samples    = GAVL_PTR_2_32LE(data); data+=4;
  ret->block_index    = GAVL_PTR_2_32LE(data); data+=4;
  ret->block_samples      = GAVL_PTR_2_32LE(data); data+=4;
  ret->flags            = GAVL_PTR_2_32LE(data); data+=4;
  ret->crc              = GAVL_PTR_2_32LE(data);
  }

static void dump_header(wvpk_header_t * h)
  {
  bgav_dprintf("wavpack header\n");
  
  bgav_dprintf("  fourcc:          ");
  bgav_dump_fourcc(h->fourcc);
  bgav_dprintf("\n");

  bgav_dprintf("  block_size:        %d\n", h->block_size);
  bgav_dprintf("  version:           %d\n", h->version);
  bgav_dprintf("  h->block_index_u8: %d\n", h->block_index_u8);
  bgav_dprintf("  total_samples_u8:  %d\n", h->total_samples_u8);
  bgav_dprintf("  total_samples:     %d\n", h->total_samples);
  bgav_dprintf("  block_index:       %d\n", h->block_index);
  bgav_dprintf("  block_samples:     %d\n", h->block_samples);
  bgav_dprintf("  flags:             %08x\n", h->flags);
  bgav_dprintf("  crc:               %08x\n", h->crc);
  }

static int probe_wavpack(bgav_input_context_t * input)
  {
  uint32_t fourcc;
  if(bgav_input_get_fourcc(input, &fourcc) &&
     (fourcc == BGAV_MK_FOURCC('w','v','p','k')))
    return 1;
  return 0;
  }

static int open_wavpack(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;
  uint8_t header[HEADER_SIZE];
  wvpk_header_t h;
  wvpk_priv_t * priv;
  int len;
  gavl_buffer_t buf;
  priv = calloc(1, sizeof(*priv));

  ctx->priv = priv;
  
  if(bgav_input_get_data(ctx->input, header, HEADER_SIZE) < HEADER_SIZE)
    return 0;

  parse_header(&h, header);

  if(ctx->opt->dump_headers)
    dump_header(&h);

#if 0 // Not the demuxers business  
  /* Use header data to set up stream */
  if(h.flags & WV_FLOAT)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Floating point data is not supported");
    return 0;
    }
  
  if(h.flags & WV_HYBRID)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Hybrid coding mode is not supported");
    return 0;
    }
  
  if(h.flags & WV_INT32)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Integer point data is not supported");
    return 0;
    }
#endif
  
  /* Create the track and the stream */
  ctx->tt = bgav_track_table_create(1);
  s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);

  s->data.audio.format->samplerate   = wv_rates[(h.flags >> 23) & 0xF];
  s->fourcc = BGAV_MK_FOURCC('w','v','p','k');
  s->data.audio.bits_per_sample = ((h.flags & 3) + 1) << 3;

  bgav_track_set_format(ctx->tt->cur, "Wavpack", "audio/x-wavpack");
  
  s->stats.pts_end = h.total_samples;
  
  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;

  ctx->index_mode = INDEX_MODE_SIMPLE;

  /* Read remaining blocks for the case of multichannel files */
  
  s->data.audio.format->num_channels = 1 + !(h.flags & WV_MONO);

  gavl_buffer_init(&buf);

  len = HEADER_SIZE + h.block_size - 24 + HEADER_SIZE;
  
  while(!(h.flags & WV_MCEND))
    {
    gavl_buffer_alloc(&buf, len);

    if(bgav_input_get_data(ctx->input, buf.buf, len) < len)
      return 0;

    buf.len = len;

    parse_header(&h, buf.buf + (len - HEADER_SIZE));
    
    s->data.audio.format->num_channels += 1 + !(h.flags & WV_MONO);

    len += h.block_size - 24 + HEADER_SIZE;
    }

  gavl_buffer_free(&buf);
    
  bgav_demuxer_init_cue(ctx);
  
  return 1;
  }

static int next_packet_wavpack(bgav_demuxer_context_t * ctx)
  {
  uint8_t header[HEADER_SIZE];
  wvpk_header_t h;
  bgav_packet_t * p;
  bgav_stream_t * s;
  int size;
  int64_t pos;
  wvpk_priv_t * priv = ctx->priv;

  pos = ctx->input->position;
  
  if(bgav_input_read_data(ctx->input, header, HEADER_SIZE) < HEADER_SIZE)
    return 0; // EOF

  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  p = bgav_stream_get_packet_write(s);

  p->position = pos;

  
  /* The last 12 bytes of the header must be copied to the
     packet */

  parse_header(&h, header);
  //  dump_header(&h);
  
  if(h.fourcc != BGAV_MK_FOURCC('w', 'v', 'p', 'k'))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Lost sync");
    return 0;
    }
    
  size = h.block_size - 24;
  
  bgav_packet_alloc(p, size + HEADER_SIZE);
  
  memcpy(p->buf.buf, header, HEADER_SIZE);
  
  if(bgav_input_read_data(ctx->input, p->buf.buf + HEADER_SIZE, size) < size)
    return 0; // EOF
  
  p->buf.len = HEADER_SIZE + size;

  p->pts = priv->pts;
  p->duration = h.block_samples;
  priv->pts += h.block_samples;

  /* Read remaining blocks for multichannel files */

  while(!(h.flags & WV_MCEND))
    {
    if(bgav_input_read_data(ctx->input, header, HEADER_SIZE) < HEADER_SIZE)
      return 0; // EOF

    parse_header(&h, header);

    bgav_packet_alloc(p, p->buf.len + h.block_size - 24 + HEADER_SIZE);

    memcpy(p->buf.buf + p->buf.len, header, HEADER_SIZE);
    p->buf.len += HEADER_SIZE;

    if(bgav_input_read_data(ctx->input, p->buf.buf + p->buf.len, h.block_size - 24) < h.block_size - 24)
      return 0; // EOF
    
    }
  
  bgav_stream_done_packet_write(s, p);
  
  return 1;
  }

static void seek_wavpack(bgav_demuxer_context_t * ctx,
                         int64_t time, int scale)
  {
  int64_t time_scaled;
  bgav_stream_t * s;
  
  uint8_t header[HEADER_SIZE];
  wvpk_header_t h;
  wvpk_priv_t * priv = ctx->priv;
  
  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  
  priv->pts = 0;
  time_scaled = gavl_time_rescale(scale, s->timescale, time);
  
  bgav_input_seek(ctx->input, 0, SEEK_SET);

  while(1)
    {
    if(bgav_input_get_data(ctx->input, header, HEADER_SIZE) < HEADER_SIZE)
      return;
    parse_header(&h, header);
    if(priv->pts + h.block_samples > time_scaled)
      break;

    bgav_input_skip(ctx->input, HEADER_SIZE + h.block_size - 24);
    priv->pts += h.block_samples;

    while(!(h.flags & WV_MCEND))
      {
      if(bgav_input_get_data(ctx->input, header, HEADER_SIZE) < HEADER_SIZE)
        return; // EOF

      parse_header(&h, header);

      bgav_input_skip(ctx->input, HEADER_SIZE + h.block_size - 24);
      }
    
    }
  STREAM_SET_SYNC(s, priv->pts);
  }

static void close_wavpack(bgav_demuxer_context_t * ctx)
  {
  wvpk_priv_t * priv = ctx->priv;
  free(priv);
  }

static void resync_wavpack(bgav_demuxer_context_t * ctx, bgav_stream_t * s)
  {
  wvpk_priv_t * priv;
  priv = ctx->priv;
  priv->pts = STREAM_GET_SYNC(s);
  }

static int select_track_wavpack(bgav_demuxer_context_t * ctx, int track)
  {
  wvpk_priv_t * priv;
  priv = ctx->priv;
  priv->pts = 0;
  return 1;
  }


const bgav_demuxer_t bgav_demuxer_wavpack =
  {
    .probe =       probe_wavpack,
    .open =        open_wavpack,
    .select_track = select_track_wavpack,
    .next_packet = next_packet_wavpack,
    .seek =        seek_wavpack,
    .resync =      resync_wavpack,
    .close =       close_wavpack
  };
