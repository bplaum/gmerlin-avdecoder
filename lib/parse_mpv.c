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
#include <mpv_header.h>

#define LOG_DOMAIN "parse_mpv"

/* States for finding the frame boundary */

#define STATE_SYNC                            100
#define STATE_SEQUENCE                        1
#define STATE_GOP                             2
#define STATE_PICTURE                         3
#define STATE_SLICE                           4

#define FLAG_HAVE_SH             (1<<0)
#define FLAG_D10                 (1<<1)
#define FLAG_INTRA_SLICE_REFRESH (1<<2)

typedef struct
  {
  /* Sequence header */
  bgav_mpv_sequence_header_t sh;
  int state;
  
  int flags;
  } mpeg12_priv_t;

static void reset_mpeg12(bgav_packet_parser_t * parser)
  {
  mpeg12_priv_t * priv = parser->priv;
  priv->state = STATE_SYNC;
  }

static int extract_header(bgav_packet_parser_t * parser, bgav_packet_t * p,
                          const uint8_t * header_end)
  {
  mpeg12_priv_t * priv = parser->priv;
  
  if(!p->header_size)
    p->header_size = header_end - p->buf.buf;

  if(parser->ci.codec_header.len)
    return 1;
  
  gavl_buffer_append_data(&parser->ci.codec_header, p->buf.buf, header_end - p->buf.buf);
  
  if(parser->fourcc == BGAV_MK_FOURCC('m','p','g','v'))
    {
    if(priv->sh.mpeg2)
      {
      parser->fourcc = BGAV_MK_FOURCC('m','p','v','2');
      gavl_dictionary_set_string_nocopy(parser->m, GAVL_META_FORMAT,
                                        bgav_sprintf("MPEG-2"));
      }
    else
      {
      parser->fourcc = BGAV_MK_FOURCC('m','p','v','1');
      gavl_dictionary_set_string_nocopy(parser->m, GAVL_META_FORMAT,
                                        bgav_sprintf("MPEG-1"));
      }
    }

  /* Set framerate */
  
  if(!parser->vfmt->timescale)
    {
    bgav_mpv_get_framerate(priv->sh.frame_rate_index,
                           &parser->vfmt->timescale,
                           &parser->vfmt->frame_duration);
    
    if(priv->sh.mpeg2)
      {
      if(parser->vfmt->framerate_mode == GAVL_FRAMERATE_STILL)
        {
        parser->vfmt->timescale = 90000;
        parser->vfmt->frame_duration = 0;
        }
      else
        {
        parser->vfmt->timescale *= (priv->sh.ext.timescale_ext+1) * 2;
        parser->vfmt->frame_duration *= (priv->sh.ext.frame_duration_ext+1) * 2;
        parser->vfmt->framerate_mode = GAVL_FRAMERATE_VARIABLE;
        }
      }
    }

  /* Set picture size */
  
  if(!parser->vfmt->image_width)
    bgav_mpv_get_size(&priv->sh, parser->vfmt);
  
  /* Special handling for D10 */
  if(priv->flags & FLAG_D10)
    {
    if(parser->vfmt->image_height == 608)
      parser->vfmt->image_height = 576;
    else if(parser->vfmt->image_height == 512)
      parser->vfmt->image_height = 486;
    }
  
  /* Set pixel size */
  bgav_mpv_get_pixel_aspect(&priv->sh, parser->vfmt);
  
  /* Pixelformat */
  if(parser->vfmt->pixelformat == GAVL_PIXELFORMAT_NONE)
    parser->vfmt->pixelformat = bgav_mpv_get_pixelformat(&priv->sh);

  /* Other stuff */
  if(priv->sh.mpeg2 && priv->sh.ext.low_delay)
    parser->ci.flags &= ~GAVL_COMPRESSION_HAS_B_FRAMES;
  
  /* Bitrate */
  parser->ci.bitrate = priv->sh.bitrate * 400;
  if(priv->sh.mpeg2)
    parser->ci.bitrate += (priv->sh.ext.bitrate_ext << 18) * 400;
  
  /* VBV buffer size */
  parser->ci.video_buffer_size = priv->sh.vbv_buffer_size_value;
  if(priv->sh.mpeg2)
    parser->ci.video_buffer_size +=
      (priv->sh.ext.vbv_buffer_size_ext<<10);
  
  parser->ci.video_buffer_size *= (1024 * 16);
  parser->ci.video_buffer_size /= 8; // bits -> bytes
  
  return 1;
  }

static int parse_frame_mpeg12(bgav_packet_parser_t * parser, bgav_packet_t * p)
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
        if(!(priv->flags & FLAG_HAVE_SH))
          {
          len = bgav_mpv_sequence_header_parse(&priv->sh,
                                               start, end - start);
          if(!len)
            return 0;

          /* Sequence header and sequence end in one packet means
             still images */
          if(p->sequence_end_pos)
            {
            if(parser->vfmt->framerate_mode != GAVL_FRAMERATE_STILL)
              {
              gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Detected still image");
              parser->vfmt->framerate_mode = GAVL_FRAMERATE_STILL;
              parser->ci.flags &= ~(GAVL_COMPRESSION_HAS_P_FRAMES|GAVL_COMPRESSION_HAS_B_FRAMES);
              }
            }
          priv->flags |= FLAG_HAVE_SH;
          start += len;
          }
        else
          start += 4;
        got_sh = 1;

        break;
      case MPEG_CODE_SEQUENCE_EXT:
        if((priv->flags |= FLAG_HAVE_SH) && !priv->sh.mpeg2)
          {
          len =
            bgav_mpv_sequence_extension_parse(&priv->sh.ext,
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
        if(!(priv->flags & FLAG_HAVE_SH))
          PACKET_SET_SKIP(p);
        else if(got_sh && !extract_header(parser, p, sc))
          return 0;
        
        len = bgav_mpv_picture_header_parse(&ph, start, end - start);
        
        if(parser->vfmt->framerate_mode == GAVL_FRAMERATE_STILL)
          {
          if(p->pes_pts != GAVL_TIME_UNDEFINED)
            p->pts = gavl_time_rescale(parser->packet_timescale,
                                       parser->vfmt->timescale,
                                       p->pes_pts);
          p->duration = -1;
          }
        else
          p->duration = parser->vfmt->frame_duration;
          
        if(!len)
          return PARSER_ERROR;

        PACKET_SET_CODING_TYPE(p, ph.coding_type);
        
        if(got_sh)
          {
          if(!(priv->flags & FLAG_INTRA_SLICE_REFRESH) &&
             (ph.coding_type == BGAV_CODING_TYPE_P))
            {
            priv->flags |= FLAG_INTRA_SLICE_REFRESH;
            gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN,
                     "Detected intra slice refresh");
            }
          }
        
        start += len;
        
        if(!priv->sh.mpeg2)
          return 1;
        break;
      case MPEG_CODE_PICTURE_EXT:
        len = bgav_mpv_picture_extension_parse(&pe, start, end - start);
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
                    delta_d = parser->vfmt->frame_duration * 2;
                  else
                    delta_d = parser->vfmt->frame_duration;
                  }
                else if(pe.progressive_frame)
                  delta_d = parser->vfmt->frame_duration / 2;
              
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
        
        len = bgav_mpv_gop_header_parse(&gh, start, end - start);
        
        if(!len)
          return PARSER_ERROR;
        
        start += len;

        if(!parser->vfmt->timecode_format.int_framerate && parser->vfmt->frame_duration)
          {
          parser->vfmt->timecode_format.int_framerate =
            parser->vfmt->timescale /
            parser->vfmt->frame_duration;
          if(gh.drop)
            parser->vfmt->timecode_format.flags |=
              GAVL_TIMECODE_DROP_FRAME;
          }

        if(parser->vfmt->timecode_format.int_framerate)
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

static int find_frame_boundary_mpeg12(bgav_packet_parser_t * parser, int * skip)

  {
  const uint8_t * sc;
  int start_code;
  mpeg12_priv_t * priv = parser->priv;
  int new_state;
  
  while(1)
    {
    sc = bgav_mpv_find_startcode(parser->buf.buf + parser->buf.pos,
                                 parser->buf.buf + parser->buf.len - 1);
    if(!sc)
      {
      parser->buf.pos = parser->buf.len - 3;
      if(parser->buf.pos < 0)
        parser->buf.pos = 0;
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
        parser->buf.pos = (sc - parser->buf.buf) + 4;
        *skip = 4;
        priv->state = STATE_SEQUENCE;
        return 1;
        break;
      case MPEG_CODE_EXTENSION:
        break;
      }

    parser->buf.pos = sc - parser->buf.buf;
    
    if(new_state < 0)
      parser->buf.pos += 4;
    else if(((new_state <= STATE_PICTURE) && (new_state < priv->state)) ||
            ((priv->state == STATE_SYNC) && (new_state >=  STATE_PICTURE)))
      {
      *skip = 4;
      parser->buf.pos = sc - parser->buf.buf;
      priv->state = new_state;
      return 1;
      }
    else
      {
      parser->buf.pos += 4;
      priv->state = new_state;
      }
    }
  return 0;
  }

static void cleanup_mpeg12(bgav_packet_parser_t * parser)
  {
  free(parser->priv);
  }

void bgav_packet_parser_init_mpeg12(bgav_packet_parser_t * parser)
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

  parser->ci.flags |= GAVL_COMPRESSION_HAS_B_FRAMES;
  
  if((parser->fourcc == BGAV_MK_FOURCC('m', 'x', '5', 'p')) ||
     (parser->fourcc == BGAV_MK_FOURCC('m', 'x', '4', 'p')) ||
     (parser->fourcc == BGAV_MK_FOURCC('m', 'x', '3', 'p')) ||
     (parser->fourcc == BGAV_MK_FOURCC('m', 'x', '5', 'n')) ||
     (parser->fourcc == BGAV_MK_FOURCC('m', 'x', '4', 'n')) ||
     (parser->fourcc == BGAV_MK_FOURCC('m', 'x', '3', 'n')))
    {
    parser->ci.bitrate =
      (((parser->fourcc & 0x0000FF00) >> 8) - '0') * 10000000;
    priv->flags |= FLAG_D10;
    parser->ci.flags &= ~(GAVL_COMPRESSION_HAS_P_FRAMES|
                          GAVL_COMPRESSION_HAS_B_FRAMES);
    parser->vfmt->interlace_mode = GAVL_INTERLACE_TOP_FIRST;
    }
  
  }

