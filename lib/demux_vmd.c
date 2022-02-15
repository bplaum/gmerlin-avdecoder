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

/* Ported from ffmpeg */

#define VMD_HEADER_SIZE 0x0330
#define BYTES_PER_FRAME_RECORD 16

#define LOG_DOMAIN "demux_vmd"

#define AUDIO_ID 0
#define VIDEO_ID 1

typedef struct
  {
  int stream_index;
  uint32_t frame_offset;
  unsigned int frame_size;
  int64_t pts;
  unsigned char frame_record[BYTES_PER_FRAME_RECORD];
  } vmd_frame_t;

#if 0
static void dump_frames(vmd_frame_t * f, int num)
  {
  int i;
  for(i = 0; i < num; i++)
    {
    bgav_dprintf("ID: %d, O: %d, S: %d, PTS: %ld, rec: ",
                 f[i].stream_index,
                 f[i].frame_offset,
                 f[i].frame_size,
                 f[i].pts);
    gavl_hexdump(f[i].frame_record, 16, 16);
    }
  }
#endif

typedef struct
  {
  uint8_t header[VMD_HEADER_SIZE];

  vmd_frame_t *frame_table;
  int current_frame;
  uint32_t frame_count;
  uint32_t frames_per_block;
 
  } vmd_priv_t;

static int probe_vmd(bgav_input_context_t * input)
  {
  uint16_t size;
  char * pos;
  if(input->filename)
    {
    pos = strrchr(input->filename, '.');
    if(!pos)
      return 0;
    if(strcasecmp(pos, ".vmd"))
      return 0;
    }
  if(!bgav_input_get_16_le(input, &size) ||
     (size != VMD_HEADER_SIZE - 2))
    return 0;
  return 1;
  }

static int open_vmd(bgav_demuxer_context_t * ctx)
  {
  vmd_priv_t * priv;
  bgav_stream_t * vs = NULL;
  bgav_stream_t * as = NULL;
  int samplerate;
  uint32_t toc_offset;
  uint8_t * raw_frame_table = NULL;
  int raw_frame_table_size;
  int ret = 0;
  int i, j;
  uint8_t chunk[BYTES_PER_FRAME_RECORD];
  uint32_t current_offset;

  uint8_t type;
  uint32_t size;

  int64_t current_video_pts = 0;
  int frame_index = 0;
   
  if(!(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Cannot open VMD file from nonseekable source");
    goto fail;
    }
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  if(bgav_input_read_data(ctx->input, priv->header, VMD_HEADER_SIZE) <
     VMD_HEADER_SIZE)
    goto fail;

  ctx->tt = bgav_track_table_create(1);

  /* Initialize video stream */
  
  vs = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);

  vs->stream_id = VIDEO_ID;
  vs->fourcc = BGAV_MK_FOURCC('V','M','D','V');
  vs->data.video.format->image_width  = GAVL_PTR_2_16LE(&priv->header[12]);
  vs->data.video.format->image_height = GAVL_PTR_2_16LE(&priv->header[14]);

  vs->data.video.format->frame_width  = vs->data.video.format->image_width;
  vs->data.video.format->frame_height = vs->data.video.format->image_height;
  vs->data.video.format->pixel_width  = 1;
  vs->data.video.format->pixel_height = 1;

  bgav_stream_set_extradata(vs, priv->header, VMD_HEADER_SIZE);
  /* Initialize audio stream */
  samplerate = GAVL_PTR_2_16LE(&priv->header[804]);

  if(samplerate)
    {
    as = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
    as->stream_id = AUDIO_ID;
    as->fourcc = BGAV_MK_FOURCC('V','M','D','A');
    as->data.audio.format->samplerate = samplerate;
    as->data.audio.format->num_channels =
      (priv->header[811] & 0x80) ? 2 : 1;
    as->data.audio.block_align = GAVL_PTR_2_16LE(&priv->header[806]);
    if(as->data.audio.block_align & 0x8000)
      {
      as->data.audio.bits_per_sample = 16;
      as->data.audio.block_align = -(as->data.audio.block_align - 0x10000);
      }
    else
      as->data.audio.bits_per_sample = 8;
    
    vs->data.video.format->frame_duration = as->data.audio.block_align;
    vs->data.video.format->timescale =
      as->data.audio.format->num_channels * as->data.audio.format->samplerate;
    }
  else
    {
    /* Assume 10 fps */
    vs->data.video.format->frame_duration = 1;
    vs->data.video.format->timescale = 10;
    }


  /* Get table of contents */

  toc_offset = GAVL_PTR_2_32LE(&priv->header[812]);
  priv->frame_count = GAVL_PTR_2_16LE(&priv->header[6]);
  priv->frames_per_block = GAVL_PTR_2_16LE(&priv->header[18]);
  
  bgav_input_seek(ctx->input, toc_offset, SEEK_SET);

  raw_frame_table_size = priv->frame_count * 6;
  raw_frame_table = malloc(raw_frame_table_size);
  priv->frame_table =
    malloc(priv->frame_count*priv->frames_per_block*
           sizeof(*priv->frame_table));

  if(bgav_input_read_data(ctx->input, raw_frame_table, raw_frame_table_size)
     < raw_frame_table_size)
    goto fail;

  for(i = 0; i < priv->frame_count; i++)
    {
    current_offset = GAVL_PTR_2_32LE(&raw_frame_table[6*i+2]);
    for (j = 0; j < priv->frames_per_block; j++)
      {
      if(bgav_input_read_data(ctx->input, chunk, BYTES_PER_FRAME_RECORD) <
         BYTES_PER_FRAME_RECORD)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Unexpected end of file %d %d", i, j);
        goto fail;
        }
      type = chunk[0];
      size = GAVL_PTR_2_32LE(&chunk[2]);

      if(!size && type != 1)
        continue;
      
      /* Common fields */
      priv->frame_table[frame_index].frame_offset = current_offset;
      priv->frame_table[frame_index].frame_size = size;
      memcpy(priv->frame_table[frame_index].frame_record,
             chunk, BYTES_PER_FRAME_RECORD);
      
      switch(type)
        {
        case 1: /* Audio */
          priv->frame_table[frame_index].stream_index = AUDIO_ID;
          break;
        case 2: /* Video */
          priv->frame_table[frame_index].stream_index = VIDEO_ID;
          priv->frame_table[frame_index].pts = current_video_pts;
          break;
        }
      current_offset += size;
      frame_index++;
      }
    current_video_pts += vs->data.video.format->frame_duration;
    }

  priv->frame_count = frame_index;

  //  dump_frames(priv->frame_table, priv->frame_count);
  
  /* */

  bgav_track_set_format(ctx->tt->cur, "Sierra VMD", NULL);
    
  ret = 1;
  
  ctx->data_start = ctx->input->position;
  ctx->flags |= BGAV_DEMUXER_HAS_DATA_START;

  fail:
  if(raw_frame_table)
    free(raw_frame_table);
  
  return ret;
  }


static int next_packet_vmd(bgav_demuxer_context_t * ctx)
  {
  vmd_priv_t * priv;
  vmd_frame_t *frame;
  bgav_stream_t * s;
  bgav_packet_t * p;
  
  priv = ctx->priv;

  if(priv->current_frame >= priv->frame_count)
    return 0;

  frame = &priv->frame_table[priv->current_frame];

  s = bgav_track_find_stream(ctx, frame->stream_index);
  if(s)
    {
    bgav_input_seek(ctx->input, frame->frame_offset, SEEK_SET);
    p = bgav_stream_get_packet_write(s);

    bgav_packet_alloc(p, frame->frame_size + BYTES_PER_FRAME_RECORD);
    memcpy(p->data, frame->frame_record, BYTES_PER_FRAME_RECORD);
    if(bgav_input_read_data(ctx->input, p->data + BYTES_PER_FRAME_RECORD,
                            frame->frame_size) < frame->frame_size)
      return 0;

    p->data_size = frame->frame_size + BYTES_PER_FRAME_RECORD;
    if(s->type == GAVL_STREAM_VIDEO)
      {
      p->pts = frame->pts;
      //      fprintf(stderr, "Got video frame\n");
      //      bgav_packet_dump(p);
      }
#if 0
    else
      {
      fprintf(stderr, "Got audio frame\n");
      bgav_packet_dump(p);
      }
#endif
    bgav_stream_done_packet_write(s, p);
    }
  priv->current_frame++;
  return 1;
  }

static int select_track_vmd(bgav_demuxer_context_t * ctx, int track)
  {
  vmd_priv_t * priv;
  priv = ctx->priv;
  priv->current_frame = 0;
  return 1;
  }

static void close_vmd(bgav_demuxer_context_t * ctx)
  {
  vmd_priv_t * priv;
  priv = ctx->priv;
  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_vmd =
  {
    .probe =       probe_vmd,
    .open =        open_vmd,
    .select_track = select_track_vmd,
    .next_packet = next_packet_vmd,
    .close =       close_vmd
  };
