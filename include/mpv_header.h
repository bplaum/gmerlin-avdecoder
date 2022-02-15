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

#define MPV_PROBE_SIZE 5 /* Sync code + extension type */

#define MPEG_CODE_SEQUENCE             1
#define MPEG_CODE_SEQUENCE_EXT         2
#define MPEG_CODE_PICTURE              3
#define MPEG_CODE_PICTURE_EXT          4
#define MPEG_CODE_GOP                  5
#define MPEG_CODE_SLICE                6
#define MPEG_CODE_END                  7
#define MPEG_CODE_SEQUENCE_DISPLAY_EXT 8
#define MPEG_CODE_EXTENSION            9

#define MPEG_PICTURE_TOP_FIELD    1
#define MPEG_PICTURE_BOTTOM_FIELD 2
#define MPEG_PICTURE_FRAME        3

const uint8_t * bgav_mpv_find_startcode( const uint8_t *p,
                                         const uint8_t *end );

int bgav_mpv_get_start_code(const uint8_t * data, int get_ext);


void bgav_mpv_get_framerate(int code, uint32_t * timescale, uint32_t *frame_duration);

/*
 *  Return values of the parse functions:
 *   0: Not enough data
 *  -1: Parse error
 *   1: Success
 */

typedef struct
  {
  int profile_level_id;
  
  int progressive_sequence;
  int chroma_format;
  
  int horizontal_size_ext;
  int vertical_size_ext;
  
  uint32_t bitrate_ext;
  int timescale_ext;
  int frame_duration_ext;
  int low_delay;
  int vbv_buffer_size_ext;
  } bgav_mpv_sequence_extension_t;

typedef struct
  {
  int video_format;
  int has_color_description;

  int color_primaries;
  int transfer_characteristics;
  int matrix_coefficients;

  int display_width;
  int display_height;
  
  } bgav_mpv_sequence_display_extension_t;

typedef struct
  {
  int mpeg2;
  int has_dpy_ext;
  
  uint32_t bitrate; /* In 400 bits per second */

  int horizontal_size_value;
  int vertical_size_value;
  int aspect_ratio;
  int frame_rate_index;

  int vbv_buffer_size_value;
  
  bgav_mpv_sequence_extension_t ext;
  bgav_mpv_sequence_display_extension_t dpy_ext;
  } bgav_mpv_sequence_header_t;

int bgav_mpv_sequence_header_parse(const bgav_options_t * opt,
                                   bgav_mpv_sequence_header_t *,
                                   const uint8_t * buffer, int len);

int bgav_mpv_sequence_display_extension_parse(const bgav_options_t * opt,
                                              bgav_mpv_sequence_display_extension_t *,
                                              const uint8_t * buffer, int len);

int bgav_mpv_sequence_extension_parse(const bgav_options_t * opt,
                                      bgav_mpv_sequence_extension_t *,
                                      const uint8_t * buffer, int len);

typedef struct
  {
  int top_field_first;
  int repeat_first_field;
  int progressive_frame;
  int picture_structure;
  } bgav_mpv_picture_extension_t;

typedef struct
  {
  int coding_type;
  bgav_mpv_picture_extension_t ext;
  } bgav_mpv_picture_header_t;

int bgav_mpv_picture_header_parse(const bgav_options_t * opt,
                                  bgav_mpv_picture_header_t *,
                                  const uint8_t * buffer, int len);



int bgav_mpv_picture_extension_parse(const bgav_options_t * opt,
                                     bgav_mpv_picture_extension_t *,
                                     const uint8_t * buffer, int len);

typedef struct
  {
  int drop;
  int hours;
  int minutes;
  int seconds;
  int frames;
  } bgav_mpv_gop_header_t;

int bgav_mpv_gop_header_parse(const bgav_options_t * opt,
                              bgav_mpv_gop_header_t *,
                              const uint8_t * buffer, int len);

/* Getting the pixel aspect ratio is a bit messy... */

void bgav_mpv_get_pixel_aspect(bgav_mpv_sequence_header_t * h,
                               gavl_video_format_t * ret);

gavl_pixelformat_t bgav_mpv_get_pixelformat(bgav_mpv_sequence_header_t * h);

void bgav_mpv_get_size(bgav_mpv_sequence_header_t * h,
                       gavl_video_format_t * ret);
