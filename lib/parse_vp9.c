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


#include <avdec_private.h>
#include <parser.h>
#include <videoparser_priv.h>

#include <bitstream.h>


static int parse_frame_vp9(bgav_video_parser_t * parser,
                           bgav_packet_t * p,
                           int64_t pts_orig)
  {
  int val;
  bgav_bitstream_t b;

  int show_frame;
  int show_existing_frame;
  int frame_type;
  int profile;
  int profile_high_bit;
  int profile_low_bit;
  
  memset(&b, 0, sizeof(b));
  
  bgav_bitstream_init(&b, p->buf.buf, p->buf.len);

  // frame_marker shall be equal to 2

  if(!bgav_bitstream_get(&b, &val, 2) ||
     (val != 2))
    {
    return 0;
    }

  if(!bgav_bitstream_get(&b, &profile_low_bit, 1) ||
     !bgav_bitstream_get(&b, &profile_high_bit, 1))
    {
    return 0;
    }

  profile = (profile_high_bit << 1) | profile_low_bit;

  if(profile == 3)
    {
    /* reserved_zero */
    if(!bgav_bitstream_get(&b, &val, 1) || (val != 0))
      return 0;
    }

  /* Show existing frame */

  if(!bgav_bitstream_get(&b, &show_existing_frame, 1))
    return 0;

  if(show_existing_frame)
    {
    PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_P);
    return 1;
    }

  if(!bgav_bitstream_get(&b, &frame_type, 1) ||
     !bgav_bitstream_get(&b, &show_frame, 1))
    return 0;

  //  fprintf(stderr, "frame type: %d, show_frame: %d\n",
  //          frame_type, show_frame);

  if(!show_frame)
    p->flags |= GAVL_PACKET_NOOUTPUT; 
  else
    {
    if(!frame_type)
      PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_I);
    else
      PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_P);
    }
  
  /*
  if(!(p->data[0] & 0x10))
    p->flags |= GAVL_PACKET_NOOUTPUT; 
  else if(PACKET_GET_KEYFRAME(p))
    PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_I);
  else
    PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_P);
  */    

  return 1;
  }

void bgav_video_parser_init_vp9(bgav_video_parser_t * parser)
  {
  parser->parse_frame = parse_frame_vp9;
  }
