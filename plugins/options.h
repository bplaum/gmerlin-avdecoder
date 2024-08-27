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



/* Commonly used parameters */

#define PARAM_DYNRANGE \
  {                    \
  .name = "audio_dynrange",    \
  .long_name = TRS("Dynamic range control"),         \
  .type = BG_PARAMETER_CHECKBUTTON,           \
  .val_default = GAVL_VALUE_INIT_INT(1),              \
  .help_string = TRS("Enable dynamic range control for codecs, which support this (currently only A52 and DTS).") \
  }

#define PARAM_PP_LEVEL \
  {                    \
  .name = "video_postprocessing_level",    \
  .long_name = TRS("Postprocessing level"),         \
  .type = BG_PARAMETER_SLIDER_FLOAT,           \
  .val_default = GAVL_VALUE_INIT_FLOAT(0.2),              \
  .val_min =     GAVL_VALUE_INIT_FLOAT(0.0),              \
  .val_max = GAVL_VALUE_INIT_FLOAT(1.0),              \
  .num_digits = 2,                           \
  .help_string = TRS("Set postprocessing (to remove compression artifacts). 0 means no postprocessing, 1 means maximum postprocessing.") \
  }


#define PARAM_THREADS \
  {                    \
  .name = "threads",    \
  .long_name = TRS("Number of decoding threads"),         \
  .type = BG_PARAMETER_INT,           \
  .val_default = GAVL_VALUE_INIT_INT(1),              \
  .val_min =     GAVL_VALUE_INIT_INT(1),              \
  .val_max = GAVL_VALUE_INIT_INT(1024),              \
  .help_string = TRS("Set the number of threads used by Video codecs") \
  }

#define PARAM_VIDEO_GENERIC \
  PARAM_PP_LEVEL,           \
  PARAM_THREADS,          \
  {                         \
    .name      =  "shrink",                \
    .long_name =  TRS("Shrink factor"),    \
    .type      =  BG_PARAMETER_SLIDER_INT, \
    .val_min     = GAVL_VALUE_INIT_INT(0), \
    .val_max     = GAVL_VALUE_INIT_INT(3), \
    .val_default = GAVL_VALUE_INIT_INT(0), \
    .help_string = TRS("This enables downscaling of images while decoding. Currently only supported for JPEG-2000."), \
  }, \
  {  \
    .name      =  "vaapi",                  \
    .long_name =  TRS("Use vaapi"),         \
    .type      =  BG_PARAMETER_CHECKBUTTON, \
    .help_string = TRS("Use VAAPI"),        \
    .val_default = GAVL_VALUE_INIT_INT(1),          \
  }

void
bg_avdec_option_set_parameter(bgav_options_t * opt, const char * name,
                              const gavl_value_t * val);
