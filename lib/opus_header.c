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
#include <opus_header.h>
#include <vorbis_comment.h>

/*

typedef struct
  {
  uint8_t  version;
  uint8_t  channel_count;
  uint16_t pre_skip;
  uint32_t samplerate; // Bogus number!
  int16_t  output_gain;
  uint8_t  channel_mapping;
  
  struct
    {
    uint8_t stream_count;
    uint8_t coupled_count;

    uint8_t map[256]; // 255 actually
    
    } chtab; // Channel mapping table
  } bgav_opus_header_t;

*/

int bgav_opus_header_read(bgav_input_context_t * input,
                          bgav_opus_header_t * ret)
  {
  bgav_input_skip(input, 8); /* Signature */
  
  if(!bgav_input_read_data(input, &ret->version, 1) ||
     !bgav_input_read_data(input, &ret->channel_count, 1) ||
     !bgav_input_read_16_le(input, &ret->pre_skip) ||
     !bgav_input_read_32_le(input, &ret->samplerate) ||
     !bgav_input_read_16_le(input, (uint16_t*)&ret->output_gain) ||
     !bgav_input_read_data(input, &ret->channel_mapping, 1))
    return 0;

  if(ret->channel_mapping != 0)
    {
    if(!bgav_input_read_data(input, &ret->chtab.stream_count, 1) ||
       !bgav_input_read_data(input, &ret->chtab.coupled_count, 1))
      return 0;

    if(bgav_input_read_data(input, ret->chtab.map, ret->channel_count) <
       ret->channel_count)
      return 0;
    }
  else
    {
    /* Set defaults if they are not coded */
    ret->chtab.stream_count = 1;
    ret->chtab.coupled_count = ret->channel_count-1;
    ret->chtab.map[0] = 0;
    if(ret->channel_count == 2)
      ret->chtab.map[1] = 1;
    }
  
  return 1;
  }

void bgav_opus_header_dump(const bgav_opus_header_t * h)
  {
  bgav_dprintf("Opus header\n");
  bgav_dprintf("  Version:         %d\n", h->version);
  bgav_dprintf("  Channel count:   %d\n", h->channel_count);
  bgav_dprintf("  Pre Skip:        %d\n", h->pre_skip);
  bgav_dprintf("  Samplerate:      %d\n", h->samplerate);
  bgav_dprintf("  Output Gain:     %d\n", h->output_gain);
  bgav_dprintf("  Channel Mapping: %d\n", h->channel_mapping);
  
  if(h->channel_mapping != 0)
    {
    int i;
    bgav_dprintf("  Channel Mapping Table\n");
    bgav_dprintf("    Stream Count:  %d\n", h->chtab.stream_count);
    bgav_dprintf("    Coupled Count: %d\n", h->chtab.coupled_count);

    bgav_dprintf("    Map\n");
    
    for(i = 0; i < h->channel_count; i++)
      {
      bgav_dprintf("      Index %d: %d\n", i+1, h->chtab.map[i]);
      }
    }
  }

void bgav_opus_set_channel_setup(const bgav_opus_header_t * h,
                                 gavl_audio_format_t * fmt)
  {
  switch(h->channel_mapping)
    {
    case 0: // Simple
      gavl_set_channel_setup(fmt);
      break;
    case 1: // Vorbis
      bgav_vorbis_set_channel_setup(fmt);
      break;
    default: // treat as 255
      {
      int i;
      for(i = 0; i < fmt->num_channels; i++)
        fmt->channel_locations[i] = GAVL_CHID_AUX;
      }
    }
  }
