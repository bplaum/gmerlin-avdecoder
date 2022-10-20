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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define LOG_DOMAIN "aiff"

typedef struct
  {
  uint32_t fourcc;
  uint32_t size;
  } chunk_header_t;

static int read_chunk_header(bgav_input_context_t * input,
                             chunk_header_t * ch)
  {
  return bgav_input_read_fourcc(input, &ch->fourcc) &&
    bgav_input_read_32_be(input, &ch->size);
  }

typedef struct
  {
  uint16_t num_channels;
  uint32_t num_sample_frames;
  uint16_t num_bits;
  int32_t samplerate;

  /* For AIFC only */
  uint32_t compression_type;
  char compression_name[256];
  } comm_chunk_t;

static int read_float_80(bgav_input_context_t * input, int32_t * ret)
  {
  uint8_t buffer[10];
  uint8_t * pos;
  
  uint32_t mantissa;
  uint32_t last = 0;
  uint8_t exp;

  if(bgav_input_read_data(input, buffer, 10) < 10)
    return 0;

  pos = buffer + 2;

  mantissa = GAVL_PTR_2_32BE(pos);

  exp = 30 - buffer[1];
  while (exp--)
    {
    last = mantissa;
    mantissa >>= 1;
    }
  if (last & 0x00000001) mantissa++;
  *ret = mantissa;
  return 1;
  }

static int comm_chunk_read(chunk_header_t * h,
                           bgav_input_context_t * input,
                           comm_chunk_t * ret, int is_aifc)
  {
  int64_t start_pos;

  start_pos = input->position;

  if(!bgav_input_read_16_be(input, &ret->num_channels) ||
     !bgav_input_read_32_be(input, &ret->num_sample_frames) ||
     !bgav_input_read_16_be(input, &ret->num_bits) ||
     !read_float_80(input, &ret->samplerate))
    return 0;

  if(is_aifc)
    {
    if(!bgav_input_read_fourcc(input, &ret->compression_type) ||
       !bgav_input_read_string_pascal(input, ret->compression_name))
      return 0;
    }
  if(input->position - start_pos < h->size)
    bgav_input_skip(input, h->size - (input->position - start_pos));
  
  return 1;
  }

typedef struct
  {
  int is_aifc;
  
  uint32_t data_size;
  int samples_per_block;
  
  } aiff_priv_t;

#define PADD(n) ((n&1)?(n+1):n)

static int64_t pos_2_time(bgav_demuxer_context_t * ctx, int64_t pos)
  {
  aiff_priv_t * priv;
  bgav_stream_t * s;
  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  priv = ctx->priv;
  
  return ((pos - ctx->data_start)/s->data.audio.block_align) *
    priv->samples_per_block;
  }

static int64_t time_2_pos(bgav_demuxer_context_t * ctx, int64_t time)
  {
  aiff_priv_t * priv;

  bgav_stream_t * s;
  priv = ctx->priv;
  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  
  return ctx->data_start + (time/priv->samples_per_block)
    * s->data.audio.block_align;
  }


static int probe_aiff(bgav_input_context_t * input)
  {
  uint8_t test_data[12];
  if(bgav_input_get_data(input, test_data, 12) < 12)
    return 0;
  if((test_data[0] ==  'F') &&
     (test_data[1] ==  'O') &&
     (test_data[2] ==  'R') &&
     (test_data[3] ==  'M') &&
     (test_data[8] ==  'A') &&
     (test_data[9] ==  'I') &&
     (test_data[10] == 'F') &&
     ((test_data[11] == 'F') || (test_data[11] == 'C')))
    return 1;
  return 0;
  }

static char * read_meta_string(bgav_input_context_t * input,
                               chunk_header_t * h)
  {
  char * ret;
  ret = calloc(h->size+1, 1);
  if(bgav_input_read_data(input, (uint8_t*)ret, h->size) < h->size)
    {
    free(ret);
    return NULL;
    }
  if(h->size & 1)
    bgav_input_skip(input, 1);
  return ret;
  }

static int open_aiff(bgav_demuxer_context_t * ctx)
  {
  chunk_header_t ch;
  comm_chunk_t comm;

  uint32_t fourcc;
  bgav_stream_t * s = NULL;
  aiff_priv_t * priv;
  int keep_going = 1;
  bgav_track_t * track;
  char * tmp_string;
  const char * format;
  
  /* Create track */
  ctx->tt = bgav_track_table_create(1);
  track = ctx->tt->cur;
  
  /* Check file magic */
  
  if(!read_chunk_header(ctx->input, &ch) ||
     (ch.fourcc != BGAV_MK_FOURCC('F','O','R','M')))
    return 0;

  if(!bgav_input_read_fourcc(ctx->input, &fourcc))
    return 0;

  /* Allocate private struct */

  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
  
  if(fourcc == BGAV_MK_FOURCC('A','I','F','C'))
    {
    priv->is_aifc = 1;
    }
  else if(fourcc != BGAV_MK_FOURCC('A','I','F','F'))
    {
    return 0;
    }
  

  /* Read chunks until we are done */

  while(keep_going)
    {
    if(!read_chunk_header(ctx->input, &ch))
      return 0;
    switch(ch.fourcc)
      {
      case BGAV_MK_FOURCC('C','O','M','M'):
        s = bgav_track_add_audio_stream(track, ctx->opt);
        
        memset(&comm, 0, sizeof(comm));
        
        if(!comm_chunk_read(&ch,
                            ctx->input,
                            &comm, priv->is_aifc))
          {
          return 0;
          }
        
        s->data.audio.format->samplerate =   comm.samplerate;
        s->data.audio.format->num_channels = comm.num_channels;
        s->data.audio.bits_per_sample =     comm.num_bits;

        /*
         *  Multichannel support according to
         *  http://music.calarts.edu/~tre/AIFFC/
         */

        switch(s->data.audio.format->num_channels)
          {
          case 1:
            s->data.audio.format->channel_locations[0] = GAVL_CHID_FRONT_CENTER;
            break;
          case 2:
            s->data.audio.format->channel_locations[0] = GAVL_CHID_FRONT_LEFT;
            s->data.audio.format->channel_locations[1] = GAVL_CHID_FRONT_RIGHT;
            break;
          case 3:
            s->data.audio.format->channel_locations[0] = GAVL_CHID_FRONT_LEFT;
            s->data.audio.format->channel_locations[1] = GAVL_CHID_FRONT_CENTER;
            s->data.audio.format->channel_locations[2] = GAVL_CHID_FRONT_RIGHT;
            break;
          case 4: /* Note: 4 channels can also be "left center right surround" but we
                     believe, that quad is more common */
            s->data.audio.format->channel_locations[0] = GAVL_CHID_FRONT_LEFT;
            s->data.audio.format->channel_locations[1] = GAVL_CHID_FRONT_RIGHT;
            s->data.audio.format->channel_locations[2] = GAVL_CHID_REAR_LEFT;
            s->data.audio.format->channel_locations[3] = GAVL_CHID_REAR_RIGHT;
            break;
          case 6:
            s->data.audio.format->channel_locations[0] = GAVL_CHID_FRONT_LEFT;
            s->data.audio.format->channel_locations[1] = GAVL_CHID_FRONT_CENTER_LEFT;
            s->data.audio.format->channel_locations[2] = GAVL_CHID_FRONT_CENTER;
            s->data.audio.format->channel_locations[3] = GAVL_CHID_FRONT_RIGHT;
            s->data.audio.format->channel_locations[4] = GAVL_CHID_FRONT_CENTER_RIGHT;
            s->data.audio.format->channel_locations[5] = GAVL_CHID_REAR_CENTER;
            break;
          }
        
        if(!priv->is_aifc)
          s->fourcc = BGAV_MK_FOURCC('a','i','f','f');
        else if(comm.compression_type == BGAV_MK_FOURCC('N','O','N','E'))
          s->fourcc = BGAV_MK_FOURCC('a','i','f','f');
        else
          s->fourcc = comm.compression_type;
        
        switch(s->fourcc)
          {
          case BGAV_MK_FOURCC('a','i','f','f'):
            if(s->data.audio.bits_per_sample <= 8)
              {
              s->data.audio.block_align = comm.num_channels;
              }
            else if(s->data.audio.bits_per_sample <= 16)
              {
              s->data.audio.block_align = 2 * comm.num_channels;
              }
            else if(s->data.audio.bits_per_sample <= 24)
              {
              s->data.audio.block_align = 3 * comm.num_channels;
              }
            else if(s->data.audio.bits_per_sample <= 32)
              {
              s->data.audio.block_align = 4 * comm.num_channels;
              }
            else
              {
              gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                       "%d bit aiff not supported", 
                      s->data.audio.bits_per_sample);
              return 0;
              }
            s->data.audio.format->samples_per_frame = 1024;
            priv->samples_per_block = 1;
            break;
          case BGAV_MK_FOURCC('f','l','3','2'):
            s->data.audio.block_align = 4 * comm.num_channels;
            priv->samples_per_block = 1;
            break;
          case BGAV_MK_FOURCC('f','l','6','4'):
            s->data.audio.block_align = 8 * comm.num_channels;
            priv->samples_per_block = 1;
            s->data.audio.format->samples_per_frame = 1024;
            break;
          case BGAV_MK_FOURCC('a','l','a','w'):
          case BGAV_MK_FOURCC('A','L','A','W'):
          case BGAV_MK_FOURCC('u','l','a','w'):
          case BGAV_MK_FOURCC('U','L','A','W'):
            s->data.audio.block_align = comm.num_channels;
            priv->samples_per_block = 1;
            s->data.audio.format->samples_per_frame = 1024;
            break;
#if 0
          case BGAV_MK_FOURCC('G','S','M',' '):
            s->data.audio.block_align = 33;
            priv->samples_per_block = 160;
            s->data.audio.format->samples_per_frame = 160;
            break;
#endif
          case BGAV_MK_FOURCC('M','A','C','3'):
            s->data.audio.block_align = comm.num_channels * 2;
            priv->samples_per_block = 6;
            s->data.audio.format->samples_per_frame = 6*128;
            break;
          case BGAV_MK_FOURCC('M','A','C','6'):
            s->data.audio.block_align = comm.num_channels;
            priv->samples_per_block = 6;
            s->data.audio.format->samples_per_frame = 6*128;
            break;
          default:
            gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Compression %c%c%c%c not supported",
                    comm.compression_type >> 24,
                    (comm.compression_type >> 16) & 0xff,
                    (comm.compression_type >> 8) & 0xff,
                    (comm.compression_type) & 0xff);
            return 0;
          }
        break;
      case BGAV_MK_FOURCC('N','A','M','E'):
        tmp_string = read_meta_string(ctx->input, &ch);
        gavl_dictionary_set_string_nocopy(ctx->tt->cur->metadata,
                                GAVL_META_TITLE, tmp_string);
        break;
      case BGAV_MK_FOURCC('A','U','T','H'):
        tmp_string = read_meta_string(ctx->input, &ch);
        gavl_dictionary_set_string_nocopy(ctx->tt->cur->metadata,
                                GAVL_META_AUTHOR, tmp_string);
        break;
      case BGAV_MK_FOURCC('(','c',')',' '):
        tmp_string = read_meta_string(ctx->input, &ch);
        gavl_dictionary_set_string_nocopy(ctx->tt->cur->metadata,
                                GAVL_META_COPYRIGHT, tmp_string);
        break;
      case BGAV_MK_FOURCC('A','N','N','O'):
        tmp_string = read_meta_string(ctx->input, &ch);
        gavl_dictionary_set_string_nocopy(ctx->tt->cur->metadata,
                                GAVL_META_COMMENT, tmp_string);
        break;
      case BGAV_MK_FOURCC('S','S','N','D'):
        bgav_input_skip(ctx->input, 4); /* Offset */
        bgav_input_skip(ctx->input, 4); /* Blocksize */
        ctx->data_start = ctx->input->position;
        ctx->flags |= BGAV_DEMUXER_HAS_DATA_START;
        priv->data_size = ch.size - 8;
        s->stats.pts_end = pos_2_time(ctx, priv->data_size + ctx->data_start);
        keep_going = 0;
        break;
      default:
        bgav_input_skip(ctx->input, PADD(ch.size));
        break;
      }
    }
  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;

  if(priv->is_aifc)
    format = "AIFF-C";
  else
    format = "AIFF";

  bgav_track_set_format(ctx->tt->cur, format, "audio/aiff");
  
  ctx->index_mode = INDEX_MODE_PCM;
  return 1;
  }

static int next_packet_aiff(bgav_demuxer_context_t * ctx)
  {
  bgav_packet_t * p;
  aiff_priv_t * priv;
  int bytes_to_read;
  int bytes_read;
  bgav_stream_t * s;
  priv = ctx->priv;

  s = bgav_track_get_audio_stream(ctx->tt->cur, 0 );
  p = bgav_stream_get_packet_write(s);
  
  bytes_to_read =
    (s->data.audio.format->samples_per_frame * s->data.audio.block_align) /
    priv->samples_per_block;
  
  bgav_packet_alloc(p, bytes_to_read);
  
  p->pts = pos_2_time(ctx, ctx->input->position);
  
  bytes_read = bgav_input_read_data(ctx->input, p->buf.buf, bytes_to_read);
  p->buf.len = bytes_read;
  PACKET_SET_KEYFRAME(p);
  bgav_stream_done_packet_write(s, p);

  if(!bytes_read)
    return 0;
  
  return 1;
  }

static void seek_aiff(bgav_demuxer_context_t * ctx, int64_t time, int scale)
  {
  int64_t pos;
  int64_t time_scaled;
  bgav_stream_t * s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  time_scaled =
    gavl_time_rescale(scale,
                      s->data.audio.format->samplerate,
                      time);
  
  pos = time_2_pos(ctx, time_scaled);
  bgav_input_seek(ctx->input, pos, SEEK_SET);

  STREAM_SET_SYNC(s, pos_2_time(ctx, pos));
  
  }

static void close_aiff(bgav_demuxer_context_t * ctx)
  {
  aiff_priv_t * priv;
  priv = ctx->priv;
  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_aiff =
  {
    .probe =       probe_aiff,
    .open =        open_aiff,
    .next_packet = next_packet_aiff,
    .seek =        seek_aiff,
    .close =       close_aiff
  };
