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

void bgav_options_set_connect_timeout(bgav_options_t *b, int timeout)
  {
  b->connect_timeout = timeout;
  }

void bgav_options_set_read_timeout(bgav_options_t *b, int timeout)
  {
  b->read_timeout = timeout;
  }

/*
 *  Set network bandwidth (in bits per second)
 */

void bgav_options_set_network_bandwidth(bgav_options_t *b, int bandwidth)
  {
  b->network_bandwidth = bandwidth;
  }


void bgav_options_set_http_use_proxy(bgav_options_t*b, int use_proxy)
  {
  }

void bgav_options_set_http_proxy_host(bgav_options_t*b, const char * h)
  {
  }

void bgav_options_set_http_proxy_port(bgav_options_t*b, int p)
  {
  }

void bgav_options_set_rtp_port_base(bgav_options_t*b, int p)
  {
  b->rtp_port_base = p;
  }

void bgav_options_set_rtp_try_tcp(bgav_options_t*b, int p)
  {
  b->rtp_try_tcp = p;
  }

void bgav_options_set_sample_accurate(bgav_options_t*b, int p)
  {
  b->sample_accurate = p;
  }

void bgav_options_set_cache_time(bgav_options_t*opt, int t)
  {
  opt->cache_time = t;
  }

void bgav_options_set_cache_size(bgav_options_t*opt, int s)
  {
  opt->cache_size = s;
  }

void bgav_options_set_http_proxy_auth(bgav_options_t*b, int i)
  {
  }

void bgav_options_set_http_proxy_user(bgav_options_t*b, const char * h)
  {
  }

void bgav_options_set_http_proxy_pass(bgav_options_t*b, const char * h)
  {
  }


void bgav_options_set_http_shoutcast_metadata(bgav_options_t*b, int m)
  {
  
  }

void bgav_options_set_ftp_anonymous_password(bgav_options_t*b, const char * h)
  {

  }

void bgav_options_set_ftp_anonymous(bgav_options_t*b, int anonymous)
  {
  
  }


void bgav_options_set_audio_dynrange(bgav_options_t* opt, int audio_dynrange)
  {
  opt->audio_dynrange = audio_dynrange;
  }



void bgav_options_set_default_subtitle_encoding(bgav_options_t* b,
                                                const char* encoding)
  {
  b->default_subtitle_encoding = gavl_strrep(b->default_subtitle_encoding, encoding);
  }

void bgav_options_set_seek_subtitles(bgav_options_t* opt,
                                     int seek_subtitles)
  {
  }

void bgav_options_set_pp_level(bgav_options_t* opt,
                               int pp_level)
  {
  opt->pp_level = pp_level;
  if(opt->pp_level < 0)
    opt->pp_level = 0;
  if(opt->pp_level > 6)
    opt->pp_level = 6;
  }

BGAV_PUBLIC
void bgav_options_set_postprocessing_level(bgav_options_t* opt,
                                           float pp_level)
  {
  opt->pp_level = pp_level;
  if(opt->pp_level < 0.0)
    opt->pp_level = 0.0;
  if(opt->pp_level > 1.0)
    opt->pp_level = 1.0;
  }

void bgav_options_set_dvb_channels_file(bgav_options_t* opt,
                                        const char * file)
  {
  }

void bgav_options_set_prefer_ffmpeg_demuxers(bgav_options_t* opt,
                                             int prefer)
  {
  opt->prefer_ffmpeg_demuxers = prefer;
  }

void bgav_options_set_dv_datetime(bgav_options_t* opt,
                                  int datetime)
  {
  opt->dv_datetime = datetime;
  }

void bgav_options_set_shrink(bgav_options_t* opt,
                             int shrink)
  {
  opt->shrink = shrink;
  }

void bgav_options_set_vdpau(bgav_options_t* opt,
                            int vdpau)
  {
  /* NOOP */
  }

void bgav_options_set_vaapi(bgav_options_t* opt,
                            int vaapi)
  {
  opt->vaapi = vaapi;
  }

void bgav_options_set_threads(bgav_options_t * opt, int threads)
  {
  /* NOOP */
  }

void bgav_options_set_dump_headers(bgav_options_t* opt,
                                   int enable)
  {
  opt->dump_headers = enable;
  }

void bgav_options_set_dump_indices(bgav_options_t* opt,
                                   int enable)
  {
  opt->dump_indices = enable;
  }

void bgav_options_set_dump_packets(bgav_options_t* opt,
                                   int enable)
  {
  opt->dump_packets = enable;
  }


#define FREE(ptr) if(ptr) free(ptr)

void bgav_options_free(bgav_options_t*opt)
  {
  FREE(opt->default_subtitle_encoding);
  }

void bgav_options_set_defaults(bgav_options_t * b)
  {
  memset(b, 0, sizeof(*b));
  b->connect_timeout = 10000;
  b->read_timeout = 10000;
  b->default_subtitle_encoding = gavl_strrep(b->default_subtitle_encoding, "LATIN1");
  b->audio_dynrange = 1;
  b->cache_time = 500;
  b->cache_size = 20;

  b->vaapi = 1;

  b->log_level =
    GAVL_LOG_INFO | \
    GAVL_LOG_ERROR | \
    GAVL_LOG_WARNING;

  //  b->sample_accurate = 1;
  
  
  // Test
  // b->dump_packets = 1;
  }

bgav_options_t * bgav_options_create()
  {
  bgav_options_t * ret;
  bgav_translation_init();
  ret = calloc(1, sizeof(*ret));
  bgav_options_set_defaults(ret);
  return ret;
  }

void bgav_options_destroy(bgav_options_t * opt)
  {
  bgav_options_free(opt);
  free(opt);
  }

#define CP_INT(i) dst->i = src->i
#define CP_FLOAT(i) dst->i = src->i

#define CP_STR(s) if(dst->s) free(dst->s); dst->s = gavl_strdup(src->s)

void bgav_options_copy(bgav_options_t * dst, const bgav_options_t * src)
  {
  CP_INT(sample_accurate);
  CP_INT(cache_time);
  CP_INT(cache_size);
  /* Generic network options */
  CP_INT(connect_timeout);
  CP_INT(read_timeout);
  CP_INT(hwctx_video);

  CP_INT(network_bandwidth);
  
  CP_INT(rtp_try_tcp);
  CP_INT(rtp_port_base);
  
  /* Subtitle */
  
  CP_STR(default_subtitle_encoding);
  /* Postprocessing */
  
  CP_FLOAT(pp_level);
  
  /* DVD */

  
  /* Audio */

  CP_INT(audio_dynrange);
  
  CP_INT(prefer_ffmpeg_demuxers);
  CP_INT(dv_datetime);
  CP_INT(shrink);

  CP_INT(vaapi);
  CP_INT(dump_headers);
  CP_INT(dump_indices);
  CP_INT(dump_packets);
  
  /* Callbacks */
  
  CP_INT(log_level);
  
  CP_INT(metadata_callback);
  CP_INT(metadata_callback_data);

  CP_INT(buffer_callback);
  CP_INT(buffer_callback_data);

  CP_INT(user_pass_callback);
  CP_INT(user_pass_callback_data);

  CP_INT(aspect_callback);
  CP_INT(aspect_callback_data);

  CP_INT(index_callback);
  CP_INT(index_callback_data);

  }

#undef CP_INT
#undef CP_STR


void
bgav_options_set_metadata_change_callback(bgav_options_t * opt,
                                          bgav_metadata_change_callback callback,
                                          void * data)
  {
  opt->metadata_callback      = callback;
  opt->metadata_callback_data = data;
  }

void
bgav_options_set_user_pass_callback(bgav_options_t * opt,
                                    bgav_user_pass_callback callback,
                                    void * data)
  {
  opt->user_pass_callback      = callback;
  opt->user_pass_callback_data = data;
  }

void
bgav_options_set_buffer_callback(bgav_options_t * opt,
                         bgav_buffer_callback callback,
                         void * data)
  {
  opt->buffer_callback      = callback;
  opt->buffer_callback_data = data;
  }


void
bgav_options_set_aspect_callback(bgav_options_t * opt,
                              bgav_aspect_callback callback,
                              void * data)
  {
  opt->aspect_callback      = callback;
  opt->aspect_callback_data = data;
  }

void
bgav_options_set_index_callback(bgav_options_t * opt,
                                bgav_index_callback callback,
                                void * data)
  {
  opt->index_callback      = callback;
  opt->index_callback_data = data;
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


void bgav_options_set_video_hw_context(bgav_options_t * opt, gavl_hw_context_t * hwctx)
  {
  opt->hwctx_video = hwctx;
  }
