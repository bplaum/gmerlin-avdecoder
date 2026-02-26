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


void
bg_avdec_option_set_parameter(bgav_options_t * opt, const char * name,
                              const gavl_value_t * val);
