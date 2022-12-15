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


/* TGA decoder for quicktime */

#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <avdec_private.h>
#include <codecs.h>

#include "targa.h"

#define LOG_DOMAIN "video_tga"

typedef struct
  {
  tga_image tga;
  gavl_video_frame_t * frame;
  int bytes_per_pixel;
  
  bgav_packet_t * p;
  
  /* Read the first frame during initialization to get the format */
  int do_init;
  
  /* Colormap */
  uint8_t * ctab;
  int ctab_size;
  } tga_priv_t;

static gavl_pixelformat_t
get_pixelformat(int depth, int * bytes_per_pixel)
  {
  switch(depth)
    {
    case 8: /* Grayscale */
      *bytes_per_pixel = 1;
      return GAVL_GRAY_8;
      break;
    case 16:
      *bytes_per_pixel = 2;
      return GAVL_RGB_15;
      break;
    case 24:
      *bytes_per_pixel = 3;
      return GAVL_BGR_24;
      break;
    case 32:
      *bytes_per_pixel = 4;
      return GAVL_RGBA_32;
      break;
    default:
      return GAVL_PIXELFORMAT_NONE;
    }
  }

#if 0
static int packet_count = 0;
static void dump_packet(uint8_t * data, int size)
  {
  FILE * f;
  char filename_buffer[512];
  sprintf(filename_buffer, "out%03d.tga", packet_count++);
  f = fopen(filename_buffer, "w");
  fwrite(data, 1, size, f);
  fclose(f);
  }
#endif

static int set_palette(bgav_stream_t * s, gavl_palette_t * pal)
  {
  int i;
  tga_priv_t * priv;
  
  priv = s->decoder_priv;
  if(priv->ctab_size && (priv->ctab_size != pal->num_entries * 4))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Palette size changed %d -> %d",
             priv->ctab_size/4, pal->num_entries);
    return 0;
    }
  
  priv->ctab_size = pal->num_entries * 4;

  if(!priv->ctab)
    priv->ctab = malloc(priv->ctab_size);
  
  for(i = 0; i < pal->num_entries; i++)
    {
    priv->ctab[i*4+0] = (pal->entries[i].r) >> 8;
    priv->ctab[i*4+1] = (pal->entries[i].g) >> 8;
    priv->ctab[i*4+2] = (pal->entries[i].b) >> 8;
    priv->ctab[i*4+3] = (pal->entries[i].a) >> 8;
    }
  gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN,
           "Setting palette %d entries", pal->num_entries);
  return 1;
  }

static gavl_source_status_t decode_tga(bgav_stream_t * s, gavl_video_frame_t * frame)
  {
  gavl_source_status_t st;
  int result;
  tga_priv_t * priv;
  gavl_palette_t * pal;
  
  priv = s->decoder_priv;
  
  if(!(s->flags & STREAM_HAVE_FRAME))
    {
    /* Decode a frame */
    if((st = bgav_stream_get_packet_read(s, &priv->p)) != GAVL_SOURCE_OK)
      return st;

    /* Set palette */
    
    if((pal = gavl_packet_get_extradata(priv->p, GAVL_PACKET_EXTRADATA_PALETTE)) &&
       !set_palette(s, pal))
      return GAVL_SOURCE_EOF;
    
    result = tga_read_from_memory(&priv->tga, priv->p->buf.buf,
                                  priv->p->buf.len,
                                  priv->ctab, priv->ctab_size);
    if(result != TGA_NOERR)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "tga_read_from_memory failed: %s (%d bytes)",
              tga_error(result), priv->p->buf.len);
      //      dump_packet(p->data, p->data_size);
      return GAVL_SOURCE_EOF;
      }

    s->flags |= STREAM_HAVE_FRAME;
    }
  if(priv->do_init)
    {
    /* Figure out the format */

    s->data.video.format->frame_width  = priv->tga.width;
    s->data.video.format->frame_height = priv->tga.height;
    
    s->data.video.format->image_width  = priv->tga.width;
    s->data.video.format->image_height = priv->tga.height;
    
    s->data.video.format->pixel_width  = 1;
    s->data.video.format->pixel_height = 1;
    switch(priv->tga.image_type)
      {
      case TGA_IMAGE_TYPE_COLORMAP:
      case TGA_IMAGE_TYPE_COLORMAP_RLE:
        s->data.video.format->pixelformat =
          get_pixelformat(priv->tga.color_map_depth,
                          &priv->bytes_per_pixel);
        break;
      default:
        s->data.video.format->pixelformat =
          get_pixelformat(priv->tga.pixel_depth,
                          &priv->bytes_per_pixel);
        break;
      }
    if(s->data.video.format->pixelformat == GAVL_PIXELFORMAT_NONE)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Cannot detect image type: %d", priv->tga.image_type);
      return GAVL_SOURCE_EOF;
      }
    priv->frame = gavl_video_frame_create(NULL);
    return 1;
    }
  /* Copy it */
  
  if(frame)
    {
    switch(priv->tga.image_type)
      {
      case TGA_IMAGE_TYPE_COLORMAP:
      case TGA_IMAGE_TYPE_COLORMAP_RLE:
        if(tga_color_unmap(&priv->tga) != TGA_NOERR)
          {
          tga_free_buffers(&priv->tga);
          memset(&priv->tga, 0, sizeof(priv->tga));
          return GAVL_SOURCE_EOF;
          }
        break;
      default:
        break;
      }
    if(s->data.video.format->pixelformat == GAVL_RGBA_32)
      tga_swap_red_blue(&priv->tga);

    priv->frame->strides[0] =
      priv->bytes_per_pixel * s->data.video.format->image_width;
    priv->frame->planes[0] = priv->tga.image_data;
    
    /* Figure out the copy function */
    
    if(priv->tga.image_descriptor & TGA_R_TO_L_BIT)
      {
      if(priv->tga.image_descriptor & TGA_T_TO_B_BIT)
        {
        gavl_video_frame_copy_flip_x(s->data.video.format,
                                     frame, priv->frame);
        }
      else
        {
        gavl_video_frame_copy_flip_xy(s->data.video.format,
                                      frame, priv->frame);
        }
      }
    else
      {
      if(priv->tga.image_descriptor & TGA_T_TO_B_BIT)
        {
        gavl_video_frame_copy(s->data.video.format,
                              frame, priv->frame);
        }
      else
        {
        gavl_video_frame_copy_flip_y(s->data.video.format,
                                     frame, priv->frame);
        }
      }

    bgav_set_video_frame_from_packet(priv->p, frame);
    bgav_stream_done_packet_read(s, priv->p);
    }
  /* Free anything */

  tga_free_buffers(&priv->tga);
  memset(&priv->tga, 0, sizeof(priv->tga));
  return GAVL_SOURCE_OK;
  }

static int init_tga(bgav_stream_t * s)
  {
  tga_priv_t * priv;
  priv = calloc(1, sizeof(*priv));

  s->ci->flags &= ~GAVL_COMPRESSION_HAS_P_FRAMES;
  
  s->decoder_priv = priv;
  
  /* Get format by decoding first frame */

  priv->do_init = 1;
  
  if(!decode_tga(s, NULL))
    return 0;
  
  priv->do_init = 0;
  
  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT, "TGA");
  return 1;
  }

static void close_tga(bgav_stream_t * s)
  {
  tga_priv_t * priv;
  priv = s->decoder_priv;
  gavl_video_frame_null(priv->frame);
  gavl_video_frame_destroy(priv->frame);
  if(priv->ctab)
    free(priv->ctab);
  free(priv);
  }

static bgav_video_decoder_t decoder =
  {
    .name =   "TGA video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('t', 'g', 'a', ' '),
                            0x00  },
    .init =   init_tga,
    .decode = decode_tga,
    .close =  close_tga,
    .resync = NULL,
  };

void bgav_init_video_decoders_tga()
  {
  bgav_video_decoder_register(&decoder);
  }

