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

#include <mpv_header.h>

#define LOG_DOMAIN "parse_mpv"

/* States for finding the frame boundary */

#define STATE_SYNC                            100
#define STATE_SEQUENCE                        1
#define STATE_GOP                             2
#define STATE_PICTURE                         3
#define STATE_SLICE                           4

typedef struct
  {
  /* Sequence header */
  bgav_mpv_sequence_header_t sh;
  int have_sh;
  int state;
  
  int d10; // Special handling for D10 */
  
  int framerate_from_container;

  int have_header;
  
  } mpeg12_priv_t;

static void reset_mpeg12(bgav_video_parser_t * parser)
  {
  mpeg12_priv_t * priv = parser->priv;
  priv->state = STATE_SYNC;
  }

static int extract_header(bgav_video_parser_t * parser, bgav_packet_t * p,
                          const uint8_t * header_end)
  {
  mpeg12_priv_t * priv = parser->priv;
  
  if(!p->header_size)
    p->header_size = header_end - p->buf.buf;
  
  if(priv->have_header)
    return 1;

  if(!parser->s->ci->global_header)
    bgav_stream_set_extradata(parser->s, p->buf.buf, header_end - p->buf.buf);
  
  if(parser->s->fourcc == BGAV_MK_FOURCC('m','p','g','v'))
    {
    if(priv->sh.mpeg2)
      {
      parser->s->fourcc = BGAV_MK_FOURCC('m','p','v','2');
      gavl_dictionary_set_string_nocopy(parser->s->m, GAVL_META_FORMAT,
                              bgav_sprintf("MPEG-2"));
      }
    else
      {
      parser->s->fourcc = BGAV_MK_FOURCC('m','p','v','1');
      gavl_dictionary_set_string_nocopy(parser->s->m, GAVL_META_FORMAT,
                              bgav_sprintf("MPEG-1"));
      }
    }

  /* Set framerate */
  
  if(!parser->format->timescale)
    {
    bgav_mpv_get_framerate(priv->sh.frame_rate_index,
                           &parser->format->timescale,
                           &parser->format->frame_duration);
    
    if(priv->sh.mpeg2)
      {
      parser->format->timescale *= (priv->sh.ext.timescale_ext+1) * 2;
      parser->format->frame_duration *= (priv->sh.ext.frame_duration_ext+1) * 2;
      parser->format->framerate_mode = GAVL_FRAMERATE_VARIABLE;
      }
    }

  /* Set picture size */
  
  if(!parser->format->image_width)
    bgav_mpv_get_size(&priv->sh, parser->format);
  
  /* Special handling for D10 */
  if(priv->d10)
    {
    if(parser->format->image_height == 608)
      parser->format->image_height = 576;
    else if(parser->format->image_height == 512)
      parser->format->image_height = 486;
    }
  
  /* Set pixel size */
  bgav_mpv_get_pixel_aspect(&priv->sh, parser->format);
  
  /* Pixelformat */
  if(parser->format->pixelformat == GAVL_PIXELFORMAT_NONE)
    parser->format->pixelformat = bgav_mpv_get_pixelformat(&priv->sh);

  /* Other stuff */
  if(priv->sh.mpeg2 && priv->sh.ext.low_delay)
    parser->s->ci->flags &= ~GAVL_COMPRESSION_HAS_B_FRAMES;

  /* Bitrate */
  parser->s->codec_bitrate = priv->sh.bitrate * 400;
  if(priv->sh.mpeg2)
    parser->s->codec_bitrate += (priv->sh.ext.bitrate_ext << 18) * 400;

  /* VBV buffer size */
  parser->s->ci->video_buffer_size = priv->sh.vbv_buffer_size_value;
  if(priv->sh.mpeg2)
    parser->s->ci->video_buffer_size +=
      (priv->sh.ext.vbv_buffer_size_ext<<10);

  parser->s->ci->video_buffer_size *= (1024 * 16);
  parser->s->ci->video_buffer_size /= 8; // bits -> bytes
  
  priv->have_header = 1;
  return 1;
  }

static int parse_frame_mpeg12(bgav_video_parser_t * parser, bgav_packet_t * p,
                              int64_t pts_orig)
  {
  const uint8_t * sc;
  mpeg12_priv_t * priv = parser->priv;
  //  cache_t * c;
  bgav_mpv_picture_extension_t pe;
  bgav_mpv_picture_header_t    ph;
  int start_code;
  int len;
  int delta_d;
  int got_sh = 0;
  int ret = 0;
  
  const uint8_t * start =   p->buf.buf;
  const uint8_t * end = p->buf.buf + p->buf.len;
  
  /* Check for sequence end code within this frame */

  if(p->buf.len >= 4)
    {
    end = p->buf.buf + (p->buf.len - 4);
    if(GAVL_PTR_2_32BE(end) == 0x000001B7)
      p->sequence_end_pos = p->buf.len - 4;
    }

  
  end = p->buf.buf + p->buf.len;
  
  while(1)
    {
    sc = bgav_mpv_find_startcode(start, end);
    if(!sc)
      return ret;
    
    start_code = bgav_mpv_get_start_code(sc, 1);

    /* Update position */
    start = sc;
    
    switch(start_code)
      {
      case MPEG_CODE_SEQUENCE:
        if(!priv->have_sh)
          {
          len = bgav_mpv_sequence_header_parse(parser->s->opt,
                                               &priv->sh,
                                               start, end - start);
          if(!len)
            return 0;

          /* Sequence header and sequence end in one packet means
             still images */
          if(p->sequence_end_pos)
            {
            if(!STREAM_IS_STILL(parser->s))
              {
              gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
                       "Detected still image");
              STREAM_SET_STILL(parser->s);
              }
            }
          
          priv->have_sh = 1;
          start += len;
          }
        else
          start += 4;
        got_sh = 1;

        break;
      case MPEG_CODE_SEQUENCE_EXT:
        if(priv->have_sh && !priv->sh.mpeg2)
          {
          len =
            bgav_mpv_sequence_extension_parse(parser->s->opt,
                                              &priv->sh.ext,
                                              start, end - start);
          if(!len)
            return 0;
          priv->sh.mpeg2 = 1;
          start += len;
          }
        else
          start += 4;
        break;
      case MPEG_CODE_PICTURE:
        if(!priv->have_sh)
          PACKET_SET_SKIP(p);
        else if(got_sh && !extract_header(parser, p, sc))
          return 0;
        
        len = bgav_mpv_picture_header_parse(parser->s->opt,
                                            &ph, start, end - start);
        
        if(parser->format->framerate_mode == GAVL_FRAMERATE_STILL)
          {
          parser->timestamp = gavl_time_rescale(parser->s->timescale,
                                                parser->format->timescale,
                                                pts_orig);
          p->duration = -1;
          }
        else
          p->duration = parser->format->frame_duration;
          
        if(!len)
          return PARSER_ERROR;

        PACKET_SET_CODING_TYPE(p, ph.coding_type);
        
        if(got_sh)
          {
          if(!(parser->flags & PARSER_NO_I_FRAMES) &&
             (ph.coding_type == BGAV_CODING_TYPE_P))
            {
            parser->flags |= PARSER_NO_I_FRAMES;
            gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN,
                     "Detected intra slice refresh");
            }
          }
        
        start += len;
        
        if(!priv->sh.mpeg2)
          return 1;
        break;
      case MPEG_CODE_PICTURE_EXT:
        len = bgav_mpv_picture_extension_parse(parser->s->opt,
                                               &pe, start, end - start);
        if(!len)
          return PARSER_ERROR;

        /* Set interlacing stuff */
        switch(pe.picture_structure)
          {
          case MPEG_PICTURE_TOP_FIELD:
            PACKET_SET_FIELD_PIC(p);
            p->interlace_mode = GAVL_INTERLACE_TOP_FIRST;
            break;
          case MPEG_PICTURE_BOTTOM_FIELD:
            PACKET_SET_FIELD_PIC(p);
            p->interlace_mode = GAVL_INTERLACE_BOTTOM_FIRST;
            break;
          case MPEG_PICTURE_FRAME:

            if(p->duration > 0)
              {
              if(pe.repeat_first_field)
                {
                delta_d = 0;
                if(priv->sh.ext.progressive_sequence)
                  {
                  if(pe.top_field_first)
                    delta_d = parser->format->frame_duration * 2;
                  else
                    delta_d = parser->format->frame_duration;
                  }
                else if(pe.progressive_frame)
                  delta_d = parser->format->frame_duration / 2;
              
                p->duration += delta_d;
                }
              }
            
            if(!pe.repeat_first_field && !priv->sh.ext.progressive_sequence)
              {
              if(pe.progressive_frame)
                p->interlace_mode = GAVL_INTERLACE_NONE;
              else if(pe.top_field_first)
                p->interlace_mode = GAVL_INTERLACE_TOP_FIRST;
              else
                p->interlace_mode = GAVL_INTERLACE_BOTTOM_FIRST;
              }
            break;
          }
        // start += len;
        return 1;
        break;
      case MPEG_CODE_GOP:
        {
        bgav_mpv_gop_header_t        gh;

        if(got_sh && !extract_header(parser, p, sc))
          return 0;
        
        len = bgav_mpv_gop_header_parse(parser->s->opt,
                                        &gh, start, end - start);
        
        if(!len)
          return PARSER_ERROR;
        
        start += len;

        if(!parser->format->timecode_format.int_framerate && parser->format->frame_duration)
          {
          parser->format->timecode_format.int_framerate =
            parser->format->timescale /
            parser->format->frame_duration;
          if(gh.drop)
            parser->format->timecode_format.flags |=
              GAVL_TIMECODE_DROP_FRAME;
          }

        if(parser->format->timecode_format.int_framerate)
          {
          gavl_timecode_from_hmsf(&p->timecode,
                                  gh.hours,
                                  gh.minutes,
                                  gh.seconds,
                                  gh.frames);
          }
        }
        break;
      case MPEG_CODE_SLICE:
        return 1;
      default:
        start += 4;
      }
    }
  }

static int find_frame_boundary_mpeg12(bgav_video_parser_t * parser, int * skip)

  {
  const uint8_t * sc;
  int start_code;
  mpeg12_priv_t * priv = parser->priv;
  int new_state;
  
  while(1)
    {
    sc = bgav_mpv_find_startcode(parser->buf.buf + parser->pos,
                                 parser->buf.buf + parser->buf.len - 1);
    if(!sc)
      {
      parser->pos = parser->buf.len - 3;
      if(parser->pos < 0)
        parser->pos = 0;
      return 0;
      }

    start_code = bgav_mpv_get_start_code(sc, 0);

    new_state = -1;
    switch(start_code)
      {
      case MPEG_CODE_SEQUENCE:
        /* Sequence header */
        new_state = STATE_SEQUENCE;
        break;
      case MPEG_CODE_PICTURE:
        new_state = STATE_PICTURE;
        break;
      case MPEG_CODE_GOP:
        new_state = STATE_GOP;
        break;
      case MPEG_CODE_SLICE:
        new_state = STATE_SLICE;
        break;
      case MPEG_CODE_END:
        //        fprintf(stderr, "Got sequence end\n");
        /* Sequence end is always a picture start */
        parser->pos = (sc - parser->buf.buf) + 4;
        *skip = 4;
        priv->state = STATE_SEQUENCE;
        return 1;
        break;
      case MPEG_CODE_EXTENSION:
        break;
      }

    parser->pos = sc - parser->buf.buf;
    
    if(new_state < 0)
      parser->pos += 4;
    else if(((new_state <= STATE_PICTURE) && (new_state < priv->state)) ||
            ((priv->state == STATE_SYNC) && (new_state >=  STATE_PICTURE)))
      {
      *skip = 4;
      parser->pos = sc - parser->buf.buf;
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

static void cleanup_mpeg12(bgav_video_parser_t * parser)
  {
  free(parser->priv);
  }

void bgav_video_parser_init_mpeg12(bgav_video_parser_t * parser)
  {
  mpeg12_priv_t * priv;
  priv = calloc(1, sizeof(*priv));
  parser->priv        = priv;
  priv->state = STATE_SYNC;
  //  parser->parse       = parse_mpeg12;
  parser->parse_frame = parse_frame_mpeg12;
  parser->cleanup     = cleanup_mpeg12;
  parser->reset       = reset_mpeg12;
  parser->find_frame_boundary = find_frame_boundary_mpeg12;

  parser->s->ci->flags |= GAVL_COMPRESSION_HAS_B_FRAMES;
  
  if((parser->s->fourcc == BGAV_MK_FOURCC('m', 'x', '5', 'p')) ||
     (parser->s->fourcc == BGAV_MK_FOURCC('m', 'x', '4', 'p')) ||
     (parser->s->fourcc == BGAV_MK_FOURCC('m', 'x', '3', 'p')) ||
     (parser->s->fourcc == BGAV_MK_FOURCC('m', 'x', '5', 'n')) ||
     (parser->s->fourcc == BGAV_MK_FOURCC('m', 'x', '4', 'n')) ||
     (parser->s->fourcc == BGAV_MK_FOURCC('m', 'x', '3', 'n')))
    {
    parser->s->codec_bitrate =
      (((parser->s->fourcc & 0x0000FF00) >> 8) - '0') * 10000000;
    priv->d10 = 1;
    parser->s->ci->flags &= ~(GAVL_COMPRESSION_HAS_P_FRAMES|
                             GAVL_COMPRESSION_HAS_B_FRAMES);
    parser->format->interlace_mode = GAVL_INTERLACE_TOP_FIRST;
    }
  
  }

