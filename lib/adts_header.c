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



#include <avdec_private.h>
#include <adts_header.h>

#define IS_ADTS(h) ((h[0] == 0xff) && \
                    ((h[1] & 0xf0) == 0xf0) && \
                    ((h[1] & 0x06) == 0x00))

static const int adts_samplerates[] =
  {96000,88200,64000,48000,44100,
   32000,24000,22050,16000,12000,
   11025,8000,7350,0,0,0};

int bgav_adts_header_read(const uint8_t * data,
                          bgav_adts_header_t * ret)
  {

  if(!IS_ADTS(data))
    return 0;
  
  if(data[1] & 0x08)
    ret->mpeg_version = 2;
  else
    ret->mpeg_version = 4;

  ret->protection_absent = data[1] & 0x01;

  ret->profile = (data[2] & 0xC0) >> 6;

  ret->samplerate_index = (data[2]&0x3C)>>2;
  ret->samplerate = adts_samplerates[ret->samplerate_index];
  
  ret->channel_configuration = ((data[2]&0x01)<<2)|((data[3]&0xC0)>>6);

  ret->frame_bytes = ((((unsigned int)data[3] & 0x3)) << 11)
    | (((unsigned int)data[4]) << 3) | (data[5] >> 5);
  
  ret->num_blocks = (data[6] & 0x03) + 1;
  return 1;
  }

void bgav_adts_header_dump(const bgav_adts_header_t * adts)
  {
  gavl_dprintf( "ADTS\n");
  gavl_dprintf( "  MPEG Version:          %d\n", adts->mpeg_version);
  gavl_dprintf( "  Profile:               ");
  
  if(adts->mpeg_version == 2)
    {
    switch(adts->profile)
      {
      case 0:
        gavl_dprintf( "MPEG-2 AAC Main profile\n");
        break;
      case 1:
        gavl_dprintf( "MPEG-2 AAC Low Complexity profile (LC)\n");
        break;
      case 2:
        gavl_dprintf( "MPEG-2 AAC Scalable Sample Rate profile (SSR)\n");
        break;
      case 3:
        gavl_dprintf( "MPEG-2 AAC (reserved)\n");
        break;
      }
    }
  else
    {
    switch(adts->profile)
      {
      case 0:
        gavl_dprintf( "MPEG-4 AAC Main profile\n");
        break;
      case 1:
        gavl_dprintf( "MPEG-4 AAC Low Complexity profile (LC)\n");
        break;
      case 2:
        gavl_dprintf( "MPEG-4 AAC Scalable Sample Rate profile (SSR)\n");
        break;
      case 3:
        gavl_dprintf( "MPEG-4 AAC Long Term Prediction (LTP)\n");
        break;
      }
    }
  gavl_dprintf( "  Samplerate:            %d\n", adts->samplerate);
  gavl_dprintf( "  Channel configuration: %d\n", adts->channel_configuration);
  gavl_dprintf( "  Frame bytes:           %d\n", adts->frame_bytes);
  gavl_dprintf( "  Num blocks:            %d\n", adts->num_blocks);
  gavl_dprintf( "  Protection absent:     %d\n", adts->protection_absent);
  }

void bgav_adts_header_get_format(const bgav_adts_header_t * adts,
                                 gavl_audio_format_t * format)
  {
  if(adts->profile == 2) 
    format->samples_per_frame = 960;
  else
    format->samples_per_frame = 1024;
  
  format->samplerate = adts->samplerate;

  switch(adts->channel_configuration)
    {
    case 1:
      format->num_channels = 1;
      format->channel_locations[0] = GAVL_CHID_FRONT_CENTER;
      break;
    case 2:
      format->num_channels = 2;
      format->channel_locations[0] = GAVL_CHID_FRONT_LEFT;
      format->channel_locations[1] = GAVL_CHID_FRONT_RIGHT;
      break;
    case 3:
      format->num_channels = 3;
      format->channel_locations[0] = GAVL_CHID_FRONT_CENTER;
      format->channel_locations[1] = GAVL_CHID_FRONT_LEFT;
      format->channel_locations[2] = GAVL_CHID_FRONT_RIGHT;
      break;
    case 4:
      format->num_channels = 4;
      format->channel_locations[0] = GAVL_CHID_FRONT_CENTER;
      format->channel_locations[1] = GAVL_CHID_FRONT_LEFT;
      format->channel_locations[2] = GAVL_CHID_FRONT_RIGHT;
      format->channel_locations[3] = GAVL_CHID_REAR_CENTER;
      break;
    case 5:
      format->num_channels = 5;
      format->channel_locations[0] = GAVL_CHID_FRONT_CENTER;
      format->channel_locations[1] = GAVL_CHID_FRONT_LEFT;
      format->channel_locations[2] = GAVL_CHID_FRONT_RIGHT;
      format->channel_locations[3] = GAVL_CHID_REAR_LEFT;
      format->channel_locations[4] = GAVL_CHID_REAR_RIGHT;
      break;
    case 6:
      format->num_channels = 6;
      format->channel_locations[0] = GAVL_CHID_FRONT_CENTER;
      format->channel_locations[1] = GAVL_CHID_FRONT_LEFT;
      format->channel_locations[2] = GAVL_CHID_FRONT_RIGHT;
      format->channel_locations[3] = GAVL_CHID_REAR_LEFT;
      format->channel_locations[4] = GAVL_CHID_REAR_RIGHT;
      format->channel_locations[5] = GAVL_CHID_LFE;
      break;
    }
  
  }
