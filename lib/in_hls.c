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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>


#include <avdec_private.h>
#include <gavl/gavlsocket.h>
#include <gavl/value.h>
#include <gavl/http.h>
#include <gavl/state.h>

#include <id3.h>

#define LOG_DOMAIN "in_hls"

#define SEGMENT_START_TIME_ABS "start_abs"
#define SEGMENT_DURATION        "duration"

#define SEGMENT_CIPHER         "cipher"
#define SEGMENT_CIPHER_KEY_URI "cipherkeyuri"
#define SEGMENT_CIPHER_IV      "cipheriv"

/* States for opening the next source. */

#define NEXT_STATE_START           0
#define NEXT_STATE_READ_M3U        1
#define NEXT_STATE_WAIT_M3U        2
#define NEXT_STATE_READ_CIPHER_KEY 3
#define NEXT_STATE_GOT_TS          4
#define NEXT_STATE_START_OPEN_TS   5
#define NEXT_STATE_OPEN_TS         6
#define NEXT_STATE_DONE            7
#define NEXT_STATE_LOST_SYNC       8


typedef struct
  {
  gavl_timer_t * m3u_timer;
  gavl_time_t m3u_time;
  
  gavf_io_t * m3u_io;
  gavf_io_t * ts_io;
  gavf_io_t * ts_io_next;

  gavf_io_t * cipher_io;
  gavf_io_t * cipher_key_io;
  
  /* ts_io or cipher_io */
  gavf_io_t * io;
  
  gavl_array_t segments;
  
  gavl_buffer_t m3u_buf;
  
  int64_t seq_start; // First sequence number of the array
  int64_t seq_cur;   // Current sequence number
  
  gavl_time_t window_start;
  gavl_time_t window_end;
  
  gavl_buffer_t cipher_key;
  gavl_buffer_t cipher_iv;
  
  gavl_dictionary_t http_vars;

  int next_state;
  int end_of_sequence;

  
  } hls_priv_t;

static int parse_m3u8(bgav_input_context_t * ctx)
  {
  int ret = 0;
  hls_priv_t * p = ctx->priv;
  char ** lines;
  int idx;
  int64_t new_seq = -1;
  int skip;
  gavl_value_t val;
  gavl_dictionary_t * dict;
  gavl_time_t segment_start_time_abs = GAVL_TIME_UNDEFINED;
  gavl_time_t segment_duration = 0;
  int window_changed = 0;

  gavl_dictionary_t cipher_params;

  int num_appended = 0;

  gavl_dictionary_init(&cipher_params);
  
  lines = gavl_strbreak((char*)p->m3u_buf.buf, '\n');

  idx = 0;

  while(lines[idx])
    {
    if(gavl_string_starts_with(lines[idx], "#EXT-X-MEDIA-SEQUENCE:"))
      {
      new_seq = strtoll(lines[idx] + 22, NULL, 10);
      break;
      }
    idx++;
    }

  if(new_seq < 0)
    goto fail;

  if(p->seq_start < 0)
    {
    p->seq_start = new_seq;
    window_changed = 1;
    }
  else
    {
    const gavl_dictionary_t * d;
    int i;
    int num_deleted = new_seq - p->seq_start;
    
    if(num_deleted > 0)
      {
      for(i = 0; i < num_deleted; i++)
        {
        if((d = gavl_value_get_dictionary(&p->segments.entries[i])) &&
           gavl_dictionary_get_long(d, SEGMENT_DURATION, &segment_duration) &&
           (segment_duration > 0))
          p->window_start += segment_duration;
        }
      gavl_array_splice_val(&p->segments, 0, num_deleted, NULL);
      p->seq_start += num_deleted;
      window_changed = 1;
      }
    }
  
  skip = p->segments.num_entries;
  //  fprintf(stderr, "seq_start: %"PRId64" skip: %d\n", p->seq_start, skip);
  
  idx = 0;
  
  gavl_value_init(&val);
  dict = gavl_value_set_dictionary(&val);

  while(skip)
    {
    gavl_strtrim(lines[idx]);

    if((*(lines[idx]) != '\0') &&
       !gavl_string_starts_with(lines[idx], "#"))
      skip--;
    idx++;
    if(!lines[idx])
      break;
    }
  
  while(lines[idx])
    {
    gavl_strtrim(lines[idx]);

    if(*(lines[idx]) == '\0')
      {
      idx++;
      continue;
      }

    /* Skip header lines to suppress fake warnings */
    if(gavl_string_starts_with(lines[idx], "#EXTM3U") ||
       gavl_string_starts_with(lines[idx], "#EXT-X-VERSION") ||
       gavl_string_starts_with(lines[idx], "#EXT-X-MEDIA-SEQUENCE") ||
       gavl_string_starts_with(lines[idx], "#EXT-X-TARGETDURATION"))
      {
      idx++;
      continue;
      }
       
    
    if(gavl_string_starts_with(lines[idx], "#EXT-X-KEY:"))
      {
      const char * start;
      const char * end;

      gavl_dictionary_reset(&cipher_params);
      
      // #EXT-X-KEY:METHOD=AES-128,URI="https://livestreamdirect-three.mediaworks.nz/K110346488-2000.key",IV=0x0000000000000000000000000005497B

      if((start = strstr(lines[idx], "METHOD=")))
        {
        start += strlen("METHOD=");

        if(!(end = strchr(start, ',')))
          end = start + strlen(start);

        gavl_dictionary_set_string_nocopy(&cipher_params, SEGMENT_CIPHER, gavl_strndup(start, end));
        }
      
      if((start = strstr(lines[idx], "URI=\"")))
        {
        
        start += strlen("URI=\"");
        
        if((end = strchr(start, '"')))
          {
          char * tmp_string = gavl_strndup(start, end);
          gavl_dictionary_set_string_nocopy(&cipher_params,
                                            SEGMENT_CIPHER_KEY_URI,
                                            gavl_get_absolute_uri(tmp_string, ctx->location));
          free(tmp_string);
          }
        }

      if((start = strstr(lines[idx], "IV=")))
        {
        start += strlen("IV=");

        if(!(end = strchr(start, ',')))
          end = start + strlen(start);
        
        gavl_dictionary_set_string_nocopy(&cipher_params, SEGMENT_CIPHER_IV, gavl_strndup(start, end));
        }
      }
    else if(gavl_string_starts_with(lines[idx], "#EXT-X-PROGRAM-DATE-TIME:"))
      {

      /* Time in ISO-8601 format */
      // #EXT-X-PROGRAM-DATE-TIME:2022-04-26T21:15:13Z
      
      if(!gavl_time_parse_iso8601(lines[idx] + strlen("#EXT-X-PROGRAM-DATE-TIME:") , &segment_start_time_abs))
        segment_start_time_abs = GAVL_TIME_UNDEFINED;
      }
    else if(gavl_string_starts_with(lines[idx], "#EXTINF:"))
      {
      double duration = strtod(lines[idx] + strlen("#EXTINF:"), NULL);
      segment_duration = gavl_seconds_to_time(duration);
      }
    else if(!gavl_string_starts_with(lines[idx], "#"))
      {
      char * uri;
      
      if(segment_start_time_abs != GAVL_TIME_UNDEFINED)
        gavl_dictionary_set_long(dict, SEGMENT_START_TIME_ABS, segment_start_time_abs);
      if(segment_duration >  0)
        gavl_dictionary_set_long(dict, SEGMENT_DURATION, segment_duration);

      gavl_dictionary_merge2(dict, &cipher_params);

      uri = bgav_input_absolute_url(ctx, lines[idx]);

      uri = gavl_url_append_http_vars(uri, &p->http_vars);

      
      gavl_dictionary_set_string_nocopy(dict, GAVL_META_URI, uri);
      gavl_array_splice_val_nocopy(&p->segments, -1, 0, &val);

      if(segment_duration > 0)
        p->window_end += segment_duration;
      
      
      segment_start_time_abs = GAVL_TIME_UNDEFINED;
      segment_duration = 0;
      
      gavl_value_reset(&val);
      dict = gavl_value_set_dictionary(&val);
      num_appended++;
      }
    else
      {
      fprintf(stderr, "Unknown line %s\n", lines[idx]);
      }
    
    idx++;
    }
  
  gavl_strbreak_free(lines);
  
  ret = 1;
  
  fail:

  if(window_changed)
    {
    //    const gavl_dictionary_t * d;

    if(p->segments.num_entries)
      {
      bgav_seek_window_changed(ctx->b, p->window_start, p->window_end);
      }
    }
  
  gavl_value_free(&val);

  gavl_dictionary_free(&cipher_params);

  //  fprintf(stderr, "*** Appended %d entries\n", num_appended);
  
  return ret;
  
  }

static int parse_iv(bgav_input_context_t * ctx, const char * str, uint8_t * out, int len)
  {
  int i;
  int val;
  
  if((str[0] == '0') &&
     (tolower(str[1] == 'x')))
    str+=2;

  if(strlen(str) != len * 2)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Parsing IV failed: wrong string length");
    return 0;
    }

  for(i = 0; i < len; i++)
    {
    if(!sscanf(str, "%02x", &val))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Parsing IV failed: Invalid hex sequence: %c%c", str[0], str[1]);
      return 0;
      }
    out[i] = val;
    str += 2;
    }
  //  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "IV");
  return 1;
  }

static int handle_id3(bgav_input_context_t * ctx)
  {
  hls_priv_t * p = ctx->priv;
  int len;
  uint8_t probe_buf[BGAV_ID3V2_DETECT_LEN];
  gavl_buffer_t buf;
  bgav_input_context_t * mem;
  bgav_id3v2_tag_t * id3;
  
  gavl_buffer_init(&buf);
  
  if(gavf_io_get_data(p->io, probe_buf, BGAV_ID3V2_DETECT_LEN) < BGAV_ID3V2_DETECT_LEN)
    return 1;
  
  //  fprintf(stderr, "Segment start:\n");
  //  gavl_hexdump(probe_buf, BGAV_ID3V2_DETECT_LEN, BGAV_ID3V2_DETECT_LEN);
  
  if((len = bgav_id3v2_detect(probe_buf)) <= 0)
    return 1;
  gavl_buffer_alloc(&buf, len);

  if(gavf_io_read_data(p->io, buf.buf, len) < len)
    return 0;

  buf.len = len;
  mem = bgav_input_open_memory(buf.buf, buf.len);

  if((id3 = bgav_id3v2_read(mem)))
    {
    int64_t pts;
    
    //    bgav_id3v2_dump(id3);
    pts = bgav_id3v2_get_pts(id3);

    if(pts != GAVL_TIME_UNDEFINED)
      ctx->input_pts = pts;
    
#if 0
    fprintf(stderr, "Got ID3V2 %"PRId64"\n", ctx->input_pts);
    bgav_id3v2_dump(id3);
#endif

    
    bgav_id3v2_2_metadata(id3, &ctx->m);
    bgav_id3v2_destroy(id3);
    }
  
  bgav_input_close(mem);
  bgav_input_destroy(mem);
  gavl_buffer_free(&buf);
  return 1;
  }

static int init_cipher(bgav_input_context_t * ctx)
  {
  int ret = 0;
  const char * cipher_key_uri;
  const char * cipher_iv;
  const char * cipher;
  int cipher_block_size = 0;
  int cipher_key_size = 0;
  gavl_cipher_algo_t algo = 0;
  gavl_cipher_mode_t mode = 0;
  gavl_cipher_padding_t padding = 0;
  const gavl_dictionary_t * dict;
  hls_priv_t * p = ctx->priv;

  dict = gavl_value_get_dictionary(&p->segments.entries[p->seq_cur - p->seq_start]);
  
  /* Check for encryption */
  if(!(cipher = gavl_dictionary_get_string(dict, SEGMENT_CIPHER)) ||
     !strcmp(cipher, "NONE"))
    {
    if(p->cipher_io)
      {
      gavf_io_destroy(p->cipher_io);
      p->cipher_io = NULL;
      }
    return 1;
    }

  cipher_key_uri = gavl_dictionary_get_string(dict, SEGMENT_CIPHER_KEY_URI);
  cipher_iv = gavl_dictionary_get_string(dict, SEGMENT_CIPHER_IV);

  if(!cipher_key_uri)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Got encrypred segment but no key URI");
    goto fail;
    }
    
  //    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got encrypted segment: %s %s %s",
  //             cipher, cipher_key_uri, cipher_iv);
    
  if(!strcmp(cipher, "AES-128"))
    {
    algo = GAVL_CIPHER_AES128;
    mode = GAVL_CIPHER_MODE_CBC;
    padding = GAVL_CIPHER_PADDING_PKCS7;
    cipher_block_size = 16;
    cipher_key_size = 16;
    }
  else
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Unsupported cipher: %s", cipher);
    goto fail;
    }

  if(p->cipher_key.len != cipher_key_size)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Invalid key length (expected %d got %d)",
             cipher_key_size, p->cipher_key.len);
    goto fail;
    }
  //  if(!download_key(ctx, cipher_key_uri, cipher_key_size))
  //    goto fail;
  
  if(!p->cipher_io)
    p->cipher_io = gavf_io_create_cipher(algo, mode, padding, 0);
  
  if(cipher_iv)
    {
    gavl_buffer_alloc(&p->cipher_iv, cipher_block_size);
    
    if(!parse_iv(ctx, cipher_iv, p->cipher_iv.buf, cipher_block_size))
      goto fail;
    p->cipher_iv.len = cipher_block_size;
    }
  

  ret = 1;
  fail:
  
  return ret;
  }

static void init_seek_window(bgav_input_context_t * ctx)
  {
  int idx;
  int i;
  const gavl_dictionary_t * d;
  gavl_time_t segment_duration = 0;
  gavl_time_t start_time_abs = 0;
  hls_priv_t * p = ctx->priv;

  gavl_value_t val;
  gavl_dictionary_t * dict;
  
  idx = p->seq_cur - p->seq_start;

  p->window_start = 0;
  p->window_end = 0;

  for(i = 0; i < idx; i++)
    {
    if((d = gavl_value_get_dictionary(&p->segments.entries[i])) &&
       gavl_dictionary_get_long(d, SEGMENT_DURATION, &segment_duration) &&
       (segment_duration > 0))
      p->window_start -= segment_duration;
    }
  for(i = idx; i < p->segments.num_entries; i++)
    {
    if((d = gavl_value_get_dictionary(&p->segments.entries[i])))
      {
      if(gavl_dictionary_get_long(d, SEGMENT_DURATION, &segment_duration) &&
         (segment_duration > 0))
        p->window_end += segment_duration;
      
      if((i==idx) && gavl_dictionary_get_long(d, SEGMENT_START_TIME_ABS, &start_time_abs))
        gavl_dictionary_set_long(&ctx->m, GAVL_STATE_SRC_SEEK_WINDOW_ABSOLUTE, start_time_abs);
      
      }
    }

  gavl_value_init(&val);
  dict = gavl_value_set_dictionary(&val);  
  gavl_dictionary_set_long(dict, GAVL_STATE_SRC_SEEK_WINDOW_START, p->window_start);
  gavl_dictionary_set_long(dict, GAVL_STATE_SRC_SEEK_WINDOW_END, p->window_end);
  gavl_dictionary_set_nocopy(&ctx->m, GAVL_STATE_SRC_SEEK_WINDOW, &val);
  
  }

static int open_next_async(bgav_input_context_t * ctx, int timeout)
  {
  hls_priv_t * p = ctx->priv;

  if(p->next_state == NEXT_STATE_WAIT_M3U)
    {
    if(gavl_timer_get(p->m3u_timer) >= p->m3u_time)
      p->next_state = NEXT_STATE_START;
    }
  
  if(p->next_state == NEXT_STATE_START)
    {
    gavl_buffer_reset(&p->m3u_buf);
    
    if(!gavl_http_client_run_async(p->m3u_io, "GET", ctx->location))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Opening m3u8 failed");
      return -1;
      }
    p->next_state = NEXT_STATE_READ_M3U;
    }
  
  if(p->next_state == NEXT_STATE_READ_M3U)
    {
    int result = gavl_http_client_run_async_done(p->m3u_io, timeout);
    
    //    fprintf(stderr, "Read m3u %d %d\n", result,  p->m3u_buf.len);
    
    if(result <= 0)
      {
      if(result < 0)
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Downloading m3u8 failed");
      return result;
      }
    if(!parse_m3u8(ctx))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Parsing m3u8 failed");
      return -1;
      }
    if(p->seq_cur < 0)
      {
      if(p->segments.num_entries <= 10)
        p->seq_cur = p->seq_start;
      else
        p->seq_cur = p->seq_start + p->segments.num_entries - 2;
      init_seek_window(ctx);
      }

    if(p->seq_cur >= p->seq_start + p->segments.num_entries)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Next segment not available in m3u8. Trying again in 1 sec.");
      p->m3u_time = gavl_timer_get(p->m3u_timer) + GAVL_TIME_SCALE;
      p->next_state = NEXT_STATE_WAIT_M3U;
      return 0;
      }
    else
      p->next_state = NEXT_STATE_GOT_TS;
    }

  
  if(p->next_state == NEXT_STATE_GOT_TS)
    {
    const gavl_dictionary_t * dict;
    //    const char * uri = NULL;
    const char * cipher_key_uri = NULL;
    int idx;

    /* Got m3u8 with the next TS segment */
    
    idx = p->seq_cur - p->seq_start;

    if((idx < 0) || (idx >= p->segments.num_entries))
      {
      bgav_signal_restart(ctx->b, GAVL_MSG_SRC_RESTART_ERROR);

      if(idx < 0)
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Lost sync: Position before m3u8 segments %d", -idx);
      else // Probably just wait a bit?
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Lost sync: Position after m3u8 segments %d",
                 idx - (p->segments.num_entries - 1));
      p->end_of_sequence = 1;
      return -1;
      }
      
    dict = gavl_value_get_dictionary(&p->segments.entries[idx]);

    //    uri = gavl_dictionary_get_string(dict, GAVL_META_URI);
    

    if((cipher_key_uri = gavl_dictionary_get_string(dict, SEGMENT_CIPHER_KEY_URI)))
      {
      if(!p->cipher_key_io)
        {
        p->cipher_key_io = gavl_http_client_create();
        gavl_http_client_set_req_vars(p->cipher_key_io, &p->http_vars);
        gavl_http_client_set_response_body(p->cipher_key_io, &p->cipher_key);
        }
      gavl_buffer_reset(&p->cipher_key);

      gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Downloading cipher key from %s",
               cipher_key_uri);
      
      if(!gavl_http_client_run_async(p->cipher_key_io, "GET", cipher_key_uri))
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Downloading cipher key failed");
        return -1;
        }
      p->next_state = NEXT_STATE_READ_CIPHER_KEY;
      }
    else
      {
      if(p->cipher_key_io)
        {
        gavf_io_destroy(p->cipher_key_io);
        p->cipher_key_io = NULL;
        }
      p->next_state = NEXT_STATE_START_OPEN_TS;
      }
    }
  
  if(p->next_state == NEXT_STATE_READ_CIPHER_KEY)
    {
    int result = gavl_http_client_run_async_done(p->cipher_key_io, timeout);

    if(result <= 0)
      {
      if(result < 0)
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Downloading cipher key failed");
      return result;
      }

    gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Got cipher key");
    
    /* Initialize cipher */
    if(!init_cipher(ctx))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Initializing cipher failed");
      return -1;
      }
    p->next_state = NEXT_STATE_START_OPEN_TS;
    }

  if(p->next_state == NEXT_STATE_START_OPEN_TS)
    {
    //    int result;
    const gavl_dictionary_t * dict;
    int idx;
    const char * uri;
    
    idx = p->seq_cur - p->seq_start;
    dict = gavl_value_get_dictionary(&p->segments.entries[idx]);
    uri = gavl_dictionary_get_string(dict, GAVL_META_URI);

    if(!uri)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Got no TS uri");
      return -1;
      }
    if(!gavl_http_client_run_async(p->ts_io_next, "GET", uri))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Opening TS failed");
      return -1;
      }
    p->next_state = NEXT_STATE_OPEN_TS;
    }
  
  if(p->next_state == NEXT_STATE_OPEN_TS)
    {
    int result = gavl_http_client_run_async_done(p->ts_io_next, timeout);
    if(result <= 0)
      {
      if(result < 0)
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Opening TS failed");
      
      //  fprintf(stderr, "Open TS: %d\n", result);
      return result;
      }
    p->next_state = NEXT_STATE_DONE;
    return 1;
    }
  
  if(p->next_state == NEXT_STATE_DONE)
    {
    return 1;
    }
  
  return 0;
  }

static void init_segment_io(bgav_input_context_t * ctx)
  {
  gavf_io_t * swp;
  hls_priv_t * p = ctx->priv;
  
  swp = p->ts_io;
  p->ts_io = p->ts_io_next;
  p->ts_io_next = swp;

  if(p->cipher_io)
    {
    gavf_io_cipher_init(p->cipher_io, p->ts_io, p->cipher_key.buf, p->cipher_iv.buf);
    p->io = p->cipher_io;
    }
  else
    p->io = p->ts_io;

  //  fprintf(stderr, "init_segment_io %p %p %p\n", p->io, p->ts_io, p->ts_io_next);

  p->next_state = NEXT_STATE_START;
  p->seq_cur++;
  handle_id3(ctx);
  
  /* Some streams (NDR3) have two id3 tags with different infos */
  handle_id3(ctx);
  
  }

static int open_hls(bgav_input_context_t * ctx, const char * url1, char ** r)
  {
  int result;
  int i;
  int ret = 0;
  char * url;
  gavl_dictionary_t * src;

  hls_priv_t * priv = calloc(1, sizeof(*priv));

  //  fprintf(stderr, "Open HLS: %s\n", url1);
  
  ctx->priv = priv;
  priv->m3u_timer = gavl_timer_create();
  gavl_timer_start(priv->m3u_timer);
  
  ctx->location = gavl_strdup(url1);
  
  priv->m3u_io = gavl_http_client_create();
  
  gavl_http_client_set_response_body(priv->m3u_io, &priv->m3u_buf);

  priv->ts_io      = gavl_http_client_create();
  priv->ts_io_next = gavl_http_client_create();
  
  url = gavl_strdup(url1);
  url = gavl_url_extract_http_vars(url, &priv->http_vars);
  free(url);

  gavl_http_client_set_req_vars(priv->ts_io,      &priv->http_vars);
  gavl_http_client_set_req_vars(priv->ts_io_next, &priv->http_vars);
  gavl_http_client_set_req_vars(priv->m3u_io,     &priv->http_vars);
  
  priv->seq_start = -1;
  priv->seq_cur = -1;
  
  //  if(!load_m3u8(ctx))
  //    goto fail;

  
  //  if(!open_ts(ctx))
  //    goto fail;

  priv->next_state = NEXT_STATE_START;

  for(i = 0; i < 1000; i++)
    {
    result = open_next_async(ctx, 3000);

    if(result < 0)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "open next async failed %d", result);
      goto fail;
      }
    
    if(priv->next_state == NEXT_STATE_DONE)
      break;
    }

  if(priv->next_state != NEXT_STATE_DONE)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "open next async: Too many iterations %d",
             priv->next_state);
    goto fail;
    }

  init_segment_io(ctx);
  
  if((priv->window_end == GAVL_TIME_UNDEFINED) ||
     (priv->window_start == GAVL_TIME_UNDEFINED))
    ctx->flags &= ~BGAV_INPUT_CAN_SEEK_TIME;

  if((src = gavl_metadata_get_src_nc(&ctx->m, GAVL_META_SRC, 0)))
    {
    const gavl_dictionary_t * resp = gavl_http_client_get_response(priv->ts_io);
    gavl_dictionary_set_string(src, GAVL_META_MIMETYPE,
                               gavl_dictionary_get_string_i(resp, "Content-Type"));
    }
  
  ret = 1;
  fail:
  
  return ret;
  }

static void seek_time_hls(bgav_input_context_t * ctx, int64_t t1, int scale)
  {
  
  }

static int can_read_hls(bgav_input_context_t * ctx, int timeout)
  {
  hls_priv_t * p = ctx->priv;
  if(p->io)
    return gavf_io_can_read(p->io, timeout);
  else
    return 0;
  }

static int do_read_hls(bgav_input_context_t* ctx, uint8_t * buffer, int len, int block)
  {
  int bytes_read = 0;
  int result;
  
  hls_priv_t * p = ctx->priv;

  //  fprintf(stderr, "read_hls %d\n", len);
  
  while(bytes_read < len)
    {
    if(!block)
      {
      if(!gavf_io_can_read(p->io, 0))
        return bytes_read;
      
      result = gavf_io_read_data_nonblock(p->io, buffer + bytes_read, len - bytes_read);
      }
    else
      result = gavf_io_read_data(p->io, buffer + bytes_read, len - bytes_read);
    
    if(result < 0)
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Read error");
    
#if 0    
    if(result < len - bytes_read)
      {
      fprintf(stderr, "Wanted %d, got %d\n", len - bytes_read, result);
      }
#endif
    //    fprintf(stderr, "read_hls 1 %d\n", len - bytes_read);

    if((p->next_state != NEXT_STATE_DONE) && !p->end_of_sequence)
      {
      if(open_next_async(ctx, 0) < 0)
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Opening next segment failed");
      }

    bytes_read += result;
    
    if(result < len - bytes_read)
      {
      if(gavf_io_got_error(p->io))
        {
        fprintf(stderr, "Got I/O error\n");
        bgav_signal_restart(ctx->b, GAVL_MSG_SRC_RESTART_ERROR);
        return bytes_read;
        }
      else if(!block)
        {
        if(gavf_io_got_eof(p->io) && (p->next_state == NEXT_STATE_DONE))
          init_segment_io(ctx);
        else
          return bytes_read;
        }
      else
        {
        int i;
        int result1;

        if(p->end_of_sequence)
          return bytes_read;
        
        //  fprintf(stderr, "Opening next segment: %d %d %d\n", result, len, bytes_read);
        
        if(p->next_state != NEXT_STATE_DONE)
          {
          gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Need to open next syncronously");
          
          for(i = 0 ; i < 100; i++)
            {
            result1 = open_next_async(ctx, 100);
            
            if(result1 > 0)
              break;

            if(result1 < 0)
              return bytes_read;
          
            if(p->next_state == NEXT_STATE_DONE)
              break;
            }
          }
        init_segment_io(ctx);
        }
      }
    }
  return bytes_read;
  }

static int read_hls(bgav_input_context_t* ctx,
                    uint8_t * buffer, int len)
  {
  return do_read_hls(ctx, buffer, len, 1);
  }

static int read_nonblock_hls(bgav_input_context_t* ctx,
                            uint8_t * buffer, int len)
  {
  return do_read_hls(ctx, buffer, len, 0);
  }

static void close_hls(bgav_input_context_t * ctx)
  {
  hls_priv_t * p = ctx->priv;

  //  fprintf(stderr, "Close HLS\n");
  if(p->m3u_timer)
    gavl_timer_destroy(p->m3u_timer);
  
  if(p->m3u_io)
    gavf_io_destroy(p->m3u_io);
  if(p->ts_io)
    gavf_io_destroy(p->ts_io);

  if(p->cipher_io)
    gavf_io_destroy(p->cipher_io);

  if(p->cipher_key_io)
    gavf_io_destroy(p->cipher_key_io);
  
  gavl_array_free(&p->segments);
  gavl_dictionary_free(&p->http_vars);
  
  gavl_buffer_free(&p->m3u_buf);
  gavl_buffer_free(&p->cipher_key);
  gavl_buffer_free(&p->cipher_iv);
  
  
  free(p);
  }


const bgav_input_t bgav_input_hls =
  {
    .name          = "hls",
    .open          = open_hls,
    .read          = read_hls,
    .read_nonblock = read_nonblock_hls,
    .can_read      = can_read_hls,
    .close         = close_hls,
    .seek_time     = seek_time_hls,
  };
