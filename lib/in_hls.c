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


#include <avdec_private.h>
#include <gavl/gavlsocket.h>
#include <gavl/value.h>
#include <gavl/http.h>

#define LOG_DOMAIN "in_hls"

#define SEGMENT_START_TIME "start"
#define SEGMENT_END_TIME "end"

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

  gavl_time_t window_start;
  gavl_time_t window_end;
  
  gavl_time_t abs_offset;
  
  } hls_priv_t;

static int load_m3u8(bgav_input_context_t * ctx)
  {
  int ret = 0;
  hls_priv_t * p = ctx->priv;
  gavl_dictionary_t res;
  char ** lines;
  int idx;
  int64_t new_seq = -1;
  int skip;
  gavl_value_t val;
  gavl_dictionary_t * dict;
  gavl_time_t segment_start_time = GAVL_TIME_UNDEFINED;
  gavl_time_t segment_duration = 0;
  int window_changed = 0;
  
  p->window_start = GAVL_TIME_UNDEFINED;
  p->window_end = GAVL_TIME_UNDEFINED;
    
  gavl_dictionary_init(&res);
  
  if(p->m3u_io)
    {
    if(!gavl_http_request_write(p->m3u_io, &p->m3u_req))
      {
      gavf_io_destroy(p->m3u_io);
      p->m3u_io = NULL; // re-open
      }
    }
  
  if(!p->m3u_io)
    {
    int fd;
    if((fd = gavl_socket_connect_inet(p->m3u_addr, 1000)) < 0)
      goto fail;

    if(p->use_tls_m3u)
      p->m3u_io = gavf_io_create_tls_client(fd, p->host, GAVF_IO_SOCKET_DO_CLOSE);
    else
      p->m3u_io = gavf_io_create_socket(fd, 1000, GAVF_IO_SOCKET_DO_CLOSE);
    
    if(!gavl_http_request_write(p->m3u_io, &p->m3u_req))
      {
      gavf_io_destroy(p->m3u_io);
      p->m3u_io = NULL; // re-open
      goto fail;
      }
    }
  
  if(!gavl_http_response_read(p->m3u_io, &res))
    {
    goto fail;
    }

  if(gavl_http_response_get_status_int(&res) != 200)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Cannot load m3u, got code: %d %s",
             gavl_http_response_get_status_int(&res),
             gavl_http_response_get_status_str(&res));
    goto fail;
    }
  
  //  gavl_dprintf("Got response\n");
  //  gavl_dictionary_dump(&res, 2);
  
  gavl_buffer_reset(&p->m3u_buf);
  
  if(!gavl_http_read_body(p->m3u_io, &res, &p->m3u_buf))
    {
    goto fail;
    }
  
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
      gavl_array_splice_val(&p->segments, 0, new_seq - p->seq_start, NULL);
      p->seq_start = new_seq;
      window_changed = 1;
      }
    }

  skip = p->segments.num_entries;
  
  idx = 0;
  
  gavl_value_init(&val);
  dict = gavl_value_set_dictionary(&val);
  
  while(lines[idx])
    {
    if(skip)
      {
      if(!gavl_string_starts_with(lines[idx], "#"))
        skip--;
      idx++;
      continue;
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
    else if(gavl_string_starts_with(lines[idx], "#EXT-X-KEY:"))
      {
      // #EXT-X-KEY:METHOD=AES-128,URI="https://livestreamdirect-three.mediaworks.nz/K110346488-2000.key",IV=0x0000000000000000000000000005497B

      
      }
    else if(gavl_string_starts_with(lines[idx], "#EXTINF:"))
      {
      double duration = strtod(lines[idx] + strlen("#EXTINF:"), NULL);
      segment_duration = gavl_seconds_to_time(duration);
      }
    else if(!gavl_string_starts_with(lines[idx], "#"))
      {
      if(segment_start_time != GAVL_TIME_UNDEFINED)
        {
        gavl_dictionary_set_long(dict, SEGMENT_START_TIME, segment_start_time);
        if(segment_duration >  0)
          gavl_dictionary_set_long(dict, SEGMENT_END_TIME, segment_start_time + segment_duration);
        }
      
      gavl_dictionary_set_string_nocopy(dict, GAVL_META_URI, bgav_input_absolute_url(ctx, lines[idx]));
      gavl_array_splice_val_nocopy(&p->segments, -1, 0, &val);

      segment_start_time = GAVL_TIME_UNDEFINED;
      segment_duration = 0;
      
      gavl_value_init(&val);
      dict = gavl_value_set_dictionary(&val);
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
  
  gavl_dictionary_free(&res);

  return ret;
  
  }

static int open_ts(bgav_input_context_t * ctx)
  {
  int ret = 0;
  int idx;
  const gavl_dictionary_t * dict;
  const char * uri;
  //  int use_tls;
  hls_priv_t * p = ctx->priv;

  char * host = NULL;
  char * http_host = NULL;
  char * protocol = NULL;
  char * path = NULL;
  int port = -1;
  const char * var;
  int do_close = 0;
  int addr_changed = 0;
  int tries = 0;
  gavl_dictionary_t res;

  gavl_dictionary_init(&res);
  
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
    fprintf(stderr, "Got absolute time 1 %lld\n", p->abs_offset);
    bgav_start_time_absolute_changed(ctx->b, p->abs_offset);
    }
  gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Opening TS %s", uri);
  
  if(!bgav_url_split(uri,
                     &protocol,
                     NULL, /* User */
                     NULL, /* Pass */
                     &host,
                     &port,
                     &path))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Unvalid TS URL: %s", uri);
    goto fail;
    }
  
  if(port > 0)
    http_host = gavl_sprintf("%s:%d", host, port);
  else
    http_host = gavl_strdup(host);

  if(!strcasecmp(protocol, "https") ||
     !strcasecmp(protocol, "hlss"))
    {
    if(!p->use_tls_ts && p->ts_io)
      {
      do_close = 1;
      addr_changed = 1;
      }
    p->use_tls_ts = 1;
    }
  else
    {
    if(p->use_tls_ts && p->ts_io)
      {
      do_close = 1;
      addr_changed = 1;
      }
    p->use_tls_ts = 0;
    }

  if((var = gavl_dictionary_get_string(&p->ts_req, "Host")) &&
     strcmp(var, http_host))
    {
    do_close = 1;
    addr_changed = 1;
    }
  
  if(port <= 0)
    {
    if(p->use_tls_ts)
      port = 443;
    else
      port = 80;
    }

  if(do_close)
    {
    if(p->ts_io)
      {
      gavf_io_destroy(p->ts_io);
      p->ts_io = NULL;
      }
    }

  if(addr_changed && p->ts_addr)
    {
    gavl_socket_address_destroy(p->ts_addr);
    p->ts_addr = NULL;
    }
  
  if(p->ts_io)
    {
    gavl_http_request_set_path(&p->ts_req, path);
    
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Trying keepalive connection");

    if(!gavl_http_request_write(p->ts_io, &p->ts_req))
      {
      gavf_io_destroy(p->ts_io);
      p->ts_io = NULL;
      }
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "keepalive succeeded");
    }
  
  if(!p->ts_io)
    {
    int fd;

    if(!p->ts_addr)
      {
      p->ts_addr = gavl_socket_address_create();
      
      if(!gavl_socket_address_set(p->ts_addr, host, port, SOCK_STREAM))
        goto fail;
      }

    if((fd = gavl_socket_connect_inet(p->ts_addr, 1000)) < 0)
      goto fail;

    if(p->use_tls_ts)
      p->ts_io = gavf_io_create_tls_client(fd, p->host, GAVF_IO_SOCKET_DO_CLOSE);
    else
      p->ts_io = gavf_io_create_socket(fd, 1000, GAVF_IO_SOCKET_DO_CLOSE);
    
    gavl_dictionary_reset(&p->ts_req);
    gavl_http_request_init(&p->ts_req,  "GET", path, "HTTP/1.1");
    gavl_dictionary_set_string_nocopy(&p->ts_req, "Host", http_host);
    http_host = NULL;
    
    if(!gavl_http_request_write(p->ts_io, &p->ts_req))
      goto fail;
    }

  if(!gavl_http_response_read(p->ts_io, &res))
    {
    goto fail;
    }

  if(gavl_http_response_get_status_int(&res) != 200)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Got code: %d %s",
             gavl_http_response_get_status_int(&res),
             gavl_http_response_get_status_str(&res));
    goto fail;
    }
  
  if(!gavl_dictionary_get_long(&res, "Content-Length", &p->ts_len))
    {
    var = gavl_dictionary_get_string_i(&res, "Transfer-Encoding");
    if(var && !strcasecmp(var, "chunked"))
      p->ts_chunked = 1;
    goto fail;
    }
  else
    {
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Segment size: %"PRId64, p->ts_len);
    }
  
  if((var = gavl_dictionary_get_string(&res, "Content-Type")) &&
     !(gavl_dictionary_get_string(&ctx->m, GAVL_META_MIMETYPE)))
    gavl_dictionary_set_string(&ctx->m, GAVL_META_MIMETYPE, var);
  
  p->ts_pos = 0;
  ret = 1;
  
  fail:

  if(http_host)
    free(http_host);
  
  return ret;
  }

static int open_hls(bgav_input_context_t * ctx, const char * url, char ** r)
  {
  int ret = 0;
  char * path = NULL;
  char * protocol = NULL;
  int port = 0;

  hls_priv_t * priv = calloc(1, sizeof(*priv));

  ctx->priv = priv;

  ctx->url = gavl_strdup(url);

  if(!bgav_url_split(ctx->url,
                     &protocol,
                     NULL, /* User */
                     NULL, /* Pass */
                     &priv->host,
                     &port,
                     &path))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Unvalid URL");
    goto fail;
    }

  if(!strcmp(protocol, "hlss"))
    priv->use_tls_m3u = 1;

  priv->m3u_addr = gavl_socket_address_create();
  
  gavl_http_request_init(&priv->m3u_req,  "GET", path, "HTTP/1.1");

  if(port > 0)
    gavl_dictionary_set_string_nocopy(&priv->m3u_req, "Host", gavl_sprintf("%s:%d", priv->host, port));
  else
    gavl_dictionary_set_string(&priv->m3u_req, "Host", priv->host);
  
  if(port < 0)
    {
    if(priv->use_tls_m3u)
      port = 443;
    else
      port = 80;
    }

  if(!gavl_socket_address_set(priv->m3u_addr, priv->host, port, SOCK_STREAM))
    goto fail;

  gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Connecting to: %s:%d", priv->host, port);
  
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
  
  if(path)
    free(path);
  if(protocol)
    free(protocol);
  
  
  return ret;
  }

static void seek_time_hls(bgav_input_context_t * ctx, int64_t t1, int scale)
  {
  
  }

static int read_hls(bgav_input_context_t* ctx,
                    uint8_t * buffer, int len)
  {
  int bytes_to_read = 0;
  int bytes_read = 0;
  int result;
  
  hls_priv_t * p = ctx->priv;

  while(1)
    {

    bytes_to_read = len;

    if(bytes_to_read + p->ts_pos > p->ts_len)
      bytes_to_read = p->ts_len - p->ts_pos;
    
    if(bytes_to_read > 0)
      {
      result = gavf_io_read_data(p->ts_io, buffer + bytes_read, bytes_to_read);
      if(result < bytes_to_read)
        {
        if(result > 0)
          {
          bytes_read += result;
          }
        return bytes_read;
        }
      len -= result;
      p->ts_pos += result;
      bytes_read += result;
      }

    if(!len)
      {
      return bytes_read;
      }

    if(p->ts_len == p->ts_pos)
      {
      p->seq_cur++;
      if(!open_ts(ctx))
        return bytes_read;
      }
    }
  
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
  
  gavl_array_free(&p->segments);
  gavl_dictionary_free(&p->m3u_req);
  gavl_dictionary_free(&p->ts_req);
  
  if(p->m3u_addr)
    gavl_socket_address_destroy(p->m3u_addr);
  if(p->ts_addr)
    gavl_socket_address_destroy(p->ts_addr);

  gavl_buffer_free(&p->m3u_buf);
  
  if(p->host)
    free(p->host);
  
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

