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
#include <stdio.h>
#include <FLAC/stream_decoder.h>

#include <codecs.h>

#define MAX_FRAME_SAMPLES 65535

#define LOG_DOMAIN "flac"

// #define MAKE_VERSION(maj,min,pat) ((maj<<16)|(min<<8)|pat)
// #define BGAV_FLAC_VERSION_INT MAKE_VERSION(BGAV_FLAC_MAJOR,BGAV_FLAC_MINOR,BGAV_FLAC_PATCHLEVEL)

typedef struct
  {
  FLAC__StreamDecoder * dec;

  bgav_packet_t * p;
  uint8_t * data_ptr;
  uint8_t * header_ptr;
  
  gavl_audio_frame_t * frame;

  void (*copy_samples)(gavl_audio_frame_t * f,
                       const FLAC__int32 *const buffer[],
                       int num_channels,
                       int shift_bits);
  int shift_bits;
  } flac_priv_t;

/* FLAC Callbacks */

static FLAC__StreamDecoderReadStatus
read_callback(const FLAC__StreamDecoder *decoder,
              FLAC__byte buffer[],
              size_t *bytes, void *client_data)
  {
  int bytes_read = 0;
  int bytes_to_copy;
  flac_priv_t * priv;
  bgav_stream_t * s;
  s = client_data;

  priv = s->decoder_priv;
  
  while(bytes_read < *bytes)
    {
    /* Read header */
    if(priv->header_ptr - s->ci->codec_header.buf < s->ci->codec_header.len)
      {
      bytes_to_copy =
        (s->ci->codec_header.len - (priv->header_ptr - s->ci->codec_header.buf));
      if(bytes_to_copy > *bytes - bytes_read)
        bytes_to_copy = *bytes - bytes_read;
      memcpy(&buffer[bytes_read], priv->header_ptr, bytes_to_copy);
      bytes_read += bytes_to_copy;
      priv->header_ptr += bytes_to_copy;
      }
    
    if(!priv->p)
      {
      gavl_source_status_t st;
      if((st = bgav_stream_get_packet_read(s, &priv->p)) != GAVL_SOURCE_OK)
        break;
      priv->data_ptr = priv->p->buf.buf;
      }

    /* Bytes, which are left from the packet */
    bytes_to_copy = (priv->p->buf.len - (priv->data_ptr - priv->p->buf.buf));
    if(bytes_to_copy > *bytes - bytes_read)
      bytes_to_copy = *bytes - bytes_read;

    memcpy(&buffer[bytes_read], priv->data_ptr, bytes_to_copy);

    bytes_read += bytes_to_copy;
    priv->data_ptr += bytes_to_copy;

    if(priv->data_ptr - priv->p->buf.buf == priv->p->buf.len)
      {
      bgav_stream_done_packet_read(s, priv->p);
      priv->p = NULL;
      }
    if(bytes_read) // This ensures we read at most one packet
      break;
    }
  *bytes = bytes_read;
  if(!bytes_read)
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  else
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
  }

static void copy_samples_8(gavl_audio_frame_t * f,
                           const FLAC__int32 *const buffer[],
                           int num_channels,
                           int shift_bits)
  {
  int i, j;
  
  for(i = 0; i < num_channels; i++)
    {
    for(j = 0; j < f->valid_samples; j++)
      {
      f->channels.s_8[i][j] = buffer[i][j] << shift_bits;
      }
    }

  }

static void copy_samples_16(gavl_audio_frame_t * f,
                            const FLAC__int32 *const buffer[],
                            int num_channels,
                            int shift_bits)
  {
  int i, j;
  
  for(i = 0; i < num_channels; i++)
    {
    for(j = 0; j < f->valid_samples; j++)
      {
      f->channels.s_16[i][j] = buffer[i][j] << shift_bits;
      }
    }
  
  }

static void copy_samples_32(gavl_audio_frame_t * f,
                            const FLAC__int32 *const buffer[],
                            int num_channels,
                            int shift_bits)
  {
  int i, j;
  
  for(i = 0; i < num_channels; i++)
    {
    for(j = 0; j < f->valid_samples; j++)
      {
      f->channels.s_32[i][j] = buffer[i][j] << shift_bits;
      }
    }
  }

//static int64_t __total = 0;

static FLAC__StreamDecoderWriteStatus
write_callback(const FLAC__StreamDecoder *decoder,
               const FLAC__Frame *frame,
               const FLAC__int32 *const buffer[],
               void *client_data)
  {
  flac_priv_t * priv;
  bgav_stream_t * s;
  s = client_data;
  priv = s->decoder_priv;
#if 0
  fprintf(stderr, "%d %ld %ld\n",
          frame->header.blocksize, __total,
          frame->header.number_type == FLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER ?
          frame->header.number.frame_number * frame->header.blocksize :
          frame->header.number.sample_number);
  __total += frame->header.blocksize;
#endif
  priv->frame->valid_samples = frame->header.blocksize;
  priv->copy_samples(priv->frame, buffer, s->data.audio.format->num_channels,
                     priv->shift_bits);
  
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
  }

static void
metadata_callback(const FLAC__StreamDecoder *decoder,
                  const FLAC__StreamMetadata *metadata,
                  void *client_data)
  {
  bgav_stream_t * s;
  s = client_data;
  if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
    {
    s->data.audio.format->num_channels = metadata->data.stream_info.channels;
    s->data.audio.bits_per_sample     = metadata->data.stream_info.bits_per_sample;
    }
  }

static void
error_callback(const FLAC__StreamDecoder *decoder,
               FLAC__StreamDecoderErrorStatus status,
               void *client_data)
  {
  }

static int init_flac(bgav_stream_t * s)
  {
  flac_priv_t * priv;
  gavl_audio_format_t frame_format;
  if(s->ci->codec_header.len < 42)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "FLAC decoder needs 42 bytes extradata");
    return 0;
    }

  // gavl_hexdump(s->ci->global_header, s->ci->global_header_len, 16); 
  
  priv = calloc(1, sizeof(*priv));
  s->decoder_priv = priv;
  priv->header_ptr = s->ci->codec_header.buf;
  priv->dec = FLAC__stream_decoder_new();
 
  FLAC__stream_decoder_init_stream(
               priv->dec,
               read_callback,
               NULL, NULL, NULL, NULL,
               write_callback,
               metadata_callback,
               error_callback,
               (void*) s);
  
  if(!FLAC__stream_decoder_process_until_end_of_metadata(priv->dec))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Reading metadata failed");
    return 0;
    }
  
  s->data.audio.format->interleave_mode = GAVL_INTERLEAVE_NONE;
  
  gavl_set_channel_setup(s->data.audio.format);

  if(!s->data.audio.format->samples_per_frame)
    s->data.audio.format->samples_per_frame = 1024;
  
  /* Set sample format stuff */

  if(s->data.audio.bits_per_sample <= 8)
    {
    priv->shift_bits = 8 - s->data.audio.bits_per_sample;
    s->data.audio.format->sample_format = GAVL_SAMPLE_S8;
    priv->copy_samples = copy_samples_8;
    }
  else if(s->data.audio.bits_per_sample <= 16)
    {
    priv->shift_bits = 16 - s->data.audio.bits_per_sample;
    s->data.audio.format->sample_format = GAVL_SAMPLE_S16;
    priv->copy_samples = copy_samples_16;
    }
  else
    {
    priv->shift_bits = 32 - s->data.audio.bits_per_sample;
    s->data.audio.format->sample_format = GAVL_SAMPLE_S32;
    priv->copy_samples = copy_samples_32;
    }
  
  gavl_audio_format_copy(&frame_format,
                         s->data.audio.format);

  frame_format.samples_per_frame = MAX_FRAME_SAMPLES;
  priv->frame = gavl_audio_frame_create(&frame_format);

  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                    "FLAC");
  gavl_dictionary_set_int(s->m, GAVL_META_BITRATE,
                        GAVL_BITRATE_LOSSLESS);
  return 1;
  }

static gavl_source_status_t decode_frame_flac(bgav_stream_t * s)
  {
  flac_priv_t * priv;
  gavl_source_status_t st;
  priv = s->decoder_priv;

  priv->frame->valid_samples = 0;
  
  if((st = bgav_stream_peek_packet_read(s, NULL)) != GAVL_SOURCE_OK)
    return st; 

  /* Decode another frame */
  while(1)
    {
    priv->frame->valid_samples = 0;
    FLAC__stream_decoder_process_single(priv->dec);

    if(FLAC__stream_decoder_get_state(priv->dec) ==
       FLAC__STREAM_DECODER_END_OF_STREAM)
      {
      fprintf(stderr, "Detected EOF: %d\n", priv->frame->valid_samples);
      return GAVL_SOURCE_EOF;
      }
    if(priv->frame->valid_samples)
      {
      gavl_audio_frame_copy_ptrs(s->data.audio.format, 
                                 s->data.audio.frame, priv->frame);
      return GAVL_SOURCE_OK;
      }
    }
  return GAVL_SOURCE_EOF;
  }

static void close_flac(bgav_stream_t * s)
  {
  flac_priv_t * priv;
  priv = s->decoder_priv;
  if(priv->frame)
    gavl_audio_frame_destroy(priv->frame);
  if(priv->dec)
    FLAC__stream_decoder_delete(priv->dec);
  free(priv);
  }

static void resync_flac(bgav_stream_t * s)
  {
  flac_priv_t * priv;
  priv = s->decoder_priv;
  
  FLAC__stream_decoder_flush(priv->dec);
  priv->frame->valid_samples = 0;

  if(priv->p)
    {
    bgav_stream_done_packet_read(s, priv->p);
    priv->p = NULL;
    }
  }


static bgav_audio_decoder_t decoder =
  {
    .fourccs = (uint32_t[]){
      BGAV_MK_FOURCC('F', 'L', 'A', 'C'),
      BGAV_MK_FOURCC('F', 'L', 'C', 'N'),
      0x00 },
    .name = "FLAC audio decoder",
    .init = init_flac,
    .close = close_flac,
    .resync = resync_flac,
    .decode_frame = decode_frame_flac
  };

void bgav_init_audio_decoders_flac()
  {
  bgav_audio_decoder_register(&decoder);
  }
