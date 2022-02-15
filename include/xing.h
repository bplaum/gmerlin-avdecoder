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

/* XING VBR header (stolen from xmms) */

#define FRAMES_FLAG     0x0001
#define BYTES_FLAG      0x0002
#define TOC_FLAG        0x0004
#define VBR_SCALE_FLAG  0x0008

typedef struct
  {
  int flags;
  
  uint32_t frames;             /* total bit stream frames from Xing header data */
  uint32_t bytes;              /* total bit stream bytes from Xing header data  */
  unsigned char toc[100]; /* "table of contents" */
  } bgav_xing_header_t;

/* Read the xing header, buf MUST be at least one complete mpeg audio frame */

int bgav_xing_header_read(bgav_xing_header_t * xing, unsigned char *buf);

int64_t bgav_xing_get_seek_position(bgav_xing_header_t * xing, float percent);

void bgav_xing_header_dump(bgav_xing_header_t * xing);

int bgav_xing_header_probe(unsigned char *buf);

int bgav_mp3_info_header_probe(unsigned char *buf);

