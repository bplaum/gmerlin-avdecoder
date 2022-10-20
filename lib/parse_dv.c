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
#include <dvframe.h>

typedef struct
  {
  bgav_dv_dec_t * dv;
  int have_format;
  } dv_priv_t;

static int parse_frame_dv(bgav_video_parser_t * parser, bgav_packet_t * p, int64_t pts_orig)
  {
  dv_priv_t * priv = parser->priv;
  gavl_video_format_t * fmt = parser->s->data.video.format;
  
  PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_I);
  
  /* Extract format */
  if(!priv->have_format)
    {
    bgav_dv_dec_set_header(priv->dv, p->buf.buf);
    bgav_dv_dec_set_frame(priv->dv, p->buf.buf);

    bgav_dv_dec_get_video_format(priv->dv, fmt);
    fmt->pixelformat = bgav_dv_dec_get_pixelformat(priv->dv);
    
    bgav_dv_dec_get_timecode_format(priv->dv,
                                    &fmt->timecode_format,
                                    parser->s->opt);
    priv->have_format = 1;

    if(!gavl_dictionary_get_string(parser->s->m, GAVL_META_DATE_CREATE))
      {
      int year, month, day;
      int hour, minute, second;

      if(bgav_dv_dec_get_date(priv->dv, &year, &month, &day) &&
         bgav_dv_dec_get_time(priv->dv, &hour, &minute, &second))
        {
        gavl_dictionary_set_date_time(parser->s->m,
                                    GAVL_META_DATE_CREATE,
                                    year,
                                    month,
                                    day,
                                    hour,
                                    minute,
                                    second);
        }
      }
    }
  else
    bgav_dv_dec_set_frame(priv->dv, p->buf.buf);
  
  /* Extract timecode */  
  if(fmt->timecode_format.int_framerate)
    bgav_dv_dec_get_timecode(priv->dv, &p->timecode);
  
  return PARSER_CONTINUE;
  }


static void cleanup_dv(bgav_video_parser_t * parser)
  {
  dv_priv_t * priv = parser->priv;
  bgav_dv_dec_destroy(priv->dv);
  free(priv);
  }

void bgav_video_parser_init_dv(bgav_video_parser_t * parser)
  {
  dv_priv_t * priv;
  priv = calloc(1, sizeof(*priv));
  priv->dv = bgav_dv_dec_create();
  parser->priv        = priv;
  parser->parse_frame = parse_frame_dv;
  parser->cleanup     = cleanup_dv;
  }
