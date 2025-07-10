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



#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <config.h>
#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>
#include <gmerlin/utils.h>
#include <avdec.h>

#include "avdec_common.h"

#include <cdio/cdio.h> // Version

static int open_dvd(void * priv, const char * location)
  {
  const char * pos;
  bgav_options_t * opt;
  avdec_priv * avdec = priv;

  if((pos = strstr(location, "://")))
    location = pos + 3;
  
  avdec->dec = bgav_create();
  opt = bgav_get_options(avdec->dec);
  
  bgav_options_copy(opt, avdec->opt);
  
  if(!bgav_open_dvd(avdec->dec, location))
    return 0;
  return 1;
  }


static const bg_parameter_info_t parameters[] =
  {
#if 0
    {
      .name =       "dvd_chapters_as_tracks",
      .long_name =  TRS("Handle chapters as tracks"),
      .type =       BG_PARAMETER_CHECKBUTTON,
      .val_default = GAVL_VALUE_INIT_INT(1),
      .gettext_domain =    PACKAGE,
      .gettext_directory = LOCALE_DIR,
    },
#endif
    PARAM_DYNRANGE,
    { /* End of parameters */ }
  };

static const bg_parameter_info_t * get_parameters_dvd(void * priv)
  {
  return parameters;
  }

static char const * const protocols = "dvd";

static const char * get_protocols(void * priv)
  {
  return protocols;
  }


const bg_input_plugin_t the_plugin =
  {
    .common =
    {
      BG_LOCALE,
      .name =          "i_dvd",
      .long_name =     TRS("DVD Player"),
      .description =   TRS("Plugin for playing DVDs. Based on Gmerlin avdecoder."),
      .type =          BG_PLUGIN_INPUT,
      .flags =         0,
      .priority =      BG_PLUGIN_PRIORITY_MAX,
      .create =        bg_avdec_create,
      .destroy =       bg_avdec_destroy,
      .get_parameters = get_parameters_dvd,
      .set_parameter =  bg_avdec_set_parameter,
      .get_controllable = bg_avdec_get_controllable,
      .get_protocols = get_protocols,
    },
    
    .set_video_hw_context = bg_avdec_set_video_hw_context,
    
    /* Open file/device */
    .open = open_dvd,

    .get_media_info = bg_avdec_get_media_info,

    .get_src = bg_avdec_get_src,

    /*
     *  Do percentage seeking (can be NULL)
     *  Media streams are supposed to be seekable, if this
     *  function is non-NULL AND the duration field of the track info
     *  is > 0
     */
    /* Stop playback, close all decoders */
    .stop =         NULL,
    .close =        bg_avdec_close,
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
