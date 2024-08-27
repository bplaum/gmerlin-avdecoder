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

#include "codec_common.h"

static const gavl_codec_id_t * get_compressions(void * priv)
  {
  bg_avdec_codec_t * c = priv;
  if(!c->compressions)
    c->compressions = bgav_supported_audio_compressions();
  return c->compressions;
  }

static const uint32_t * get_codec_tags(void * priv)
  {
  bg_avdec_codec_t * c = priv;
  if(!c->compressions)
    c->compressions = bgav_supported_audio_compressions();
  return c->compressions;
  }

static gavl_audio_source_t *
connect_decode_audio(void * priv,
                     gavl_packet_source_t * src,
                     gavl_dictionary_t * s)
  {
  bg_avdec_codec_t * c = priv;
  return bgav_stream_decoder_connect_audio(c->dec, src, s);
  }

static const bg_parameter_info_t parameters[] =
  {
    PARAM_DYNRANGE,
    { /* End */ }
  };

static const bg_parameter_info_t * get_parameters(void * priv)
  {
  return parameters;
  }

const bg_codec_plugin_t the_plugin =
  {
    .common =
    {
      BG_LOCALE,
      .name =           "c_avdec_audio",
      .long_name =      TRS("AVDecoder audio decompressor"),
      .description =    TRS("Audio decompressor based on the Gmerlin avdecoder library."),
      .type =           BG_PLUGIN_DECOMPRESSOR_AUDIO,
      .flags =          0,
      .priority =       BG_PLUGIN_PRIORITY_MAX,
      .create =         bg_avdec_codec_create,
      .destroy =        bg_avdec_codec_destroy,
      .get_parameters = get_parameters,
      .set_parameter =  bg_avdec_codec_set_parameter,
    },
    .get_compressions     = get_compressions,
    .get_codec_tags      = get_codec_tags,
    .open_decode_audio = connect_decode_audio,
    .reset                = bg_avdec_codec_reset,
    .skip                 = bg_avdec_codec_skip,
    
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
