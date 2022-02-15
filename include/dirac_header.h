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

#define DIRAC_CODE_SEQUENCE 1
#define DIRAC_CODE_PICTURE  2
#define DIRAC_CODE_END      3
#define DIRAC_CODE_ERROR    -1 // Lost sync
#define DIRAC_CODE_OTHER    0 // Something else

/* Bytes needed for gettin the parse info */
#define DIRAC_PARSE_SIZE    9

int bgav_dirac_get_code(uint8_t * data, int len, int * size);

typedef struct
  {
  int version_major;
  int version_minor;
  int profile;
  int level;
  int base_video_format;

  int width;
  int height;

  int timescale;
  int frame_duration;

  int pixel_width;
  int pixel_height;

  int source_sampling; // 1 = interlaced
  int top_first;
  
  /*
   *  If the picture coding mode value is 1 then pictures
   *  shall correspond to fields. If it is 0 then pictures shall
   *  correspond to frames
   */
  
  int picture_coding_mode;
  } bgav_dirac_sequence_header_t;

int bgav_dirac_sequence_header_parse(bgav_dirac_sequence_header_t *,
                                     const uint8_t * buffer, int len);

void bgav_dirac_sequence_header_dump(const bgav_dirac_sequence_header_t *);

typedef struct
  {
  uint32_t pic_num;
  int num_refs;
  } bgav_dirac_picture_header_t;

int bgav_dirac_picture_header_parse(bgav_dirac_picture_header_t *,
                                    const uint8_t * buffer, int len);

void bgav_dirac_picture_header_dump(const bgav_dirac_picture_header_t *);
