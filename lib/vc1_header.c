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
#include <vc1_header.h>
#include <bitstream.h>

const int vc1_fps_nr[5] = { 24000, 25000, 30000, 50000, 60000 };
const int vc1_fps_dr[2] = { 1000, 1001 };

int bgav_vc1_unescape_buffer(const uint8_t *src, int size, uint8_t *dst)
  {
  int dsize = 0, i;

  if(size < 4)
    {
    for(dsize = 0; dsize < size; dsize++) *dst++ = *src++;
    return size;
    }
  for(i = 0; i < size; i++, src++)
    {
    if(src[0] == 3 && i >= 2 && !src[-1] && !src[-2] && i < size-1 && src[1] < 4)
      {
      dst[dsize++] = src[1];
      src++;
      i++;
      }
    else
      dst[dsize++] = *src;
    }
  return dsize;
  }


static int read_sequence_header_advanced(bgav_bitstream_t * b,
                                         bgav_vc1_sequence_header_t * ret)
  {
  int dummy;

  if(!bgav_bitstream_get(b, &ret->h.adv.level, 3) ||
     !bgav_bitstream_get(b, &ret->h.adv.chromaformat, 2) ||
     !bgav_bitstream_get(b, &ret->h.adv.frmrtq_postproc, 3) ||
     !bgav_bitstream_get(b, &ret->h.adv.bitrtq_postproc, 5) ||
     !bgav_bitstream_get(b, &ret->h.adv.postprocflag, 1) ||
     !bgav_bitstream_get(b, &ret->h.adv.width, 12) ||
     !bgav_bitstream_get(b, &ret->h.adv.height, 12) ||
     !bgav_bitstream_get(b, &ret->h.adv.broadcast, 1) ||
     !bgav_bitstream_get(b, &ret->h.adv.interlace, 1) ||
     !bgav_bitstream_get(b, &ret->h.adv.tfcntrflag, 1) ||
     !bgav_bitstream_get(b, &ret->h.adv.finterpflag, 1) ||
     !bgav_bitstream_get(b, &ret->h.adv.reserved, 1) ||
     !bgav_bitstream_get(b, &ret->h.adv.psf, 1) ||
     !bgav_bitstream_get(b, &ret->h.adv.display_info_flag, 1))
    return 0;
  
  if(ret->h.adv.display_info_flag)
    {
    if(!bgav_bitstream_get(b, &ret->h.adv.display_width, 14) ||
       !bgav_bitstream_get(b, &ret->h.adv.display_height, 14) ||
       !bgav_bitstream_get(b, &ret->h.adv.aspect_ratio_flag, 1))
      return 0;
    
    if(ret->h.adv.aspect_ratio_flag)
      {
      if(!bgav_bitstream_get(b, &ret->h.adv.aspect_ratio_code, 4))
        return 0;
      if(ret->h.adv.aspect_ratio_code == 15)
        {
        if(!bgav_bitstream_get(b, &ret->h.adv.pixel_width, 8) ||
           !bgav_bitstream_get(b, &ret->h.adv.pixel_height, 8))
          return 0;
        }
      }

    if(!bgav_bitstream_get(b, &ret->h.adv.framerate_flag, 1))
      return 0;

    if(ret->h.adv.framerate_flag)
      {
      if(!bgav_bitstream_get(b, &dummy, 1))
        return 0;
      if(dummy)
        {
        int tmp;
        if(!bgav_bitstream_get(b, &tmp, 16))
          return 0;

        ret->h.adv.frame_duration = 32;
        ret->h.adv.timescale = tmp + 1;
        }
      else
        {
        int nr, dr;
        if(!bgav_bitstream_get(b, &nr, 8) ||
           !bgav_bitstream_get(b, &dr, 4))
          return 0;
        if(nr && (nr < 8) && dr && (dr < 3))
          {
          ret->h.adv.timescale = vc1_fps_nr[nr - 1];
          ret->h.adv.frame_duration = vc1_fps_dr[dr - 1];
          }
        }
      }
    
    }
  return 1;
  }


int bgav_vc1_sequence_header_read(const bgav_options_t * opt,
                                  bgav_vc1_sequence_header_t * ret,
                                  const uint8_t * buffer, int len)
  {
  bgav_bitstream_t b;
  //  int dummy;

  buffer+=4;
  len -= 4;
  
  bgav_bitstream_init(&b, buffer, len);

  if(!bgav_bitstream_get(&b, &ret->profile, 2))
    return 0;
  
  if(ret->profile == PROFILE_ADVANCED)
    {
    if(read_sequence_header_advanced(&b, ret))
      return len - bgav_bitstream_get_bits(&b) / 8;
    else
      return 0;
    }
  return 0;
  }

void bgav_vc1_sequence_header_dump(const bgav_vc1_sequence_header_t * h)
  {
  bgav_dprintf("VC-1 sequence header\n");
  bgav_dprintf("  profile:         %d\n", h->profile);
  if(h->profile == PROFILE_ADVANCED)
    {
    bgav_dprintf("  level:             %d\n", h->h.adv.level);
    bgav_dprintf("  chromaformat:      %d\n", h->h.adv.chromaformat);
    bgav_dprintf("  frmrtq_postproc:   %d\n", h->h.adv.frmrtq_postproc);
    bgav_dprintf("  bitrtq_postproc:   %d\n", h->h.adv.bitrtq_postproc);
    bgav_dprintf("  postprocflag:      %d\n", h->h.adv.postprocflag);
    bgav_dprintf("  width:             %d [%d]\n", h->h.adv.width, (h->h.adv.width+1)<<1);
    bgav_dprintf("  height:            %d [%d]\n", h->h.adv.height, (h->h.adv.height+1)<<1);
    bgav_dprintf("  broadcast:         %d\n", h->h.adv.broadcast);
    bgav_dprintf("  interlace:         %d\n", h->h.adv.interlace);
    bgav_dprintf("  tfcntrflag:        %d\n", h->h.adv.tfcntrflag);
    bgav_dprintf("  finterpflag:       %d\n", h->h.adv.finterpflag);
    bgav_dprintf("  reserved:          %d\n", h->h.adv.reserved);
    bgav_dprintf("  psf:               %d\n", h->h.adv.psf);
    bgav_dprintf("  display_info_flag: %d\n", h->h.adv.display_info_flag);

    if(h->h.adv.display_info_flag)
      {
      bgav_dprintf("  display_width:     %d [%d]\n",
                   h->h.adv.display_width, h->h.adv.display_width+1);
      bgav_dprintf("  display_height:    %d [%d]\n",
                   h->h.adv.display_height, h->h.adv.display_height+1);

      bgav_dprintf("  aspect_ratio_flag: %d\n", h->h.adv.aspect_ratio_flag);

      if(h->h.adv.aspect_ratio_flag)
        {
        bgav_dprintf("  aspect_ratio_code: %d\n", h->h.adv.aspect_ratio_code);
        bgav_dprintf("  pixel_width:       %d\n", h->h.adv.pixel_width);
        bgav_dprintf("  pixel_height:      %d\n", h->h.adv.pixel_height);
        }

      bgav_dprintf("  framerate_flag:    %d\n", h->h.adv.framerate_flag);
      if(h->h.adv.framerate_flag)
        {
        bgav_dprintf("  timescale:         %d\n", h->h.adv.timescale);
        bgav_dprintf("  frame_duration:    %d\n", h->h.adv.frame_duration);
        }
      }
    
    }
  }

int bgav_vc1_picture_header_adv_read(const bgav_options_t * opt,
                                     bgav_vc1_picture_header_adv_t * ret,
                                     const uint8_t * buffer, int len,
                                     const bgav_vc1_sequence_header_t * seq)
  {
  bgav_bitstream_t b;
  int dummy;

  buffer+=4;
  len -= 4;
  memset(ret, 0, sizeof(*ret));
  
  bgav_bitstream_init(&b, buffer, len);
  
  if(seq->h.adv.interlace)
    {
    if(!bgav_bitstream_decode012(&b, &ret->fcm))
      return 0;
    }

  if(!bgav_bitstream_get_unary(&b, 0, 4, &dummy))
    return 0;

  switch(dummy)
    {
    case 0:
      ret->coding_type = BGAV_CODING_TYPE_P;
      break;
    case 1:
      ret->coding_type = BGAV_CODING_TYPE_B;
      break;
    case 2:
      ret->coding_type = BGAV_CODING_TYPE_I;
      break;
    case 3: // BI
      ret->coding_type = BGAV_CODING_TYPE_I;
      break;
    case 4: // Skipped P
      ret->coding_type = BGAV_CODING_TYPE_P;
      break;
    }
  return 1;
  }

void bgav_vc1_picture_header_adv_dump(bgav_vc1_picture_header_adv_t * ret)
  {
  bgav_dprintf("VC-1 picture header\n");
  bgav_dprintf("  fcm:  %d\n", ret->fcm);
  bgav_dprintf("  type: %s\n", bgav_coding_type_to_string(ret->coding_type));
  
  }
