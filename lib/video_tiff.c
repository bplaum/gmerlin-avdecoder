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




/* Tiff decoder for quicktime. Based on gmerlin tiff reader plugin by
   one78 */

#include <stdlib.h>
#include <string.h>
#include <tiffio.h>

#include <avdec_private.h>
#include <codecs.h>

typedef struct
  {
  TIFF * tiff;
  uint8_t *buffer;
  uint64_t buffer_size;
  uint32_t buffer_position;
  uint32_t buffer_alloc;
  uint32_t Width;
  uint32_t Height;
  uint16_t SampleSperPixel;
  uint16_t Orientation;
  uint32_t * raster;
  bgav_packet_t * packet;
  } tiff_t;

/* libtiff read callbacks */

static tsize_t read_function(thandle_t fd, tdata_t data, tsize_t length)
  {
  uint32_t bytes_read;
  tiff_t *p = (tiff_t*)fd;

  bytes_read = length;
  if(length > p->buffer_size - p->buffer_position)
    bytes_read = p->buffer_size - p->buffer_position;

  memcpy(data, p->buffer + p->buffer_position, bytes_read);
  p->buffer_position += bytes_read;
  return bytes_read;
  }

static toff_t seek_function(thandle_t fd, toff_t off, int whence)
  {
  tiff_t *p = (tiff_t*)fd;

  if (whence == SEEK_SET) p->buffer_position = off;
  else if (whence == SEEK_CUR) p->buffer_position += off;
  else if (whence == SEEK_END) p->buffer_size += off;

  if (p->buffer_position > p->buffer_size) {
  return -1;
  }

  if (p->buffer_position > p->buffer_size)
    p->buffer_position = p->buffer_size;

  return (p->buffer_position);
  }

static toff_t size_function(thandle_t fd)
  {
  tiff_t *p = (tiff_t*)fd;
  return (p->buffer_size);
  }

static int close_function(thandle_t fd)
  {
  return (0);
  }

static tsize_t write_function(thandle_t fd, tdata_t data, tsize_t length)
  {
  return 0;
  }

static int map_file_proc(thandle_t a, tdata_t* b, toff_t* c)
  {
  return 0;
  }

static void unmap_file_proc(thandle_t a, tdata_t b, toff_t c)
  {
  }

static TIFF* open_tiff_mem(char *mode, tiff_t* p)
  {
  return TIFFClientOpen("gmerlin_avdecoder", mode, (thandle_t)p,
                        read_function,write_function ,
                        seek_function, close_function,
                        size_function, map_file_proc ,unmap_file_proc);
  }



static gavl_source_status_t
read_header_tiff(bgav_stream_t * s,
                 gavl_video_format_t * format)
  {
  gavl_source_status_t st;
  tiff_t *p = s->decoder_priv;

  p->packet = NULL;

  if((st = bgav_stream_get_packet_read(s, &p->packet)) != GAVL_SOURCE_OK)
    return st;
  
  //  tiff_read_mem(priv, filename);
  
  p->buffer = p->packet->buf.buf;
  p->buffer_size = p->packet->buf.len;
  p->buffer_position = 0;
  
  if(!(p->tiff = open_tiff_mem("rm", p))) return 0;
  
  if(format)
    {
    if(!(TIFFGetField(p->tiff, TIFFTAG_IMAGEWIDTH, &p->Width))) return 0;
    if(!(TIFFGetField(p->tiff, TIFFTAG_IMAGELENGTH, &p->Height))) return 0;
    if(!(TIFFGetField(p->tiff, TIFFTAG_SAMPLESPERPIXEL, &p->SampleSperPixel)))return 0;
    
    if(!(TIFFGetField(p->tiff, TIFFTAG_ORIENTATION, &p->Orientation)))
      p->Orientation = ORIENTATION_TOPLEFT;
    
    format->frame_width  = p->Width;
    format->frame_height = p->Height;
    
    format->image_width  = format->frame_width;
    format->image_height = format->frame_height;
    format->pixel_width = 1;
    format->pixel_height = 1;
    
    if(p->SampleSperPixel ==4)
      {
      format->pixelformat = GAVL_RGBA_32;
      }
    else
      {
      format->pixelformat = GAVL_RGB_24;
      }
    }
  return GAVL_SOURCE_OK;
  }

#define GET_RGBA(fp, rp)  \
  fp[0]=TIFFGetR(*rp); \
  fp[1]=TIFFGetG(*rp); \
  fp[2]=TIFFGetB(*rp); \
  fp[3]=TIFFGetA(*rp); \
  fp += 4;

#define GET_RGB(fp, rp)  \
  fp[0]=TIFFGetR(*rp); \
  fp[1]=TIFFGetG(*rp); \
  fp[2]=TIFFGetB(*rp); \
  fp += 3;


static int read_image_tiff(bgav_stream_t * s, gavl_video_frame_t * frame)
  {
  uint32_t * raster_ptr;
  uint8_t * frame_ptr;
  uint8_t * frame_ptr_start;
  int i, j;
  
  tiff_t *p = s->decoder_priv;

  if(!p->raster)
    p->raster =
      (uint32_t*)_TIFFmalloc(p->Height * p->Width * sizeof(uint32_t));
  
  if(!TIFFReadRGBAImage(p->tiff, p->Width, p->Height, (uint32_t*)p->raster, 0))
    return 0;

  if(p->SampleSperPixel ==4)
    {
    frame_ptr_start = frame->planes[0];

    for (i=0;i<p->Height; i++)
      {
      frame_ptr = frame_ptr_start;

      raster_ptr = p->raster + (p->Height - 1 - i) * p->Width;

      for(j=0;j<p->Width; j++)
        {
        GET_RGBA(frame_ptr, raster_ptr);
        raster_ptr++;
        }
      frame_ptr_start += frame->strides[0];
      }
    }
  else
    {
    frame_ptr_start = frame->planes[0];

    for (i=0;i<p->Height; i++)
      {
      frame_ptr = frame_ptr_start;

      raster_ptr = p->raster + (p->Height - 1 - i) * p->Width;

      for(j=0;j<p->Width; j++)
        {
        GET_RGB(frame_ptr, raster_ptr);
        raster_ptr++;
        }
      frame_ptr_start += frame->strides[0];
      }
    }

  bgav_set_video_frame_from_packet(p->packet, frame);
  
  TIFFClose( p->tiff );
  p->tiff = NULL;
  bgav_stream_done_packet_read(s, p->packet);
  p->packet = NULL;
  
  return 1;
  
  }

static int init_tiff(bgav_stream_t * s)
  {
  tiff_t * priv;
  priv = calloc(1, sizeof(*priv));
  s->decoder_priv = priv;
  s->ci->flags &= ~GAVL_COMPRESSION_HAS_P_FRAMES;
  
  /* We support RGBA for streams with a depth of 32 */

  if(!read_header_tiff(s, s->data.video.format))
    return 0;
    
  if(s->data.video.depth == 32)
    s->data.video.format->pixelformat = GAVL_RGBA_32;
  else
    s->data.video.format->pixelformat = GAVL_RGB_24;

  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT, "TIFF");
  return 1;
  }

static gavl_source_status_t
decode_tiff(bgav_stream_t * s, gavl_video_frame_t * frame)
  {
  gavl_source_status_t st;
  tiff_t * priv;
  priv = s->decoder_priv;

  
  /* We decode only if we have a frame */

  if(frame)
    {
    if(!priv->packet &&
       ((st = read_header_tiff(s, NULL)) != GAVL_SOURCE_OK))
      return st;
    read_image_tiff(s, frame);
    }
  else
    {
    if((st = bgav_stream_get_packet_read(s, &priv->packet)) != GAVL_SOURCE_OK)
      return st;
    bgav_stream_done_packet_read(s, priv->packet);
    priv->packet = NULL;
    }
  return GAVL_SOURCE_OK;
  }

static void close_tiff(bgav_stream_t * s)
  {
  tiff_t * priv;
  priv = s->decoder_priv;

  if (priv->raster) _TIFFfree(priv->raster);

  free(priv);
  }

static void resync_tiff(bgav_stream_t * s)
  {
  tiff_t *p = s->decoder_priv;
  
  if(p->packet)
    {
    bgav_stream_done_packet_read(s, p->packet);
    p->packet = NULL;
    }
  }

static bgav_video_decoder_t decoder =
  {
    .name =   "TIFF video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('t', 'i', 'f', 'f'),
                            0x00  },
    .init =   init_tiff,
    .decode = decode_tiff,
    .close =  close_tiff,
    .resync = resync_tiff,
  };

void bgav_init_video_decoders_tiff()
  {
  bgav_video_decoder_register(&decoder);
  }
