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
#include <codecs.h>
#include <bswap.h>

#define LOG_DOMAIN "subovl_dvd"

typedef struct
  {
  int packet_size; /* Size of the entire packet (read from the first 2 bytes) */
  int64_t pts;
  
  int pts_mult;
  int field_height;
  // gavl_video_format_t vs_format;

  } dvdsub_t;

static int init_dvdsub(bgav_stream_t * s)
  {
  bgav_stream_t * vs;

  gavl_video_format_t * video_stream_format;
  dvdsub_t * priv;

  if(!(vs = bgav_track_get_video_stream(s->track, 0)))
    return 0;
  
  priv = calloc(1, sizeof(*priv));
  s->decoder_priv = priv;

  /* Initialize format */
  video_stream_format = vs->data.video.format;
  
  if(!s->data.subtitle.video.format->image_width ||
     !s->data.subtitle.video.format->image_height)
    gavl_video_format_copy(s->data.subtitle.video.format, video_stream_format);

  // gavl_video_format_copy(&priv->vs_format, video_stream_format);
  
  s->data.subtitle.video.format->pixelformat = GAVL_YUVA_32;
  s->data.subtitle.video.format->timescale = s->timescale;
  s->data.subtitle.video.format->frame_duration = 0;
  s->data.subtitle.video.format->framerate_mode = GAVL_FRAMERATE_VARIABLE;
  
  priv->pts_mult = s->timescale / 100;
  priv->field_height = s->data.subtitle.video.format->image_height / 2;
  
  return 1;
  }

static int get_nibble(const uint8_t *buf, int nibble_offset)
  {
  return (buf[nibble_offset >> 1] >> ((1 - (nibble_offset & 1)) << 2)) & 0xf;
  }

static int decode_scanline(const uint8_t * ptr, uint32_t * dst,
                            uint32_t * palette, int width)
  {
  int i, x = 0, len, color;
  uint32_t v;
  int nibble_offset = 0;
  
  while(1)
    {
    /* Read value */
    v = get_nibble(ptr, nibble_offset++);
    if(v < 0x4)
      {
      v = (v << 4) | get_nibble(ptr, nibble_offset++);
      if (v < 0x10)
        {
        v = (v << 4) | get_nibble(ptr, nibble_offset++);
        if (v < 0x040)
          {
          v = (v << 4) | get_nibble(ptr, nibble_offset++);
          if (v < 4)
            {
            v |= (width - x) << 2;
            }
          }
        }
      }

    /* Fill pixels */
    len = v >> 2;
    color = v & 0x03;

    if(x + len > width)
      {
      fprintf(stderr, "x + len > width: %d + %d > %d\n", x, len, width);
      len = width - x;
      }
    
    for(i = 0; i < len; i++)
      {
      *dst = palette[color];
      dst++;
      }
    x += len;
    if(x >= width)
      return (nibble_offset + (nibble_offset & 1)) >> 1;
    }
  return 0;
  }

static void decode_field(uint8_t * ptr, int len,
                         uint8_t * dst, int width, int dst_stride,
                         uint32_t * palette, int max_height)
  {
  int ypos = 0;
  int pos = 0;

  while(pos < len)
    {
    pos += decode_scanline(ptr + pos, (uint32_t*)dst, palette, width);
    dst += dst_stride;
    ypos++;
    if(ypos >= max_height)
      break;
    }
  }

static const float fill_color[4] = { 0.0, 0.0, 0.0, 0.0 };

static gavl_source_status_t decode_dvdsub(bgav_stream_t * s, gavl_overlay_t * ovl)
  {
  gavl_source_status_t st;
  uint8_t * ptr;
  uint8_t cmd;
  uint16_t ctrl_offset, ctrl_start, next_ctrl_offset;
  dvdsub_t * priv;
  //  uint16_t date;
  uint8_t palette[4] = { 0,0,0,0 }, alpha[4] = { 0,0,0,0 };
  int x1 = 0, y1 = 0, x2= 0, y2= 0, i;
  uint16_t offset1 = 0, offset2 = 0;
  int ctrl_seq_end;
  uint32_t * ifo_palette;
  uint32_t local_palette[4];
  bgav_packet_t * p = NULL;
  
  if(s->flags & STREAM_EOF_C)
    return GAVL_SOURCE_EOF;
  
  priv = s->decoder_priv;

  ifo_palette = (uint32_t*)(s->ci->global_header);
  
  if((st = bgav_stream_peek_packet_read(s, &p, 0)) != GAVL_SOURCE_OK)
    return st;

  bgav_stream_get_packet_read(s, &p);

  fprintf(stderr, "Got overlay\n");
  bgav_packet_dump(p);

  gavl_video_frame_fill(ovl, s->data.subtitle.video.format,
                        fill_color);
  
  /* Data size */
  ctrl_offset = GAVL_PTR_2_16BE(p->data+2);
  ctrl_start = ctrl_offset;
  
  /* Decode command section */
  ptr = p->data + ctrl_offset;

  while(1) /* Control packet loop */
    {
    //date             = GAVL_PTR_2_16BE(ptr);
    ptr += 2;
    
    next_ctrl_offset = GAVL_PTR_2_16BE(ptr); ptr += 2;
    
    ctrl_seq_end = 0;
    
    while(!ctrl_seq_end)
      {
      cmd = *ptr;
      ptr++;
      
      switch(cmd)
        {
        case 0x00: /* Force display (or menu subtitle?) */
          break;
        case 0x01: /* Start display time */
          //          start_date = date;
          break;
        case 0x02: /* End display time */
          //          end_date = date;
          break;
        case 0x03: /* Set Palette */
          palette[3] = ptr[0] >> 4;
          palette[2] = ptr[0] & 0x0f;
          palette[1] = ptr[1] >> 4;
          palette[0] = ptr[1] & 0x0f;
          ptr += 2;
          break;
        case 0x04: /* Set Alpha */
          alpha[3] = ptr[0] >> 4;
          alpha[2] = ptr[0] & 0x0f;
          alpha[1] = ptr[1] >> 4;
          alpha[0] = ptr[1] & 0x0f;
          ptr += 2;
          break;
        case 0x05:
        case 0x85:
          x1 =  (ptr[0] << 4)         | (ptr[1] >> 4);
          x2 = ((ptr[1] & 0x0f) << 8) | ptr[2];
          y1 =  (ptr[3] << 4)         | (ptr[4] >> 4);
          y2 = ((ptr[4] & 0x0f) << 8) | ptr[5];
          ptr += 6;
          break;
        case 0x06:
          offset1 = GAVL_PTR_2_16BE(ptr); ptr += 2;
          offset2 = GAVL_PTR_2_16BE(ptr); ptr += 2;
          break;
        case 0xff:
          ctrl_seq_end = 1;
          break;
        default:
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                  "Unknown command %02x, decoding is doomed to failure",
                  cmd);
          break;
        }
      }

    if(ctrl_offset == next_ctrl_offset)
      break;
    ctrl_offset = next_ctrl_offset;
    }

  /* Create the local palette */

  for(i = 0; i < 4; i++)
    {
    local_palette[i] = ifo_palette[palette[i]] << 8 | alpha[i] << 4 | alpha[i];
#ifndef WORDS_BIGENDIAN
    local_palette[i] = bswap_32(local_palette[i]);
#endif
    }
  
  
  /* Dump the information we have right now */
#if 0
  bgav_dprintf("Subtitle packet %d bytes\n", priv->packet_size);
  bgav_dprintf("Coords:  [%d,%d] -> [%d,%d]\n", x1, y1, x2, y2);
  bgav_dprintf("Palette: [ %02x, %02x, %02x, %02x ]\n",
          palette[0], palette[1], palette[2], palette[3]);
  bgav_dprintf("Alpha:   [ %02x, %02x, %02x, %02x ]\n",
          alpha[0], alpha[1], alpha[2], alpha[3]);
  bgav_dprintf("PTS:     %" PRId64 "\n", priv->pts);
  bgav_dprintf("Time:    %d -> %d\n", start_date, end_date);
  bgav_dprintf("Offsets: %d %d\n", offset1, offset2);
  bgav_dprintf("IFO Palette:\n");
  for(i = 0; i < 16; i++)
    {
    bgav_dprintf("%08x\n", ifo_palette[i]);
    }
  
  bgav_dprintf("Local Palette:\n");
  for(i = 0; i < 4; i++)
    {
    bgav_dprintf("%08x\n", local_palette[i]);
    }
#endif
  /* Decode the image */
  
  decode_field(p->data + offset1, offset2 - offset1,
               ovl->planes[0],
               x2 - x1 + 1,
               2 * ovl->strides[0],
               local_palette, priv->field_height);
  
  decode_field(p->data + offset2, ctrl_start - offset2,
               ovl->planes[0] + ovl->strides[0],
               x2 - x1 + 1,
               2 * ovl->strides[0],
               local_palette, priv->field_height);

  /* Set rest of overlay structure */

  ovl->timestamp = p->pts;
  ovl->duration = p->duration;

  ovl->src_rect.x = 0;
  ovl->src_rect.y = 0;
  ovl->src_rect.w = x2 - x1 + 1;
  ovl->src_rect.h = y2 - y1 + 1;
  ovl->dst_x = x1;
  ovl->dst_y = y1;

  /* Shift the overlays (can happen in some pathological cases) */
  if(ovl->dst_x + ovl->src_rect.w > s->data.subtitle.video.format->image_width)
    ovl->dst_x = s->data.subtitle.video.format->image_width - ovl->src_rect.w;

  if(ovl->dst_y + ovl->src_rect.h > s->data.subtitle.video.format->image_height)
    ovl->dst_y = s->data.subtitle.video.format->image_height - ovl->src_rect.h;

#if 0
  fprintf(stderr, "Got overlay ");
  fprintf(stderr, " %f %f\n",
          ovl->timestamp / 90000.0, 
          (ovl->timestamp + ovl->duration) / 90000.0);
  fprintf(stderr, "src: %dx%d+%d+%d dst: %d %d\n",
          ovl->src_rect.w, ovl->src_rect.h,
          ovl->src_rect.x, ovl->src_rect.y,
          ovl->dst_x, ovl->dst_y);
#endif
  
  bgav_stream_done_packet_read(s, p);
  
  return 1;
  }

static void close_dvdsub(bgav_stream_t * s)
  {
  dvdsub_t * priv;
  priv = s->decoder_priv;
  free(priv);
  }

static bgav_video_decoder_t decoder =
  {
    .fourccs = bgav_dvdsub_fourccs,
    .name =    "DVD subtitle decoder",
    .init =         init_dvdsub,
    //    .has_subtitle = has_subtitle_dvdsub,
    .decode =       decode_dvdsub,
    .close =        close_dvdsub,
    //     .resync =       resync_dvdsub,
  };

void bgav_init_video_decoders_dvdsub()
  {
  bgav_video_decoder_register(&decoder);
  }
