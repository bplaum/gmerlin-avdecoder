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
#include <string.h>

#define SEQ_FRAME_SIZE         6144
#define SEQ_FRAME_W            256
#define SEQ_FRAME_H            128
#define SEQ_NUM_FRAME_BUFFERS  30
#define SEQ_AUDIO_BUFFER_SIZE  (882*2)
#define SEQ_SAMPLE_RATE        22050
#define SEQ_FRAME_RATE         25

#define AUDIO_ID 0
#define VIDEO_ID 1

typedef struct
  {
  uint16_t sound_data_offset;
  uint16_t palette_offset;

  int buffer_num[4];
  int offset_table[4];
  } frame_header_t;

typedef struct
  {
  int fill_size;
  int data_size;
  uint8_t *data;
  } frame_buffer_t;


static void parse_frame_header(uint8_t * data,
                               frame_header_t * h)
  {
  int i;
  h->sound_data_offset = GAVL_PTR_2_16LE(data); data+=2;
  h->palette_offset    = GAVL_PTR_2_16LE(data); data+=2;

  for(i = 0; i < 4; i++)
    {
    h->buffer_num[i] = *data; data++;
    }
  
  for(i = 0; i < 4; i++)
    {
    h->offset_table[i] = GAVL_PTR_2_16LE(data); data+=2;
    }
  }

#if 0
static void dump_frame_header(frame_header_t * h)
  {
  int i;
  bgav_dprintf("Tiertex header\n");
  bgav_dprintf("  sound_data_offset: %d\n", h->sound_data_offset);
  bgav_dprintf("  palette_offset:    %d\n", h->palette_offset);

  for(i = 0; i < 4; i++)
    bgav_dprintf("  buffer[%d]:        %d\n", i, h->buffer_num[i]);

  for(i = 0; i < 4; i++)
    bgav_dprintf("  offset_table[%d]:  %d\n", i, h->offset_table[i]);
  }
#endif

typedef struct
  {
  uint32_t video_pts;

  frame_buffer_t frame_buffers[SEQ_NUM_FRAME_BUFFERS];
  int num_frame_buffers;
  } tiertex_priv_t;

static int probe_tiertex(bgav_input_context_t * input)
  {
  int i;
  const char * pos;
  uint8_t test_data[256];

  /* These files have no definite signature, so we test them
     the following way:

     1. Total file size must be available and a multiple of 6144
     2. Filename must end with .seq (case insensitive)
     3. First 256 bytes must be zero
  */

  if(!input->total_bytes || (input->total_bytes % SEQ_FRAME_SIZE))
    return 0;

  if(!input->filename)
    return 0;

  pos = strrchr(input->filename, '.');
  if(!pos)
    return 0;
  if(strcasecmp(pos, ".seq"))
    return 0;

  if(bgav_input_get_data(input, test_data, 256) < 256)
    return 0;

  for(i = 0; i < 256; i++)
    if(test_data[i]) return 0;

  return 1;
  }

static int open_tiertex(bgav_demuxer_context_t * ctx)
  {
  int i;
  uint8_t buf[SEQ_FRAME_SIZE];
  uint8_t * ptr;
  
  tiertex_priv_t * priv;
  bgav_stream_t * s;
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
  /* Stream formats are always the same */

  ctx->tt = bgav_track_table_create(1);

  /* Setup audio stream */
  s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
  //  s->fourcc = BGAV_WAVID_2_FOURCC(0x0001);
  s->fourcc = BGAV_MK_FOURCC('t','w','o','s');
  s->data.audio.format->samplerate = SEQ_SAMPLE_RATE;
  s->data.audio.format->num_channels = 1;
  s->data.audio.bits_per_sample = 16;
  s->stream_id = AUDIO_ID;
  
  /* Setup video stream */
  s = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
  s->fourcc = BGAV_MK_FOURCC('T','I','T','X');
  s->data.video.format->image_width  = SEQ_FRAME_W;
  s->data.video.format->image_height = SEQ_FRAME_H;
  s->data.video.format->frame_width  = SEQ_FRAME_W;
  s->data.video.format->frame_height = SEQ_FRAME_H;
  s->data.video.format->pixel_width  = 1;
  s->data.video.format->pixel_height = 1;
  s->data.video.format->timescale = SEQ_FRAME_RATE;
  s->data.video.format->frame_duration = 1;
  s->stream_id = VIDEO_ID;
  s->data.video.format->framerate_mode = GAVL_FRAMERATE_VARIABLE;
  s->flags |= STREAM_NO_DURATIONS;

  /* Set up buffers */
  
  if(bgav_input_read_data(ctx->input, buf, SEQ_FRAME_SIZE) < SEQ_FRAME_SIZE)
    return 0;

  ptr = &buf[256];
  for(i = 0; i < SEQ_NUM_FRAME_BUFFERS; i++)
    {
    priv->frame_buffers[i].data_size = GAVL_PTR_2_16LE(ptr);ptr+=2;
    if(!priv->frame_buffers[i].data_size)
      break;
    priv->frame_buffers[i].data = malloc(priv->frame_buffers[i].data_size);
    }
  priv->num_frame_buffers = i;

  bgav_track_set_format(ctx->tt->cur, "Tiertex SEQ", NULL);
  
  ctx->data_start = ctx->input->position;
  ctx->flags |= BGAV_DEMUXER_HAS_DATA_START;
  
  return 1;
  }

static int fill_buffer(frame_buffer_t * buffer,
                       uint8_t * data, int offset, int size)
  {
  if(buffer->fill_size + size > buffer->data_size)
    return 0;
  memcpy(buffer->data + buffer->fill_size, data + offset, size);
  buffer->fill_size += size;
  return 1;
  }

static gavl_source_status_t next_packet_tiertex(bgav_demuxer_context_t * ctx)
  {
  int i, j;
  int video_size, palette_size;
  frame_header_t fh;
  bgav_stream_t * s;
  uint8_t buf[SEQ_FRAME_SIZE];
  bgav_packet_t * p;
  
  tiertex_priv_t * priv;
  priv = ctx->priv;
  
  while(1)
    {
    if(bgav_input_read_data(ctx->input, buf, SEQ_FRAME_SIZE) < SEQ_FRAME_SIZE)
      return GAVL_SOURCE_EOF;
    parse_frame_header(buf, &fh);
    //    dump_frame_header(&fh);
    if(fh.offset_table[3] > 16)
      break;
    }

  /* Fill buffers */
  for(i = 0; i < 3; i++)
    {
    if(fh.offset_table[i] != 0)
      {
      for(j = i + 1; j < 4 && fh.offset_table[j] == 0; j++);
      if(!fill_buffer(&priv->frame_buffers[fh.buffer_num[1 + i]],
                      buf,
                      fh.offset_table[i],
                      fh.offset_table[j] - fh.offset_table[i]))
        return GAVL_SOURCE_EOF;
      }
    }

  /* Check if we got video data */
  if(fh.buffer_num[0] != 255)
    video_size = priv->frame_buffers[fh.buffer_num[0]].fill_size;
  else
    video_size = 0;

  if(fh.palette_offset)
    palette_size = 768;
  else
    palette_size = 0;

  if(video_size || palette_size)
    {
    s = bgav_track_find_stream(ctx, VIDEO_ID);
    if(s)
      {
      p = bgav_stream_get_packet_write(s);

      bgav_packet_alloc(p, video_size + palette_size + 1);
      p->buf.buf[0] = 0;

      if(palette_size)
        {
        memcpy(p->buf.buf + 1, buf + fh.palette_offset, palette_size);
        p->buf.buf[0] |= 1;
        }
      if(video_size)
        {
        memcpy(p->buf.buf + palette_size + 1,
               priv->frame_buffers[fh.buffer_num[0]].data,
               priv->frame_buffers[fh.buffer_num[0]].fill_size);
        priv->frame_buffers[fh.buffer_num[0]].fill_size = 0;
        p->buf.buf[0] |= 2;
        }
      p->buf.len = video_size + palette_size + 1;
      p->pts = priv->video_pts;
      bgav_stream_done_packet_write(s, p);
      }
    }

  /* Check for audio data */

  if(fh.sound_data_offset)
    {
    
    //    dump_frame_header(&fh);
    
    s = bgav_track_find_stream(ctx, AUDIO_ID);
    if(s)
      {
      p = bgav_stream_get_packet_write(s);
      
      bgav_packet_alloc(p, SEQ_AUDIO_BUFFER_SIZE);
      memcpy(p->buf.buf, buf + fh.sound_data_offset, SEQ_AUDIO_BUFFER_SIZE);
      p->buf.len = SEQ_AUDIO_BUFFER_SIZE;
      bgav_stream_done_packet_write(s, p);
      }
    priv->video_pts++;
    }
  
  //  
  
  return GAVL_SOURCE_OK;
  }

static void close_tiertex(bgav_demuxer_context_t * ctx)
  {
  int i;
  tiertex_priv_t * priv;
  priv = ctx->priv;

  if(!priv)
    return;
  
  for(i = 0; i < priv->num_frame_buffers; i++)
    free(priv->frame_buffers[i].data);
  free(priv);
  }

static int select_track_tiertex(bgav_demuxer_context_t * ctx, int t)
  {
  int i;
  tiertex_priv_t * priv;
  priv = ctx->priv;

  if(!priv)
    return 0;

  for(i = 0; i < SEQ_NUM_FRAME_BUFFERS; i++)
    priv->frame_buffers[i].fill_size = 0;
  
  priv->video_pts = 0;
  return 1;
  }

const bgav_demuxer_t bgav_demuxer_tiertex =
  {
    .probe =        probe_tiertex,
    .open =         open_tiertex,
    .select_track = select_track_tiertex,
    .next_packet =  next_packet_tiertex,
    .close =        close_tiertex
  };
