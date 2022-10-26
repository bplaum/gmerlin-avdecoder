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

#define LOG_DOMAIN "voc"

#define VOC_TYPE_EOF              0x00
#define VOC_TYPE_VOICE_DATA       0x01
#define VOC_TYPE_VOICE_DATA_CONT  0x02
#define VOC_TYPE_SILENCE          0x03
#define VOC_TYPE_MARKER           0x04
#define VOC_TYPE_ASCII            0x05
#define VOC_TYPE_REPETITION_START 0x06
#define VOC_TYPE_REPETITION_END   0x07
#define VOC_TYPE_EXTENDED         0x08
#define VOC_TYPE_NEW_VOICE_DATA   0x09

static char const * const voc_magic = "Creative Voice File\x1A";
#define PROBE_LEN 26

#define MAX_PACKET_LEN 2048

/* Common chunk format */

typedef struct
  {
  uint8_t type;
  uint32_t len;
  } chunk_header_t;

static int read_chunk_header(bgav_input_context_t * input,
                             chunk_header_t * ret)
  {
  if(!bgav_input_read_data(input, &ret->type, 1))
    return 0;

  if(ret->type == VOC_TYPE_EOF)
    return 0; /* EOF */

  if(!bgav_input_read_24_le(input, &ret->len))
    return 0;

  return 1;
  }

/* Fourccs */

static const struct
  {
  int codec_id;
  uint32_t fourcc;
  int bits_per_sample;
  }
voc_codec_tags[] = {
  {0x00,   BGAV_WAVID_2_FOURCC(0x0001),      8 }, // CODEC_ID_PCM_U8
  {0x01,   BGAV_MK_FOURCC('S','B','P','4'),  4 }, // CODEC_ID_ADPCM_SBPRO_4
  {0x02,   BGAV_MK_FOURCC('S','B','P','3'),  3 }, // CODEC_ID_ADPCM_SBPRO_3
  {0x03,   BGAV_MK_FOURCC('S','B','P','2'),  2 }, // CODEC_ID_ADPCM_SBPRO_2
  {0x04,   BGAV_WAVID_2_FOURCC(0x0001),     16 }, // CODEC_ID_PCM_S16LE
  {0x06,   BGAV_MK_FOURCC('a','l','a','w'),  8 }, // CODEC_ID_PCM_ALAW
  {0x07,   BGAV_MK_FOURCC('u','l','a','w'),  8 }, // CODEC_ID_PCM_MULAW
  {0x0200, BGAV_MK_FOURCC('a','d','c','t'),  4 }, // CODEC_ID_ADPCM_CT
};

static int set_codec(int codec_id, bgav_stream_t * s, int set_bits)
  {
  int i;
  for(i = 0; i < sizeof(voc_codec_tags)/sizeof(voc_codec_tags[0]); i++)
    {
    if(voc_codec_tags[i].codec_id == codec_id)
      {
      s->fourcc                     = voc_codec_tags[i].fourcc;
      if(set_bits)
        s->data.audio.bits_per_sample = voc_codec_tags[i].bits_per_sample;
      return 1;
      }
    }
  return 0;
  }

/* */

typedef struct
  {
  int remaining_bytes;
  } voc_priv_t;


static int probe_voc(bgav_input_context_t * input)
  {
  uint8_t test_data[PROBE_LEN];
  int version, check;

  if(bgav_input_get_data(input, test_data, PROBE_LEN) < PROBE_LEN)
    return 0;

  if(memcmp(test_data, voc_magic, 20))
    return 0;

  version = test_data[22] | (test_data[23] << 8);
  check   = test_data[24] | (test_data[25] << 8);

  if (~version + 0x1234 != check)
    return 0;
  
  return 1;
  }

static int open_voc(bgav_demuxer_context_t * ctx)
  {
  int len;
  uint8_t test_data[PROBE_LEN];
  chunk_header_t h;
  int64_t pos;
  bgav_stream_t * s;
  uint8_t tmp_8;
  uint16_t tmp_16;
  uint32_t tmp_32;
  int done;
  
  int have_extended_info = 0;
  int frequency_divisor;
  voc_priv_t * priv;
  
  if(bgav_input_read_data(ctx->input, test_data, PROBE_LEN) < PROBE_LEN)
    return 0;

  len = test_data[20] | (test_data[21] << 8);

  if(len > PROBE_LEN)
    bgav_input_skip(ctx->input, len - PROBE_LEN);

  /* Create generic data */
  ctx->tt = bgav_track_table_create(1);
  s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);

  /* Private data */
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
  
  done = 0;
  while(!done)
    {
    if(!read_chunk_header(ctx->input, &h))
      return 0;

    switch(h.type)
      {
      case VOC_TYPE_VOICE_DATA:
        pos = ctx->input->position;
        
        if(!have_extended_info)
          {
          if(!bgav_input_read_data(ctx->input, &tmp_8, 1))
            return 0;
          s->data.audio.format->samplerate = 1000000 / (256 - tmp_8);
          if(!bgav_input_read_data(ctx->input, &tmp_8, 1))
            return 0;
          if(!set_codec(tmp_8, s, 1))
            return 0;
          s->data.audio.format->num_channels = 1;
          }
        else
          bgav_input_skip(ctx->input, 2);
        priv->remaining_bytes = h.len - (ctx->input->position - pos);
        done = 1;
        break;
      case VOC_TYPE_EXTENDED: /* Extended data, followed by Voice Data */
        if(!bgav_input_read_16_le(ctx->input, &tmp_16))
          return 0;
        frequency_divisor = tmp_16;
        
        if(!bgav_input_read_data(ctx->input, &tmp_8, 1))
          return 0;
        if(!set_codec(tmp_8, s, 1))
          return 0;

        if(!bgav_input_read_data(ctx->input, &tmp_8, 1))
          return 0;
        s->data.audio.format->num_channels = tmp_8+1;
        
        s->data.audio.format->samplerate =
          256000000/(s->data.audio.format->num_channels * (65536 - frequency_divisor));
        have_extended_info = 1;
        break;
      case VOC_TYPE_NEW_VOICE_DATA: /* New format */
        pos = ctx->input->position;
        if(!bgav_input_read_32_le(ctx->input, &tmp_32))
          return 0;
        s->data.audio.format->samplerate = tmp_32;

        if(!bgav_input_read_data(ctx->input, &tmp_8, 1))
          return 0;
        s->data.audio.bits_per_sample = tmp_8;

        if(!bgav_input_read_data(ctx->input, &tmp_8, 1))
          return 0;
        s->data.audio.format->num_channels = tmp_8;
        
        if(!bgav_input_read_16_le(ctx->input, &tmp_16))
          return 0;

        if(!set_codec(tmp_16, s, 0))
          return 0;
        priv->remaining_bytes = h.len - (ctx->input->position - pos);
        done = 1;
        break;
      default:
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
                 "Skipping %d bytes of chunk type %02x",
                 h.len, h.type);
        bgav_input_skip(ctx->input, h.len);
        break;
      }
    }

  bgav_track_set_format(ctx->tt->cur, "VOC", NULL);
  
  return 1;
  }

static gavl_source_status_t next_packet_voc(bgav_demuxer_context_t * ctx)
  {
  int bytes_to_read;
  voc_priv_t * priv;
  bgav_stream_t * s;
  bgav_packet_t * p;
  chunk_header_t h;

  priv = ctx->priv;

  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  
  while(!priv->remaining_bytes)
    {
    /* Look if we have something else to read */
    if(!read_chunk_header(ctx->input, &h))
      return GAVL_SOURCE_EOF; /* EOF */
    
    switch(h.type)
      {
      case VOC_TYPE_NEW_VOICE_DATA:
      case VOC_TYPE_VOICE_DATA:
        /* New sound data, probably in a different format -> exit here */
        return GAVL_SOURCE_EOF;
      case VOC_TYPE_VOICE_DATA_CONT:
        /* Same format as before */
        priv->remaining_bytes = h.len;
        break;
      case VOC_TYPE_SILENCE:
      case VOC_TYPE_MARKER:
      case VOC_TYPE_ASCII:
      case VOC_TYPE_REPETITION_START:
      case VOC_TYPE_REPETITION_END:
      case VOC_TYPE_EXTENDED:
        bgav_input_skip(ctx->input, h.len);
        break;
      }
    }

  bytes_to_read = priv->remaining_bytes;
  if(bytes_to_read > MAX_PACKET_LEN)
    bytes_to_read = MAX_PACKET_LEN;

  p = bgav_stream_get_packet_write(s);
  bgav_packet_alloc(p, bytes_to_read);

  p->buf.len = bgav_input_read_data(ctx->input, p->buf.buf, bytes_to_read);
  bgav_stream_done_packet_write(s, p);

  if(!p->buf.len)
    return GAVL_SOURCE_EOF;
  
  return GAVL_SOURCE_OK;
  }

#if 0 // Seeking in VOCs is doomed to failure

static void seek_voc(bgav_demuxer_context_t * ctx, gavl_time_t time)
  {
  
  }
#endif

static void close_voc(bgav_demuxer_context_t * ctx)
  {
  voc_priv_t * priv;
  priv = ctx->priv;
  
  if(priv)
    free(priv);
  }

const bgav_demuxer_t bgav_demuxer_voc =
  {
    .probe =       probe_voc,
    .open =        open_voc,
    .next_packet = next_packet_voc,
    //    .seek =        seek_voc,
    .close =       close_voc
  };
