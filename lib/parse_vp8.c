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

#include <avdec_private.h>
#include <parser.h>



static int parse_frame_vp8(bgav_packet_parser_t * parser,
                           bgav_packet_t * p)
  {
  if(!(p->buf.buf[0] & 0x10))
    p->flags |= GAVL_PACKET_NOOUTPUT; 
  else if(PACKET_GET_KEYFRAME(p))
    PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_I);
  else
    PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_P);
    

  return 1;
  }

void bgav_packet_parser_init_vp8(bgav_packet_parser_t * parser)
  {
  parser->parse_frame = parse_frame_vp8;
  }
