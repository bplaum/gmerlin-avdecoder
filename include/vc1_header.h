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

#define PROFILE_SIMPLE   0
#define PROFILE_MAIN     1
#define PROFILE_COMPLEX  2
#define PROFILE_ADVANCED 3

#define VC1_CODE_SEQUENCE    0x0f
#define VC1_CODE_ENTRY_POINT 0x0e
#define VC1_CODE_PICTURE     0x0d

int bgav_vc1_unescape_buffer(const uint8_t *src, int size, uint8_t *dst);

typedef struct
  {
  int profile;

  union
    {
    struct
      {
      int level;
      int chromaformat;
      int frmrtq_postproc;
      int bitrtq_postproc;
      int postprocflag;
      int width;
      int height;
      int broadcast;
      int interlace;
      int tfcntrflag;
      int finterpflag;
      int reserved;
      int psf;
      int display_info_flag;
      int display_width;
      int display_height;
      int aspect_ratio_flag;
      int aspect_ratio_code;
      int pixel_width;
      int pixel_height;
      int framerate_flag;
      int timescale;
      int frame_duration;
      } adv;
    } h;
  
  } bgav_vc1_sequence_header_t;

int bgav_vc1_sequence_header_read(const bgav_options_t * opt,
                                  bgav_vc1_sequence_header_t * ret,
                                  const uint8_t * buffer, int len);

void bgav_vc1_sequence_header_dump(const bgav_vc1_sequence_header_t * h);

typedef struct
  {
  int fcm;
  int coding_type;
  } bgav_vc1_picture_header_adv_t;

int bgav_vc1_picture_header_adv_read(const bgav_options_t * opt,
                                     bgav_vc1_picture_header_adv_t * ret,
                                     const uint8_t * buffer, int len,
                                     const bgav_vc1_sequence_header_t * seq);

void bgav_vc1_picture_header_adv_dump(bgav_vc1_picture_header_adv_t * ret);
