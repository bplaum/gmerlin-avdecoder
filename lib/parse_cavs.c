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

#include <string.h>
#include <stdlib.h>


#include <avdec_private.h>
#include <parser.h>
#include <videoparser_priv.h>

#include <cavs_header.h>
#include <mpv_header.h>

#define CAVS_NEED_SYNC                   0
#define CAVS_NEED_STARTCODE              1
#define CAVS_HAS_SEQ_CODE                2
#define CAVS_HAS_SEQ_HEADER              3
#define CAVS_HAS_PIC_CODE                4
#define CAVS_HAS_PIC_HEADER              5

#define STATE_SYNC                       100 // Need start code
#define STATE_SEQUENCE                   1 // Got sequence header
#define STATE_PICTURE                    2 // Got picture header

typedef struct
  {
  /* Sequence header */
  bgav_cavs_sequence_header_t seq;
  int have_seq;
  int has_picture_start;
  int state;
  } cavs_priv_t;

static void reset_cavs(bgav_video_parser_t * parser)
  {
  cavs_priv_t * priv = parser->priv;
  priv->state = STATE_SYNC;
  priv->has_picture_start = 0;
  }

static int parse_frame_cavs(bgav_video_parser_t * parser, bgav_packet_t * p,
                            int64_t pts_orig)
  {
  int start_code;
  int len;
  bgav_cavs_picture_header_t ph;
  const uint8_t * sc;
  const uint8_t * ptr = p->data;
  const uint8_t * end = p->data + p->data_size;
  cavs_priv_t * priv = parser->priv;
  
  while(1)
    {
    sc = bgav_mpv_find_startcode(ptr, end);
    if(!sc)
      return 0;
    
    start_code = bgav_cavs_get_start_code(sc);
    ptr = sc;
    //    fprintf(stderr, "Start code: %02x\n", sc[3]);
    //    gavl_hexdump(sc, 16, 16);
    switch(start_code)
      {
      case CAVS_CODE_SEQUENCE:
        if(!priv->have_seq)
          {
          len = bgav_cavs_sequence_header_read(parser->s->opt,
                                               &priv->seq,
                                               ptr, end - ptr);
          if(!len)
            return 0;
          
#ifdef DUMP_HEADERS
          bgav_cavs_sequence_header_dump(&priv->seq);
#endif

          ptr += len;
          
          bgav_mpv_get_framerate(priv->seq.frame_rate_code,
                                 &parser->format->timescale,
                                 &parser->format->frame_duration);
          
          parser->format->image_width  = priv->seq.horizontal_size;
          parser->format->image_height = priv->seq.vertical_size;
          parser->format->frame_width  =
            (parser->format->image_width + 15) & ~15;
          parser->format->frame_height  =
            (parser->format->image_height + 15) & ~15;
      
          priv->have_seq = 1;
          }
        else
          ptr += 4;
        break;
      case CAVS_CODE_PICTURE_I:
      case CAVS_CODE_PICTURE_PB:
        if(!priv->have_seq)
          {
          PACKET_SET_SKIP(p);
          ptr += 4;
          return 1;
          }
        
        len = bgav_cavs_picture_header_read(parser->s->opt,
                                            &ph, ptr, end - ptr, &priv->seq);

        if(!len)
          return 0;
#ifdef DUMP_HEADERS
        bgav_cavs_picture_header_dump(&ph, &priv->seq);
#endif
        PACKET_SET_CODING_TYPE(p, ph.coding_type);
        p->duration = parser->format->frame_duration;
        return 1;
        break;
      default:
        ptr += 4;
        break;
      }
    }
  return 0;
  }

static int find_frame_boundary_cavs(bgav_video_parser_t * parser,
                                    int * skip)
  {
  const uint8_t * sc;
  cavs_priv_t * priv = parser->priv;
  int new_state;
  int start_code;
  
  while(1)
    {
    sc = bgav_mpv_find_startcode(parser->buf.buffer + parser->pos,
                                 parser->buf.buffer + parser->buf.size - 4);
    if(!sc)
      {
      parser->pos = parser->buf.size - 4;
      if(parser->pos < 0)
        parser->pos = 0;
      return 0;
      }

    new_state = -1;

    start_code = bgav_cavs_get_start_code(sc);
        
    switch(start_code)
      {
      case CAVS_CODE_SEQUENCE:
        /* Sequence header */
        new_state = STATE_SEQUENCE;
        break;
      case CAVS_CODE_PICTURE_I:
      case CAVS_CODE_PICTURE_PB:
        new_state = STATE_PICTURE;
        break;
      }
    
    parser->pos = sc - parser->buf.buffer;
    
    if(new_state < 0)
      parser->pos += 4;
    else if(((new_state == STATE_PICTURE) && (priv->state == STATE_PICTURE)) ||
            (new_state < priv->state))
      {
      *skip = 4;
      parser->pos = sc - parser->buf.buffer;
      priv->state = new_state;
      return 1;
      }
    else
      {
      parser->pos += 4;
      priv->state = new_state;
      }
    }
  return 0;
  }

static void cleanup_cavs(bgav_video_parser_t * parser)
  {
  free(parser->priv);
  }

void bgav_video_parser_init_cavs(bgav_video_parser_t * parser)
  {
  cavs_priv_t * priv;
  priv = calloc(1, sizeof(*priv));

  priv->state = STATE_SYNC;
  parser->priv = priv;
  //  parser->parse = parse_cavs;
  parser->parse_frame = parse_frame_cavs;

  parser->cleanup = cleanup_cavs;
  parser->reset = reset_cavs;
  parser->find_frame_boundary = find_frame_boundary_cavs;
  }
