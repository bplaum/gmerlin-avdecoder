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

/* Header structure */

#define VQHD_TAG BGAV_MK_FOURCC('V', 'Q', 'H', 'D')
#define FINF_TAG BGAV_MK_FOURCC('F', 'I', 'N', 'F')
#define SND0_TAG BGAV_MK_FOURCC('S', 'N', 'D', '0')
#define SND1_TAG BGAV_MK_FOURCC('S', 'N', 'D', '1')
#define SND2_TAG BGAV_MK_FOURCC('S', 'N', 'D', '2')
#define VQFR_TAG BGAV_MK_FOURCC('V', 'Q', 'F', 'R')
#define VQA_HEADER_SIZE 0x2A

#define AUDIO_ID 0
#define VIDEO_ID 1


typedef struct
  {
  uint16_t  Version;       /* VQA version number                         */
  uint16_t  Flags;         /* VQA flags                                  */
  uint16_t  NumFrames;     /* Number of frames                           */
  uint16_t  Width;         /* Movie width (pixels)                       */
  uint16_t  Height;        /* Movie height (pixels)                      */
  uint8_t   BlockW;        /* Width of each image block (pixels)         */
  uint8_t   BlockH;        /* Height of each image block (pixels)        */
  uint8_t   FrameRate;     /* Frame rate of the VQA                      */
  uint8_t   CBParts;       /* How many images use the same lookup table  */
  uint16_t  Colors;        /* Maximum number of colors used in VQA       */
  uint16_t  MaxBlocks;     /* Maximum number of image blocks             */
  uint32_t  Unknown1;      /* Always 0 ???                               */
  uint16_t  Unknown2;      /* Some kind of size ???                      */
  uint16_t  Freq;          /* Sound sampling frequency                   */
  uint8_t   Channels;      /* Number of sound channels                   */
  uint8_t   Bits;          /* Sound resolution                           */
  uint32_t  Unknown3;      /* Always 0 ???                               */
  uint16_t  Unknown4;      /* 0 in old VQAs, 4 in HiColor ones ???       */
  uint32_t  MaxCBFZSize;   /* 0 in old VQAs, max. CBFZ size in HiColor   */
  uint32_t  Unknown5;      /* Always 0 ???                               */
  } VQAHeader;

static int read_file_header(bgav_input_context_t * input, VQAHeader * ret)
  {
  return (bgav_input_read_16_le(input, &ret->Version) && 
          bgav_input_read_16_le(input, &ret->Flags) && 
          bgav_input_read_16_le(input, &ret->NumFrames) &&
          bgav_input_read_16_le(input, &ret->Width) && 
          bgav_input_read_16_le(input, &ret->Height) && 
          bgav_input_read_data(input, &ret-> BlockW, 1) && 
          bgav_input_read_data(input, &ret-> BlockH, 1) && 
          bgav_input_read_data(input, &ret-> FrameRate, 1) &&
          bgav_input_read_data(input, &ret-> CBParts, 1) &&  
          bgav_input_read_16_le(input, &ret->Colors) &&   
          bgav_input_read_16_le(input, &ret->MaxBlocks) &&
          bgav_input_read_32_le(input, &ret->Unknown1) && 
          bgav_input_read_16_le(input, &ret->Unknown2) && 
          bgav_input_read_16_le(input, &ret->Freq) && 
          bgav_input_read_data(input, &ret-> Channels, 1) && 
          bgav_input_read_data(input, &ret-> Bits, 1) && 
          bgav_input_read_32_le(input, &ret->Unknown3) && 
          bgav_input_read_16_le(input, &ret->Unknown4) && 
          bgav_input_read_32_le(input, &ret->MaxCBFZSize)&& 
          bgav_input_read_32_le(input, &ret->Unknown5));
  }

#if 0
static void dump_file_header(VQAHeader * h)
  {
  bgav_dprintf("Version:     %d\n",   h->Version);   /* VQA version number                         */
  bgav_dprintf("Flags:       %d\n",     h->Flags);   /* VQA flags                                  */
  bgav_dprintf("NumFrames:   %d\n", h->NumFrames);   /* Number of frames                           */
  bgav_dprintf("Width:       %d\n",     h->Width);   /* Movie width (pixels)                       */
  bgav_dprintf("Height:      %d\n",    h->Height);   /* Movie height (pixels)                      */
  bgav_dprintf("BlockW:      %d\n",    h->BlockW);   /* Width of each image block (pixels)         */
  bgav_dprintf("BlockH:      %d\n",    h->BlockH);   /* Height of each image block (pixels)        */
  bgav_dprintf("FrameRate:   %d\n", h->FrameRate);   /* Frame rate of the VQA                      */
  bgav_dprintf("CBParts:     %d\n",   h->CBParts);   /* How many images use the same lookup table  */
  bgav_dprintf("Colors:      %d\n",    h->Colors);   /* Maximum number of colors used in VQA       */
  bgav_dprintf("MaxBlocks:   %d\n", h->MaxBlocks);   /* Maximum number of image blocks             */
  bgav_dprintf("Unknown1:    %d\n",  h->Unknown1);   /* Always 0 ???                               */
  bgav_dprintf("Unknown2:    %d\n",  h->Unknown2);   /* Some kind of size ???                      */
  bgav_dprintf("Freq:        %d\n",      h->Freq);   /* Sound sampling frequency                   */
  bgav_dprintf("Channels:    %d\n",  h->Channels);   /* Number of sound channels                   */
  bgav_dprintf("Bits:        %d\n",      h->Bits);   /* Sound resolution                           */
  bgav_dprintf("Unknown3:    %d\n",  h->Unknown3);   /* Always 0 ???                               */
  bgav_dprintf("Unknown4:    %d\n",  h->Unknown4);   /* 0 in old VQAs, 4 in HiColor ones ???       */
  bgav_dprintf("MaxCBFZSize: %d\n", h->MaxCBFZSize); /* 0 in old VQAs, max. CBFZ size in HiColor   */
  bgav_dprintf("Unknown5:    %d\n",  h->Unknown5);   /* Always 0 ???                               */
  }
#endif

typedef struct
  {
  uint32_t video_pts;
  uint8_t header[VQA_HEADER_SIZE];
  } vqa_priv_t;

static int probe_vqa(bgav_input_context_t * input)
  {
  uint8_t test_data[12];
  if(bgav_input_get_data(input, test_data, 12) < 12)
    return 0;
  if((test_data[0] == 'F') &&
     (test_data[1] == 'O') &&
     (test_data[2] == 'R') &&
     (test_data[3] == 'M') &&
     (test_data[8] == 'W') &&
     (test_data[9] == 'V') &&
     (test_data[10] == 'Q') &&
     (test_data[11] == 'A'))
    return 1;
  return 0;
  }

static int open_vqa(bgav_demuxer_context_t * ctx)
  {
  bgav_input_context_t * input_mem;
  bgav_stream_t * s;
  vqa_priv_t * priv;
  VQAHeader h;
    
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  /* Skip to header */

  bgav_input_skip(ctx->input, 20);

  if(bgav_input_read_data(ctx->input, priv->header,
                          VQA_HEADER_SIZE) < VQA_HEADER_SIZE)
    return 0;

  input_mem =
    bgav_input_open_memory(priv->header, VQA_HEADER_SIZE);
  read_file_header(input_mem, &h);
  bgav_input_close(input_mem);
  bgav_input_destroy(input_mem);

  //  dump_file_header(&h);
  
  /* Create track */
  ctx->tt = bgav_track_table_create(1);

  /* Initialize video stream */
  s = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
  s->stream_id = VIDEO_ID;

  bgav_stream_set_extradata(s, priv->header, VQA_HEADER_SIZE);
  
  s->fourcc = BGAV_MK_FOURCC('W','V','Q','A');
  s->data.video.format->image_width = h.Width;
  s->data.video.format->frame_width = h.Width;

  s->data.video.format->image_height = h.Height;
  s->data.video.format->frame_height = h.Height;
  s->data.video.format->pixel_width = 1;
  s->data.video.format->pixel_height = 1;
  s->data.video.format->timescale = h.FrameRate;
  s->data.video.format->frame_duration = 1;
  s->stats.pts_end = h.NumFrames * s->data.video.format->frame_duration;
  
  /* Initialize audio stream */
  if(h.Freq || ((h.Version == 1) && (h.Flags == 1)))
    {
    s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);

    if(h.Version == 1)
      s->fourcc = BGAV_MK_FOURCC('w','s','p','1');
    else
      s->fourcc = BGAV_MK_FOURCC('w','s','p','c');

    s->stream_id = AUDIO_ID;
    s->data.audio.format->samplerate = h.Freq;

    if(!s->data.audio.format->samplerate)
      s->data.audio.format->samplerate = 22050;
    
    s->data.audio.format->num_channels = h.Channels;
    if(!s->data.audio.format->num_channels)
      s->data.audio.format->num_channels = 1;
    
    s->data.audio.bits_per_sample = h.Bits;
    }
  
  ctx->tt->cur->data_start = ctx->input->position;
  bgav_track_set_format(ctx->tt->cur, "Westwood VQA", NULL);
  
  return 1;
  
  }

static gavl_source_status_t next_packet_vqa(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s = NULL;
  bgav_packet_t * p;
  uint32_t type;
  uint32_t size;

  if(!bgav_input_read_fourcc(ctx->input, &type) ||
     !bgav_input_read_32_be(ctx->input, &size))
    return GAVL_SOURCE_EOF;

  /* Audio */
  if((type == SND2_TAG) || (type == SND1_TAG))
    s = bgav_track_find_stream(ctx, AUDIO_ID);
  else if(type == VQFR_TAG)
    s = bgav_track_find_stream(ctx, VIDEO_ID);
  
  if(!s)
    {
    bgav_input_skip(ctx->input, size);
    if(size & 1)
      bgav_input_skip(ctx->input, 1);
    
    return GAVL_SOURCE_OK;
    }

  p = bgav_stream_get_packet_write(s);

  bgav_packet_alloc(p, size);
  if(bgav_input_read_data(ctx->input, p->buf.buf, size) < size)
    return GAVL_SOURCE_EOF;
  p->buf.len = size;

  if(size & 1)
    bgav_input_skip(ctx->input, 1);
  
  if(s->type == GAVL_STREAM_VIDEO)
    p->pts = s->in_position;
  bgav_stream_done_packet_write(s, p);
  
  return GAVL_SOURCE_OK;
  }

static void close_vqa(bgav_demuxer_context_t * ctx)
  {
  vqa_priv_t * priv;
  priv = ctx->priv;
  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_vqa =
  {
    .probe =       probe_vqa,
    .open =        open_vqa,
    .next_packet = next_packet_vqa,
    //    .seek =        seek_vqa,
    .close =       close_vqa
  };
