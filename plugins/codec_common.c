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
#include <gmerlin/parameter.h>

#include <gmerlin/translation.h>
#include <gmerlin/utils.h>

#include <avdec.h>

#include "codec_common.h"


void * bg_avdec_codec_create()
  {
  bg_avdec_codec_t * ret = calloc(1, sizeof(*ret));
  ret->dec = bgav_stream_decoder_create();
  ret->opt = bgav_stream_decoder_get_options(ret->dec);
  return ret;
  }

void bg_avdec_codec_destroy(void * priv)
  {
  bg_avdec_codec_t * c = priv;
  if(c->dec)
    bgav_stream_decoder_destroy(c->dec);
  if(c->compressions)
    free(c->compressions);
  if(c->codec_tags)
    free(c->codec_tags);

  free(c);
  }

  
void bg_avdec_codec_reset(void * priv)
  {
  bg_avdec_codec_t * c = priv;
  bgav_stream_decoder_reset(c->dec);
  }

void bg_avdec_codec_set_parameter(void * priv, const char * name,
                                  const gavl_value_t * val)
  {
  bg_avdec_codec_t * c = priv;
  bg_avdec_option_set_parameter(c->opt, name, val);
  }

int64_t bg_avdec_codec_skip(void * priv, int64_t t, int scale)
  {
  bg_avdec_codec_t * c = priv;
  return bgav_stream_decoder_skip(c->dec, t, scale);
  }
