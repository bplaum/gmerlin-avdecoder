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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <config.h>
#include <gmerlin/translation.h>

#include <gmerlin/plugin.h>
#include <gmerlin/utils.h>
#include <avdec.h>

#include <cdio/cdio.h> // Version
/* Stuff defined by cdio includes */
#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef VERSION


#include "avdec_common.h"

static int open_vcd(void * priv, const char * location)
  {
  const char * pos;
  bgav_options_t * opt;
  avdec_priv * avdec = priv;
  
  if((pos = strstr(location, "://")))
    location = pos + 3;
  
  avdec->dec = bgav_create();
  opt = bgav_get_options(avdec->dec);
  
  bgav_options_copy(opt, avdec->opt);
  
  if(!bgav_open_vcd(avdec->dec, location))
    return 0;
  return 1;
  }


static char const * const protocols = "vcd";

static const char * get_protocols(void * priv)
  {
  return protocols;
  }

const bg_input_plugin_t the_plugin =
  {
    .common =
    {
      BG_LOCALE,
      .name =          "i_vcd",
      .long_name =     TRS("VCD Player"),
      .description =   TRS("Plugin for playing VCDs. Based on Gmerlin avdecoder."),
      .type =          BG_PLUGIN_INPUT,
      .flags =         0,
      .priority =      BG_PLUGIN_PRIORITY_MAX,
      .create =        bg_avdec_create,
      .destroy =       bg_avdec_destroy,
      //      .get_parameters = get_parameters_vcd,
      //      .set_parameter =  bg_avdec_set_parameter
      .get_controllable = bg_avdec_get_controllable,
    },
    .get_protocols = get_protocols,
    
    /* Open file/device */
    .open = open_vcd,
    //    .set_callbacks = set_callbacks_avdec,
    .get_media_info = bg_avdec_get_media_info,
    
    .skip_video =      bg_avdec_skip_video,
    .get_src = bg_avdec_get_src,
    
    /*
     *  Do percentage seeking (can be NULL)
     */
    /* Stop playback, close all decoders */
    .stop =         NULL,
    .close =        bg_avdec_close,
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
