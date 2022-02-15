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
#include <aac_frame.h>
#include <stdlib.h>

#define LOG_DOMAIN "aac_frame"

#ifdef HAVE_NEAACDEC_H
#include <neaacdec.h>

// #define DUMP_DECODE

/*
 *  Backwards compatibility names (currently in neaacdec.h,
 *  but might be removed in future versions)
 */
#ifndef faacDecHandle
/* structs */
#define faacDecHandle                  NeAACDecHandle
#define faacDecConfiguration           NeAACDecConfiguration
#define faacDecConfigurationPtr        NeAACDecConfigurationPtr
#define faacDecFrameInfo               NeAACDecFrameInfo
/* functions */
#define faacDecGetErrorMessage         NeAACDecGetErrorMessage
#define faacDecSetConfiguration        NeAACDecSetConfiguration
#define faacDecGetCurrentConfiguration NeAACDecGetCurrentConfiguration
#define faacDecInit                    NeAACDecInit
#define faacDecInit2                   NeAACDecInit2
#define faacDecInitDRM                 NeAACDecInitDRM
#define faacDecPostSeekReset           NeAACDecPostSeekReset
#define faacDecOpen                    NeAACDecOpen
#define faacDecClose                   NeAACDecClose
#define faacDecDecode                  NeAACDecDecode
#define AudioSpecificConfig            NeAACDecAudioSpecificConfig
#endif

#else
#include <faad.h>
#endif


struct bgav_aac_frame_s
  {
  faacDecHandle dec;
  unsigned long samplerate;
  unsigned char channels;
  const bgav_options_t * opt;
  faacDecFrameInfo frame_info;
  };

bgav_aac_frame_t * bgav_aac_frame_create(const bgav_options_t * opt,
                                         uint8_t * header, int header_len)
  {
  bgav_aac_frame_t * ret;
  ret = calloc(1, sizeof(*ret));

  if(header)
    {
    ret->dec = faacDecOpen();
    faacDecInit2(ret->dec, header,
                 header_len,
                 &ret->samplerate, &ret->channels);
    }
  ret->opt = opt;
  return ret;
  }

void bgav_aac_frame_destroy(bgav_aac_frame_t * f)
  {
  if(f->dec)
    faacDecClose(f->dec);
  free(f);
  }

int bgav_aac_frame_parse(bgav_aac_frame_t * f,
                         uint8_t * data, int data_len,
                         int * bytes_used, int * samples)
  {
  float * frame;
  
  if(!f->dec)
    {
    f->dec = faacDecOpen();
    faacDecInit(f->dec, data,
                data_len,
                &f->samplerate, &f->channels);
    }

#ifdef DUMP_DECODE
    bgav_dprintf("faacDecDecode %d bytes\n", data_len);
    gavl_hexdump(data, (data_len > 16) ? 16 : data_len, 16);
#endif
  
  memset(&f->frame_info, 0, sizeof(f->frame_info));
  frame = faacDecDecode(f->dec, &f->frame_info, data, data_len);

  if(!frame)
    {
    if(f->frame_info.error == 14) /* Too little data */
      return 0;

    else
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "faacDecDecode failed %s",
               faacDecGetErrorMessage(f->frame_info.error));
      return -1;
      }
    }
  
  if(bytes_used)
    *bytes_used = f->frame_info.bytesconsumed;
  if(samples)
    *samples = f->frame_info.samples / f->channels;
  
  //  fprintf(stderr, "AAC frame: %d %d\n",
  //          *bytes_used, *samples);
  
  return 1;
  }

void bgav_aac_frame_get_audio_format(bgav_aac_frame_t * frame,
                                     gavl_audio_format_t * format)
  {
  format->samplerate = frame->samplerate;
  format->num_channels = frame->channels;
  bgav_faad_set_channel_setup(&frame->frame_info,
                              format);
  format->samples_per_frame = frame->frame_info.samples / frame->channels;
  }

static const struct
  {
  int faad_channel;
  gavl_channel_id_t gavl_channel;
  }
channels[] =
  {
    { FRONT_CHANNEL_CENTER,  GAVL_CHID_FRONT_CENTER },
    { FRONT_CHANNEL_LEFT,    GAVL_CHID_FRONT_LEFT },
    { FRONT_CHANNEL_RIGHT,   GAVL_CHID_FRONT_RIGHT },
    { SIDE_CHANNEL_LEFT,     GAVL_CHID_SIDE_LEFT },
    { SIDE_CHANNEL_RIGHT,    GAVL_CHID_SIDE_RIGHT },
    { BACK_CHANNEL_LEFT,     GAVL_CHID_REAR_LEFT },
    { BACK_CHANNEL_RIGHT,    GAVL_CHID_REAR_RIGHT },
    { BACK_CHANNEL_CENTER,   GAVL_CHID_REAR_CENTER },
    { LFE_CHANNEL,           GAVL_CHID_LFE },
    { UNKNOWN_CHANNEL,       GAVL_CHID_NONE }
  };

static gavl_channel_id_t get_channel(int ch)
  {
  int i;
  for(i = 0; i < sizeof(channels)/sizeof(channels[0]); i++)
    {
    if(channels[i].faad_channel == ch)
      return channels[i].gavl_channel;
    }
  return GAVL_CHID_AUX;
  }

void bgav_faad_set_channel_setup(faacDecFrameInfo * frame_info,
                                 gavl_audio_format_t * format)
  {
  int i;

  /* This method fails for HE-AAC streams. So we
     use our own detection for mono and stereo */
  if(format->num_channels > 2)
    {
    for(i = 0; i < format->num_channels; i++)
      format->channel_locations[i] =
        get_channel(frame_info->channel_position[i]);
    }
  else
    gavl_set_channel_setup(format);
  
  }
