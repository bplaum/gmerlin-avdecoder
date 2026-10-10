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

 
#define EAC3_ERR_TRUNCATED    (-1)  /* packet ends inside a syncframe */
#define EAC3_ERR_NO_SYNC      (-2)  /* syncword missing where expected */
#define EAC3_ERR_NOT_EAC3     (-3)  /* bsid outside 11..16 (e.g. plain AC-3) */
#define EAC3_ERR_BAD_HEADER   (-4)  /* reserved or invalid header values */
#define EAC3_ERR_NO_TIMING    (-5)  /* no independent substream 0 frame */
#define EAC3_ERR_INCONSISTENT (-6)  /* sample rate changes inside packet */
 
#define EAC3_MIN_HEADER_SIZE  6     /* syncword + bytes up to and incl. bsid */
#define EAC3_SAMPLES_PER_BLOCK 256
 
struct eac3_packet_info
  {
  int sample_rate;           /* Hz */
  int samples_per_channel;   /* total over all time frames in the packet */
  int num_time_frames;       /* independent substream 0 syncframes */
  int num_syncframes;        /* all syncframes incl. dependent substreams */
  };
 
/*
 * Parse one Matroska E-AC-3 block.
 * Returns 0 on success and fills *info, or a negative EAC3_ERR_* code.
 * On error, *info is zeroed.
 */
static int eac3_parse_packet(const uint8_t *ptr, size_t len,
                             struct eac3_packet_info *info)
  {
  static const int rate_table[3]         = { 48000, 44100, 32000 };
  static const int rate_table_reduced[3] = { 24000, 22050, 16000 };
  static const int blocks_table[4]       = { 1, 2, 3, 6 };
 
  size_t pos = 0;
 
  if (!info)
    return EAC3_ERR_BAD_HEADER;
  memset(info, 0, sizeof(*info));
 
  if (!ptr || len == 0)
    return EAC3_ERR_TRUNCATED;
 
  while (pos < len)
    {
    const uint8_t *hdr = ptr + pos;
    size_t remaining = len - pos;
    int strmtyp, substreamid, fscod, bsid;
    int sample_rate, num_blocks;
    size_t frame_size;
    
    if(remaining < EAC3_MIN_HEADER_SIZE)
      {
      memset(info, 0, sizeof(*info));
      return EAC3_ERR_TRUNCATED;
      }
    
    if(hdr[0] != 0x0B || hdr[1] != 0x77)
      {
      memset(info, 0, sizeof(*info));
      return EAC3_ERR_NO_SYNC;
      }
    
    strmtyp     = hdr[2] >> 6;
    substreamid = (hdr[2] >> 3) & 0x07;
    frame_size  = ((size_t)(((hdr[2] & 0x07) << 8) | hdr[3]) + 1) * 2;
    fscod       = hdr[4] >> 6;
    bsid        = hdr[5] >> 3;
 
    if(bsid < 11 || bsid > 16)
      {
      memset(info, 0, sizeof(*info));
      return EAC3_ERR_NOT_EAC3;
      }
    
    if(strmtyp == 3)
      {                 /* reserved stream type */
      memset(info, 0, sizeof(*info));
      return EAC3_ERR_BAD_HEADER;
      }
    
    if(fscod == 3)
      {
      /* Reduced sample rate: fscod2 is sent, numblkscod is implied
       * to be 3 (six blocks). */
      int fscod2 = (hdr[4] >> 4) & 0x03;
      if(fscod2 == 3)
        {
        memset(info, 0, sizeof(*info));
        return EAC3_ERR_BAD_HEADER;
        }
      sample_rate = rate_table_reduced[fscod2];
      num_blocks  = 6;
      }
    else
      {
      sample_rate = rate_table[fscod];
      num_blocks  = blocks_table[(hdr[4] >> 4) & 0x03];
      }
    
    if(frame_size < EAC3_MIN_HEADER_SIZE)
      {
      memset(info, 0, sizeof(*info));
      return EAC3_ERR_BAD_HEADER;
      }
    
    if(frame_size > remaining)
      {
      memset(info, 0, sizeof(*info));
      return EAC3_ERR_TRUNCATED;
      }
    
    info->num_syncframes++;
 
    /* Only independent frames of substream 0 advance playback time.
     * Dependent frames (strmtyp 1) extend the same time frame, and
     * independent frames with substreamid > 0 belong to other programs. */
    if(strmtyp != 1 && substreamid == 0)
    {
    if(info->sample_rate != 0 && info->sample_rate != sample_rate)
      {
      memset(info, 0, sizeof(*info));
      return EAC3_ERR_INCONSISTENT;
      }

    info->sample_rate = sample_rate;
    info->samples_per_channel += num_blocks * EAC3_SAMPLES_PER_BLOCK;
    info->num_time_frames++;
    }
  
    pos += frame_size;
    }
 
  if (info->num_time_frames == 0)
    {
    memset(info, 0, sizeof(*info));
    return EAC3_ERR_NO_TIMING;
    }
  return 0;
  }

static int parse_frame_eac3(bgav_packet_parser_t * parser,
                           bgav_packet_t * p)
  {
  struct eac3_packet_info info;

  if(eac3_parse_packet(p->buf.buf, p->buf.len, &info))
    return 0;
#if 0  
  if(!(parser->parser_flags & PARSER_HAS_HEADER))
    {
    
    }
#endif
  p->duration = info.samples_per_channel;
  return 1;
  }


void bgav_packet_parser_init_eac3(bgav_packet_parser_t * parser)
  {
  parser->parse_frame = parse_frame_eac3;
  }
