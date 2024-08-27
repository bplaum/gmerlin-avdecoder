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



#include <string.h>
#include <stdlib.h>

#include <avdec_private.h>
#include <qt.h>
#include <stdio.h>

#define LOG_DOMAIN "qt_esds"

#define MP4ODescrTag                    0x01
#define MP4IODescrTag                   0x02
#define MP4ESDescrTag                   0x03
#define MP4DecConfigDescrTag            0x04
#define MP4DecSpecificDescrTag          0x05
#define MP4SLConfigDescrTag             0x06
#define MP4ContentIdDescrTag            0x07
#define MP4SupplContentIdDescrTag       0x08
#define MP4IPIPtrDescrTag               0x09
#define MP4IPMPPtrDescrTag              0x0A
#define MP4IPMPDescrTag                 0x0B
#define MP4RegistrationDescrTag         0x0D
#define MP4ESIDIncDescrTag              0x0E
#define MP4ESIDRefDescrTag              0x0F
#define MP4FileIODescrTag               0x10
#define MP4FileODescrTag                0x11
#define MP4ExtProfileLevelDescrTag      0x13
#define MP4ExtDescrTagsStart            0x80
#define MP4ExtDescrTagsEnd              0xFE

void bgav_qt_esds_dump(int indent, qt_esds_t * e)
  {
  gavl_diprintf(indent, "esds:\n");
  bgav_qt_atom_dump_header(indent+2, &e->h);
  gavl_diprintf(indent+2, "Version:          %d\n", e->version);
  gavl_diprintf(indent+2, "Flags:            0x%0x06x\n", e->flags);
  gavl_diprintf(indent+2, "objectTypeId:     %d\n", e->objectTypeId);
  gavl_diprintf(indent+2, "streamType:       0x%02x\n", e->streamType);
  gavl_diprintf(indent+2, "bufferSizeDB:     %d\n", e->bufferSizeDB);

  gavl_diprintf(indent+2, "maxBitrate:       %d\n", e->maxBitrate);
  gavl_diprintf(indent+2, "avgBitrate:       %d\n", e->avgBitrate);
  gavl_diprintf(indent+2, "decoderConfigLen: %d\n", e->decoderConfigLen);
  gavl_diprintf(indent+2, "decoderConfig:\n");
  gavl_hexdump(e->decoderConfig, e->decoderConfigLen, 16);
  }

static int read_mp4_descr_length(bgav_input_context_t * input)
  {
  uint8_t b;
  int num_bytes = 0;
  unsigned int length = 0;
  
  do
    {
    if(!bgav_input_read_8(input, &b))
      return -1;
    num_bytes++;
    length = (length << 7) | (b & 0x7F);
    } while ((b & 0x80) && (num_bytes < 4));
  return length;
  }

int bgav_qt_esds_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_esds_t * ret)
  {
  uint8_t tag;
  int len;
  
  READ_VERSION_AND_FLAGS;
  memcpy(&ret->h, h, sizeof(*h));
  
  if(!bgav_input_read_8(input, &tag))
    return 0;

  len = read_mp4_descr_length(input);
  
  if(tag == MP4ESDescrTag)
    {
    if(len < 20)
      return 0;
    bgav_input_skip(input, 3);
    }
  else
    bgav_input_skip(input, 2);

  if(!bgav_input_read_8(input, &tag))
    return 0;

  if(tag != MP4DecConfigDescrTag)
    return 0;

  len = read_mp4_descr_length(input);
  
  if(len < 13)
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "length of MP4DecConfigDescrTag too short: %d < 13", len);
    return 0;
    }
  
  if(!bgav_input_read_8(input, &ret->objectTypeId) ||
     !bgav_input_read_8(input, &ret->streamType) ||
     !bgav_input_read_24_be(input, &ret->bufferSizeDB) ||
     !bgav_input_read_32_be(input, &ret->maxBitrate) ||
     !bgav_input_read_32_be(input, &ret->avgBitrate))
    return 0;

  if(len >= 15)
    {
    if(!bgav_input_read_8(input, &tag))
      return 0;
    
    if(tag != MP4DecSpecificDescrTag)
      return 0;
    
    ret->decoderConfigLen = read_mp4_descr_length(input);
    
    ret->decoderConfig = calloc(ret->decoderConfigLen+16, 1);
    if(bgav_input_read_data(input, ret->decoderConfig,
                            ret->decoderConfigLen) < ret->decoderConfigLen)
      return 0;
    }
  bgav_qt_atom_skip(input, h);
  return 1;
  }

void bgav_qt_esds_free(qt_esds_t * esds)
  {
  if(esds->decoderConfig)
    free(esds->decoderConfig);
  }
