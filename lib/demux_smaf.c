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

#define LOG_DOMAIN "smaf"

typedef struct
  {
  uint32_t bytes_left;
  } smaf_priv_t;

typedef struct
  {
  uint32_t fourcc;
  uint32_t size;
  } chunk_header_t;

static int read_chunk_header(bgav_input_context_t * input,
                              chunk_header_t * ret)
  {
  return bgav_input_read_fourcc(input, &ret->fourcc) &&
    bgav_input_read_32_be(input, &ret->size);
  
  }

#if 0
static void dump_chunk_header(chunk_header_t * ch)
  {
  bgav_dprintf("SMAF Chunk header\n");
  bgav_dprintf("  .fourcc = ");
  bgav_dump_fourcc(ch->fourcc);
  bgav_dprintf("\n");
  bgav_dprintf("  size:   %d\n", ch->size);
  }
#endif

static const int mmf_rates[] = { 4000, 8000, 11025, 22050, 44100 };

static int mmf_rate(int code)
  {
  if((code < 0) || (code > 4))
    return -1;
  return mmf_rates[code];
  }

static int probe_smaf(bgav_input_context_t * input)
  {
  uint8_t data[12];
  if(bgav_input_get_data(input, data, 12) < 12)
    return 0;

  if((data[0] == 'M') &&
     (data[1] == 'M') &&
     (data[2] == 'M') &&
     (data[3] == 'D') &&
     (data[8] == 'C') &&
     (data[9] == 'N') &&
     (data[10] == 'T') &&
     (data[11] == 'I'))
    return 1;
  return 0;
  }

static int open_smaf(bgav_demuxer_context_t * ctx)
  {
  int done = 0;
  uint8_t params;
  chunk_header_t ch;
  bgav_stream_t * s;

  smaf_priv_t * priv;
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  /* Skip MMMD chunk */
  bgav_input_skip(ctx->input, 8);

  while(!done)
    {
    if(!read_chunk_header(ctx->input, &ch))
      return 0;

    //    dump_chunk_header(&ch);

    if((ch.fourcc & 0xffffff00) == BGAV_MK_FOURCC('M','T','R',0))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "MIDI like files not supported");
      return 0;
      }

    else if((ch.fourcc == BGAV_MK_FOURCC('C','N','T','I')) ||
            (ch.fourcc == BGAV_MK_FOURCC('O','P','D','A')))
      bgav_input_skip(ctx->input, ch.size);

    else if((ch.fourcc & 0xffffff00) == BGAV_MK_FOURCC('A','T','R',0))
      done = 1;

    else
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Unsupported SMAF chunk (%c%c%c%c)",
               (ch.fourcc & 0xFF000000) >> 24,
               (ch.fourcc & 0x00FF0000) >> 16,
               (ch.fourcc & 0x0000FF00) >> 8,
               (ch.fourcc & 0x000000FF));
      return 0;
      }
    
    }

  /* Initialize generic things */

  ctx->tt = bgav_track_table_create(1);
  s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
  
  /* Now, get the format */
   
  bgav_input_skip(ctx->input, 1); /* format type */
  bgav_input_skip(ctx->input, 1); /* sequence type */

  /* (channel << 7) | (format << 4) | rate */
  if(!bgav_input_read_data(ctx->input, &params, 1)) 
    return 0;
  
  s->data.audio.format->samplerate = mmf_rate(params & 0x0f);
  s->fourcc = BGAV_MK_FOURCC('S','M','A','F');
  s->data.audio.bits_per_sample = 4;
  s->data.audio.format->num_channels = 1;
  s->data.audio.bits_per_sample = 4;
  s->container_bitrate = s->data.audio.bits_per_sample *
    s->data.audio.format->samplerate;
    
  if(s->data.audio.format->samplerate < 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Invalid samplerate");
    return 0;
    }
  bgav_input_skip(ctx->input, 1); /* wave base bit */
  bgav_input_skip(ctx->input, 1); /* time base d   */
  bgav_input_skip(ctx->input, 1); /* time base g   */

  done = 0;
  while(!done)
    {
    if(!read_chunk_header(ctx->input, &ch))
      return 0;

    //    dump_chunk_header(&ch);

    if((ch.fourcc == BGAV_MK_FOURCC('A','t','s','q')) ||
       (ch.fourcc == BGAV_MK_FOURCC('A','s','p','I')))
      bgav_input_skip(ctx->input, ch.size);

    else if((ch.fourcc & 0xffffff00) == BGAV_MK_FOURCC('A','w','a', 0))
      done = 1;

    else
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Unsupported SMAF chunk (%c%c%c%c)",
               (ch.fourcc & 0xFF000000) >> 24,
               (ch.fourcc & 0x00FF0000) >> 16,
               (ch.fourcc & 0x0000FF00) >> 8,
               (ch.fourcc & 0x000000FF));
      return 0;
      }
    }
  priv->bytes_left = ch.size;

  bgav_track_set_format(ctx->tt->cur, "SMAF Ringtone", NULL);

  
  return 1;
  }

#define MAX_BYTES 4096

static gavl_source_status_t next_packet_smaf(bgav_demuxer_context_t * ctx)
  {
  int bytes_to_read;
  smaf_priv_t * priv;
  bgav_packet_t * p;
  bgav_stream_t * s;

  priv = ctx->priv;

  bytes_to_read = priv->bytes_left;

  if(!bytes_to_read)
    return GAVL_SOURCE_EOF;
  
  if(bytes_to_read > MAX_BYTES)
    bytes_to_read = MAX_BYTES;
  
  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  p = bgav_stream_get_packet_write(s);

  bgav_packet_alloc(p, bytes_to_read);

  p->buf.len = bgav_input_read_data(ctx->input, p->buf.buf, bytes_to_read);
  if(!p->buf.len)
    return GAVL_SOURCE_EOF;
  
  bgav_stream_done_packet_write(s, p);
  return GAVL_SOURCE_OK;
  }

static void close_smaf(bgav_demuxer_context_t * ctx)
  {
  smaf_priv_t * priv;
  priv = ctx->priv;
  
  if(priv)
    free(priv);
  }

const bgav_demuxer_t bgav_demuxer_smaf =
  {
    .probe =       probe_smaf,
    .open =        open_smaf,
    .next_packet = next_packet_smaf,
    .close =       close_smaf
  };
