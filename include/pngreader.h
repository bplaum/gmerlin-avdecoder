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



#ifndef BGAV_PNGREADER_H_INCLUDED
#define BGAV_PNGREADER_H_INCLUDED

typedef struct bgav_png_reader_s bgav_png_reader_t;

bgav_png_reader_t * bgav_png_reader_create(void);
void bgav_png_reader_destroy(bgav_png_reader_t* png);


void bgav_png_reader_reset(bgav_png_reader_t * png);

int bgav_png_reader_read_image(bgav_png_reader_t * png,
                               gavl_video_frame_t * frame);

int bgav_png_reader_read_header(bgav_png_reader_t * png,
                                uint8_t * buffer, int buffer_size,
                                gavl_video_format_t * format);


#endif // GAV_PNGREADER_H_INCLUDED

