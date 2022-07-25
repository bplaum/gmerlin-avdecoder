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

#include <id3.h>

#define LOG_DOMAIN "in_hls"

#define SEGMENT_START_TIME "start"
#define SEGMENT_END_TIME "end"

#define SEGMENT_CIPHER         "cipher"
#define SEGMENT_CIPHER_KEY_URI "cipherkeyuri"
#define SEGMENT_CIPHER_IV      "cipheriv"

typedef struct
  {
  gavf_io_t * m3u_io;
  gavf_io_t * ts_io;
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
  
  gavl_time_t abs_offset;

  gavl_buffer_t cipher_key;

  gavl_dictionary_t http_vars;
  
  } hls_priv_t;

static int load_m3u8(bgav_input_context_t * ctx)
  {
  int ret = 0;
  hls_priv_t * p = ctx->priv;
  char ** lines;
  int idx;
  int64_t new_seq = -1;
  int skip;
  gavl_value_t val;
  gavl_dictionary_t * dict;
  gavl_time_t segment_start_time = GAVL_TIME_UNDEFINED;
  gavl_time_t segment_duration = 0;
  int window_changed = 0;

  gavl_dictionary_t cipher_params;

  int num_appended = 0;
  
  p->window_start = GAVL_TIME_UNDEFINED;
  p->window_end = GAVL_TIME_UNDEFINED;

  gavl_dictionary_init(&cipher_params);
  
  if(!gavl_http_client_open(p->m3u_io, "GET", ctx->url))
    goto fail;
  
  //  gavl_dprintf("Got response\n");
  //  gavl_dictionary_dump(&res, 2);
  
  gavl_buffer_reset(&p->m3u_buf);
  
  if(!gavl_http_client_read_body(p->m3u_io, &p->m3u_buf))
    goto fail;
  
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
    if(p->seq_start < new_seq)
      {
      //      fprintf(stderr, "Deleting %"PRId64" elements\n", new_seq - p->seq_start);
  
      gavl_array_splice_val(&p->segments, 0, new_seq - p->seq_start, NULL);
      p->seq_start = new_seq;
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
                                            gavl_get_absolute_uri(tmp_string, ctx->url));
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
    
    gavl_strtrim(lines[idx]);

    if(*(lines[idx]) == 0)
      {
      idx++;
      continue;
      }
    
    if(gavl_string_starts_with(lines[idx], "#EXT-X-PROGRAM-DATE-TIME:"))
      {

      /* Time in ISO-8601 format */
      // #EXT-X-PROGRAM-DATE-TIME:2022-04-26T21:15:13Z
      
      if(!gavl_time_parse_iso8601(lines[idx] + strlen("#EXT-X-PROGRAM-DATE-TIME:") , &segment_start_time))
        segment_start_time = GAVL_TIME_UNDEFINED;
      }
    else if(gavl_string_starts_with(lines[idx], "#EXTINF:"))
      {
      double duration = strtod(lines[idx] + strlen("#EXTINF:"), NULL);
      segment_duration = gavl_seconds_to_time(duration);
      }
    else if(!gavl_string_starts_with(lines[idx], "#"))
      {
      char * uri;
      
      if(segment_start_time != GAVL_TIME_UNDEFINED)
        {
        gavl_dictionary_set_long(dict, SEGMENT_START_TIME, segment_start_time);
        if(segment_duration >  0)
          gavl_dictionary_set_long(dict, SEGMENT_END_TIME, segment_start_time + segment_duration);
        }

      gavl_dictionary_merge2(dict, &cipher_params);

      uri = bgav_input_absolute_url(ctx, lines[idx]);

      uri = gavl_url_append_http_vars(uri, &p->http_vars);

      
      gavl_dictionary_set_string_nocopy(dict, GAVL_META_URI, uri);
      gavl_array_splice_val_nocopy(&p->segments, -1, 0, &val);

      segment_start_time = GAVL_TIME_UNDEFINED;
      segment_duration = 0;
      
      gavl_value_reset(&val);
      dict = gavl_value_set_dictionary(&val);
      num_appended++;
      }
    idx++;
    }
  
  gavl_strbreak_free(lines);
  
  ret = 1;
  
  fail:

  if(window_changed)
    {
    const gavl_dictionary_t * d;
    if(p->segments.num_entries)
      {
      if((d = gavl_value_get_dictionary(&p->segments.entries[0])) &&
         gavl_dictionary_get_long(d, SEGMENT_START_TIME, &p->window_start) &&
         (d = gavl_value_get_dictionary(&p->segments.entries[p->segments.num_entries-1])) &&
         gavl_dictionary_get_long(d, SEGMENT_END_TIME, &p->window_end))
        bgav_seek_window_changed(ctx->b, p->window_start, p->window_end);
      }
    }
  
  gavl_value_free(&val);

  gavl_dictionary_free(&cipher_params);

  //  fprintf(stderr, "*** Appended %d entries\n", num_appended);
  
  return ret;
  
  }

static int download_key(bgav_input_context_t * ctx, const char * uri, int len)
  {
  hls_priv_t * priv = ctx->priv;
  
  if(!priv->cipher_key_io)
    priv->cipher_key_io = gavl_http_client_create();

  if(!gavl_http_client_open(priv->cipher_key_io,
                                 "GET", uri))
    return 0;

  gavl_buffer_reset(&priv->cipher_key);
  if(!gavl_http_client_read_body(priv->cipher_key_io, &priv->cipher_key))
    return 0;

  if(priv->cipher_key.len != len)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Invalid key length (expected %d got %d)", len, priv->cipher_key.len);
    return 0;
    }
  //  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got key");
  return 1;
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
  mem = bgav_input_open_memory(buf.buf, buf.len, ctx->opt);

  if((id3 = bgav_id3v2_read(mem)))
    {
    //    bgav_id3v2_dump(id3);
    bgav_id3v2_2_metadata(id3, &ctx->m);
    bgav_id3v2_destroy(id3);
#if 0
    fprintf(stderr, "Got ID3V2\n");
    gavl_dictionary_dump(&ctx->m, 2);
#endif
    }
  
  bgav_input_close(mem);
  bgav_input_destroy(mem);
  gavl_buffer_free(&buf);
  return 1;
  }

static int open_ts(bgav_input_context_t * ctx)
  {
  int ret = 0;
  int idx;
  const gavl_dictionary_t * dict;
  const char * uri;
  //  int use_tls;
  hls_priv_t * p = ctx->priv;

  char * http_host = NULL;
  const char * var;
  int tries = 0;
  const gavl_dictionary_t * res;
  uint8_t * iv = NULL;
  
  const char * cipher;
  
  if(!load_m3u8(ctx))
    return 0;
  
  idx = p->seq_cur - p->seq_start;

  if(idx < 0)
    {
    gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Lost sync");
    return 0;
    }
  
  while(idx >= p->segments.num_entries)
    {
    gavl_time_t delay_time = GAVL_TIME_SCALE / 5; // 200 ms
    
    gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Opening M8U %s seq_start: %"PRId64" num_segments: %d",
             ctx->url, p->seq_start, p->segments.num_entries);
    
    if(!load_m3u8(ctx))
      return 0;

    gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Opening M8U done seq_start: %"PRId64" num_segments: %d",
             p->seq_start, p->segments.num_entries);

    idx = p->seq_cur - p->seq_start;

    if(idx < 0)
      {
      gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Lost sync");
      return 0;
      }
    else if(idx < p->segments.num_entries)
      break;
    else if(tries >= 10)
      return 0;
    
    tries++;
    gavl_time_delay(&delay_time);
    }

  dict = gavl_value_get_dictionary(&p->segments.entries[idx]);

  uri = gavl_dictionary_get_string(dict, GAVL_META_URI);

  if(!p->abs_offset && gavl_dictionary_get_long(dict, SEGMENT_START_TIME, &p->abs_offset))
    {
    fprintf(stderr, "Got absolute time 1 %"PRId64"\n", p->abs_offset);
    bgav_start_time_absolute_changed(ctx->b, p->abs_offset);
    }
  gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Opening TS %s %d %"PRId64, uri, idx, p->seq_cur);
  
  if(!gavl_http_client_open(p->ts_io,
                                 "GET",
                                 uri))
    {
    goto fail;
    }

  res = gavl_http_client_get_response(p->ts_io);
  
  /* Check for encryption */
  if((cipher = gavl_dictionary_get_string(dict, SEGMENT_CIPHER)) &&
     strcmp(cipher, "NONE"))
    {
    const char * cipher_key_uri;
    const char * cipher_iv;
    int cipher_block_size = 0;
    int cipher_key_size = 0;
    gavl_cipher_algo_t algo = 0;
    gavl_cipher_mode_t mode = 0;
    gavl_cipher_padding_t padding = 0;

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

    if(!download_key(ctx, cipher_key_uri, cipher_key_size))
      goto fail;

    if(!p->cipher_io)
      p->cipher_io = gavf_io_create_cipher(p->ts_io, algo, mode, padding, 0);
    
    if(cipher_iv)
      {
      iv = malloc(cipher_block_size);
      if(!parse_iv(ctx, cipher_iv, iv, cipher_block_size))
        {
        free(iv);
        goto fail;
        }
      }
    
    gavf_io_cipher_init(p->cipher_io, p->cipher_key.buf, iv);
    
    if(iv)
      free(iv);
    
    p->io = p->cipher_io;
    }
  else
    {
    p->io = p->ts_io;
    if((var = gavl_dictionary_get_string(res, "Content-Type")) &&
       !(gavl_dictionary_get_string(&ctx->m, GAVL_META_MIMETYPE)))
      gavl_dictionary_set_string(&ctx->m, GAVL_META_MIMETYPE, var);
    }

  handle_id3(ctx);
  
  ret = 1;


  fail:

  if(http_host)
    free(http_host);
  
  return ret;
  }


static int open_hls(bgav_input_context_t * ctx, const char * url1, char ** r)
  {
  int ret = 0;
  char * url;
  hls_priv_t * priv = calloc(1, sizeof(*priv));

  ctx->priv = priv;

  ctx->url = gavl_strdup(url1);
  
  priv->m3u_io = gavl_http_client_create();
  priv->ts_io = gavl_http_client_create();

  url = gavl_strdup(url1);
  url = gavl_url_extract_http_vars(url, &priv->http_vars);
  free(url);
  
  priv->seq_start = -1;
  
  if(!load_m3u8(ctx))
    goto fail;

  if((priv->window_end == GAVL_TIME_UNDEFINED) ||
     (priv->window_start == GAVL_TIME_UNDEFINED))
    ctx->flags &= ~BGAV_INPUT_CAN_SEEK_TIME;
  
  if(priv->segments.num_entries < 4)
    priv->seq_cur = priv->seq_start;
  else
    priv->seq_cur = priv->seq_start + priv->segments.num_entries - 4;
  
  if(!open_ts(ctx))
    goto fail;
  
  ret = 1;
  fail:
  
  return ret;
  }

static void seek_time_hls(bgav_input_context_t * ctx, int64_t t1, int scale)
  {
  
  }

static int read_hls(bgav_input_context_t* ctx,
                    uint8_t * buffer, int len)
  {
  int bytes_read = 0;
  int result;
  
  hls_priv_t * p = ctx->priv;

  while(bytes_read < len)
    {
    result = gavf_io_read_data(p->io, buffer + bytes_read, len - bytes_read);

    if(result < len - bytes_read)
      {
      if(gavf_io_got_error(p->io))
        {
        return bytes_read;
        }
      else
        {
        p->seq_cur++;
        if(!open_ts(ctx))
          return bytes_read;
        }
      }

    bytes_read += result;
    }
  return bytes_read;
  }

#if 0
typedef struct
  {
  gavf_io_t * m3u_io;
  gavf_io_t * ts_io;

  gavl_array_t segments;
  
  gavl_socket_address_t * m3u_addr;
  gavl_dictionary_t m3u_req;
  gavl_buffer_t m3u_buf;
  
  gavl_socket_address_t * ts_addr;
  gavl_dictionary_t ts_req;
  int use_tls_m3u;
  int use_tls_ts;

  char * host;

  int64_t seq_start; // First sequence number of the array
  int64_t seq_cur;   // Current sequence number
  
  int64_t ts_len; /* Content length */
  int64_t ts_pos; /* Bytes read so far from the file or chunk */
  int ts_chunked;
  int ts_keepalive;
  
  } hls_priv_t;

#endif


static void close_hls(bgav_input_context_t * ctx)
  {
  hls_priv_t * p = ctx->priv;

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
  
  
  free(p);
  }


const bgav_input_t bgav_input_hls =
  {
    .name =          "hls",
    .open =          open_hls,
    .read =          read_hls,
    .close =         close_hls,
    .seek_time =     seek_time_hls,
  };

