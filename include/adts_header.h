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

/* The following struct is not exactly the same as in the spec */

#define ADTS_HEADER_LEN 9

typedef struct
  {
  int mpeg_version;
  int profile;
  int samplerate_index;
  
  int channel_configuration;
  int frame_bytes;
  int num_blocks;

  int protection_absent;
  
  /* Secondary */
  int samplerate;
  } bgav_adts_header_t;

int bgav_adts_header_read(const uint8_t * data, bgav_adts_header_t * adts);
void bgav_adts_header_dump(const bgav_adts_header_t * adts);
void bgav_adts_header_get_format(const bgav_adts_header_t * adts, gavl_audio_format_t * format);
