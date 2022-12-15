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

#include <avdec_private.h>
#include <parser.h>

#define LOG_DOMAIN "parse_dvdsub"

typedef struct
  {
  int packet_size;
  int pts_mult;
  int got_first;
  } dvdsub_t;

static void cleanup_dvdsub(bgav_packet_parser_t * parser)
  {
  dvdsub_t * priv = parser->priv;
  free(priv);
  }

static void reset_dvdsub(bgav_packet_parser_t *  parser)
  {
  dvdsub_t * priv = parser->priv;
  priv->packet_size = 0;
  priv->got_first = 0;
  }

static int parse_frame_dvdsub(bgav_packet_parser_t * parser, bgav_packet_t * p)
  {
  
  /* We need the start- and end date */
  uint16_t date;
  
  dvdsub_t * priv = parser->priv;
  int start_date = -1, end_date = -1;
  uint8_t * ptr;
  int ctrl_seq_end;
  uint16_t ctrl_offset, next_ctrl_offset;
  uint8_t cmd;
  
  PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_I);

  ctrl_offset = GAVL_PTR_2_16BE(p->buf.buf+2);
  //  ctrl_start = ctrl_offset;
  
  ptr = p->buf.buf + ctrl_offset;
  
  while(1) /* Control packet loop */
    {
    date             = GAVL_PTR_2_16BE(ptr); ptr += 2;
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
          start_date = date;
          break;
        case 0x02: /* End display time */
          end_date = date;
          break;
        case 0x03: /* Set Palette */
          ptr += 2;
          break;
        case 0x04: /* Set Alpha */
          ptr += 2;
          break;
        case 0x05: /* Coordinates */
#if 0
          x1 =  (ptr[0] << 4)         | (ptr[1] >> 4);
          x2 = ((ptr[1] & 0x0f) << 8) | ptr[2];
          y1 =  (ptr[3] << 4)         | (ptr[4] >> 4);
          y2 = ((ptr[4] & 0x0f) << 8) | ptr[5];
#endif
          ptr += 6;
          break;
        case 0x06:
#if 0
          offset1 = GAVL_PTR_2_16BE(ptr); ptr += 2;
          offset2 = GAVL_PTR_2_16BE(ptr); ptr += 2;
#endif
          ptr += 4;
          break;
        case 0xff:
          ctrl_seq_end = 1;
          break;
        default:
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                  "Unknown command %02x, decoding is doomed to failure",
                  cmd);
          return 0;
          break;
        }
      }

    /* Don't need anything else but start- and end time */
    if((start_date >= 0) && (end_date >= 0))
      break;
    
    if(ctrl_offset == next_ctrl_offset)
      break;
    ctrl_offset = next_ctrl_offset;
    }

  if(start_date < 0)
    start_date = 0;
  
  priv->packet_size = 0;
  //  p->pts = pts_orig + start_date * priv->pts_mult;
  p->pts = p->pes_pts + start_date * priv->pts_mult;
  
  //  fprintf(stderr, "Parse frame done, start: %d, end: %d, mult: %d\n",
  //          start_date, end_date, priv->pts_mult);
  
  p->duration = priv->pts_mult * (end_date - start_date);

  //  fprintf(stderr, "Parse frame dvdsub\n");
  //  bgav_packet_dump(p);
  return 1;
  }

/*
 *  Find a frame boundary in the bytebuffer (parser->buf)
 *
 *  If a frame boundary is found:
 *  
 *  - Set parser->pos to the byte offset of the frame start
 *  - Set *skip to the offset *relative* to the frame start,
 *    after which we continue searching the next frame boundary.
 *  - Return 1
 *
 *  If no frame boundary is found:
 *
 *  - Set parser->pos to the position where we continue searching after
 *    new data was added.
 *  - Return 0.
 */

static int find_frame_boundary_dvdsub(bgav_packet_parser_t * parser, int * skip)
  {
  dvdsub_t * priv = parser->priv;

  *skip = 0;
  
  if(!priv->packet_size)
    {
    if(parser->buf.len - parser->buf.pos < 2)
      return 0;
    
    priv->packet_size = GAVL_PTR_2_16BE(parser->buf.buf + parser->buf.pos);
    
    //    fprintf(stderr, "Packet size: %d\n", priv->packet_size);

    if(!priv->got_first)
      {
      priv->got_first = 1;
      return 1;
      }
    }

  if(parser->buf.len - parser->buf.pos >= priv->packet_size)
    {
    parser->buf.pos += priv->packet_size;
    return 1;
    }
  else
    return 0;
  }

void bgav_packet_parser_init_dvdsub(bgav_packet_parser_t * parser)
  {
  dvdsub_t * priv;
  priv = calloc(1, sizeof(*priv));
  parser->priv = priv;

  priv->pts_mult = parser->packet_timescale / 100;

  parser->vfmt->pixelformat = GAVL_YUVA_32;
  parser->vfmt->timescale = parser->packet_timescale;
  parser->vfmt->frame_duration = 0;
  parser->vfmt->framerate_mode = GAVL_FRAMERATE_VARIABLE;
  
  parser->parse_frame = parse_frame_dvdsub;
  parser->cleanup = cleanup_dvdsub;
  parser->reset = reset_dvdsub;
  parser->find_frame_boundary = find_frame_boundary_dvdsub;
  }
