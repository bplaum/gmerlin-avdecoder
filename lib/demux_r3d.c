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

#define LOG_DOMAIN "demux_r3d"

#define AUDIO_ID 0
#define VIDEO_ID 1

typedef struct
  {
  uint32_t len;
  uint32_t fourcc;
  } chunk_t;

static int read_chunk_header(bgav_input_context_t * ctx,
                             chunk_t * ret)
  {
  return bgav_input_read_32_be(ctx, &ret->len) &&
    bgav_input_read_32_be(ctx, &ret->fourcc);
  }

/* Header */

typedef struct
  {
  uint32_t unknown1;
  uint32_t pts_timescale;
  uint32_t unknown2;
  uint32_t unknown3;
  uint32_t unknown4;
  uint32_t unknown5;
  uint32_t unknown6;
  uint32_t unknown7;
  uint32_t unknown8;
  uint32_t unknown9;
  uint32_t unknown10;
  uint32_t width;
  uint32_t height;
  uint32_t video_timescale;
  uint16_t frame_duration ;
  uint8_t  unknown15;
  char name[257]; /* Maximum length: 256 + \0 */
  } red1_t;

static int read_red1(bgav_input_context_t * ctx,
                     red1_t * ret)
  {
  //  if(!read_chunk_header(ctx, &ch))
  //    return 0;
  if(!bgav_input_read_32_be(ctx, &ret->unknown1) ||
     !bgav_input_read_32_be(ctx, &ret->pts_timescale) ||
     !bgav_input_read_32_be(ctx, &ret->unknown2) ||
     !bgav_input_read_32_be(ctx, &ret->unknown3) ||
     !bgav_input_read_32_be(ctx, &ret->unknown4) ||
     !bgav_input_read_32_be(ctx, &ret->unknown5) ||
     !bgav_input_read_32_be(ctx, &ret->unknown6) ||
     !bgav_input_read_32_be(ctx, &ret->unknown7) ||
     !bgav_input_read_32_be(ctx, &ret->unknown8) ||
     !bgav_input_read_32_be(ctx, &ret->unknown9) ||
     !bgav_input_read_32_be(ctx, &ret->unknown10) ||
     !bgav_input_read_32_be(ctx, &ret->width) ||
     !bgav_input_read_32_be(ctx, &ret->height) ||
     !bgav_input_read_32_be(ctx, &ret->video_timescale) ||
     !bgav_input_read_16_be(ctx, &ret->frame_duration) ||
     !bgav_input_read_8(ctx, &ret->unknown15)
     )
    return 0;

  if(bgav_input_read_data(ctx, (uint8_t*)(ret->name), 257) < 257)
    return 0;
  return 1;
  }

static void dump_red1(red1_t * red1)
  {
  bgav_dprintf("R3D header:\n");
  bgav_dprintf("  unknown1:        %d (%08x)\n", red1->unknown1,
               red1->unknown1);
  bgav_dprintf("  pts_timescale:   %d\n", red1->pts_timescale);
  bgav_dprintf("  unknown2:        %d\n", red1->unknown2);
  bgav_dprintf("  unknown3:        %d\n", red1->unknown3);
  bgav_dprintf("  unknown4:        %d\n", red1->unknown4);
  bgav_dprintf("  unknown5:        %d\n", red1->unknown5);
  bgav_dprintf("  unknown6:        %d\n", red1->unknown6);
  bgav_dprintf("  unknown7:        %d\n", red1->unknown7);
  bgav_dprintf("  unknown8:        %d\n", red1->unknown8);
  bgav_dprintf("  unknown9:        %d\n", red1->unknown9);
  bgav_dprintf("  unknown10:       %d\n", red1->unknown10);
  bgav_dprintf("  width:           %d\n", red1->width);
  bgav_dprintf("  height:          %d\n", red1->height);
  bgav_dprintf("  video_timescale: %d\n", red1->video_timescale);
  bgav_dprintf("  frame_duration:  %d\n", red1->frame_duration);
  bgav_dprintf("  unknown15:       %d\n", red1->unknown15);
  bgav_dprintf("  name:            %s\n", red1->name);
  }
/* Packet headers are not used, we build a superindex instead.
   They are left here for reference */

#if 0

/* Audio packet header */

typedef struct
  {
  uint32_t pts;      /* in PTS timescale tics */
  uint32_t samplerate; /* Samplerate            */
  uint32_t samples; /* Samples * channels in this packet */
  uint32_t packetno; /* Packet counter */
  uint32_t unknown5; /* 0f180303 */
  uint32_t unknown6; /* Always 0 */
  } audio_header_t;

static int read_audio_header(bgav_input_context_t * ctx,
                             audio_header_t * ret)
  {
  //  if(!read_chunk_header(ctx, &ch))
  //    return 0;
  if(!bgav_input_read_32_be(ctx, &ret->pts) ||
     !bgav_input_read_32_be(ctx, &ret->samplerate) ||
     !bgav_input_read_32_be(ctx, &ret->samples) ||
     !bgav_input_read_32_be(ctx, &ret->packetno) ||
     !bgav_input_read_32_be(ctx, &ret->unknown5) ||
     !bgav_input_read_32_be(ctx, &ret->unknown6))
    return 0;
  return 1;
  }

static void dump_audio_header(audio_header_t * h)
  {
  bgav_dprintf("Audio packet header:\n");
  bgav_dprintf("  PTS:        %d\n", h->pts);
  bgav_dprintf("  samplerate: %d\n", h->samplerate);
  bgav_dprintf("  samples:    %d\n", h->samples);
  bgav_dprintf("  packetno:   %d\n", h->packetno);
  bgav_dprintf("  Unknown5:   %d (%08x)\n",
               h->unknown5,
               h->unknown5);
  bgav_dprintf("  Unknown6:   %d (%08x)\n",
               h->unknown6,
               h->unknown6);
  }

/* Video packet header */

typedef struct
  {
  uint32_t pts;
  uint32_t packetno;
  uint32_t unknown3;
  } video_header_t;

static int read_video_header(bgav_input_context_t * ctx,
                             video_header_t * ret)
  {
  //  if(!read_chunk_header(ctx, &ch))
  //    return 0;
  if(!bgav_input_read_32_be(ctx, &ret->pts) ||
     !bgav_input_read_32_be(ctx, &ret->packetno) ||
     !bgav_input_read_32_be(ctx, &ret->unknown3))
    return 0;
  return 1;
  }

static void dump_video_header(video_header_t * h)
  {
  bgav_dprintf("Video packet header:\n");
  bgav_dprintf("  PTS:      %d\n", h->pts);
  bgav_dprintf("  packetno: %d\n", h->packetno);
  bgav_dprintf("  Unknown3: %d (%08x)\n",
               h->unknown3,
               h->unknown3);
  }

#endif

/* Footer */

typedef struct
  {
  uint32_t rdvo_offset;
  uint32_t rdvs_offset;
  uint32_t rdao_offset;
  uint32_t rdas_offset;

  uint32_t video_packets;
  uint32_t audio_packets;
  uint32_t totlen;
	
  uint32_t max_video_size;
  uint32_t max_audio_size;

  uint32_t unknown3;
  uint32_t unknown4;
  uint32_t unknown5;
  } reob_t;

static int read_reob(bgav_input_context_t * ctx,
                     reob_t * ret)
  {
  return
    bgav_input_read_32_be(ctx, &ret->rdvo_offset) &&
    bgav_input_read_32_be(ctx, &ret->rdvs_offset) &&
    bgav_input_read_32_be(ctx, &ret->rdao_offset) &&
    bgav_input_read_32_be(ctx, &ret->rdas_offset) &&
    bgav_input_read_32_be(ctx, &ret->video_packets) &&
    bgav_input_read_32_be(ctx, &ret->audio_packets) &&
    bgav_input_read_32_be(ctx, &ret->totlen) &&
    bgav_input_read_32_be(ctx, &ret->max_video_size) &&
    bgav_input_read_32_be(ctx, &ret->max_audio_size) &&
    bgav_input_read_32_be(ctx, &ret->unknown3) &&
    bgav_input_read_32_be(ctx, &ret->unknown4) &&
    bgav_input_read_32_be(ctx, &ret->unknown5);
  }

static void dump_reob(reob_t * reob)
  {
  bgav_dprintf("reob:\n");
  bgav_dprintf("  rdvo_offset:    %d\n", reob->rdvo_offset);
  bgav_dprintf("  rdvs_offset:    %d\n", reob->rdvs_offset);
  bgav_dprintf("  rdao_offset:    %d\n", reob->rdao_offset);
  bgav_dprintf("  rdas_offset:    %d\n", reob->rdas_offset);
  bgav_dprintf("  video_packets:  %d\n", reob->video_packets);
  bgav_dprintf("  audio_packets:  %d\n", reob->audio_packets);
  bgav_dprintf("  totlen:         %d\n", reob->totlen);
  bgav_dprintf("  max_video_size: %d\n", reob->max_video_size);
  bgav_dprintf("  max_audio_size: %d\n", reob->max_audio_size);
  bgav_dprintf("  unknown3:       %d\n", reob->unknown3);
  bgav_dprintf("  unknown4:       %d\n", reob->unknown4);
  bgav_dprintf("  unknown5:       %d\n", reob->unknown5);
  }
/* Index */

static uint32_t * read_index(bgav_input_context_t * ctx,
                             uint32_t num_packets,
                             uint32_t fourcc,
                             uint32_t offset)
  {
  chunk_t ch;
  uint32_t * ret;
  int i;
  bgav_input_seek(ctx, offset, SEEK_SET);
  if(!read_chunk_header(ctx, &ch))
    return 0;
  if(ch.fourcc != fourcc)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Wrong index tag (broken file)");
    return NULL;
    }
  if(ch.len - 8 < num_packets / 4)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Index chunk too small");
    return NULL;
    }
  ret = malloc(num_packets * sizeof(*ret));
  for(i = 0; i < num_packets; i++)
    {
    if(!bgav_input_read_32_be(ctx, &ret[i]))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Unexpected EOF in index");
      free(ret);
      return NULL;
      }
    }
  return ret;
  }

typedef struct
  {
  red1_t red1;
  reob_t reob;
  uint32_t * rdas; /* Audio sizes */
  uint32_t * rdvs; /* Video sizes */
  uint32_t * rdao; /* Audio offsets */
  uint32_t * rdvo; /* Video offsets */
  } r3d_priv_t;

static int probe_r3d(bgav_input_context_t * input)
  {
  uint8_t test_data[8];
  if(bgav_input_get_data(input, test_data, 8) < 8)
    return 0;
  if((test_data[4] == 'R') &&
     (test_data[5] == 'E') &&
     (test_data[6] == 'D') &&
     (test_data[7] == '1'))
    return 1;
  return 0;
  }

static void build_superindex(bgav_demuxer_context_t * ctx)
  {
  int i;
  int audio_pos = 0, video_pos = 0;
  bgav_stream_t * s;
  r3d_priv_t * priv;
  int duration;
  int offset;
  priv = ctx->priv;

  ctx->si = bgav_superindex_create(priv->reob.audio_packets +
                                   priv->reob.video_packets);

  for(i = 0; i < priv->reob.audio_packets+priv->reob.video_packets; i++)
    {
    if(audio_pos < priv->reob.audio_packets &&
       ((video_pos >= priv->reob.video_packets) ||
        priv->rdao[audio_pos] < priv->rdvo[video_pos]))
      {
      /* Add audio packet */
      s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
      offset = 24+8;
      duration = (priv->rdas[audio_pos]-offset) / 16; /* 4 bytes * 4 channels */
      bgav_superindex_add_packet(ctx->si,
                                 s, priv->rdao[audio_pos] + offset,
                                 priv->rdas[audio_pos] - offset,
                                 s->stream_id,
                                 s->stats.pts_end,
                                 1, duration);
      audio_pos++;
      }
    else
      {
      /* Add video packet */
      s = bgav_track_get_video_stream(ctx->tt->cur, 0);
      offset = 12+8;
      duration = s->data.video.format->frame_duration;
      bgav_superindex_add_packet(ctx->si,
                                 s, priv->rdvo[video_pos] + offset,
                                 priv->rdvs[video_pos] - offset,
                                 s->stream_id,
                                 s->stats.pts_end,
                                 1, duration);
      video_pos++;
      }
    s->stats.pts_end += duration;
    }
  }

static int open_r3d(bgav_demuxer_context_t * ctx)
  {
  chunk_t ch;
  r3d_priv_t * priv;
  bgav_stream_t * s;
  if(!(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "R3D cannot be read from nonseekable sources");
    return 0;
    }
     
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  if(!read_chunk_header(ctx->input, &ch))
    return 0;
  if(ch.fourcc != BGAV_MK_FOURCC('R','E','D','1'))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Got no header");
    return 0;
    }
  if(!read_red1(ctx->input, &priv->red1))
    return 0;
  if(ctx->opt->dump_headers)
    dump_red1(&priv->red1);
  bgav_input_seek(ctx->input, -0x38, SEEK_END);
  if(!read_chunk_header(ctx->input, &ch))
    return 0;

  if(ch.fourcc != BGAV_MK_FOURCC('R','E','O','B'))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Got no footer");
    return 0;
    }
  
  if(!read_reob(ctx->input, &priv->reob))
    return 0;
  if(ctx->opt->dump_headers)
    dump_reob(&priv->reob);
  
  /* Read indices */

  if(priv->reob.video_packets)
    {
    priv->rdvo = read_index(ctx->input,
                            priv->reob.video_packets,
                            BGAV_MK_FOURCC('R','D','V','O'),
                            priv->reob.rdvo_offset);
    if(!priv->rdvo)
      return 0; 
    
    priv->rdvs = read_index(ctx->input,
                            priv->reob.video_packets,
                            BGAV_MK_FOURCC('R','D','V','S'),
                            priv->reob.rdvs_offset);
    if(!priv->rdvs)
      return 0; 

    
    }
  if(priv->reob.audio_packets)
    {
    priv->rdao = read_index(ctx->input,
                            priv->reob.audio_packets,
                            BGAV_MK_FOURCC('R','D','A','O'),
                            priv->reob.rdao_offset);
    if(!priv->rdao)
      return 0; 
    priv->rdas = read_index(ctx->input,
                            priv->reob.video_packets,
                            BGAV_MK_FOURCC('R','D','A','S'),
                            priv->reob.rdas_offset);
    if(!priv->rdas)
      return 0; 
    
    }

  ctx->tt = bgav_track_table_create(1);
  /* Add video stream */
  if(priv->reob.video_packets)
    {
    s = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
    s->ci->flags &= ~GAVL_COMPRESSION_HAS_P_FRAMES;
    s->data.video.format->image_width = priv->red1.width;
    s->data.video.format->image_height = priv->red1.height;
    s->data.video.format->frame_width = priv->red1.width;
    s->data.video.format->frame_height = priv->red1.height;
    s->data.video.format->pixel_width = 1;
    s->data.video.format->pixel_height = 1;
    s->data.video.format->timescale = priv->red1.video_timescale;
    s->data.video.format->frame_duration = priv->red1.frame_duration;
    s->fourcc = BGAV_MK_FOURCC('R', '3', 'D', '1');
    //    s->fourcc = BGAV_MK_FOURCC('j','p','e','g');
    s->stream_id = VIDEO_ID;
    }
  /* Add audio stream */
  if(priv->reob.audio_packets)
    {
    s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
    s->data.audio.format->samplerate = 48000;
    s->data.audio.format->num_channels = 4;
    s->data.audio.block_align = 16;
    s->data.audio.bits_per_sample = 32; // 24 actually, the lowest byte is 0
    s->fourcc = BGAV_MK_FOURCC('t','w','o','s');
    s->stream_id = AUDIO_ID;
    }

  bgav_input_seek(ctx->input, 0, SEEK_SET);
  
  build_superindex(ctx);

  ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
  ctx->index_mode = INDEX_MODE_SI_SA;
  
  return 1;
  }

static int next_packet_r3d(bgav_demuxer_context_t * ctx)
  {
  //  bgav_packet_t * p;
  //  bgav_stream_t * s;
  return 0;
  }

static void
seek_r3d(bgav_demuxer_context_t * ctx, gavl_time_t time, int scale)
  {
  }

static void close_r3d(bgav_demuxer_context_t * ctx)
  {
  r3d_priv_t * priv;
  priv = ctx->priv;

  if(priv->rdvo) free(priv->rdvo);
  if(priv->rdvs) free(priv->rdvs);
  if(priv->rdao) free(priv->rdao);
  if(priv->rdas) free(priv->rdas);
  
  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_r3d =
  {
    .probe =       probe_r3d,
    .open =        open_r3d,
    .next_packet = next_packet_r3d,
    .seek =        seek_r3d,
    .close =       close_r3d
  };

