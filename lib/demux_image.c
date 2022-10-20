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

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

// #include <yuv4mpeg.h>

#define LOG_DOMAIN "demux_image"

#define PROBE_LEN 12

typedef struct
  {
  } image_t;

static const uint8_t png_sig[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

static int is_png(const uint8_t * data)
  {
  if(!memcmp(data, png_sig, 8))
    return 1;
  else
    return 0;
  }

static const uint8_t tiff_sig_be[] = { 0x4D, 0x4D, 0x00, 0x2A };
static const uint8_t tiff_sig_le[] = { 0x49, 0x49, 0x2A, 0x00 };

static int is_tiff(const uint8_t * data)
  {
  if(!memcmp(data, tiff_sig_le, 4) ||
     !memcmp(data, tiff_sig_be, 4))
    return 1;
  else
    return 0;
 }

static const uint8_t jpeg_sig_raw[] = { 0xFF, 0xD8, 0xFF, 0xDB };

static const uint8_t jpeg_sig_jfif_l[] = { 0xFF, 0xD8, 0xFF, 0xE0 };
static const uint8_t jpeg_sig_jfif_h[] = { 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01 };

static const uint8_t jpeg_sig_exif_l[] = { 0xFF, 0xD8, 0xFF, 0xE1 };
static const uint8_t jpeg_sig_exif_h[] = { 0x45, 0x78, 0x69, 0x66, 0x00, 0x00 };

static int is_jpeg(const uint8_t * data)
  {
  if(!memcmp(data, jpeg_sig_raw, 4))
    return 1;

  if(!memcmp(data,     jpeg_sig_jfif_l, 4) &&
     !memcmp(data + 6, jpeg_sig_jfif_h, 6))
    return 1;

  if(!memcmp(data,     jpeg_sig_exif_l, 4) &&
     !memcmp(data + 6, jpeg_sig_exif_h, 6))
    return 1;

  return 0;
  }

static int probe_image(bgav_input_context_t * input)
  {
  uint8_t probe_data[PROBE_LEN];

  if(input->total_bytes <= 0)
    return 0;

  if(bgav_input_get_data(input, probe_data, PROBE_LEN) < PROBE_LEN)
    return 0;

  // https://en.wikipedia.org/wiki/List_of_file_signatures

  if(is_png(probe_data) ||
     is_tiff(probe_data) ||
     is_jpeg(probe_data)) 
    return 1;

  return 0;
  }

static int open_image(bgav_demuxer_context_t * ctx)
  {
  uint8_t probe_data[PROBE_LEN];
  bgav_stream_t * s;
  
  /* Create track table */

  ctx->tt = bgav_track_table_create(1);
  
  /* Set up the stream */
  s = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);

  if(bgav_input_get_data(ctx->input, probe_data, PROBE_LEN) < PROBE_LEN)
    return 0;

  if(is_png(probe_data))
    {
    s->fourcc = BGAV_MK_FOURCC('p', 'n', 'g', ' ');
    bgav_track_set_format(ctx->tt->cur, "PNG image", "image/png");
    }
  else if(is_tiff(probe_data))
    {
    s->fourcc = BGAV_MK_FOURCC('t', 'i', 'f', 'f');
    bgav_track_set_format(ctx->tt->cur, "TIFF image", "image/tiff");
    }
  else if(is_jpeg(probe_data))
    {
    s->fourcc = BGAV_MK_FOURCC('j', 'p', 'e', 'g');    
    bgav_track_set_format(ctx->tt->cur, "JPEG image", "image/jpeg");
    }

  s->data.video.format->timescale = 1000; // Actually arbitrary since we only have pts = 0
  s->data.video.format->frame_duration = 0;
  s->data.video.format->framerate_mode = GAVL_FRAMERATE_STILL;
  
  s->timescale = s->data.video.format->timescale;
  s->data.video.format->pixel_width  = 1;
  s->data.video.format->pixel_height = 1;  
  
  s->ci->flags &= ~(GAVL_COMPRESSION_HAS_B_FRAMES | GAVL_COMPRESSION_HAS_P_FRAMES);
  
  ctx->index_mode = INDEX_MODE_SIMPLE;
  return 1;
  }


static int next_packet_image(bgav_demuxer_context_t * ctx)
  {
  bgav_packet_t * p;
  bgav_stream_t * s;
  
  s = bgav_track_get_video_stream(ctx->tt->cur, 0);
  
  p = bgav_stream_get_packet_write(s);
  p->position = 0;

  bgav_packet_alloc(p, ctx->input->total_bytes);

  if(bgav_input_read_data(ctx->input, p->buf.buf, ctx->input->total_bytes) < ctx->input->total_bytes)
    return 0;

  p->buf.len = ctx->input->total_bytes;
  
  p->pts = 0;
  
  PACKET_SET_KEYFRAME(p);
  p->duration = s->data.video.format->frame_duration;
  bgav_stream_done_packet_write(s, p);
  return 1;
  }

static void resync_image(bgav_demuxer_context_t * ctx, bgav_stream_t * s)
  {
  }

static void close_image(bgav_demuxer_context_t * ctx)
  {
  }

const bgav_demuxer_t bgav_demuxer_image =
  {
    .probe        = probe_image,
    .open         = open_image,
    .next_packet = next_packet_image,
    .resync      = resync_image,
    .close =       close_image
  };
