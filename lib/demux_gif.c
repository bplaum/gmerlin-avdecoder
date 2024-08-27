/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
 *
 * Copyright (c) 2001 - 2024 Members of the Gmerlin project
 * http://github.com/bplaum
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

#define LOG_DOMAIN "gif"

static char const * const gif_sig = "GIF89a";
#define SIG_LEN 6

#define GLOBAL_HEADER_LEN    13 /* Signature + screen descriptor */
#define GCE_LEN               8
#define IMAGE_DESCRIPTOR_LEN 10

typedef struct
  {
  uint16_t width;
  uint16_t height;
  uint8_t  flags;
  uint8_t  bg_color;
  uint8_t  pixel_aspect_ratio;
  } screen_descriptor_t;

static void parse_screen_descriptor(uint8_t * data,
                                    screen_descriptor_t * ret)
  {
  data += SIG_LEN;
  ret->width               = GAVL_PTR_2_16LE(data); data += 2;
  ret->height              = GAVL_PTR_2_16LE(data); data += 2;
  ret->flags               = *data; data++;
  ret->bg_color            = *data; data++;
  ret->pixel_aspect_ratio  = *data; data++;
  }

#if 0
static void dump_screen_descriptor(screen_descriptor_t * sd)
  {
  gavl_dprintf("GIF Screen descriptor\n");
  gavl_dprintf("  width:              %d\n",  sd->width);
  gavl_dprintf("  height:             %d\n", sd->height);
  gavl_dprintf("  .flags =              %02x\n", sd->flags);
  gavl_dprintf("  bg_color:           %d\n", sd->bg_color);
  gavl_dprintf("  pixel_aspect_ratio: %d\n", sd->pixel_aspect_ratio);
  }
#endif

/* Private context */

typedef struct
  {
  uint8_t header[GLOBAL_HEADER_LEN];
  uint8_t global_cmap[768];
  int global_cmap_bytes;
  } gif_priv_t;

static int probe_gif(bgav_input_context_t * input)
  {
  char probe_data[SIG_LEN];
  if(bgav_input_get_data(input, (uint8_t*)probe_data, SIG_LEN) < SIG_LEN)
    return 0;
  if(!memcmp(probe_data, gif_sig, SIG_LEN))
    return 1;
  return 0;
  }

static void skip_extension(bgav_input_context_t * input)
  {
  uint8_t u_8;

  bgav_input_skip(input, 2);
  while(1)
    {
    if(!bgav_input_read_data(input, &u_8, 1))
      return;
    if(!u_8)
      return;
    else
      bgav_input_skip(input, u_8);
    }
  
  }

static int open_gif(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;
  uint8_t ext_header[2];
  gif_priv_t * priv;
  screen_descriptor_t sd;
  int done;
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  if(bgav_input_read_data(ctx->input, priv->header, GLOBAL_HEADER_LEN) < GLOBAL_HEADER_LEN)
    return 0;
  
  parse_screen_descriptor(priv->header, &sd);
  //  dump_screen_descriptor(&sd);

  if(sd.flags & 0x80)
    {
    /* Global colormap present */
    priv->global_cmap_bytes = 3*(1 << ((sd.flags & 0x07) + 1));
    if(bgav_input_read_data(ctx->input, priv->global_cmap,
                            priv->global_cmap_bytes) <
       priv->global_cmap_bytes)
      return 0;
    }
  
  /* Check for .extensions = We skip all extensions until we reach
     the first image data *or* a Graphic Control Extension */
  done = 0;
  while(!done)
    {
    if(bgav_input_get_data(ctx->input, ext_header, 2) < 2)
      return 0;
    
    switch(ext_header[0])
      {
      case '!': // Extension block
        if(ext_header[1] == 0xF9) /* Graphic Control Extension */
          done = 1;
        else
          {
          skip_extension(ctx->input);
          }
        break;
      case ',':
        /* Image data found but no Graphic Control Extension:
           Assume, this is a still image and refuse to read it */
        return 0;
        break;
      default:
        return 0; /* Invalid data */
        break;
      }
    }

  ctx->tt = bgav_track_table_create(1);
  s = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
  s->fourcc = BGAV_MK_FOURCC('g','i','f',' ');

  s->ci->flags &= ~GAVL_COMPRESSION_HAS_P_FRAMES;

  s->data.video.format->image_width  = sd.width;
  s->data.video.format->image_height = sd.height;

  s->data.video.format->frame_width  = sd.width;
  s->data.video.format->frame_height = sd.height;
  s->data.video.format->pixel_width = 1;
  s->data.video.format->pixel_height = 1;
  s->data.video.format->timescale = 100;
  s->data.video.format->frame_duration = 100; // Not reliable
  s->data.video.format->framerate_mode = GAVL_FRAMERATE_VARIABLE;
  s->data.video.depth = 32; // RGBA
  s->data.video.format->pixelformat = GAVL_RGBA_32;

  ctx->tt->cur->data_start = ctx->input->position;

  ctx->index_mode = INDEX_MODE_SIMPLE;
  
  return 1;
  }

static gavl_source_status_t next_packet_gif(bgav_demuxer_context_t * ctx)
  {
  uint8_t buf[10];
  uint8_t gce[GCE_LEN];
  uint8_t image_descriptor[IMAGE_DESCRIPTOR_LEN];
  int local_cmap_len;
  int done = 0;
  gif_priv_t * priv;
  int frame_duration;
  bgav_stream_t * s;
  bgav_packet_t * p;

  int64_t ctxposition = ctx->input->position;
  priv = ctx->priv;

  while(!done)
    {
    if(!bgav_input_get_data(ctx->input, buf, 1))
      return GAVL_SOURCE_EOF;
    switch(buf[0])
      {
      case ';':
        return GAVL_SOURCE_EOF; // Trailer
        break;
      case '!':
        if(bgav_input_get_data(ctx->input, buf, 2) < 2)
          return GAVL_SOURCE_EOF;
        if(buf[1] == 0xF9) /* Graphic Control Extension */
          done = 1;
        else
          {
          /* Skip other extension */
          skip_extension(ctx->input);
          break;
          }
        break;
      default:
        return GAVL_SOURCE_EOF; /* Unknown/unhandled chunk */
      }
    }

  /* Parse GCE */
  if(!bgav_input_read_data(ctx->input, gce, GCE_LEN))
    return GAVL_SOURCE_EOF;
  frame_duration = GAVL_PTR_2_16LE(&gce[4]);

  /* Get the next image header */
  done = 0;
  while(!done)
    {
    if(!bgav_input_get_data(ctx->input, buf, 1))
      return GAVL_SOURCE_EOF;
    switch(buf[0])
      {
      case ';':
        return GAVL_SOURCE_EOF; // Trailer
        break;
      case '!':
        if(!bgav_input_get_data(ctx->input, buf, 2))
          return GAVL_SOURCE_EOF;
        if(buf[1] == 0xF9) /* Graphic Control Extension */
          return GAVL_SOURCE_EOF;
        else
          {
          /* Skip other extension */
          skip_extension(ctx->input);
          break;
          }
      case ',':
        if(bgav_input_read_data(ctx->input, image_descriptor,
                                IMAGE_DESCRIPTOR_LEN) < IMAGE_DESCRIPTOR_LEN)
          return GAVL_SOURCE_EOF;
        done = 1;
        break;
      default:
        return GAVL_SOURCE_EOF; /* Unknown/unhandled chunk */
      }
    }
  
  /* Now assemble a valid GIF file (that's the hard part) */
  s = bgav_track_get_video_stream(ctx->tt->cur, 0);
  
  p = bgav_stream_get_packet_write(s);
  p->position = ctxposition;

  gavl_packet_alloc(p, GLOBAL_HEADER_LEN + priv->global_cmap_bytes +
                    GCE_LEN + // GCE length
                    IMAGE_DESCRIPTOR_LEN); // Image Descriptor length

  p->buf.len = 0;

  /* Global header */
  memcpy(p->buf.buf, priv->header, GLOBAL_HEADER_LEN);
  p->buf.len += GLOBAL_HEADER_LEN;

  /* Global cmap */
  if(priv->global_cmap_bytes)
    {
    memcpy(p->buf.buf + p->buf.len, priv->global_cmap, priv->global_cmap_bytes);
    p->buf.len += priv->global_cmap_bytes;
    }

  /* GCE */
  memcpy(p->buf.buf + p->buf.len, gce, GCE_LEN);
  p->buf.len += GCE_LEN;

  /* Image descriptor */

  memcpy(p->buf.buf + p->buf.len, image_descriptor, IMAGE_DESCRIPTOR_LEN);
  p->buf.len += IMAGE_DESCRIPTOR_LEN;
  
  /* Local colormap (if present) */

  if(image_descriptor[IMAGE_DESCRIPTOR_LEN-1] & 0x80)
    {
    local_cmap_len =
      3*(1 << ((image_descriptor[IMAGE_DESCRIPTOR_LEN-1] & 0x07) + 1));

    gavl_packet_alloc(p, p->buf.len + local_cmap_len);
    
    if(bgav_input_read_data(ctx->input, p->buf.buf + p->buf.len,
                            local_cmap_len) < local_cmap_len)
      return GAVL_SOURCE_EOF;
    p->buf.len += local_cmap_len;
    }
  
  /* Image data */

  /* Initial code length */

  gavl_packet_alloc(p, p->buf.len + 1);
  if(!bgav_input_read_data(ctx->input, p->buf.buf + p->buf.len, 1))
    return GAVL_SOURCE_EOF;
  p->buf.len++;
  /* Data blocks */
  while(1)
    {
    if(!bgav_input_get_data(ctx->input, buf, 1))
      return GAVL_SOURCE_EOF;

    gavl_packet_alloc(p, p->buf.len + buf[0]+1);

    if(bgav_input_read_data(ctx->input, p->buf.buf +
                            p->buf.len, buf[0]+1) < buf[0]+1)
      return GAVL_SOURCE_EOF;
    p->buf.len += buf[0]+1;
    if(!buf[0])
      break;
    }
  
  /* Trailer */
  gavl_packet_alloc(p, p->buf.len + 1);
  p->buf.buf[p->buf.len] = ';';
  p->buf.len++;
  
  p->duration = frame_duration;
  
  bgav_stream_done_packet_write(s, p);
  return GAVL_SOURCE_OK;
  }


static void close_gif(bgav_demuxer_context_t * ctx)
  {
  gif_priv_t * priv;
  priv = ctx->priv;
  if(priv)
    free(priv);
  }

const bgav_demuxer_t bgav_demuxer_gif =
  {
    .probe =        probe_gif,
    .open =         open_gif,
    .next_packet =  next_packet_gif,
    .close =        close_gif
  };
