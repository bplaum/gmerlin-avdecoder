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


bgav_packet_t * bgav_packet_create()
  {
  bgav_packet_t * ret = calloc(1, sizeof(*ret));
  return ret;
  }


void bgav_packet_destroy(bgav_packet_t * p)
  {
  gavl_packet_free(p);
  free(p);
  }

void bgav_packet_pad(bgav_packet_t * p)
  {
  /* Padding */
  memset(p->buf.buf + p->buf.len, 0, GAVL_PACKET_PADDING);
  }


void gavl_packet_dump_data(bgav_packet_t * p, int bytes)
  {
  if(bytes > p->buf.len)
    bytes = p->buf.len;
  gavl_hexdump(p->buf.buf, bytes, 16);
  }

void bgav_packet_copy_metadata(bgav_packet_t * dst,
                               const bgav_packet_t * src)
  {
  dst->pts      = src->pts;
  dst->dts      = src->dts;
  dst->duration = src->duration;
  dst->flags    = src->flags;
  dst->timecode       = src->timecode;
  }

void bgav_packet_copy(bgav_packet_t * dst,
                      const bgav_packet_t * src)
  {
  memcpy(dst, src, sizeof(*dst));
  memset(&dst->buf, 0, sizeof(dst->buf));
  gavl_buffer_copy(&dst->buf, &src->buf);
  }

