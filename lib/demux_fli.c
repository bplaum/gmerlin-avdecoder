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

#include <avdec_private.h>

/* Logic taken from libavformat */

#define FLIC_FILE_MAGIC_1    0xAF11
#define FLIC_FILE_MAGIC_2    0xAF12
#define FLIC_CHUNK_MAGIC_1   0xF1FA
#define FLIC_CHUNK_MAGIC_2   0xF5FA
#define FLIC_MC_PTS_INC      6000  /* pts increment for Magic Carpet game FLIs */
#define FLIC_DEFAULT_PTS_INC 6000  /* for FLIs that have 0 speed */

#define FLIC_HEADER_SIZE     128
#define FLIC_PREAMBLE_SIZE   6

typedef struct
  {
  uint8_t header[FLIC_HEADER_SIZE];
  int header_size;
  int skip_header;
  } fli_priv_t;

static int probe_fli(bgav_input_context_t * input)
  {
  uint16_t magic;
  uint8_t probe_data[6];

  if(bgav_input_get_data(input, probe_data, 6) < 6)
    return 0;
  
  magic = GAVL_PTR_2_16LE(&probe_data[4]);

  if((magic == FLIC_FILE_MAGIC_1) || (magic == FLIC_FILE_MAGIC_2))
    return 1;
  
  return 0;
  }

static int open_fli(bgav_demuxer_context_t * ctx)
  {
  fli_priv_t * priv;
  bgav_stream_t * s;
  uint16_t magic_number;
  uint32_t speed;
    
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  /* Read header */
  if(bgav_input_get_data(ctx->input, priv->header, FLIC_HEADER_SIZE) < FLIC_HEADER_SIZE)
    return 0;

  magic_number = GAVL_PTR_2_16LE(&priv->header[4]);

  speed = GAVL_PTR_2_32LE(&priv->header[0x10]);
  
  /* Create track */
  ctx->tt = bgav_track_table_create(1);
  
  /* Create and set up stream */
  s = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
  s->fourcc = BGAV_MK_FOURCC('F','L','I','C');

  s->data.video.format->image_width  = GAVL_PTR_2_16LE(&priv->header[0x08]);
  s->data.video.format->image_height = GAVL_PTR_2_16LE(&priv->header[0x0A]);

  if(!s->data.video.format->image_width || !s->data.video.format->image_height)
    return 0;
  
  s->data.video.format->frame_width  = s->data.video.format->image_width;
  s->data.video.format->frame_height = s->data.video.format->image_height;
  s->data.video.format->pixel_width  = 1;
  s->data.video.format->pixel_height = 1;

  priv->header_size = FLIC_HEADER_SIZE;
    
  /* Time to figure out the framerate: If there is a FLIC chunk magic
   * number at offset 0x10, assume this is from the Bullfrog game,
   * Magic Carpet. */
  
  if(GAVL_PTR_2_16LE(&priv->header[0x10]) == FLIC_CHUNK_MAGIC_1)
    {
    priv->header_size = 12;

    s->timescale = 15;
    s->data.video.format->timescale = 15;
    s->data.video.format->frame_duration = 1;
    }
  else if(magic_number == FLIC_FILE_MAGIC_1)
    {
    /*
     * in this case, the speed (n) is number of 1/70s ticks between frames:
     */
    s->timescale = 70;
    s->data.video.format->timescale = 70;
    s->data.video.format->frame_duration = speed;
    }
  else if(magic_number == FLIC_FILE_MAGIC_2)
    {
    /*
     * in this case, the speed (n) is number of milliseconds between frames:
     */
    s->timescale = 1000;
    s->data.video.format->timescale = 1000;
    s->data.video.format->frame_duration = speed;
    }
  else
    return 0;

  if(!s->data.video.format->frame_duration)
    {
    s->timescale = 15;
    s->data.video.format->timescale = 15;
    s->data.video.format->frame_duration = 1;
    }

  /* Set extradata */
  bgav_stream_set_extradata(s, priv->header, priv->header_size);
  
  bgav_track_set_format(ctx->tt->cur, "FLI/FLC", NULL);
  
  priv->skip_header = 1;
  
  ctx->tt->cur->data_start = ctx->input->position;
  return 1;
  }

static gavl_source_status_t next_packet_fli(bgav_demuxer_context_t * ctx)
  {
  bgav_packet_t * p;
  bgav_stream_t * s;

  uint32_t size;
  uint16_t magic;
  fli_priv_t * priv;
  uint8_t preamble[FLIC_PREAMBLE_SIZE];

  priv = ctx->priv;
  
  /* Skip header if not already done */
  if(priv->skip_header)
    {
    bgav_input_skip(ctx->input, priv->header_size);
    priv->skip_header = 0;
    }
  
  while(1)
    {
    if(bgav_input_read_data(ctx->input, preamble, FLIC_PREAMBLE_SIZE) < FLIC_PREAMBLE_SIZE)
      {
      return GAVL_SOURCE_EOF;
      }
      
    size  = GAVL_PTR_2_32LE(&preamble[0]);
    magic = GAVL_PTR_2_16LE(&preamble[4]);

    if(((magic == FLIC_CHUNK_MAGIC_1) || (magic == FLIC_CHUNK_MAGIC_2)) &&
       (size > FLIC_PREAMBLE_SIZE))
      {
      s = bgav_track_get_video_stream(ctx->tt->cur, 0);
      p = bgav_stream_get_packet_write(s);
      bgav_packet_alloc(p, size);
      
      memcpy(p->buf.buf, preamble, FLIC_PREAMBLE_SIZE);
      if(bgav_input_read_data(ctx->input, p->buf.buf + FLIC_PREAMBLE_SIZE,
                              size - FLIC_PREAMBLE_SIZE) < size - FLIC_PREAMBLE_SIZE)
        {
        return GAVL_SOURCE_EOF;
        }
      
      p->pts = s->in_position * s->data.video.format->frame_duration;
      p->buf.len = size;
      
      bgav_stream_done_packet_write(s, p);
      return GAVL_SOURCE_OK;
      }
    else
      {
      bgav_input_skip(ctx->input, size - FLIC_PREAMBLE_SIZE);
      }
    }
  
  return GAVL_SOURCE_EOF;
  }

static void close_fli(bgav_demuxer_context_t * ctx)
  {
  fli_priv_t * priv;
  priv = ctx->priv;
  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_fli =
  {
    .probe =       probe_fli,
    .open =        open_fli,
    .next_packet = next_packet_fli,
    .close =       close_fli
  };

