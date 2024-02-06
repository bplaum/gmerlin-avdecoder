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

#define SCHl_TAG BGAV_MK_FOURCC('S', 'C', 'H', 'l')
#define PT00_TAG BGAV_MK_FOURCC('P', 'T', 0x0, 0x0)
#define SCDl_TAG BGAV_MK_FOURCC('S', 'C', 'D', 'l')
#define pIQT_TAG BGAV_MK_FOURCC('p', 'I', 'Q', 'T')
#define SCEl_TAG BGAV_MK_FOURCC('S', 'C', 'E', 'l')

#define EA_SAMPLE_RATE 22050
#define EA_BITS_PER_SAMPLE 16
#define EA_PREAMBLE_SIZE 8

#define LOG_DOMAIN "wve"
#define AUDIO_ID 0
#define VIDEO_ID 1


static int probe_wve(bgav_input_context_t * input)
  {
  uint32_t fourcc;
  if(!bgav_input_get_fourcc(input, &fourcc))
    return 0;
  if(fourcc == SCHl_TAG)
    return 1;
  return 0;
  }

static int read_arbitrary(bgav_input_context_t * input, uint32_t * ret)
  {
  uint8_t size, byte;
  int i;
  uint32_t word;

  if(!bgav_input_read_data(input, &size, 1))
    return 0;
    
  word = 0;
  for (i = 0; i < size; i++)
    {
    if(!bgav_input_read_data(input, &byte, 1))
      return 0;
    word <<= 8;
    word |= byte;
    }
  
  *ret = word;
  return 1;
  }

static int open_wve(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * as = NULL;
  uint32_t header_size, fourcc, arbitrary;
  int in_header;
  int in_subheader;
  
  uint8_t byte, subbyte;
  

  /* Create track */
  ctx->tt = bgav_track_table_create(1);


  /* Process file header */
  bgav_input_skip(ctx->input, 4); // Skip signature
  if(!bgav_input_read_32_le(ctx->input, &header_size) ||
     !bgav_input_read_fourcc(ctx->input, &fourcc))
    return 0;

  if(fourcc != PT00_TAG)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "No PT header found");
    return 0;
    }

  in_header = 1;
  while(in_header)
    {
    if(!bgav_input_read_data(ctx->input, &byte, 1))
      return 0;

    switch(byte)
      {
      case 0xFD: // Audio subheader
        in_subheader = 1;
        as = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
        as->data.audio.format->samplerate = EA_SAMPLE_RATE;
        as->data.audio.bits_per_sample = EA_BITS_PER_SAMPLE;
        as->stream_id = AUDIO_ID;
        while(in_subheader)
          {
          if(!bgav_input_read_data(ctx->input, &subbyte, 1))
            return 0;

          switch(subbyte)
            {
            case 0x82: // Channels
              if(!read_arbitrary(ctx->input, &arbitrary))
                return 0;
              
              as->data.audio.format->num_channels = arbitrary;
              break;
            case 0x83: // Compression type
              if(!read_arbitrary(ctx->input, &arbitrary))
                return 0;

              if(arbitrary != 7)
                gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                         "Unknown audio compression type");
              else
                as->fourcc = BGAV_MK_FOURCC('w','v','e','a');
              
              break;
            case 0x85: // Num samples
              if(!read_arbitrary(ctx->input, &arbitrary))
                return 0;
              as->stats.pts_end = arbitrary;
              break;
            case 0x8a: // End of subheader
              if(!read_arbitrary(ctx->input, &arbitrary))
                return 0;
              in_subheader = 0;
              break;
            default:
              if(!read_arbitrary(ctx->input, &arbitrary))
                return 0;
              gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN,
                       "Unknown audio header element 0x%02x: 0x%08x",
                       subbyte, arbitrary);
              break;
            } 
          } // End of audio subheader
        break;
      case 0xFF: // End of header block
        in_header = 0;
        break;
      default:
        if(!read_arbitrary(ctx->input, &arbitrary))
          return 0;
        gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN,
                 "Unknown header element 0x%02x: 0x%08x",
                 subbyte, arbitrary);
      }
    
    } // End of header

  // Skip to begining of data
  if(header_size > ctx->input->position)
    bgav_input_skip(ctx->input, header_size - ctx->input->position);
  
  bgav_track_set_format(ctx->tt->cur, "Electronicarts WVE", NULL);

  
  return 1;
  }

static gavl_source_status_t next_packet_wve(bgav_demuxer_context_t * ctx)
  {
  uint8_t preamble[EA_PREAMBLE_SIZE];
  uint32_t chunk_size, chunk_type;
  bgav_stream_t * s;
  bgav_packet_t * p;
  
  
  if(bgav_input_read_data(ctx->input, preamble, EA_PREAMBLE_SIZE) <
     EA_PREAMBLE_SIZE)
    return GAVL_SOURCE_EOF;

  chunk_type = BGAV_PTR_2_FOURCC(&preamble[0]);
  chunk_size = GAVL_PTR_2_32LE(&preamble[4]);
  chunk_size -= EA_PREAMBLE_SIZE;

  switch(chunk_type)
    {
    case SCDl_TAG: // Audio data
      s = bgav_track_find_stream(ctx, AUDIO_ID);

      if(!s)
        {
        bgav_input_skip(ctx->input, chunk_size);
        break;
        }
      else
        {
        p = bgav_stream_get_packet_write(s);

        gavl_packet_alloc(p, chunk_size);

        if(bgav_input_read_data(ctx->input, p->buf.buf, chunk_size) < chunk_size)
          return GAVL_SOURCE_EOF;

        p->buf.len = chunk_size;
        bgav_stream_done_packet_write(s, p);
        }
      break;
    case SCEl_TAG: // Ending tag
      return GAVL_SOURCE_EOF;
      break;
    default: // Video data??
      bgav_input_skip(ctx->input, chunk_size);
      break;
      
    }
  return GAVL_SOURCE_OK;
  }

static void close_wve(bgav_demuxer_context_t * ctx)
  {
  }

const bgav_demuxer_t bgav_demuxer_wve =
  {
    .probe =       probe_wve,
    .open =        open_wve,
    .next_packet = next_packet_wve,
    .close =       close_wve
  };
