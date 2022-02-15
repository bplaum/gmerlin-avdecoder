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
#include <string.h>

#include <avdec_private.h>
#include <parser.h>
#include <videoparser_priv.h>

#include <dirac_header.h>

#define LOG_DOMAIN "parse_dirac"

typedef struct
  {
  /* Sequence header */
  bgav_dirac_sequence_header_t sh;
  int have_sh;
  int64_t pic_num_max;

  int64_t pic_num_first;
  int64_t pts_first;
  
  } dirac_priv_t;

static void cleanup_dirac(bgav_video_parser_t * parser)
  {
  free(parser->priv);
  }

static void reset_dirac(bgav_video_parser_t * parser)
  {
  dirac_priv_t * priv;
  priv = parser->priv;
  priv->pic_num_max = -1;
  }

static void set_format(bgav_video_parser_t * parser)
  {
  dirac_priv_t * priv = parser->priv;

  /* Framerate */
  if(!parser->format->timescale || !parser->format->frame_duration)
    {
    parser->format->timescale = priv->sh.timescale;
    parser->format->frame_duration = priv->sh.frame_duration;
    }

  /* Image size */
  if(!parser->format->image_width || !parser->format->image_height)
    {
    parser->format->image_width  = priv->sh.width;
    parser->format->image_height = priv->sh.height;

    parser->format->frame_width  = priv->sh.width;
    parser->format->frame_height = priv->sh.height;
    }

  /* Pixel size */
  if(!parser->format->pixel_width || !parser->format->pixel_height)
    {
    parser->format->pixel_width  = priv->sh.pixel_width;
    parser->format->pixel_height = priv->sh.pixel_height;
    }

  /* Interlacing */
  if(priv->sh.source_sampling == 1)
    {
    if(priv->sh.top_first)
      parser->format->interlace_mode = GAVL_INTERLACE_TOP_FIRST;
    else
      parser->format->interlace_mode = GAVL_INTERLACE_BOTTOM_FIRST;
    }
  else
      parser->format->interlace_mode = GAVL_INTERLACE_NONE;
  }

static int parse_frame_dirac(bgav_video_parser_t * parser,
                             bgav_packet_t * p, int64_t pts_orig)
  {
  int code, len;
  dirac_priv_t * priv;
  uint8_t * start =   p->data;
  uint8_t * end = p->data + p->data_size;
  bgav_dirac_picture_header_t ph;
  priv = parser->priv;
#if 0
  fprintf(stderr, "parse_frame_dirac %lld %lld\n",
          p->position, p->data_size);
  gavl_hexdump(start, 16, 16);
#endif
  while(start < end)
    {
    code = bgav_dirac_get_code(start, end - start, &len);

    switch(code)
      {
      case DIRAC_CODE_SEQUENCE:
        if(!priv->have_sh)
          {
          if(!parser->s->ci->global_header)
            bgav_stream_set_extradata(parser->s, start, len);
          
          if(!bgav_dirac_sequence_header_parse(&priv->sh,
                                               start, end - start))
            return PARSER_ERROR;
          //          bgav_dirac_sequence_header_dump(&priv->sh);
          priv->have_sh = 1;
          // ret = PARSER_CONTINUE;
          set_format(parser);
          }
        break;
      case DIRAC_CODE_PICTURE:
        if(!priv->have_sh)
          {
          PACKET_SET_SKIP(p);
          return PARSER_DISCARD;
          }
        if(!bgav_dirac_picture_header_parse(&ph, start, end - start))
          return PARSER_ERROR;
        //        bgav_dirac_picture_header_dump(&ph);

        if(priv->pic_num_first < 0)
          {
          priv->pic_num_first = ph.pic_num;
          priv->pts_first = p->pts;
          }

        /* Generate true timestamp from picture counter */
        p->pts = (ph.pic_num - priv->pic_num_first) *
          parser->s->data.video.format->frame_duration + priv->pts_first;
        p->duration = parser->s->data.video.format->frame_duration;
        
        if(ph.num_refs == 0)
          {
          PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_I);
          priv->pic_num_max = ph.pic_num;
          }
        else if((priv->pic_num_max >= 0) &&
           (ph.pic_num < priv->pic_num_max))
          {
          PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_B);
          }
        else
          {
          PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_P);
          priv->pic_num_max = ph.pic_num;
          }
        if(p->duration <= 0)
          p->duration = priv->sh.frame_duration;
        
        return 1;
        break;
      case DIRAC_CODE_END:
        fprintf(stderr, "Dirac code end %d\n", len);
        break;
      }
    start += len;
    }
  
  return 0;
  }

void bgav_video_parser_init_dirac(bgav_video_parser_t * parser)
  {
  dirac_priv_t * priv;
  priv = calloc(1, sizeof(*priv));
  parser->priv        = priv;

  if(parser->s->ci->global_header)
    {
    if(bgav_dirac_sequence_header_parse(&priv->sh,
                                        parser->s->ci->global_header,
                                        parser->s->ci->global_header_len))
      {
      priv->have_sh = 1;
      set_format(parser);
      //      fprintf(stderr, "Got sequence header:\n");
      //      bgav_dirac_sequence_header_dump(&priv->sh);
      }
    }

  priv->pic_num_first = -1;
  priv->pts_first = GAVL_TIME_UNDEFINED;
  
  priv->pic_num_max = -1;
  //  parser->parse       = parse_dirac;
  parser->parse_frame = parse_frame_dirac;
  parser->cleanup     = cleanup_dirac;
  parser->reset       = reset_dirac;
  }
