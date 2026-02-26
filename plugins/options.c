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



#include <string.h>
#include <stdlib.h>

#include <gmerlin/parameter.h>
#include <avdec.h>
#include "options.h"

void
bg_avdec_option_set_parameter(bgav_options_t * opt, const char * name,
                              const gavl_value_t * val)
  {
  if(!name)
    return;
  if(!strcmp(name, "default_subtitle_encoding"))
    {
    bgav_options_set_default_subtitle_encoding(opt, val->v.str);
    }
  else if(!strcmp(name, "audio_dynrange"))
    {
    bgav_options_set_audio_dynrange(opt, val->v.i);
    }
  else if(!strcmp(name, "sample_accuracy"))
    {
    if(!strcmp(val->v.str, "never"))
      bgav_options_set_sample_accurate(opt, 0);
    else if(!strcmp(val->v.str, "always"))
      bgav_options_set_sample_accurate(opt, 1);
    else if(!strcmp(val->v.str, "when_necessary"))
      bgav_options_set_sample_accurate(opt, 2);
    }
  }
