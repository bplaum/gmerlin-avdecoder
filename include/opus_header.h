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

typedef struct
  {
  uint8_t  version;
  uint8_t  channel_count;
  uint16_t pre_skip;
  uint32_t samplerate; /* Bogus number! */
  int16_t  output_gain;
  uint8_t  channel_mapping;
  
  struct
    {
    uint8_t stream_count;
    uint8_t coupled_count;

    uint8_t map[256]; // 255 actually
    
    } chtab; // Channel mapping table
  } bgav_opus_header_t;

int bgav_opus_header_read(bgav_input_context_t * input,
                          bgav_opus_header_t * ret);

void bgav_opus_header_dump(const bgav_opus_header_t * ret);

void bgav_opus_set_channel_setup(const bgav_opus_header_t * h,
                                 gavl_audio_format_t * fmt);
