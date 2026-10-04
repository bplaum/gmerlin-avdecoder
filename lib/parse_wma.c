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

#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <avdec_private.h>
#include <parser.h>
#include <gavl/log.h>
#define LOG_DOMAIN "wmaparser"

#define WMA_TAG_V1 0x0160
#define WMA_TAG_V2 0x0161
 
/* flags2 in the codec extradata */
#define WMA_FLAG_EXP_VLC        0x0001
#define WMA_FLAG_BIT_RESERVOIR  0x0002
#define WMA_FLAG_VARIABLE_BLOCK 0x0004
 

typedef struct
  {
  int version;               /* 1 or 2 */
  int channels;
  int sample_rate;
  int bit_rate;              /* bits per second */
  int block_align;           /* packet size in bytes */
  int frame_len_bits;        /* log2(samples per frame and channel) */
  int frame_len;             /* samples per frame and channel */
  int byte_offset_bits;      /* width of the bit offset field (minus 3) */
  int use_exp_vlc;
  int use_bit_reservoir;
  int use_variable_block_len;

  } wma_priv_t;

static uint16_t rd_le16(const uint8_t *p)
  {
  return (uint16_t)(p[0] | (p[1] << 8));
  }

static int ilog2_int(unsigned int v)
  {
  int n = 0;
  while (v >>= 1)
    n++;
  return n;
  }

/* Frame length (log2) as a function of sample rate, WMA v1/v2 */
static int wma_frame_len_bits(int sample_rate, int version)
  {
  if (sample_rate <= 16000)
    return 9;
  if(sample_rate <= 22050 || (sample_rate <= 32000 && version == 1))
    return 10;
  else
    return 11;
  }

/* MSB-first bit reader (WMA v1/v2 use big-endian bit order), n <= 32 */
static uint32_t rd_bits(const uint8_t *buf, size_t bitpos, int n)
  {
  uint32_t val = 0;
  while (n-- > 0) {
  val = (val << 1) | ((buf[bitpos >> 3] >> (7 - (bitpos & 7))) & 1);
  bitpos++;
  }
  return val;
  }
 

static int parse_frame_wma(bgav_packet_parser_t * parser, bgav_packet_t * p)
  {
  size_t hdr_bits;
  unsigned int nb_frames_field, bit_offset;
  int frames;

  wma_priv_t * ctx = parser->priv;
    
  if(!p || !p->buf.len)
    return 0;
  
  if(!ctx->use_bit_reservoir)
    {
    p->duration = ctx->frame_len;
    fprintf(stderr, "Parse wma: %"PRId64"\n", p->duration);
    return 1;
    }

  hdr_bits = 4 + 4 + (size_t)ctx->byte_offset_bits + 3;
  if(p->buf.len * 8 < hdr_bits)
    return 0;
 
  nb_frames_field = rd_bits(p->buf.buf, 4, 4);
  bit_offset      = rd_bits(p->buf.buf, 8, ctx->byte_offset_bits + 3);
 
  if(hdr_bits + bit_offset > p->buf.len * 8)
    return 0;
 
  frames = (int)nb_frames_field - (bit_offset == 0 ? 1 : 0);
  if (frames < 0 || (frames == 0 && bit_offset > 0))
    return 0;
  
  p->duration = frames * ctx->frame_len;
  //  fprintf(stderr, "Parse wma: %"PRId64"\n", p->duration);
  return 1;
  }

static void cleanup_wma(bgav_packet_parser_t * parser)
  {
  free(parser->priv);
  }


void bgav_packet_parser_init_wma(bgav_packet_parser_t * parser)
  {
  wma_priv_t * ctx;
  double bps;
  uint16_t flags2;
  
  uint32_t compression_tag = gavl_stream_get_compression_tag(parser->info);

  ctx = calloc(1, sizeof(*ctx));
  parser->priv = ctx;

  if(BGAV_FOURCC_2_WAVID(compression_tag) == WMA_TAG_V1)
    ctx->version = 1;
  else
    ctx->version = 2;
  
  
  if(!parser->ci->codec_header.len)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "No extradata found");
    return;
    }

    
  ctx->sample_rate = parser->afmt->samplerate;
  ctx->channels    = parser->afmt->num_channels;
  ctx->bit_rate    = parser->ci->bitrate;
  ctx->block_align = parser->ci->block_align;

  if(ctx->channels < 1 || ctx->sample_rate <= 0 ||
     ctx->bit_rate <= 0 || ctx->block_align <= 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Invalid stream setup");
    return;
    }

  if(parser->ci->codec_header.len < ((ctx->version == 1) ? 4 : 6))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Too little extradata");
    return;
    }
  
  flags2 = rd_le16(parser->ci->codec_header.buf + (ctx->version == 1 ? 2 : 4));

  ctx->use_exp_vlc           = !!(flags2 & WMA_FLAG_EXP_VLC);
  ctx->use_bit_reservoir     = !!(flags2 & WMA_FLAG_BIT_RESERVOIR);
  ctx->use_variable_block_len = !!(flags2 & WMA_FLAG_VARIABLE_BLOCK);
   
  ctx->frame_len_bits = wma_frame_len_bits(ctx->sample_rate, ctx->version);
  ctx->frame_len      = 1 << ctx->frame_len_bits;
   
  /* width of the "byte offset" field in the superframe header */
  bps = (double)ctx->bit_rate / ((double)ctx->channels * ctx->sample_rate);
  ctx->byte_offset_bits =
    ilog2_int((unsigned int)(bps * ctx->frame_len / 8.0 + 0.5)) + 2;
  if(ctx->byte_offset_bits + 3 > 32)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Invalid byte offset bits");
    return;
    }

  parser->parse_frame = parse_frame_wma;
  parser->cleanup = cleanup_wma;
  
  }
