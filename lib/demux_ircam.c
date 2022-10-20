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
#include <avdec_private.h>

/* VAX (native) Little-endian       \144\243\001\0 */
#define VAXN   BGAV_MK_FOURCC(0x64, 0xa3, 0x01, 0x00)   

/* VAX Big-endian                   \0\001\243\144 */
#define VAX    BGAV_MK_FOURCC(0x00, 0x01, 0xa3, 0x64)   

/* SUN (native) Big-endian          \144\243\002\0 */
#define SUNN   BGAV_MK_FOURCC(0x64, 0xa3, 0x02, 0x00)   

/* SUN Little-endian                \0\002\243\144 */
#define SUN    BGAV_MK_FOURCC(0x00, 0x02, 0xa3, 0x64)

/* MIPS (DECstation) Little-endian  \144\243\003\0 */
#define MIPSD  BGAV_MK_FOURCC(0x64, 0xa3, 0x03, 0x00)

/* MIPS (SGI) Big-endian             \0\003\243\144 */
#define MIPSS  BGAV_MK_FOURCC( 0x00, 0x03, 0xa3, 0x64)

/* NeXT Big-endian                   \144\243\004\0 */
#define NEXT   BGAV_MK_FOURCC( 0x64, 0xa3, 0x04, 0x00)   

#define HEADER_SIZE   1024
#define SAMPLES2READ 1024

#define SF_CHAR       0x00001	 /* 8-bit integer  */
#define SF_ALAW       0x10001 	 /* 8-bit A-law    */
#define	SF_ULAW       0x20001 	 /* 8-bit µ-law    */
#define	SF_SHORT      0x00002 	 /* 16-bit integer */
#define	SF_24INT      0x00003 	 /* 24-bit integer */
#define	SF_LONG       0x40004 	 /* 32-bit integer */
#define	SF_FLOAT      0x00004 	 /* 32-bit float   */
#define	SF_DOUBLE     0x00008  	 /* 64-bit float   */

/* ircam demuxer */

typedef struct
  {
  uint32_t fourcc;
  float SampleFrequenz;
  uint32_t NumChannels;
  uint32_t DataType;
  int LittleEndian;
  } ircam_header_t;


/* IEEE float reading */

static float float32_be_read (unsigned char *cptr)
  {
  int             exponent, mantissa, negative ;
  float   fvalue ;

  negative = cptr [0] & 0x80 ;
  exponent = ((cptr [0] & 0x7F) << 1) | ((cptr [1] & 0x80) ? 1 : 0) ;
  mantissa = ((cptr [1] & 0x7F) << 16) | (cptr [2] << 8) | (cptr [3]) ;

  if (! (exponent || mantissa))
    return 0.0 ;

  mantissa |= 0x800000 ;
  exponent = exponent ? exponent - 127 : 0 ;

  fvalue = mantissa ? ((float) mantissa) / ((float) 0x800000) : 0.0 ;

  if (negative)
    fvalue *= -1 ;

  if (exponent > 0)
    fvalue *= (1 << exponent) ;
  else if (exponent < 0)
    fvalue /= (1 << abs (exponent)) ;

  return fvalue ;
  } /* float32_be_read */

static float float32_le_read (unsigned char *cptr)
  {
  int             exponent, mantissa, negative ;
  float   fvalue ;

  negative = cptr [3] & 0x80 ;
  exponent = ((cptr [3] & 0x7F) << 1) | ((cptr [2] & 0x80) ? 1 : 0) ;
  mantissa = ((cptr [2] & 0x7F) << 16) | (cptr [1] << 8) | (cptr [0]) ;

  if (! (exponent || mantissa))
    return 0.0 ;

  mantissa |= 0x800000 ;
  exponent = exponent ? exponent - 127 : 0 ;

  fvalue = mantissa ? ((float) mantissa) / ((float) 0x800000) : 0.0 ;

  if (negative)
    fvalue *= -1 ;

  if (exponent > 0)
    fvalue *= (1 << exponent) ;
  else if (exponent < 0)
    fvalue /= (1 << abs (exponent)) ;

  return fvalue ;
  } /* float32_le_read */

static int ircam_header_read(bgav_input_context_t * input, ircam_header_t * ret)
  {
  uint32_t fourcc;
  uint8_t Freq_buffer[4];

  if(!bgav_input_read_fourcc( input, &fourcc))
    return 0;

  /* Set littel or big endian */
  switch(fourcc)
    {
    case VAXN:
      ret->fourcc = fourcc;
      ret->LittleEndian = 1;
      break;
    case VAX:
      ret->fourcc = fourcc;
      ret->LittleEndian = 0;
      break;
    case SUNN:
      ret->fourcc = fourcc;
      ret->LittleEndian = 0;
      break;
    case SUN:
      ret->fourcc = fourcc;
      ret->LittleEndian = 1;
      break;
    case MIPSD:
      ret->fourcc = fourcc;
      ret->LittleEndian = 1;
      break;
    case MIPSS:
      ret->fourcc = fourcc;
      ret->LittleEndian = 0;
      break;
    case NEXT:
      ret->fourcc = fourcc;
      ret->LittleEndian = 0;
      break;
    default:
      break;
    }
  
  if(ret->LittleEndian == 1)       /* LitteEndian */
    {
    if(bgav_input_read_data( input, Freq_buffer, 4) < 4)
      return 0;
    ret->SampleFrequenz = float32_le_read(Freq_buffer);
    if(!bgav_input_read_32_le( input, &ret->NumChannels))
      return 0;
    if(!bgav_input_read_32_le( input, &ret->DataType))
      return 0;
    
    
    }
  else if(ret->LittleEndian == 0)   /* BigEndian */
    {
    if(bgav_input_read_data( input, Freq_buffer, 4) < 4)
      return 0;
    ret->SampleFrequenz = float32_be_read(Freq_buffer);
    if(!bgav_input_read_32_be( input, &ret->NumChannels))
       return 0;
    if(!bgav_input_read_32_be( input, &ret->DataType))
       return 0;
    }
  else
    return 0;
  return 1;
  }

#if 0
static void ircam_header_dump(ircam_header_t * h)
  {
  bgav_dprintf("IRCAM\n");
  bgav_dprintf("  .fourcc =            0x%08x\n", h->fourcc);
  bgav_dprintf("  SampleFreqenz:     %f\n",h->SampleFrequenz);
  bgav_dprintf("  NumChanels:        %d\n",h->NumChannels);
  bgav_dprintf("  DataType:          0x%08x\n",h->DataType);
  bgav_dprintf("  is LittleEndian:   %d\n",h->LittleEndian);
  }
#endif

static int probe_ircam(bgav_input_context_t * input)
  {
  uint32_t fourcc;
  if(!bgav_input_get_fourcc(input, &fourcc))
    return 0;

  switch(fourcc)
    {
    case VAXN:
    case VAX:
    case SUNN:
    case SUN:
    case MIPSD:
    case MIPSS:
    case NEXT:
      return 1;
      break;
    default:
      break;
    }
  return 0;
  }


static int open_ircam(bgav_demuxer_context_t * ctx)
  {
  ircam_header_t h;
  bgav_stream_t * as;
  const char * format;
  
  /* Create track */
  ctx->tt = bgav_track_table_create(1);

  if(!ircam_header_read(ctx->input, &h))
    return 0;

#if 0
  ircam_header_dump(&h);
#endif
  
  as = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);

  switch(h.DataType)
    {
    case SF_CHAR:
      as->fourcc = BGAV_MK_FOURCC('t', 'w', 'o', 's'); /* Assuming signed */
      //  as->fourcc = BGAV_MK_FOURCC('r', 'a', 'w', ' ');
      as->data.audio.bits_per_sample = 8;
      as->data.audio.block_align = 1 * h.NumChannels;
      break;
    case SF_ALAW:
      as->fourcc = BGAV_MK_FOURCC('a', 'l', 'a', 'w');
      as->data.audio.bits_per_sample = 8;
      as->data.audio.block_align = 1 * h.NumChannels;
      break;
    case SF_ULAW:
      as->fourcc = BGAV_MK_FOURCC('u', 'l', 'a', 'w');
      as->data.audio.bits_per_sample = 8;
      as->data.audio.block_align = 1 * h.NumChannels;
      break;
    case SF_SHORT:
      if(h.LittleEndian == 1)
        as->fourcc = BGAV_MK_FOURCC('s', 'o', 'w', 't'); 
      else
        as->fourcc = BGAV_MK_FOURCC('t', 'w', 'o', 's');
      as->data.audio.bits_per_sample = 16;
      as->data.audio.block_align = 2 * h.NumChannels;
      break;
    case SF_24INT:
      if(h.LittleEndian == 1)
        as->data.audio.endianess = BGAV_ENDIANESS_LITTLE;
      else
        as->data.audio.endianess = BGAV_ENDIANESS_BIG;
      as->fourcc = BGAV_MK_FOURCC('i', 'n', '2', '4');
      as->data.audio.bits_per_sample = 24;
      as->data.audio.block_align = 3 * h.NumChannels;
      break;
    case SF_LONG:
      if(h.LittleEndian == 1)
        as->data.audio.endianess = BGAV_ENDIANESS_LITTLE;
      else
        as->data.audio.endianess = BGAV_ENDIANESS_BIG;
      as->fourcc = BGAV_MK_FOURCC('i', 'n', '3', '2');
      as->data.audio.bits_per_sample = 32;
      as->data.audio.block_align = 4 * h.NumChannels;
      break;
    case SF_FLOAT:
      if(h.LittleEndian == 1)
        as->data.audio.endianess = BGAV_ENDIANESS_LITTLE;
      else
        as->data.audio.endianess = BGAV_ENDIANESS_BIG;
      as->fourcc = BGAV_MK_FOURCC('f', 'l', '3', '2');
      as->data.audio.bits_per_sample = 32;
      as->data.audio.block_align = 4 * h.NumChannels;
      break;
    case SF_DOUBLE:
      if(h.LittleEndian == 1)
        as->data.audio.endianess = BGAV_ENDIANESS_LITTLE;
      else
        as->data.audio.endianess = BGAV_ENDIANESS_BIG;
      as->fourcc = BGAV_MK_FOURCC('f', 'l', '6', '4');
      as->data.audio.bits_per_sample = 64;
      as->data.audio.block_align = 8 * h.NumChannels;
      break;
    default:
      break;
    }
  
  as->data.audio.format->samplerate = h.SampleFrequenz;
  as->data.audio.format->num_channels = h.NumChannels;

  if(ctx->input->total_bytes)
    {
    as->stats.pts_end = (ctx->input->total_bytes - HEADER_SIZE) / as->data.audio.block_align;
    if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
      ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
    }
  
  switch(h.fourcc)
    {
    case VAXN:
      format = "IRCAM: VAX (native)";
      break;
    case VAX:
      format = "IRCAM: VAX";
      break;
    case SUNN:
      format = "IRCAM: Sun (native) ";
      break;
    case SUN:
      format = "IRCAM: Sun";
      break;
    case MIPSD:
      format = "IRCAM: MIPS (DECstation)";
      break;
    case MIPSS:
      format = "IRCAM: MIPS (SGI)";
      break;
    case NEXT:
      format = "IRCAM: NeXT";
      break;
    default:
      format = "IRCAM: ...";
      break;
    }

  bgav_track_set_format(ctx->tt->cur, format, NULL);
  
  bgav_input_skip(ctx->input, HEADER_SIZE - ctx->input->position);
  ctx->data_start = ctx->input->position;
  ctx->flags |= BGAV_DEMUXER_HAS_DATA_START;
  ctx->index_mode = INDEX_MODE_PCM;
  return 1;
  }

static int64_t samples_to_bytes(bgav_stream_t * s, int samples)
  {
  return  s->data.audio.block_align * samples;
  }

static int next_packet_ircam(bgav_demuxer_context_t * ctx)
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
  p->pts = (ctx->input->position - HEADER_SIZE) / s->data.audio.block_align;
  PACKET_SET_KEYFRAME(p);
  bytes_read = bgav_input_read_data(ctx->input, p->buf.buf, bytes_to_read);
  p->buf.len = bytes_read;

  if(bytes_read < s->data.audio.block_align)
    return 0;
  
  bgav_stream_done_packet_write(s, p);
  return 1;
  }

static void seek_ircam(bgav_demuxer_context_t * ctx, int64_t time,
                       int scale)
  {
  bgav_stream_t * s;
  int64_t position;
  int64_t sample;

  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  
  sample = gavl_time_rescale(scale, s->data.audio.format->samplerate, time);
    
  position =  s->data.audio.block_align * sample + HEADER_SIZE;
  bgav_input_seek(ctx->input, position, SEEK_SET);

  STREAM_SET_SYNC(s, sample);
  }

static void close_ircam(bgav_demuxer_context_t * ctx)
  {
  return;
  }

const bgav_demuxer_t bgav_demuxer_ircam =
  {
    .probe =       probe_ircam,
    .open =        open_ircam,
    .next_packet = next_packet_ircam,
    .seek =        seek_ircam,
    .close =       close_ircam
  };
