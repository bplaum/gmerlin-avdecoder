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



static int parse_frame_mjpa(bgav_packet_parser_t * parser,
                            bgav_packet_t * p)
  {
  uint8_t * ptr;
  int i;
  uint32_t num = 0;

  /* Look for APP1 marker */
  
  ptr = p->buf.buf;
  for(i = 0; i < p->buf.len - 2; i++)
    {
    num = GAVL_PTR_2_16BE(ptr);
    if(num == 0xffe1) // APP1
      break;
    ptr++;
    }

  if(num != 0xffe1)
    return 0;
  
  /* Parse the mjpa table */
  ptr += 2 // Marker
    + 2    // Size
    + 4    // Zero
    + 4    // MJPA
    + 4    // Field size
    + 4;   // Padded field size
  p->field2_offset = GAVL_PTR_2_32BE(ptr);
  return 1;
  }

void bgav_packet_parser_init_mjpa(bgav_packet_parser_t * parser)
  {
  parser->parse_frame = parse_frame_mjpa;
  }
