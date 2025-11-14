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



#include "options.h"

typedef struct
  {
  bgav_stream_decoder_t * dec;
  gavl_codec_id_t * compressions;
  bgav_options_t * opt;
  uint32_t * codec_tags;
  } bg_avdec_codec_t;

void * bg_avdec_codec_create(void);
void bg_avdec_codec_destroy(void *);
void bg_avdec_codec_reset(void*);
int64_t bg_avdec_codec_skip(void*, int64_t t, int scale);

void bg_avdec_codec_set_parameter(void *, const char * name, const gavl_value_t * val);

