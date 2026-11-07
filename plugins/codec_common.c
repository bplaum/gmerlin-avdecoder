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
#include <gmerlin/parameter.h>

#include <gmerlin/translation.h>
#include <gmerlin/utils.h>

#include <avdec.h>

#include "codec_common.h"

static int handle_cmd(void * data, gavl_msg_t * msg)
  {
  bg_avdec_codec_t * c = data;

  switch(msg->NS)
    {
    case GAVL_MSG_NS_SRC:
      switch(msg->ID)
        {
        case GAVL_CMD_SRC_SET_BUFFER_FORMATS:
          {
          gavl_stream_type_t t = gavl_msg_get_arg_int(msg, 0);
          if(t == GAVL_STREAM_VIDEO)
            {
            gavl_dictionary_set(c->opt, BGAV_OPT_VIDEOBUFFER, gavl_msg_get_arg_c(msg, 1));

            //            fprintf(stderr, "Got video stream options:\n");
            //            gavl_value_dump(gavl_dictionary_get(c->opt, BGAV_OPT_VIDEOBUFFER), 2);
            
            }
          else if(t == GAVL_STREAM_AUDIO)
            {
            gavl_dictionary_set(c->opt, BGAV_OPT_AUDIOBUFFER, gavl_msg_get_arg_c(msg, 1));

//            fprintf(stderr, "Got audio stream options:\n");
//            gavl_value_dump(gavl_dictionary_get(c->opt, BGAV_OPT_AUDIOBUFFER), 2);
            }
          }
        }
      break;
    }
  return 1;
  }

bg_controllable_t * bg_avdec_codec_get_controllable(void * data)
  {
  bg_avdec_codec_t * c = data;
  return &c->ctrl;
  }


void * bg_avdec_codec_create()
  {
  bg_avdec_codec_t * ret = calloc(1, sizeof(*ret));
  ret->dec = bgav_stream_decoder_create();
  ret->opt = bgav_stream_decoder_get_options(ret->dec);

  bg_controllable_init(&ret->ctrl,
                       bg_msg_sink_create(handle_cmd, ret, 1),
                       bg_msg_hub_create(1));
  
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

  bg_controllable_cleanup(&c->ctrl);
  
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
