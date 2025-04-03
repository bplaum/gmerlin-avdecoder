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



#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <avdec_private.h>
#include <parser.h>
#include <theora/theoradec.h>

#define LOG_DOMAIN "parse_theora"

typedef struct
  {
  th_info ti;
  th_comment tc;
  th_setup_info *ts;
  } theora_priv_t;

static int parse_frame_theora(bgav_packet_parser_t * parser, bgav_packet_t * p)
  {
  theora_priv_t * th = parser->priv;

  //  fprintf(stderr, "Parse frame theora %"PRId64"\n", p->pes_pts);
  
  p->duration = parser->vfmt->frame_duration;

  if(!(p->buf.buf[0] & 0x40))
    {
    PACKET_SET_KEYFRAME(p);
    PACKET_SET_CODING_TYPE(p, GAVL_PACKET_TYPE_I);
    }
  else
    PACKET_SET_CODING_TYPE(p, GAVL_PACKET_TYPE_P);

  // Convert granulepos
  if(p->pes_pts != GAVL_TIME_UNDEFINED)
    {
    int64_t frames = p->pes_pts >> th->ti.keyframe_granule_shift;
    frames += p->pes_pts - (frames << th->ti.keyframe_granule_shift);
    p->pes_pts = frames * parser->vfmt->frame_duration;
    
    //    fprintf(stderr, "Frames: %"PRId64" Time: %"PRId64"\n",
    //            frames, p->pes_pts);
    }

  //  gavl_packet_dump(p);
  
  return 1;
  }

static void cleanup_theora(bgav_packet_parser_t * parser)
  {
  theora_priv_t * priv = parser->priv;
  th_comment_clear(&priv->tc);
  th_info_clear(&priv->ti);
  free(priv);
  }


void bgav_packet_parser_init_theora(bgav_packet_parser_t * parser)
  {
  theora_priv_t * priv;
  ogg_packet op;
  int i;
  int len;
  th_setup_info *ts = NULL;
  
  priv = calloc(1, sizeof(*priv));
  parser->priv = priv;
  /* Get extradata and initialize codec */
  th_info_init(&priv->ti);
  th_comment_init(&priv->tc);

  memset(&op, 0, sizeof(op));

  if(!parser->ci->codec_header.len)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "No extradata found");
    return;
    }

  
  op.b_o_s = 1;

  for(i = 0; i < 3; i++)
    {
    op.packet =
      gavl_extract_xiph_header(&parser->ci->codec_header,
                               i, &len);
    
    if(!op.packet)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Truncated theora header %d", i+1);
      return;
      }
    
    if(i)
      op.b_o_s = 0;
    
    op.bytes = len;

    if(th_decode_headerin(&priv->ti, &priv->tc, &ts, &op) < 0)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Packet %d is not a theora header", i+1);
      gavl_hexdump(op.packet, op.bytes, 16);

      if(ts)
        th_setup_free(ts);
      
      return;
      }
    op.packetno++;
    }
  
  if(ts)
    th_setup_free(ts);

  parser->vfmt->image_width  = priv->ti.pic_width;
  parser->vfmt->image_height = priv->ti.pic_height;

  parser->vfmt->frame_width  = priv->ti.frame_width;
  parser->vfmt->frame_height = priv->ti.frame_height;
  
  if(!priv->ti.aspect_numerator || !priv->ti.aspect_denominator)
    {
    parser->vfmt->pixel_width  = 1;
    parser->vfmt->pixel_height = 1;
    }
  else
    {
    parser->vfmt->pixel_width  = priv->ti.aspect_numerator;
    parser->vfmt->pixel_height = priv->ti.aspect_denominator;
    }

  if(!parser->vfmt->timescale)
    {
    parser->vfmt->timescale      = priv->ti.fps_numerator;
    parser->vfmt->frame_duration = priv->ti.fps_denominator;
    gavl_dictionary_set_int(parser->m, GAVL_META_STREAM_SAMPLE_TIMESCALE, parser->vfmt->timescale);
    }

  switch(priv->ti.pixel_fmt)
    {
    case TH_PF_420:
      parser->vfmt->pixelformat = GAVL_YUV_420_P;
      break;
    case TH_PF_422:
      parser->vfmt->pixelformat = GAVL_YUV_422_P;
      break;
    case TH_PF_444:
      parser->vfmt->pixelformat = GAVL_YUV_444_P;
      break;
    default:
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Unknown pixelformat %d",
              priv->ti.pixel_fmt);
      return;
    }
  
  
  parser->parse_frame = parse_frame_theora;
  parser->cleanup = cleanup_theora;
  }
