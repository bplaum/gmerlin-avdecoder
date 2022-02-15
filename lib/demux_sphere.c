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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <avdec_private.h>

#define LOG_DOMAIN "demux_sphere"

#define SAMPLES2READ 1024
#define HEADERSIZE 1024

/* ircam demuxer */

typedef struct
  {
  uint64_t HeaderSize;
  uint64_t SampleRate;
  uint8_t Channels;
  uint8_t BitsPerSample;
  uint8_t SampleNBytes;
  char * SampleByteFormat;
  char * SampleCoding;
  uint64_t SampleCount;
  } sphere_header_t;

static int probe_sphere(bgav_input_context_t * input)
  {
  uint8_t test_data[7];
  if(bgav_input_get_data(input, test_data, 7) < 7)
    return 0;
  if((test_data[0] == 'N') && (test_data[1] == 'I') && (test_data[2] == 'S') && (test_data[3] == 'T') &&
     (test_data[4] == '_') && (test_data[5] == '1') && (test_data[6] == 'A'))
    return 1;
  return 0;
  }

static int check_key(const char * buf, const char * objetct)
  { 
  if(!strncmp(buf, objetct, strlen(objetct)))
    return 1;
  return 0;
  }

static int find_string(const char * buf, char **string)
  {
  int i;
  char *ptr;
  ptr = (char*)buf;

  while(!isspace(*ptr))    /* first space */
    {
    if(*ptr == '\0')
      return 0;
    ptr++;
    }
  ptr++;

  while(!isdigit(*ptr))    /* digit at -s */
    {
    if(*ptr == '\0')
      return 0;
    ptr++;
    }
  
  i = atoi(ptr);
  
  *string = calloc(i+1, sizeof(char));
  
  while(!isspace(*ptr))    /* first space */
    {
    if(*ptr == '\0')
      return 0;
    ptr++;
    }
  ptr++;

  strncpy(*string, ptr, i);
  return 1;
  }

static int find_num(const char * buf)
  {
  char *ptr;
  ptr = (char*)buf;
  while(!isspace(*ptr))    /* first space */
    {
    if(*ptr == '\0')
      return 0;
    ptr++;
    }
  ptr++;
  while(!isspace(*ptr))    /* second space */ 
    {
    if(*ptr == '\0')
      return 0;
    ptr++;
    }
  ptr++;
  return atoi(ptr);
  }

static int sphere_header_read(bgav_input_context_t * input, sphere_header_t * ret)
  {
  char * buffer = NULL;
  uint32_t buffer_alloc = 0;

  memset(ret, 0, sizeof(*ret));
    
  while(1)
    {
    if(!bgav_input_read_line(input, &buffer, &buffer_alloc, 0, NULL))
      return 0;
    if(check_key(buffer, "NIST_1A"))
      {
      if(!bgav_input_read_line(input, &buffer, &buffer_alloc, 0, NULL))
        return 0;
      ret->HeaderSize = atoi(buffer);
      }
    if(check_key(buffer, "sample_rate "))
      ret->SampleRate = find_num(buffer);
    if(check_key(buffer, "channel_count "))
      ret->Channels = find_num(buffer);      
    if(check_key(buffer, "sample_sig_bits "))
      ret->BitsPerSample = find_num(buffer);      
    if(check_key(buffer, "sample_n_bytes "))
      ret->SampleNBytes = find_num(buffer);
    if(check_key(buffer, "sample_byte_format "))
      if(!find_string(buffer, &ret->SampleByteFormat))
        return 0;
    if(check_key(buffer, "sample_count "))
      ret->SampleCount = find_num(buffer);
    if(check_key(buffer, "sample_coding "))
      if(!find_string(buffer, &ret->SampleCoding))
        return 0;
    if(check_key(buffer, "end_head"))
      break;
    if(input->position > ret->HeaderSize)
      {
      return 0;
      break;
      }
    }
  return 1;
  }

#if 0
static void sphere_header_dump(sphere_header_t * h)
  {
  bgav_dprintf("NIST SPHERE Header\n");
  bgav_dprintf("  HeaderSize:       %" PRId64 "\n",h->HeaderSize);
  bgav_dprintf("  SampleRate:       %" PRId64 "\n",h->SampleRate);
  bgav_dprintf("  Channels:         %d\n",h->Channels);
  bgav_dprintf("  BitsPerSample:    %d\n",h->BitsPerSample);
  bgav_dprintf("  SampleNBytes:     %d\n",h->SampleNBytes);
  bgav_dprintf("  SampleByteFormat: %s\n",h->SampleByteFormat);
  bgav_dprintf("  SampleCoding:     %s\n",h->SampleCoding);
  bgav_dprintf("  SampleCount:      %" PRId64 "\n",h->SampleCount);
  }
#endif

static int open_sphere(bgav_demuxer_context_t * ctx)
  {
  sphere_header_t h;
  bgav_stream_t * as;
  int bytes_per_sample;
  
  ctx->tt = bgav_track_table_create(1);

  
  if(!sphere_header_read(ctx->input, &h))
    return 0;

#if 0
  sphere_header_dump(&h);
#endif
  
  if(h.HeaderSize != HEADERSIZE)
    return 0;
  
  as = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);

  as->data.audio.format->num_channels = h.Channels;
  
  if(h.SampleCoding && strcmp(h.SampleCoding, "pcm"))
    {
    if(!strcmp(h.SampleCoding, "ulaw") || !strcmp(h.SampleCoding, "mu-law"))
      {
      as->fourcc = BGAV_MK_FOURCC('u','l','a','w');
      as->data.audio.block_align = as->data.audio.format->num_channels;
      }
    else if(!strcmp(h.SampleCoding, "alaw"))
      {
      as->fourcc = BGAV_MK_FOURCC('a','l','a','w');
      as->data.audio.block_align = as->data.audio.format->num_channels;
      }
    }
  else /* pcm */
    {
    bytes_per_sample = 0;
    if(h.SampleNBytes)
      bytes_per_sample = h.SampleNBytes;
    else if(h.SampleByteFormat)
      bytes_per_sample = strlen(h.SampleByteFormat);

    if(!bytes_per_sample)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Bytes per sample is zero");
      return 0;
      }
    
    as->data.audio.block_align = as->data.audio.format->num_channels * bytes_per_sample;
    as->data.audio.bits_per_sample = bytes_per_sample * 8;

    /* Endianess */
    if((bytes_per_sample > 1) && (!h.SampleByteFormat))
      return 0;
    
    if(strstr(h.SampleByteFormat, "10"))
      as->data.audio.endianess = BGAV_ENDIANESS_BIG;
    else if(strstr(h.SampleByteFormat, "01"))
      as->data.audio.endianess = BGAV_ENDIANESS_LITTLE;
    else
      return 0;
    
    
    switch(bytes_per_sample)
      {
      case 1:
        as->fourcc = BGAV_MK_FOURCC('t','w','o','s');
        break;
      case 2:
        if(as->data.audio.endianess == BGAV_ENDIANESS_BIG)
          as->fourcc = BGAV_MK_FOURCC('t','w','o','s'); /* BE */
        else
          as->fourcc = BGAV_MK_FOURCC('s','o','w','t'); /* LE */
        break;
      case 3:
        as->fourcc = BGAV_MK_FOURCC('i','n','2','4');
        break;
      case 4:
        as->fourcc = BGAV_MK_FOURCC('i','n','3','2');
        break;
      }
    }

  as->data.audio.format->samplerate = h.SampleRate;

  if(ctx->input->total_bytes)
    {
    as->stats.pts_end = (ctx->input->total_bytes - HEADERSIZE) / as->data.audio.block_align;
    if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
      ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
    }
  else if(h.SampleCount)
    {
    as->stats.pts_end = h.SampleCount;
    if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
      ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
    }

  bgav_input_skip(ctx->input, HEADERSIZE - ctx->input->position);
  
  bgav_track_set_format(ctx->tt->cur, "NIST SPHERE", NULL);
  
  ctx->index_mode = INDEX_MODE_PCM;
  
  if(h.SampleByteFormat)
    free(h.SampleByteFormat);
  if(h.SampleCoding)
    free(h.SampleCoding);
   
  return 1;
  }


static int64_t samples_to_bytes(bgav_stream_t * s, int samples)
  {
  return  s->data.audio.block_align * samples;
  }

static int next_packet_sphere(bgav_demuxer_context_t * ctx)
  {
  bgav_packet_t * p;
  bgav_stream_t * s;
  int bytes_read;
  int bytes_to_read;

  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  p = bgav_stream_get_packet_write(s);

  bytes_to_read = samples_to_bytes(s, SAMPLES2READ);

  if((ctx->input->total_bytes > 0) && (ctx->input->position + bytes_to_read > ctx->input->total_bytes))
    bytes_to_read = ctx->input->total_bytes - ctx->input->position;

  if(bytes_to_read <= 0)
    return 0;
  
  bgav_packet_alloc(p, bytes_to_read);
  
  p->pts = (ctx->input->position - HEADERSIZE) / s->data.audio.block_align;
  PACKET_SET_KEYFRAME(p);
  bytes_read = bgav_input_read_data(ctx->input, p->data, bytes_to_read);
  p->data_size = bytes_read;

  if(bytes_read < s->data.audio.block_align)
    return 0;
  
  bgav_stream_done_packet_write(s, p);
  return 1;
  }

static void seek_sphere(bgav_demuxer_context_t * ctx,
                        int64_t time, int scale)
  {
  bgav_stream_t * s;
  int64_t position;
  int64_t sample;

  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  
  sample = gavl_time_rescale(scale,
                             s->data.audio.format->samplerate, time);
    
  position =  s->data.audio.block_align * sample + HEADERSIZE;
  bgav_input_seek(ctx->input, position, SEEK_SET);
  STREAM_SET_SYNC(s, sample);
  }

static void close_sphere(bgav_demuxer_context_t * ctx)
  {
  return;
  }

const bgav_demuxer_t bgav_demuxer_sphere =
  {
    .probe =       probe_sphere,
    .open =        open_sphere,
    .next_packet = next_packet_sphere,
    .seek =        seek_sphere,
    .close =       close_sphere
  };
