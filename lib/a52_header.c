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
#include <a52_header.h>

#define BGAV_A52_CHANNEL 0
#define BGAV_A52_MONO 1
#define BGAV_A52_STEREO 2
#define BGAV_A52_3F 3
#define BGAV_A52_2F1R 4
#define BGAV_A52_3F1R 5
#define BGAV_A52_2F2R 6
#define BGAV_A52_3F2R 7
#define BGAV_A52_CHANNEL1 8
#define BGAV_A52_CHANNEL2 9
#define BGAV_A52_DOLBY 10

#define LEVEL_3DB 0.7071067811865476
#define LEVEL_45DB 0.5946035575013605
#define LEVEL_6DB 0.5

#define FRAME_SAMPLES 1536

static const uint8_t halfrate[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3};
static const int rate[] = { 32,  40,  48,  56,  64,  80,  96, 112,
                      128, 160, 192, 224, 256, 320, 384, 448,
                      512, 576, 640};
static const uint8_t lfeon[8] = {0x10, 0x10, 0x04, 0x04, 0x04, 0x01, 0x04, 0x01};

static const float clev[4] = {LEVEL_3DB, LEVEL_45DB, LEVEL_6DB, LEVEL_45DB};
static const float slev[4] = {LEVEL_3DB, LEVEL_6DB,          0, LEVEL_6DB};

int bgav_a52_header_read(bgav_a52_header_t * ret, uint8_t * buf)
  {
  int half;
  int frmsizecod;
  int bitrate;

  int cmixlev;
  int smixlev;

  memset(ret, 0, sizeof(*ret));
  
  if ((buf[0] != 0x0b) || (buf[1] != 0x77))   /* syncword */
    {
    return 0;
    }
  if (buf[5] >= 0x60)         /* bsid >= 12 */
    {
    return 0;
    }
  half = halfrate[buf[5] >> 3];

  /* acmod, dsurmod and lfeon */
  ret->acmod = buf[6] >> 5;

  /* cmixlev and surmixlev */

  if((ret->acmod & 0x01) && (ret->acmod != 0x01))
    {
    cmixlev = (buf[6] & 0x18) >> 3;
    }
  else
    cmixlev = -1;
  
  if(ret->acmod & 0x04)
    {
    if((cmixlev) == -1)
      smixlev = (buf[6] & 0x18) >> 3;
    else
      smixlev = (buf[6] & 0x06) >> 1;
    }
  else
    smixlev = -1;

  if(smixlev >= 0)
    ret->smixlev = slev[smixlev];
  if(cmixlev >= 0)
    ret->cmixlev = clev[cmixlev];
  
  if((buf[6] & 0xf8) == 0x50)
    ret->dolby = 1;

  if(buf[6] & lfeon[ret->acmod])
    ret->lfe = 1;

  frmsizecod = buf[4] & 63;
  if (frmsizecod >= 38)
    return 0;
  bitrate = rate[frmsizecod >> 1];
  ret->bitrate = (bitrate * 1000) >> half;

  switch (buf[4] & 0xc0)
    {
    case 0:
      ret->samplerate = 48000 >> half;
      ret->total_bytes = 4 * bitrate;
      break;
    case 0x40:
      ret->samplerate = 44100 >> half;
      ret->total_bytes =  2 * (320 * bitrate / 147 + (frmsizecod & 1));
      break;
    case 0x80:
      ret->samplerate = 32000 >> half;
      ret->total_bytes =  6 * bitrate;
      break;
    default:
      return 0;
    }
  
  return 1;
  }

void bgav_a52_header_dump(bgav_a52_header_t * h)
  {
  bgav_dprintf("A52 header:\n");
  bgav_dprintf("  Frame bytes: %d\n", h->total_bytes);
  bgav_dprintf("  Samplerate:  %d\n", h->samplerate);
  bgav_dprintf("  acmod:  0x%0x\n",   h->acmod);
  if(h->smixlev >= 0.0)
    bgav_dprintf("  smixlev: %f\n", h->smixlev);
  if(h->cmixlev >= 0.0)
    bgav_dprintf("  cmixlev: %f\n", h->cmixlev);
  }

void bgav_a52_header_get_format(const bgav_a52_header_t * h,
                                gavl_audio_format_t * format)
  {
  format->samplerate = h->samplerate;
  format->samples_per_frame = FRAME_SAMPLES;
  
  if(h->lfe)
    {
    format->num_channels = 1;
    format->channel_locations[0] = GAVL_CHID_LFE;
    }
  else
    format->num_channels = 0;


  switch(h->acmod)
    {
    case BGAV_A52_CHANNEL:
    case BGAV_A52_STEREO:
      
      format->channel_locations[format->num_channels] = 
        GAVL_CHID_FRONT_LEFT;
      format->channel_locations[format->num_channels+1] = 
        GAVL_CHID_FRONT_RIGHT;
      format->num_channels += 2;
      break;
    case BGAV_A52_MONO:
      format->channel_locations[format->num_channels] = 
        GAVL_CHID_FRONT_CENTER;
      format->num_channels += 1;
      break;
    case BGAV_A52_3F:
      format->channel_locations[format->num_channels] = 
        GAVL_CHID_FRONT_LEFT;
      format->channel_locations[format->num_channels+1] = 
        GAVL_CHID_FRONT_CENTER;
      format->channel_locations[format->num_channels+2] = 
        GAVL_CHID_FRONT_RIGHT;
      format->num_channels += 3;
      break;
    case BGAV_A52_2F1R:
      format->channel_locations[format->num_channels] = 
        GAVL_CHID_FRONT_LEFT;
      format->channel_locations[format->num_channels+1] = 
        GAVL_CHID_FRONT_RIGHT;
      format->channel_locations[format->num_channels+2] = 
        GAVL_CHID_REAR_CENTER;
      format->num_channels += 3;
      break;
    case BGAV_A52_3F1R:
      format->channel_locations[format->num_channels] = 
        GAVL_CHID_FRONT_LEFT;
      format->channel_locations[format->num_channels+1] = 
        GAVL_CHID_FRONT_CENTER;
      format->channel_locations[format->num_channels+2] = 
        GAVL_CHID_FRONT_RIGHT;
      format->channel_locations[format->num_channels+3] = 
        GAVL_CHID_REAR_CENTER;
      format->num_channels += 4;

      break;
    case BGAV_A52_2F2R:
      format->channel_locations[format->num_channels] = 
        GAVL_CHID_FRONT_LEFT;
      format->channel_locations[format->num_channels+1] = 
        GAVL_CHID_FRONT_RIGHT;
      format->channel_locations[format->num_channels+2] = 
        GAVL_CHID_REAR_LEFT;
      format->channel_locations[format->num_channels+3] = 
        GAVL_CHID_REAR_RIGHT;
      format->num_channels += 4;
      break;
    case BGAV_A52_3F2R:
      format->channel_locations[format->num_channels] = 
        GAVL_CHID_FRONT_LEFT;
      format->channel_locations[format->num_channels+1] = 
        GAVL_CHID_FRONT_CENTER;
      format->channel_locations[format->num_channels+2] = 
        GAVL_CHID_FRONT_RIGHT;
      format->channel_locations[format->num_channels+3] = 
        GAVL_CHID_REAR_LEFT;
      format->channel_locations[format->num_channels+4] = 
        GAVL_CHID_REAR_RIGHT;
      format->num_channels += 5;
      break;
    }

  if(gavl_front_channels(format) == 3)
    format->center_level = h->cmixlev;
  if(gavl_rear_channels(format))
    format->rear_level = h->smixlev;
  
  }
