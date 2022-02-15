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

/* minimum number of bytes to read from a DV stream in order to
   determine the profile */
#define DV_HEADER_SIZE (6*80) /* 6 DIF blocks */

/*
 *  Handler for DV frames
 *  It detects most interesting parameters from the DV
 *  data and can extract audio.
 */

typedef struct bgav_dv_dec_s bgav_dv_dec_t;

bgav_dv_dec_t * bgav_dv_dec_create();
void bgav_dv_dec_destroy(bgav_dv_dec_t*);

/* Sets the header for parsing. Data must be DV_HEADER_SIZE bytes long */

void bgav_dv_dec_set_header(bgav_dv_dec_t*, uint8_t * data);

int bgav_dv_dec_get_frame_size(bgav_dv_dec_t*);

/* Call this after seeking */
void bgav_dv_dec_set_frame_counter(bgav_dv_dec_t*, int64_t frames);

/* Call this after seeking */
void bgav_dv_dec_set_sample_counter(bgav_dv_dec_t*, int64_t samples);

/* Sets the frame for parsing. data must be frame_size bytes long */

void bgav_dv_dec_set_frame(bgav_dv_dec_t*, uint8_t * data);

/* ffmpeg is not able to tell the right pixel aspect ratio for DV streams */

void bgav_dv_dec_get_pixel_aspect(bgav_dv_dec_t*, uint32_t * pixel_width,
                                  uint32_t * pixel_height);

void bgav_dv_dec_get_image_size(bgav_dv_dec_t*, uint32_t * width,
                                uint32_t * height);

void bgav_dv_dec_get_video_format(bgav_dv_dec_t * d, gavl_video_format_t * fmt);

gavl_pixelformat_t bgav_dv_dec_get_pixelformat(bgav_dv_dec_t*);


void bgav_dv_dec_get_timecode_format(bgav_dv_dec_t * d,
                                     gavl_timecode_format_t * tf,
                                     const bgav_options_t * opt);


/* Set up audio and video streams */

void bgav_dv_dec_init_audio(bgav_dv_dec_t*, bgav_stream_t * s);
void bgav_dv_dec_init_video(bgav_dv_dec_t*, bgav_stream_t * s);

/* Extract audio and video packets suitable for the decoders */

int bgav_dv_dec_get_audio_packet(bgav_dv_dec_t*, bgav_packet_t * p);
void bgav_dv_dec_get_video_packet(bgav_dv_dec_t*, bgav_packet_t * p);

int bgav_dv_dec_get_date(bgav_dv_dec_t * d,
                         int * year, int * month, int * day);

int bgav_dv_dec_get_time(bgav_dv_dec_t * d,
                         int * hour, int * minute, int * second);

int bgav_dv_dec_get_timecode(bgav_dv_dec_t * d,
                             gavl_timecode_t * tc);


