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

typedef struct
  {
  int have_format;
  } jpeg_priv_t;

static gavl_pixelformat_t get_pixelformat(bgav_packet_t * p)
  {
  const uint8_t * ptr = p->buf.buf;
  int marker;
  int len;
  int components[3];
  int sub_h[3];
  int sub_v[3];

  while(1)
    {
    marker = GAVL_PTR_2_16BE(ptr); ptr+=2;
    
    switch(marker)
      {
      case 0xFFD8:
        //        fprintf(stderr, "Got SOI\n");
        break;
      case 0xFFC0:
      case 0xFFC1:
      case 0xFFC2:
      case 0xFFC3:
      case 0xFFC5:
      case 0xFFC6:
      case 0xFFC7:
      case 0xFFC8:
      case 0xFFC9:
      case 0xFFCa:
      case 0xFFCb:
      case 0xFFCd:
      case 0xFFCe:
      case 0xFFCf:
        {
        int i;
        int num_components;
        //        gavl_hexdump(ptr, 16, 16);
      
        len = GAVL_PTR_2_16BE(ptr); ptr+=2;
        //        fprintf(stderr, "Got SOF %d\n", len-2);
        
        ptr++; // tmp = *ptr; ptr++;

        //        fprintf(stderr, "Bits: %d\n", tmp);
      
        ptr+=2; // tmp = GAVL_PTR_2_16BE(ptr); ptr+=2;
        //        fprintf(stderr, "Height: %d\n", tmp);

        ptr+=2; // tmp = GAVL_PTR_2_16BE(ptr); ptr+=2;
        //        fprintf(stderr, "Width: %d\n", tmp);
        
        num_components = *ptr; ptr++;
        //        fprintf(stderr, "Components: %d\n", tmp);
                
        for(i = 0; i < num_components; i++)
          {
          components[i] = *ptr; ptr++;
          sub_h[i]      = (*ptr) >> 4;
          sub_v[i]      = (*ptr) & 0xF;
          ptr += 2; /* Skip huffman table */
          //          fprintf(stderr, "Component: ID: %d, sub_h: %d, sub_v: %d\n",
          //                  components[i], sub_h[i], sub_v[i]);
          }
        
        if((num_components != 3) ||
           (components[0] != 1) ||
           (components[1] != 2) ||
           (components[2] != 3) ||
           (sub_h[1] != sub_h[2]) ||
           (sub_v[1] != sub_v[2]))
          {
          return GAVL_PIXELFORMAT_NONE;
          }

        if((sub_h[0] == 1) &&
           (sub_v[0] == 1) &&
           (sub_h[1] == 1) &&
           (sub_v[1] == 1))
          return GAVL_YUVJ_444_P;
        else if((sub_h[0] == 2) &&
                (sub_v[0] == 2) &&
                (sub_h[1] == 1) &&
                (sub_v[1] == 1))
          return GAVL_YUVJ_420_P;
        else if((sub_h[0] == 2) &&
                (sub_v[0] == 1) &&
                (sub_h[1] == 1) &&
                (sub_v[1] == 1))
          return GAVL_YUVJ_422_P;
        return 1;
        }
        break;
      case 0xFFDA: // SOS
        return 0;
        break;
      default:
        len = GAVL_PTR_2_16BE(ptr); ptr+=2;
        //        fprintf(stderr, "Got %04x %d\n", marker, len-2);
        ptr+=len-2;
        break;
      }
    }
  return GAVL_PIXELFORMAT_NONE;
  }

static int parse_frame_jpeg(bgav_video_parser_t * parser, bgav_packet_t * p,
                            int64_t prs_orig)
  {
  jpeg_priv_t * priv = parser->priv;
  
  PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_I);

  /* Extract format */
  if(!priv->have_format)
    {
    parser->s->data.video.format->pixelformat =
      get_pixelformat(p);
    priv->have_format = 1;
    }
  return PARSER_CONTINUE;
  }


static void cleanup_jpeg(bgav_video_parser_t * parser)
  {
  jpeg_priv_t * priv = parser->priv;
  free(priv);
  }

void bgav_video_parser_init_jpeg(bgav_video_parser_t * parser)
  {
  jpeg_priv_t * priv;
  priv = calloc(1, sizeof(*priv));
  parser->priv        = priv;
  parser->parse_frame = parse_frame_jpeg;
  parser->cleanup     = cleanup_jpeg;
  }
