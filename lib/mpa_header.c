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
#include <mpa_header.h>


/*
 *  This demuxer handles mpegaudio (mp3) along with id3tags,
 *  AlbumWrap files seeking (incl VBR) and so on
 */

/* MPEG Audio header parsing code */

static const int mpeg_bitrates[5][16] = {
  /* MPEG-1 */
  { 0,  32000,  64000,  96000, 128000, 160000, 192000, 224000,    // I
       256000, 288000, 320000, 352000, 384000, 416000, 448000, 0},
  { 0,  32000,  48000,  56000,  64000,  80000,  96000, 112000,    // II
       128000, 160000, 192000, 224000, 256000, 320000, 384000, 0 },
  { 0,  32000,  40000,  48000,  56000,  64000,  80000,  96000,    // III
       112000, 128000, 160000, 192000, 224000, 256000, 320000, 0 },
   
  /* MPEG-2 LSF */
  { 0,  32000,  48000,  56000,  64000,  80000,  96000, 112000,    // I
       128000, 144000, 160000, 176000, 192000, 224000, 256000, 0 },
  { 0,   8000,  16000,  24000,  32000,  40000,  48000,  56000,
        64000,  80000,  96000, 112000, 128000, 144000, 160000, 0 } // II & III
};

static const int mpeg_samplerates[3][3] = {
  { 44100, 48000, 32000 }, // MPEG1
  { 22050, 24000, 16000 }, // MPEG2
  { 11025, 12000, 8000 }   // MPEG2.5
};

#define MPEG_ID_MASK        0x00180000
#define MPEG_MPEG1          0x00180000
#define MPEG_MPEG2          0x00100000
#define MPEG_MPEG2_5        0x00000000
                                                                                
#define MPEG_LAYER_MASK     0x00060000
#define MPEG_LAYER_III      0x00020000
#define MPEG_LAYER_II       0x00040000
#define MPEG_LAYER_I        0x00060000
#define MPEG_PROTECTION     0x00010000
#define MPEG_BITRATE_MASK   0x0000F000
#define MPEG_FREQUENCY_MASK 0x00000C00
#define MPEG_PAD_MASK       0x00000200
#define MPEG_PRIVATE_MASK   0x00000100
#define MPEG_MODE_MASK      0x000000C0
#define MPEG_MODE_EXT_MASK  0x00000030
#define MPEG_COPYRIGHT_MASK 0x00000008
#define MPEG_HOME_MASK      0x00000004
#define MPEG_EMPHASIS_MASK  0x00000003
#define LAYER_I_SAMPLES       384
#define LAYER_II_III_SAMPLES 1152

/* Header detection stolen from the mpg123 plugin of xmms */

static int header_check(uint32_t head)
{
        if ((head & 0xffe00000) != 0xffe00000)
                return 0;
        if (!((head >> 17) & 3))
                return 0;
        if (((head >> 12) & 0xf) == 0xf)
                return 0;
        if (!((head >> 12) & 0xf))
                return 0;
        if (((head >> 10) & 0x3) == 0x3)
                return 0;
#if 0 /*; These are supported by us (but probably not by xmms :) */
        if (((head >> 19) & 1) == 1 &&
            ((head >> 17) & 3) == 3 &&
            ((head >> 16) & 1) == 1)
                return 0;
#endif
        if ((head & 0xffff0000) == 0xfffe0000)
          return 0;
        return 1;
}


#if 0
#define IS_MPEG_AUDIO_HEADER(h) (((h&0xFFE00000)==0xFFE00000)&&\
                                ((h&0x0000F000)!=0x0000F000)&&\
                                ((h&0x00060000)!=0)&&\
                                ((h&0x00180000)!=0x00080000)&&\
                                ((h&0x00000C00)!=0x00000C00))
#endif

int bgav_mpa_header_equal(bgav_mpa_header_t * h1, bgav_mpa_header_t * h2)
  {
  return ((h1->version == h2->version) &&
          (h1->layer == h2->layer) &&
          (h1->samplerate == h2->samplerate));
  }

void bgav_mpa_header_dump(bgav_mpa_header_t * h)
  {
  bgav_dprintf( "Header:\n");
  bgav_dprintf( "  Version:     %s\n",
          (h->version == MPEG_VERSION_1 ? "1" :
           (h->version == MPEG_VERSION_2 ? "2" : "2.5")));
  bgav_dprintf( "  Layer:       %d\n", h->layer);
  bgav_dprintf( "  Bitrate:     %d\n", h->bitrate);
  bgav_dprintf( "  Samplerate:  %d\n", h->samplerate);
  bgav_dprintf( "  Frame bytes: %d\n", h->frame_bytes);

  switch(h->channel_mode)
    {
    case CHANNEL_STEREO:
      bgav_dprintf( "  Channel mode: Stereo\n");
      break;
    case CHANNEL_JSTEREO:
      bgav_dprintf( "  Channel mode: Joint Stereo\n");
      break;
    case CHANNEL_DUAL:
      bgav_dprintf( "  Channel mode: Dual\n");
      break;
    case CHANNEL_MONO:
      bgav_dprintf( "  Channel mode: Mono\n");
      break;
    }
  
  }


int bgav_mpa_header_decode(bgav_mpa_header_t * h, uint8_t * ptr)
  {
  uint32_t header;
  int index;
  /* For calculation of the byte length of a frame */
  int pad;
  int slots_per_frame;
  h->frame_bytes = 0;
  header =
    ptr[3] | (ptr[2] << 8) | (ptr[1] << 16) | (ptr[0] << 24);
  if(!header_check(header))
    return 0;

  if(!(header & MPEG_PROTECTION))
    h->has_crc = 1;
  else
    h->has_crc = 0;
  
  index = (header & MPEG_MODE_MASK) >> 6;
  switch(index)
    {
    case 0:
      h->channel_mode = CHANNEL_STEREO;
      break;
    case 1:
      h->channel_mode = CHANNEL_JSTEREO;
      break;
    case 2:
      h->channel_mode = CHANNEL_DUAL;
      break;
    case 3:
      h->channel_mode = CHANNEL_MONO;
      break;
    }
  /* Get Version */
  switch(header & MPEG_ID_MASK)
    {
    case MPEG_MPEG1:
      h->version = MPEG_VERSION_1;
        break;
    case MPEG_MPEG2:
      h->version = MPEG_VERSION_2;
      break;
    case MPEG_MPEG2_5:
      h->version = MPEG_VERSION_2_5;
      break;
    default:
      return 0;
    }
  /* Get Layer */
  switch(header & MPEG_LAYER_MASK)
    {
    case MPEG_LAYER_I:
      h->layer = 1;
      break;
    case MPEG_LAYER_II:
      h->layer = 2;
      break;
    case MPEG_LAYER_III:
      h->layer = 3;
      break;
    }
  index = (header & MPEG_BITRATE_MASK) >> 12;
  switch(h->version)
    {
    case MPEG_VERSION_1:
      //         cerr << "MPEG-1 audio layer " << h->layer
      //              << " bitrate index: " << bitrate_index << "\n";
      switch(h->layer)
        {
        case 1:
          h->bitrate = mpeg_bitrates[0][index];
          break;
        case 2:
          h->bitrate = mpeg_bitrates[1][index];
          break;
        case 3:
          h->bitrate = mpeg_bitrates[2][index];
          break;
        }
      break;
    case MPEG_VERSION_2:
    case MPEG_VERSION_2_5:
      //         cerr << "MPEG-2 audio layer " << h->layer
      //              << " bitrate index: " << bitrate_index << "\n";
      switch(h->layer)
        {
        case 1:
          h->bitrate = mpeg_bitrates[3][index];
          break;
        case 2:
        case 3:
          h->bitrate = mpeg_bitrates[4][index];
          break;
        }
      break;
    default: // This won't happen, but keeps gcc quiet
      return 0;
    }
  index = (header & MPEG_FREQUENCY_MASK) >> 10;
  switch(h->version)
    {
    case MPEG_VERSION_1:
      h->samplerate = mpeg_samplerates[0][index];
      break;
    case MPEG_VERSION_2:
      h->samplerate = mpeg_samplerates[1][index];
      break;
    case MPEG_VERSION_2_5:
      h->samplerate = mpeg_samplerates[2][index];
      break;
    default: // This won't happen, but keeps gcc quiet
      return 0;
    }
  pad = (header & MPEG_PAD_MASK) ? 1 : 0;
  if(h->layer == 1)
    {
    h->frame_bytes = ((12 * h->bitrate / h->samplerate) + pad) * 4;
    }
  else
    {
    slots_per_frame = ((h->layer == 3) &&
      ((h->version == MPEG_VERSION_2) ||
       (h->version == MPEG_VERSION_2_5))) ? 72 : 144;
    h->frame_bytes = (slots_per_frame * h->bitrate) / h->samplerate + pad;
    }
  // h->mode = (ptr[3] >> 6) & 3;

  h->samples_per_frame =
    (h->layer == 1) ? LAYER_I_SAMPLES : LAYER_II_III_SAMPLES;

  if(h->version != MPEG_VERSION_1)
    h->samples_per_frame /= 2;

  /* Side info len */

  if((header & MPEG_ID_MASK) == MPEG_MPEG1)
    {
    if(h->channel_mode == CHANNEL_MONO)
      h->side_info_size = 0x11;
    else
      h->side_info_size = 0x20;
    }
  else
    {
    if(h->channel_mode == CHANNEL_MONO)
      h->side_info_size = 0x09;
    else
      h->side_info_size = 0x11;
    }
  
  //  dump_header(h);
  return 1;
  }

void bgav_mpa_header_get_format(const bgav_mpa_header_t * h,
                                gavl_audio_format_t * format)
  {
  format->samplerate        = h->samplerate;
  format->samples_per_frame = h->samples_per_frame;

  switch(h->channel_mode)
    {
    case CHANNEL_STEREO:
    case CHANNEL_JSTEREO:
    case CHANNEL_DUAL:
      format->num_channels = 2;
      break;
    case CHANNEL_MONO:
      format->num_channels = 1;
      break;
    }
  gavl_set_channel_setup(format);
  }
