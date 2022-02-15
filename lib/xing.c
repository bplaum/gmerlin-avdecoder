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
#include <stdio.h>
#include <xing.h>
#include <mpa_header.h>

#define GET_INT32BE(b) \
(i = (b[0] << 24) | (b[1] << 16) | b[2] << 8 | b[3], b += 4, i)

int bgav_xing_header_probe(unsigned char *buf)
  {
  bgav_mpa_header_t h;
  uint8_t * ptr;

  memset(&h, 0, sizeof(h));
  
  if(!bgav_mpa_header_decode(&h, buf))
    return 0;
  
  ptr = buf + 4 + (h.has_crc * 2) + h.side_info_size;

  if(!memcmp(ptr, "Xing", 4))
    return 1;
  return 0;
  }

int bgav_mp3_info_header_probe(unsigned char *buf)
  {
  bgav_mpa_header_t h;
  uint8_t * ptr;

  memset(&h, 0, sizeof(h));

  if(!bgav_mpa_header_decode(&h, buf))
    return 0;

  ptr = buf + 4 + (h.has_crc * 2) + h.side_info_size;
  if(!memcmp(ptr, "Info", 4))
    return 1;
  return 0;
  }


int bgav_xing_header_read(bgav_xing_header_t * xing, unsigned char *buf)
  {
  int i;
  
  bgav_mpa_header_t h;
  
  memset(xing, 0, sizeof(*xing));

  memset(&h, 0, sizeof(h));
  
  if(!bgav_mpa_header_decode(&h, buf))
    return 0;

  buf += 4 + (h.has_crc * 2) + h.side_info_size;
  
  if (strncmp((char*)buf, "Xing", 4))
    return 0;
  buf += 4;
  
  xing->flags = GET_INT32BE(buf);
  
  if (xing->flags & FRAMES_FLAG)
    xing->frames = GET_INT32BE(buf);
  if (xing->frames < 1)
    xing->frames = 1;
  if (xing->flags & BYTES_FLAG)
    xing->bytes = GET_INT32BE(buf);

  if (xing->flags & TOC_FLAG)
    {
    for (i = 0; i < 100; i++)
      xing->toc[i] = buf[i];
    buf += 100;
    }
  return 1;
  }

#define CLAMP(i, min, max) i<min?min:(i>max?max:i)
#define MIN(i1,i2) i1<i2?i1:i2

int64_t bgav_xing_get_seek_position(bgav_xing_header_t * xing, float percent)
  {
          /* interpolate in TOC to get file seek point in bytes */
  int a, seekpoint;
  float fa, fb, fx;
  
  percent = CLAMP(percent, 0.0, 100.0);
  a = MIN(percent, 99);
  
  fa = xing->toc[a];
  
  if (a < 99)
    fb = xing->toc[a + 1];
  else
    fb = 256;
  
  fx = fa + (fb - fa) * (percent - a);
  seekpoint = (1.0f / 256.0f) * fx * xing->bytes;
  
  return seekpoint;
  
  }

void bgav_xing_header_dump(bgav_xing_header_t * xing)
  {
  int i, j;
  bgav_dprintf( "Xing header:\n");
  bgav_dprintf( "Flags: %08x, ", xing->flags);
  if(xing->flags & FRAMES_FLAG)
    bgav_dprintf( "FRAMES_FLAG ");
  if(xing->flags & BYTES_FLAG)
    bgav_dprintf( "BYTES_FLAG ");
  if(xing->flags & TOC_FLAG)
    bgav_dprintf( "TOC_FLAG ");
  if(xing->flags & VBR_SCALE_FLAG)
    bgav_dprintf( "VBR_SCALE_FLAG ");
  
  bgav_dprintf( "\nFrames: %u\n", xing->frames);
  bgav_dprintf( "Bytes: %u\n", xing->bytes);
  bgav_dprintf( "TOC:\n");
  for(i = 0; i < 10; i++)
    {
    for(j = 0; j < 10; j++)
      {
      bgav_dprintf( "%02x ", xing->toc[i*10+j]);
      }
    bgav_dprintf( "\n");
    }
  }
