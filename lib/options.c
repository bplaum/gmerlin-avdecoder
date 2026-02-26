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
#include <string.h>

#include <avdec_private.h>
#include <gavl/state.h>
#include <gavl/msg.h>

/* Configuration stuff */

void bgav_options_set_sample_accurate(bgav_options_t*b, int p)
  {
  gavl_dictionary_set_int(b, BGAV_OPT_SAMPLE_ACCURATE, p);
  }

void bgav_options_set_audio_dynrange(bgav_options_t* b, int audio_dynrange)
  {
  gavl_dictionary_set_int(b, BGAV_OPT_DYNRANGE, audio_dynrange);
  }

void bgav_options_set_default_subtitle_encoding(bgav_options_t* b,
                                                const char* encoding)
  {
  gavl_dictionary_set_string(b, BGAV_OPT_DEFAULT_SUBTITLE_ENCODING,
                             encoding);
  }

void bgav_options_set_dump_headers(bgav_options_t* opt,
                                   int enable)
  {
  gavl_dictionary_set_int(opt, BGAV_OPT_DUMP_HEADERS, enable);
  }

void bgav_options_set_dump_indices(bgav_options_t* opt,
                                   int enable)
  {
  gavl_dictionary_set_int(opt, BGAV_OPT_DUMP_INDICES, enable);
  }

void bgav_options_set_dump_packets(bgav_options_t* opt,
                                   int enable)
  {
  gavl_dictionary_set_int(opt, BGAV_OPT_DUMP_PACKETS, enable);
  }

int bgav_options_get_bool(const bgav_options_t*opt, const char * key)
  {
  int val_i = 0;
  if(gavl_dictionary_get_int(opt, key, &val_i) && val_i)
    return 1;
  else
    return 0;
  }
  



#define FREE(ptr) if(ptr) free(ptr)

void bgav_options_free(bgav_options_t*opt)
  {
  gavl_dictionary_reset(opt);
  }

void bgav_options_set_defaults(bgav_options_t * b)
  {
  gavl_dictionary_set_int(b, BGAV_OPT_DYNRANGE, 1);
  gavl_dictionary_set_string(b, BGAV_OPT_DEFAULT_SUBTITLE_ENCODING, "LATIN1");
  
  }

bgav_options_t * bgav_options_create(void)
  {
  bgav_options_t * ret;
  bgav_translation_init();
  ret = gavl_dictionary_create();
  bgav_options_set_defaults(ret);
  return ret;
  }

void
bgav_set_msg_callback_by_id(bgav_t * bgav,
                            int id,
                            gavl_handle_msg_func callback,
                            void * data)
  {
  bgav_stream_t * s;

  if((s = bgav_track_get_msg_stream_by_id(bgav->tt->cur, id)))
    {
    s->data.msg.msg_callback = callback;
    s->data.msg.msg_callback_data = data;
    }
  
  // opt->msg_callback      = callback;
  // opt->msg_callback_data = data;
  
  }

static void state_changed(bgav_t * b,
                          const char * var,
                          gavl_value_t * val)
  {
  bgav_stream_t * s;

  /* Store locally */
  gavl_dictionary_set(&b->state, var, val);

  //  fprintf(stderr, "state changed 1\n");
  
  /* End here if we are not running yet */
  if(!(b->flags && BGAV_FLAG_IS_RUNNING))
    return;

  if((s = bgav_track_get_msg_stream_by_id(b->tt->cur, GAVL_META_STREAM_ID_MSG_PROGRAM)) &&
     s->data.msg.msg_callback)
    {
    gavl_msg_t msg;

    gavl_msg_init(&msg);
    gavl_msg_set_state_nocopy(&msg, GAVL_MSG_STATE_CHANGED, 1, GAVL_STATE_CTX_SRC, var, val);
    s->data.msg.msg_callback(s->data.msg.msg_callback_data, &msg);
    gavl_msg_free(&msg);
    }
  
  }


void bgav_send_msg(bgav_t * b, gavl_msg_t * msg)
  {
  bgav_stream_t * s;

  if(!b->tt)
    return;

  if((s = bgav_track_get_msg_stream_by_id(b->tt->cur, GAVL_META_STREAM_ID_MSG_PROGRAM)) &&
     s->data.msg.msg_callback)
    {
    s->data.msg.msg_callback(s->data.msg.msg_callback_data, msg);
    }
  }


void bgav_metadata_changed(bgav_t * b,
                           const gavl_dictionary_t * new_metadata)
  {
  gavl_value_t val;
  gavl_dictionary_t * dict;
  gavl_value_init(&val);

  dict = gavl_value_set_dictionary(&val);  
  gavl_dictionary_copy(dict, new_metadata);

  state_changed(b, GAVL_STATE_SRC_METADATA, &val);
  }

void bgav_seek_window_changed(bgav_t * b,
                              gavl_time_t start, gavl_time_t end)
  {
  gavl_value_t val;
  gavl_dictionary_t * dict;
  gavl_value_init(&val);

  dict = gavl_value_set_dictionary(&val);  
  gavl_dictionary_set_long(dict, GAVL_STATE_SRC_SEEK_WINDOW_START, start);
  gavl_dictionary_set_long(dict, GAVL_STATE_SRC_SEEK_WINDOW_END, end);
  state_changed(b, GAVL_STATE_SRC_SEEK_WINDOW, &val);
  }


