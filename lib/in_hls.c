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



#include <string.h>
#include <stdlib.h>
#include <ctype.h>


#include <avdec_private.h>
#include <gavl/gavlsocket.h>
#include <gavl/value.h>
#include <gavl/http.h>
#include <gavl/state.h>
#include <gavl/io.h>

#include <id3.h>

#define LOG_DOMAIN "in_hls"

#define SEGMENT_DURATION        "duration"

#define SEGMENT_CIPHER         "cipher"
#define SEGMENT_CIPHER_KEY_URI "cipherkeyuri"
#define SEGMENT_CIPHER_IV      "cipheriv"

#define DISCONTINUITY_SEQUENCE "dseq"

/* States for opening the next source. */

#define NEXT_STATE_START           0
#define NEXT_STATE_READ_M3U        1
#define NEXT_STATE_WAIT_M3U        2
#define NEXT_STATE_READ_CIPHER_KEY 3
#define NEXT_STATE_GOT_TS          4
#define NEXT_STATE_START_OPEN_TS   5
#define NEXT_STATE_OPEN_TS         6
#define NEXT_STATE_DONE            7
#define NEXT_STATE_DISCONT         8 // Encountered discontinuity, client must reopen us with a proper dseq argument
#define NEXT_STATE_ERROR           9 // Error, client must reopen us with a proper clock-time

// #define NEXT_STATE_LOST_SYNC       8

#define END_OF_SEQUENCE (1<<0)
#define HAVE_HEADER     (1<<1)
#define SENT_HEADER     (1<<2)
#define NEED_PTS        (1<<3)
#define INIT            (1<<4)

typedef struct
  {
  gavl_timer_t * m3u_timer;
  gavl_time_t m3u_time;
  
  gavl_io_t * m3u_io;
  gavl_io_t * ts_io;
  gavl_io_t * ts_io_next;

  gavl_io_t * cipher_io;
  gavl_io_t * cipher_key_io;
  
  /* ts_io or cipher_io */
  gavl_io_t * io;
  
  gavl_array_t segments;
  
  gavl_buffer_t m3u_buf;
  
  int64_t seq_start; // First sequence number of the array
  int64_t seq_cur;   // Current sequence number
  
  gavl_buffer_t cipher_key;
  gavl_buffer_t cipher_iv;
  
  gavl_dictionary_t http_vars;

  int next_state;
  int flags;

  /* used only for pause */
  int64_t ts_pos;
  char * ts_uri;
  
  int have_clock_time;
  gavl_time_t clock_time_start;
  int64_t dseq_start;

  
  /* Global header */
  char * header_uri;
  int64_t header_offset;
  int64_t header_length;
  gavl_buffer_t header_buf;

  int64_t dseq;
  
  } hls_priv_t;

#define HAVE_HEADER_BYTES(p) ((p->flags & (HAVE_HEADER | SENT_HEADER)) == HAVE_HEADER)

static gavl_io_t * create_http_client(bgav_input_context_t * ctx)
  {
  hls_priv_t * priv = ctx->priv;
  gavl_io_t * ret = gavl_http_client_create();
  gavl_http_client_set_req_vars(ret,     &priv->http_vars);
  return ret;
  }

static gavl_time_t get_segment_clock_time(bgav_input_context_t * ctx, int idx)
  {
  gavl_time_t ret = GAVL_TIME_UNDEFINED;
  const gavl_dictionary_t * d;
  hls_priv_t * p = ctx->priv;
  
  if((d = gavl_value_get_dictionary(&p->segments.entries[idx])) &&
     gavl_dictionary_get_long(d, GAVL_URL_VAR_CLOCK_TIME, &ret) &&
     (ret > 0))
    return ret;
  else
    return GAVL_TIME_UNDEFINED;
  }

static int64_t get_segment_dseq(bgav_input_context_t * ctx, int idx)
  {
  int64_t ret = -1;
  const gavl_dictionary_t * d;
  hls_priv_t * p = ctx->priv;
  
  if((d = gavl_value_get_dictionary(&p->segments.entries[idx])) &&
     gavl_dictionary_get_long(d, DISCONTINUITY_SEQUENCE, &ret))
    return ret;
  else
    return -1;
  }

static gavl_time_t get_segment_duration(bgav_input_context_t * ctx, int idx)
  {
  gavl_time_t ret = GAVL_TIME_UNDEFINED;
  const gavl_dictionary_t * d;
  hls_priv_t * p = ctx->priv;
  
  if((d = gavl_value_get_dictionary(&p->segments.entries[idx])) &&
     gavl_dictionary_get_long(d, SEGMENT_DURATION, &ret) &&
     (ret > 0))
    return ret;
  else
    return GAVL_TIME_UNDEFINED;
  }

static int clock_time_to_idx(bgav_input_context_t * ctx, gavl_time_t * time)
  {
  int i;
  gavl_time_t test_time;
  hls_priv_t * p = ctx->priv;


  i = p->segments.num_entries-1;
  
  while(i > 0)
    {
    test_time = get_segment_clock_time(ctx, i);
    if(test_time <= *time)
      {
      *time = test_time;
      return i;
      }
    i--;
    }
  *time = get_segment_clock_time(ctx, 0);
  return 0;
  }

static int dseq_to_idx(bgav_input_context_t * ctx, int64_t dseq)
  {
  int i;
  hls_priv_t * p = ctx->priv;
  
  for(i = 0; i < p->segments.num_entries; i++)
    {
    if(dseq == get_segment_dseq(ctx, i))
      return i;
    }
  return -1;
  }

static void get_seek_window(bgav_input_context_t * ctx, gavl_time_t * start, gavl_time_t * end)
  {
  hls_priv_t * p = ctx->priv;

  gavl_time_t duration;
  
  *start = get_segment_clock_time(ctx, 0);
  *end = get_segment_clock_time(ctx, p->segments.num_entries-1);
  
  duration = get_segment_duration(ctx, p->segments.num_entries-1);

  if(duration > 0)
    *end += duration;
  
  }

static int parse_byterange(const char * str, int64_t * start, int64_t * len)
  {
  char * rest = NULL;
  *start = 0;

  *len = strtol(str, &rest, 10);

  if(str == rest)
    return 0;

  str = rest;
  if(*str == '@')
    {
    str++;
    *start = strtol(str, &rest, 10);

    if(str == rest)
      return 0;
    }
  return 1;
  }

static int parse_m3u8(bgav_input_context_t * ctx)
  {
  int ret = 0;
  hls_priv_t * p = ctx->priv;
  char ** lines;
  int idx;
  int64_t new_seq = -1;
  int64_t dseq = -1;
  int skip;
  gavl_value_t val;
  gavl_dictionary_t * dict;
  gavl_time_t segment_start_time_abs = GAVL_TIME_UNDEFINED;
  gavl_time_t segment_duration = 0;
  int window_changed = 0;
  gavl_time_t total_duration;
  
  gavl_dictionary_t cipher_params;

  int num_appended = 0;

  gavl_dictionary_init(&cipher_params);

  //  fprintf(stderr, "Parse m3u:\n%s\n", (char*)p->m3u_buf.buf);
  
  lines = gavl_strbreak((char*)p->m3u_buf.buf, '\n');
  
  //  fprintf(stderr, "parse_m3u8 %d\n", p->m3u_buf.len);
  //  gavl_hexdump(p->m3u_buf.buf, 128, 16);
  
  idx = 0;

  total_duration = 0;
  
  while(lines[idx])
    {
    if(gavl_string_starts_with(lines[idx],      "#EXT-X-MEDIA-SEQUENCE:"))
      new_seq = strtoll(lines[idx] + 22, NULL, 10);
    else if(gavl_string_starts_with(lines[idx], "#EXT-X-DISCONTINUITY-SEQUENCE:"))
      dseq = strtoll(lines[idx] + 30, NULL, 10);

    if((new_seq >= 0) && (dseq >= 0))
      break;
    idx++;
    }

  if(dseq < 0)
    dseq = 0;
  
  if(new_seq < 0)
    goto fail;

  if(p->seq_start < 0)
    {
    p->seq_start = new_seq;
    window_changed = 1;
    }
  else
    {
    int num_deleted = new_seq - p->seq_start;
    
    if(num_deleted > 0)
      {
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

    if(!strcmp(lines[idx], "#EXT-X-DISCONTINUITY"))
      dseq++;
    
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

    if(!strcmp(lines[idx], "#EXT-X-DISCONTINUITY"))
      {
      dseq++;
      idx++;
      continue;
      }
    
    /* Skip header lines to suppress fake warnings */
    else if(gavl_string_starts_with(lines[idx], "#EXTM3U") ||
            gavl_string_starts_with(lines[idx], "#EXT-X-VERSION") ||
            gavl_string_starts_with(lines[idx], "#ENCODER") ||
            gavl_string_starts_with(lines[idx], "#EXT-X-MEDIA-SEQUENCE") ||
            gavl_string_starts_with(lines[idx], "#EXT-X-TARGETDURATION") ||
            gavl_string_starts_with(lines[idx], "#EXT-X-DISCONTINUITY-SEQUENCE") ||
            gavl_string_starts_with(lines[idx], "#PLUTO"))
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
      
      if(!gavl_time_parse_iso8601(lines[idx] + strlen("#EXT-X-PROGRAM-DATE-TIME:") ,
                                  &segment_start_time_abs))
        segment_start_time_abs = GAVL_TIME_UNDEFINED;
      else
        p->have_clock_time = 1;
      }
    else if(gavl_string_starts_with(lines[idx], "#EXTINF:"))
      {
      double duration = strtod(lines[idx] + strlen("#EXTINF:"), NULL);
      segment_duration = gavl_seconds_to_time(duration);
      }
    else if(gavl_string_starts_with(lines[idx], "#EXT-X-MAP:"))
      {
      if(!p->header_uri)
        {
        char * pos;
        char * end;
        char * tmp_string;
        if((pos = strstr(lines[idx], "URI=\"")))
          {
          // URI="
          pos += 5;

          if((end = strchr(pos, '\"')))
            {
            tmp_string = gavl_strndup(pos, end);
            p->header_uri = bgav_input_absolute_url(ctx, tmp_string);
            p->header_uri = gavl_url_append_http_vars(p->header_uri, &p->http_vars);
            gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got global header: %s", p->header_uri);
            free(tmp_string);            
            }
          }
        
        if(p->header_uri && (pos = strstr(lines[idx], "BYTERANGE=\"")))
          {
          // BYTERANGE="
          pos += 11;
          parse_byterange(pos, &p->header_offset, &p->header_length);
          }
        }
      }
    else if(!gavl_string_starts_with(lines[idx], "#"))
      {
      char * uri;
      
      if(segment_start_time_abs != GAVL_TIME_UNDEFINED)
        gavl_dictionary_set_long(dict, GAVL_URL_VAR_CLOCK_TIME, segment_start_time_abs);
      if(segment_duration >  0)
        {
        gavl_dictionary_set_long(dict, SEGMENT_DURATION, segment_duration);
        total_duration += segment_duration;
        }
      gavl_dictionary_merge2(dict, &cipher_params);

      uri = bgav_input_absolute_url(ctx, lines[idx]);
      uri = gavl_url_append_http_vars(uri, &p->http_vars);
      
      gavl_dictionary_set_string_nocopy(dict, GAVL_META_URI, uri);

      gavl_dictionary_set_long(dict, DISCONTINUITY_SEQUENCE, dseq);
      
      gavl_array_splice_val_nocopy(&p->segments, -1, 0, &val);

      if(segment_start_time_abs != GAVL_TIME_UNDEFINED)
        {
        if(segment_duration >  0)
          segment_start_time_abs += segment_duration;
        else
          segment_start_time_abs = GAVL_TIME_UNDEFINED;
        }
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

  if(p->flags & INIT)
    {
    /* We say if we have a total duration of more than half an hour, we are seekable */
    if(p->have_clock_time && (total_duration > 30 * 60 * GAVL_TIME_SCALE))
      ctx->flags |= (BGAV_INPUT_CAN_SEEK_TIME | BGAV_INPUT_CAN_PAUSE);
    }
  
  if(window_changed && p->segments.num_entries && (ctx->flags & BGAV_INPUT_CAN_SEEK_TIME))
    {
    gavl_time_t start_time = 0;
    gavl_time_t end_time = 0;
    get_seek_window(ctx, &start_time, &end_time);
    bgav_seek_window_changed(ctx->b, start_time, end_time);
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
  
  if(gavl_io_get_data(p->io, probe_buf, BGAV_ID3V2_DETECT_LEN) < BGAV_ID3V2_DETECT_LEN)
    return 1;
  
  //  fprintf(stderr, "handle_id3: %d\n", !!(p->flags & NEED_PTS));
  //  gavl_hexdump(probe_buf, BGAV_ID3V2_DETECT_LEN, BGAV_ID3V2_DETECT_LEN);
  
  if((len = bgav_id3v2_detect(probe_buf)) <= 0)
    return 1;
  gavl_buffer_alloc(&buf, len);

  if(gavl_io_read_data(p->io, buf.buf, len) < len)
    return 0;

  buf.len = len;
  mem = bgav_input_open_memory(buf.buf, buf.len);
  
  if((id3 = bgav_id3v2_read(mem)))
    {
    int64_t pts;

#if 0
    fprintf(stderr, "Got ID3V2\n");
    bgav_id3v2_dump(id3);
#endif

    if(p->flags & NEED_PTS)
      {
      if((pts = bgav_id3v2_get_pts(id3)) != GAVL_TIME_UNDEFINED)
        {
        ctx->input_pts = pts;
        fprintf(stderr, "Got PTS from ID3: %"PRId64"\n", pts);
        }
#if 0 // The ID3 clock time is sometimes terriblly wrong. Lets use the time from the m3u8 instead
      if((pts = bgav_id3v2_get_clock_time(id3)) != GAVL_TIME_UNDEFINED)
        {
        /* Sometimes the calendar time is in local time instead of UTC. We try to correct this here */
        while(pts - ctx->clock_time > 900 * GAVL_TIME_SCALE)
          pts -= 1800 * GAVL_TIME_SCALE;
        while(pts - ctx->clock_time < -900 * GAVL_TIME_SCALE)
          pts += 1800 * GAVL_TIME_SCALE;
      
        fprintf(stderr, "Clock time: %"PRId64" %"PRId64" %"PRId64"\n", ctx->clock_time, pts, pts - ctx->clock_time);
        // ctx->clock_time = pts;

        ctx->input_pts += gavl_time_scale(90000, ctx->clock_time - pts);
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Shifting PTS according to ID3 data by %f seconds",
                 gavl_time_to_seconds(ctx->clock_time - pts));
        }
#endif
      }
    
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
      gavl_io_destroy(p->cipher_io);
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
    p->cipher_io = gavl_io_create_cipher(algo, mode, padding, 0);
  
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
  gavl_time_t win_start = 0;
  gavl_time_t win_end = 0;
  gavl_value_t val;
  gavl_dictionary_t * dict;
  
  if(!(ctx->flags & BGAV_INPUT_CAN_SEEK_TIME))
    return;
  
  get_seek_window(ctx, &win_start, &win_end);
  
  gavl_value_init(&val);
  dict = gavl_value_set_dictionary(&val);  
  gavl_dictionary_set_long(dict, GAVL_STATE_SRC_SEEK_WINDOW_START, win_start);
  gavl_dictionary_set_long(dict, GAVL_STATE_SRC_SEEK_WINDOW_END, win_end);
  gavl_dictionary_set_nocopy(&ctx->m, GAVL_STATE_SRC_SEEK_WINDOW, &val);
  
  }

static int open_next_async(bgav_input_context_t * ctx, int timeout)
  {
  hls_priv_t * p = ctx->priv;

  if(p->next_state == NEXT_STATE_WAIT_M3U)
    {
    gavl_time_t cur = gavl_timer_get(p->m3u_timer);

    /* Wait */
    if((cur < p->m3u_time) && (timeout > 0))
      {
      gavl_time_t delay_time = p->m3u_time - cur;
      if(delay_time > timeout)
        delay_time = timeout;
      gavl_time_delay(&delay_time);
      cur += delay_time;
      }
    
    if(cur >= p->m3u_time)
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
    
    if((result > 0) && !p->m3u_buf.len)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Got no m3u8 data");
      gavl_dictionary_dump(gavl_http_client_get_response(p->m3u_io), 2);
      return -1;
      }
    
    if(result <= 0)
      {
      if(result < 0)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Downloading m3u8 failed (client state: %d)",
                 gavl_http_client_get_state(p->m3u_io));
        
        if(p->seq_cur >= 0)
          {
          gavl_io_destroy(p->m3u_io);
          p->m3u_io = create_http_client(ctx);
          gavl_buffer_reset(&p->m3u_buf);
          gavl_http_client_set_response_body(p->m3u_io, &p->m3u_buf);
          gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Re-starting m3u8 download");
          p->next_state = NEXT_STATE_START;
          return 0;
          }
        }
      return result;
      }
    if(!parse_m3u8(ctx))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Parsing m3u8 failed");
      return -1;
      }
    if(p->seq_cur < 0)
      {
      if(p->clock_time_start > 0)
        {
        gavl_time_t t = p->clock_time_start;
        p->seq_cur = p->seq_start + clock_time_to_idx(ctx, &t);
        
        if(p->seq_cur >= 0)
          gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Initialized from clock time, difference: %f secs",
                   gavl_time_to_seconds(p->clock_time_start - t));
        }
      else if(p->dseq_start > 0)
        {
        p->seq_cur = p->seq_start + dseq_to_idx(ctx, p->dseq_start);

        if(p->seq_cur >= 0)
          gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Initialized from dseq (%"PRId64"): %"PRId64"\n",
                   p->dseq_start, p->seq_cur);
        }

      if(p->seq_cur < 0)
        {
        /* Short window: Place in the middle */
        
        if(p->segments.num_entries <= 10)
          p->seq_cur = p->seq_start + p->segments.num_entries/2;
        /* Longer window: Place near the end */
        else
          p->seq_cur = p->seq_start + p->segments.num_entries - 2;
        }
      
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Initializing sequence number: %"PRId64, p->seq_cur);

      ctx->clock_time = get_segment_clock_time(ctx, p->seq_cur - p->seq_start);
      
      //      init_seek_window(ctx);
      }

    if(p->seq_cur >= p->seq_start + p->segments.num_entries)
      {
      //      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Next segment not available in m3u8, cur: %"PRId64", last: %"PRId64". Trying again in 1 sec.", p->seq_cur, p->seq_start + p->segments.num_entries);
      p->m3u_time = gavl_timer_get(p->m3u_timer) + GAVL_TIME_SCALE;
      p->next_state = NEXT_STATE_WAIT_M3U;
      return 0;
      }
    else
      p->next_state = NEXT_STATE_GOT_TS;
    }

  
  if(p->next_state == NEXT_STATE_GOT_TS)
    {
    int64_t dseq;
    int64_t clock_time;
    
    const gavl_dictionary_t * dict;
    //    const char * uri = NULL;
    const char * cipher_key_uri = NULL;
    int idx;

    /* Got m3u8 with the next TS segment */
    
    idx = p->seq_cur - p->seq_start;

    if((idx < 0) || (idx >= p->segments.num_entries))
      {

      if(idx < 0)
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Lost sync: Position before m3u8 segments %d", -idx);
      else // Probably just wait a bit?
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Lost sync: Position after m3u8 segments %d",
                 idx - (p->segments.num_entries - 1));
      p->flags |= END_OF_SEQUENCE;
      return -1;
      }
      
    dict = gavl_value_get_dictionary(&p->segments.entries[idx]);

    dseq = -1;
    clock_time = GAVL_TIME_UNDEFINED;
    gavl_dictionary_get_long(dict, DISCONTINUITY_SEQUENCE, &dseq);
    gavl_dictionary_get_long(dict, GAVL_URL_VAR_CLOCK_TIME, &clock_time);

    if(p->dseq < 0)
      p->dseq = dseq;
    else if(p->dseq != dseq)
      {
      gavl_msg_t msg;
      gavl_dictionary_t vars;
      
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Detected discontinuity: %"PRId64" %"PRId64" %"PRId64"\n",
               p->dseq, dseq, clock_time);
      //      gavl_dictionary_dump(dict, 2);

      p->flags |= END_OF_SEQUENCE;

      gavl_msg_init(&msg);
      gavl_msg_set_id_ns(&msg, GAVL_MSG_SRC_RESTART_VARS, GAVL_MSG_NS_SRC);
      gavl_dictionary_init(&vars);

      /* Try to synchronize to the clock time of the next segment */
      if(clock_time != GAVL_TIME_UNDEFINED)
        gavl_dictionary_set_long(&vars, GAVL_URL_VAR_CLOCK_TIME, clock_time);
      else
        gavl_dictionary_set_long(&vars, DISCONTINUITY_SEQUENCE, dseq);
      gavl_msg_set_arg_dictionary(&msg, 0, &vars);
      bgav_send_msg(ctx->b, &msg);
      gavl_msg_free(&msg);
      gavl_dictionary_free(&vars);
      return -1;
      }
    
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
        gavl_io_destroy(p->cipher_key_io);
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
  gavl_io_t * swp;
  hls_priv_t * p = ctx->priv;
  
  swp = p->ts_io;
  p->ts_io = p->ts_io_next;
  p->ts_io_next = swp;
  
  if(p->cipher_io)
    {
    gavl_io_cipher_init(p->cipher_io, p->ts_io, p->cipher_key.buf, p->cipher_iv.buf);
    p->io = p->cipher_io;
    }
  else
    p->io = p->ts_io;
  
  //  fprintf(stderr, "init_segment_io %"PRId64" %p %s\n", p->seq_cur, ctx, ctx->location);
  
  p->next_state = NEXT_STATE_START;
  p->seq_cur++;
  handle_id3(ctx);
  
  /* Some streams (NDR3) have two id3 tags with different infos */
  handle_id3(ctx);

  p->flags &= ~NEED_PTS;
  
  }

static int open_next_sync(bgav_input_context_t * ctx)
  {
  int i, result;
  hls_priv_t * priv = ctx->priv;
  
  for(i = 0; i < 1000; i++)
    {
    result = open_next_async(ctx, 3000);

    if(result < 0)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "open next sync failed (result: %d)", result);
      return 0;
      }
    
    if(priv->next_state == NEXT_STATE_DONE)
      return 1;
    }
  gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "open_next_sync failed: %d Iterations exceeded, next_state: %d",
           i, priv->next_state);
  
  if(priv->next_state == NEXT_STATE_READ_M3U)
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "m3u client state: %d",
             gavl_http_client_get_state(priv->m3u_io));
  
  return 0;
  }

/* Download header to buffer */
static int download_header(bgav_input_context_t * ctx)
  {
  gavl_io_t * io;
  hls_priv_t * p = ctx->priv;

  io = create_http_client(ctx);
  if(p->header_length)
    gavl_http_client_set_range(io, p->header_offset, p->header_offset + p->header_length);

  gavl_http_client_set_response_body(io, &p->header_buf);

  if(!gavl_http_client_open(io, "GET", p->header_uri))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Downloading header failed");
    return 0;
    }
  gavl_io_destroy(io);

  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Downloaded header: %d bytes", p->header_buf.len);
  //  gavl_hexdump(p->header_buf.buf, 16, 16);
  
  p->flags |= HAVE_HEADER;
  return 1;
  }

static int open_hls(bgav_input_context_t * ctx, const char * url1, char ** r)
  {
  int ret = 0;
  char * url;
  gavl_dictionary_t * src;

  hls_priv_t * priv = calloc(1, sizeof(*priv));

  priv->dseq = -1;
  priv->dseq_start = -1;
  
  //  fprintf(stderr, "Open HLS: %s\n", url1);
  
  ctx->priv = priv;
  priv->m3u_timer = gavl_timer_create();
  gavl_timer_start(priv->m3u_timer);
  
  ctx->location = gavl_strdup(url1);

  ctx->location = gavl_url_extract_var_long(ctx->location,
                                            GAVL_URL_VAR_CLOCK_TIME,
                                            &priv->clock_time_start);

  ctx->location = gavl_url_extract_var_long(ctx->location,
                                            DISCONTINUITY_SEQUENCE,
                                            &priv->dseq_start);
  
  if(priv->clock_time_start > 0)
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got clock time: %"PRId64, priv->clock_time_start);

  if(priv->dseq_start > 0)
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got dseq: %"PRId64, priv->dseq_start);
  
  url = gavl_strdup(url1);
  url = gavl_url_extract_http_vars(url, &priv->http_vars);
  free(url);
  
  priv->m3u_io = create_http_client(ctx);
  gavl_http_client_set_response_body(priv->m3u_io, &priv->m3u_buf);

  priv->ts_io = create_http_client(ctx);
  priv->ts_io_next = create_http_client(ctx);
  
  priv->seq_start = -1;
  priv->seq_cur = -1;

  priv->flags |= (NEED_PTS|INIT);
  
  //  if(!load_m3u8(ctx))
  //    goto fail;
  
  //  if(!open_ts(ctx))
  //    goto fail;
  
  priv->next_state = NEXT_STATE_START;
  
  if(!open_next_sync(ctx))
    goto fail;
  
  init_segment_io(ctx);

  if(priv->header_uri && !download_header(ctx))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Could not download global header");
    goto fail;
    }

  if((src = gavl_metadata_get_src_nc(&ctx->m, GAVL_META_SRC, 0)))
    {
    const gavl_dictionary_t * resp = gavl_http_client_get_response(priv->ts_io);
    gavl_dictionary_set_string(src, GAVL_META_MIMETYPE,
                               gavl_dictionary_get_string_i(resp, "Content-Type"));
    }

  init_seek_window(ctx);
  
  priv->flags &= ~INIT;
  
  ret = 1;
  fail:
  
  return ret;
  }

static int jump_to_idx(bgav_input_context_t * ctx, int idx)
  {
  hls_priv_t * p = ctx->priv;

  p->seq_cur = p->seq_start + idx;

  ctx->input_pts = GAVL_TIME_UNDEFINED;
    
  if(p->ts_io)
    {
    gavl_io_destroy(p->ts_io);
    p->ts_io = NULL;
    }
  if(p->ts_io_next)
    {
    gavl_io_destroy(p->ts_io_next);
    p->ts_io_next = NULL;
    }
  if(p->m3u_io)
    {
    gavl_io_destroy(p->m3u_io);
    p->m3u_io = NULL;
    }
  
  p->io = NULL;

  p->m3u_io = create_http_client(ctx);
  gavl_buffer_reset(&p->m3u_buf);
  gavl_http_client_set_response_body(p->m3u_io, &p->m3u_buf);

  gavl_buffer_reset(&p->header_buf);
  
  p->ts_io = create_http_client(ctx);
  p->ts_io_next = create_http_client(ctx);
  p->next_state = NEXT_STATE_GOT_TS;

  //  fprintf(stderr, "Jump to idx: %d\n", idx);

  if(!open_next_sync(ctx))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Failed to re-open after seek");
    return 0;
    }
  init_segment_io(ctx);

  //  fprintf(stderr, "Jump to idx: %d done\n", idx);
  return 1;
  }

static void seek_time_hls(bgav_input_context_t * ctx, gavl_time_t *t1)
  {
  int idx;
  hls_priv_t * p = ctx->priv;

  if(!p->segments.num_entries)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "No segments loaded");
    return;
    }
  
  idx = clock_time_to_idx(ctx, t1);

  if(idx < 0)
    {
    if(*t1 < get_segment_clock_time(ctx, 0))
      idx = 0;
    else
      idx = p->segments.num_entries - 1;
    }

  fprintf(stderr, "seek_time_hls: time: %"PRId64", idx %d\n", *t1, idx);

  p->flags |= NEED_PTS;
  
  jump_to_idx(ctx, idx);
  }

static void pause_hls(bgav_input_context_t * ctx)
  {
  hls_priv_t * p = ctx->priv;

  //  fprintf(stderr, "pause_hls %p\n", ctx);
  
  if(gavl_io_can_seek(p->ts_io))
    gavl_http_client_pause(p->ts_io);
  else
    {
    p->ts_pos = gavl_io_position(p->ts_io);
    p->ts_uri = gavl_strdup(gavl_io_filename(p->ts_io));

    
    gavl_io_destroy(p->ts_io);
    p->ts_io = NULL;
    }
  p->next_state = NEXT_STATE_START;
  }

#define RESUME_SKIP_BYTES 1024*1024

static void resume_hls(bgav_input_context_t * ctx)
  {
  hls_priv_t * p = ctx->priv;

  //  fprintf(stderr, "resume_hls %p %s\n", p->ts_io, p->ts_uri);
  
  if(p->ts_io)
    {
    gavl_http_client_resume(p->ts_io);
    handle_id3(ctx);
    handle_id3(ctx);
    }
  else
    {
    int skip_bytes;
    uint8_t * dummy;

    if(!p->ts_uri)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "No TS Uri available for resuming");
      return;
      }
    
    p->ts_io = create_http_client(ctx);

    /* Open, skip bytes */

    //    fprintf(stderr, "resume_hls %p %s, pos: %"PRId64"\n", ctx, p->ts_uri, p->ts_pos);
    
    if(!gavl_http_client_open(p->ts_io, "GET", p->ts_uri))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Re-opening stream uri after pause failed");
      gavl_io_destroy(p->ts_io);
      p->ts_io = NULL;
      return;
      }
    dummy = malloc(RESUME_SKIP_BYTES);
    
    while(p->ts_pos > 0)
      {
      skip_bytes = RESUME_SKIP_BYTES;
      if(skip_bytes > p->ts_pos)
        skip_bytes = p->ts_pos;
      if(gavl_io_read_data(p->ts_io, dummy, skip_bytes) < skip_bytes)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Skipping bytes after pause failed");
        gavl_io_destroy(p->ts_io);
        p->ts_io = NULL;
        free(dummy);
        return;
        }
      p->ts_pos -= skip_bytes;
      }

    if(p->cipher_io)
      {
      p->io = p->cipher_io;
      gavl_io_cipher_set_src(p->cipher_io, p->ts_io);
      }
    else
      p->io = p->ts_io;
    
    free(dummy);
    }
  }


static int can_read_hls(bgav_input_context_t * ctx, int timeout)
  {
  hls_priv_t * p = ctx->priv;
  
  if(HAVE_HEADER_BYTES(p) || !p->ts_io)
    return 1;
  
  if(p->io)
    return gavl_io_can_read(p->io, timeout);
  else
    return 0;
  }

static int do_read_hls(bgav_input_context_t* ctx, uint8_t * buffer, int len, int block)
  {
  int bytes_read = 0;
  int result;
  
  hls_priv_t * p = ctx->priv;

  //  fprintf(stderr, "read_hls %d\n", len);

  if(!p->io)
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Read error: underlying I/0 missing");
    return 0;
    }
  while(bytes_read < len)
    {
    if(HAVE_HEADER_BYTES(p))
      {
      int bytes_to_copy = p->header_buf.len - p->header_buf.pos;

      if(bytes_to_copy > len)
        bytes_to_copy = len;
      
      memcpy(buffer + bytes_read, p->header_buf.buf + p->header_buf.pos, bytes_to_copy);
      
      bytes_read        += bytes_to_copy;
      p->header_buf.pos += bytes_to_copy;

      if(p->header_buf.pos == p->header_buf.len)
        {
        p->flags |= SENT_HEADER;
        gavl_buffer_free(&p->header_buf);
        gavl_buffer_init(&p->header_buf);
        }
      continue;
      }
    if(!block)
      {
      if(!gavl_io_can_read(p->io, 0))
        {
        if(!bytes_read)
          gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Detected EOF 4");

        return bytes_read;
        }
      result = gavl_io_read_data_nonblock(p->io, buffer + bytes_read, len - bytes_read);
      }
    else
      result = gavl_io_read_data(p->io, buffer + bytes_read, len - bytes_read);
    
    if(result < 0)
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Read error");
    
#if 0
    if(result < len - bytes_read)
      {
      fprintf(stderr, "read_hls Wanted %d, got %d\n", len - bytes_read, result);
      }
    if(!result)
      fprintf(stderr, "read_hls 1 %d result: %d %d\n", len - bytes_read, result,
              gavl_io_got_eof(p->io));
#endif
    
    if((p->next_state != NEXT_STATE_DONE) && !(p->flags & END_OF_SEQUENCE))
      {
      if(open_next_async(ctx, 0) < 0)
        {
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Opening next segment failed");
        p->flags |= END_OF_SEQUENCE;
        return bytes_read;
        }
      }

    bytes_read += result;
    
    if(result < len - bytes_read)
      {
      if(gavl_io_got_error(p->io))
        {
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Got I/O error from underlying stream");

        if(!bytes_read)
          gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Detected EOF 3");
        
        return bytes_read;
        }
      else if(!block)
        {
        if(gavl_io_got_eof(p->io) && (p->next_state == NEXT_STATE_DONE))
          init_segment_io(ctx);
        else
          return bytes_read;
        }
      else
        {
        if(p->flags & END_OF_SEQUENCE)
          {
          if(!bytes_read)
            gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Got end of sequence");
          
          return bytes_read;
          }
        //  fprintf(stderr, "Opening next segment: %d %d %d\n", result, len, bytes_read);
        
        if(p->next_state != NEXT_STATE_DONE)
          {
          gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Need to open next synchronously");

          if(open_next_sync(ctx))
            init_segment_io(ctx);
          else
            {
            gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Open next segment failed");
            p->flags |= END_OF_SEQUENCE;
            return bytes_read;
            }
          }
        else
          init_segment_io(ctx);
        }
      }
    }

  if(!bytes_read)
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Detected EOF");
  
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
    gavl_io_destroy(p->m3u_io);
  if(p->ts_io)
    gavl_io_destroy(p->ts_io);
  if(p->ts_io_next)
    gavl_io_destroy(p->ts_io_next);
  
  if(p->cipher_io)
    gavl_io_destroy(p->cipher_io);

  if(p->cipher_key_io)
    gavl_io_destroy(p->cipher_key_io);

  if(p->header_uri)
    free(p->header_uri);
  
  gavl_array_free(&p->segments);
  gavl_dictionary_free(&p->http_vars);
  
  gavl_buffer_free(&p->m3u_buf);
  gavl_buffer_free(&p->cipher_key);
  gavl_buffer_free(&p->cipher_iv);
  gavl_buffer_free(&p->header_buf);
  
  
  free(p);
  }


const bgav_input_t bgav_input_hls =
  {
    .name          = "hls",
    .open          = open_hls,
    .read          = read_hls,
    .read_nonblock = read_nonblock_hls,
    .can_read      = can_read_hls,
    .pause         = pause_hls,
    .resume         = resume_hls,
    .close         = close_hls,
    .seek_time     = seek_time_hls,
  };
