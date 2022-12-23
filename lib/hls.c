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

#include <avdec_private.h>
// #include <id3.h>

#include <http.h>
#include <hls.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define LOG_DOMAIN "hls"

typedef struct
  {
  char * url;
  char * title;
  double duration;
  int seq;
  } hls_url_t;

#define PROBE_LEN BGAV_ID3V2_DETECT_LEN

struct bgav_hls_s
  {
  uint8_t probe_buffer[PROBE_LEN];
  int probe_len;
  int probe_pos;
  
  char * url;
  
  bgav_http_t * stream_socket;
  bgav_http_t * m3u8_socket;

  gavl_dictionary_t stream_header;
  gavl_dictionary_t m3u8_header;
  
  hls_url_t * urls;
  int urls_alloc;
  int num_urls;
  
  int seq;
  int end_of_sequence; // End of sequence detected
  
  int64_t segment_pos;  // Byte counter
  int64_t segment_len; // End of segment

  bgav_input_context_t * ctx;
  
  int eof;
  };

int bgav_hls_detect(bgav_input_context_t * ctx)
  {
  const char * var = NULL;
  char * probe_buffer = NULL;
  int result = 0;

  if(!gavl_metadata_get_src(&ctx->m, GAVL_META_SRC, 0, &var, NULL) || !var)
    goto end;
  
  if(strncasecmp(var, "application/x-mpegurl", 21) && 
     strncasecmp(var, "application/vnd.apple.mpegurl", 29))
    goto end;

  if(ctx->total_bytes <= 0)
    goto end;
  
  probe_buffer = malloc(ctx->total_bytes+1);

  if(bgav_input_get_data(ctx, (uint8_t*)probe_buffer, ctx->total_bytes) <
     ctx->total_bytes)
    goto end;

  probe_buffer[ctx->total_bytes] = '\0';
  
  if(strstr(probe_buffer, "\n#EXT-X-STREAM-INF") ||
     strstr(probe_buffer, "\n#EXT-X-I-FRAME-STREAM-INF"))
    goto end;
  
  result = 1;
  end:

  if(probe_buffer)
    free(probe_buffer);
  
  return result;
  }

static void free_urls(bgav_hls_t * h)
  {
  int i;
  for(i = 0; i < h->urls_alloc; i++)
    {
    if(h->urls[i].url)
      {
      free(h->urls[i].url);
      h->urls[i].url = NULL;
      }
    if(h->urls[i].title)
      {
      free(h->urls[i].title);
      h->urls[i].title = NULL;
      }
    }
  h->num_urls = 0;
  }

#if 0
static void dump_urls(bgav_hls_t * h)
  {
  int i;
  bgav_dprintf("HLS url list\n");
  for(i = 0; i < h->num_urls; i++)
    {
    bgav_dprintf("  URL %d\n", i+1);
    bgav_dprintf("    Duration %f\n", h->urls[i].duration);
    bgav_dprintf("    Title    %s\n", h->urls[i].title);
    bgav_dprintf("    Sequence %d\n", h->urls[i].seq);
    bgav_dprintf("    URL      %s\n", h->urls[i].url);
    }
  }
#endif

static void parse_urls(bgav_hls_t * h, const char * m3u8)
  {
  int i;
  char ** lines;
  char * pos;
  int seq = 0;
  int have_extinf = 0;
  free_urls(h);

  /* Split m3u into lines */
  
  lines = gavl_strbreak(m3u8, '\n');
  i = 0;

  while(lines[i])
    {
    if((pos = strchr(lines[i], '\r')))
      *pos = '\0';
    i++;
    }

  /* Parse the header */
  i = 0;
  while(lines[i])
    {
    if(!strncasecmp(lines[i], "#EXT-X-MEDIA-SEQUENCE", 21))
      {
      if((pos = strchr(lines[i], ':')))
        {
        pos++;
        seq = atoi(pos);
        }
      }
    else if(!strncasecmp(lines[i], "#EXTINF", 7))
      break;
    i++;
    }

  /* Parse the urls */

  while(lines[i])
    {
    // URL info
    if(!strncasecmp(lines[i], "#EXTINF", 7))
      {
      if(h->urls_alloc < h->num_urls + 1)
        {
        h->urls_alloc += 16;
        h->urls = realloc(h->urls, h->urls_alloc * sizeof(*h->urls));
        memset(h->urls + h->num_urls, 0,
               (h->urls_alloc - h->num_urls) * sizeof(*h->urls));
        }
      
      if((pos = strchr(lines[i], ':')))
        {
        pos++;
        /*
         *  This breaks when the LC_NUMERIC locale is German
         *  for example
         */
        h->urls[h->num_urls].duration = strtod(pos, NULL);

        if((pos = strchr(pos, ',')))
          {
          pos++;

          while(isspace(*pos) && *pos != '\0')
            pos++;
          
          if(*pos != '\0')
            h->urls[h->num_urls].title = gavl_strdup(pos);
          }
        }
      have_extinf = 1;
      }
    else if((*(lines[i]) != '#') && have_extinf)
      {
      h->urls[h->num_urls].url = bgav_input_absolute_url(h->ctx, lines[i]);
      h->urls[h->num_urls].seq = seq++;
      h->num_urls++;
      have_extinf = 0;
      }
    else if(!strncasecmp(lines[i], "#EXT-X-ENDLIST", 14))
      {
      h->end_of_sequence = 1;
      break;
      }
    i++;
    }

  gavl_strbreak_free(lines);
  //  dump_urls(h);
  }

static void handle_id3(bgav_hls_t * h, gavl_dictionary_t * m)
  {
  int len;
  uint8_t * buf;
  bgav_input_context_t * mem;
  bgav_id3v2_tag_t * id3;
  
  //  char * 

  len = bgav_id3v2_detect(h->probe_buffer);
    
  if(!len)
    return;
  
  // fprintf(stderr, "HLS: Detected ID3 tag\n");
  
  buf = malloc(len);
  memcpy(buf, h->probe_buffer, h->probe_len);

  if(bgav_http_read(h->stream_socket, buf + h->probe_len, len - h->probe_len) < len - h->probe_len)
    {
    free(buf);
    return;
    }

  mem = bgav_input_open_memory(buf, len, h->ctx->opt);

  if((id3 = bgav_id3v2_read(mem)))
    {
    // bgav_id3v2_dump(id3);
    bgav_id3v2_2_metadata(id3, m);
    bgav_id3v2_destroy(id3);
    }

  bgav_input_close(mem);
  bgav_input_destroy(mem);
    
  h->probe_len = 0;
  h->segment_pos += len;
  free(buf);
  }

static int load_stream_url(bgav_hls_t * h, int idx)
  {
  gavl_dictionary_t m;
  gavl_dictionary_init(&m);

  gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Loading %s", h->urls[idx].url);
  
  h->stream_socket = bgav_http_reopen(h->stream_socket, h->urls[idx].url, h->ctx->opt,
                                      NULL,
                                      &h->stream_header);

  if(!h->stream_socket)
    return 0;
  
  h->segment_len = bgav_http_total_bytes(h->stream_socket);
  h->segment_pos = 0;
  
  /* Get metadata from the stream socket */
  bgav_http_set_metadata(h->stream_socket, &m);

  /* Get metadata from m3u8 */
  
  
  /* Get metadata from ID3V2 tags */

  h->probe_len = bgav_http_read(h->stream_socket, h->probe_buffer, PROBE_LEN);
  h->probe_pos = 0;

  handle_id3(h, &m);


  if(h->ctx->tt &&
     !gavl_metadata_equal(h->ctx->tt->cur->metadata, &m))
    {
    gavl_dictionary_reset(h->ctx->tt->cur->metadata);
    memcpy(h->ctx->tt->cur->metadata, &m, sizeof(m));
    gavl_dictionary_init(&m);

    bgav_metadata_changed(h->ctx->b, h->ctx->tt->cur->metadata);
    }
  gavl_dictionary_free(&m);
  return 1;
  }

bgav_hls_t * bgav_hls_create(bgav_input_context_t * ctx)
  {
  char * m3u8 = NULL;
  
  bgav_hls_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->ctx = ctx;
  
  // fprintf(stderr, "URL: %s\n", ctx->url);

  /* Read the initial m3u8 */
  m3u8 = malloc(ctx->total_bytes + 1);
  if(bgav_input_read_data(ctx, (uint8_t*)m3u8, ctx->total_bytes) <
     ctx->total_bytes)
    goto fail;

  m3u8[ctx->total_bytes] = '\0';
  parse_urls(ret, m3u8);
  free(m3u8);
  m3u8 = NULL;
  
  if(!ret->num_urls)
    goto fail;
  
  gavl_dictionary_set_string(&ret->stream_header, "User-Agent", PACKAGE"/"VERSION);
  gavl_dictionary_set_string(&ret->stream_header, "Accept", "*/*");
  gavl_dictionary_set_string(&ret->stream_header, "Connection", "Keep-Alive");

  gavl_dictionary_set_string(&ret->m3u8_header, "User-Agent", PACKAGE"/"VERSION);
  gavl_dictionary_set_string(&ret->m3u8_header, "Accept", "*/*");
  gavl_dictionary_set_string(&ret->m3u8_header, "Connection", "Keep-Alive");
  
  ret->seq = ret->urls[0].seq;
  
  if(!load_stream_url(ret, 0))
    goto fail;
  
  gavl_dictionary_reset(&ret->ctx->m);
  
  ret->ctx->total_bytes = 0;
  
  return ret;

  fail:

  if(m3u8)
    free(m3u8);
  
  bgav_hls_close(ret);
  return NULL;
  }

static int reload_m3u8(bgav_hls_t * h)
  {
  int len;
  char * buf;
  //  fprintf(stderr, "Reloading m3u8\n");

  if(!(h->m3u8_socket = bgav_http_reopen(h->m3u8_socket, h->ctx->url, h->ctx->opt,
                                         NULL,
                                         &h->m3u8_header)))
    return 0;

  len = bgav_http_total_bytes(h->m3u8_socket);
  if(len <= 0)
    return 0;
  
  buf = malloc(len+1);
  if(bgav_http_read(h->m3u8_socket, (uint8_t*)buf, len) < len)
    {
    free(buf);
    return 0;
    }
  
  buf[len] = '\0';
  parse_urls(h, buf);
  free(buf);

  //  fprintf(stderr, "Reloaded m3u8\n");
  
  return 1;
  }

int bgav_hls_read(bgav_hls_t * h, uint8_t * data, int len)
  {
  int bytes_to_read;
  int result;
  int bytes_read = 0;
  int idx;
  int i;

  if(h->eof)
    return 0;
  
  while(bytes_read < len)
    {
    if(h->segment_pos >= h->segment_len)
      {
      //      fprintf(stderr, "End of segment\n");

      /* Check if we should reload the m3u8 */
      
      if(!h->end_of_sequence && !reload_m3u8(h))
        return 0;
      
      /* Open next segment */

      h->seq++;

      idx = -1;

      for(i = 0; i < h->num_urls; i++)
        {
        //        fprintf(stderr, "Find next segment %d %d\n", h->urls[i].seq, h->seq);

        if(h->urls[i].seq == h->seq)
          {
          idx = i;
          break;
          }
        }

      if(idx < 0)
        {
        h->eof = 1;
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "No more segments");
        return bytes_read;
        }
      if(!load_stream_url(h, idx))
        return bytes_read;
      }
    
    bytes_to_read = len - bytes_read;

    /* Try probe buffer */
    if(h->probe_pos < h->probe_len)
      {
      if(bytes_to_read > h->probe_len - h->probe_pos)
        bytes_to_read = h->probe_len - h->probe_pos;

      memcpy(data + bytes_read, &h->probe_buffer[h->probe_pos], bytes_to_read);
      h->probe_pos += bytes_to_read;
      result = bytes_to_read;
      }
    else
      {
      if(bytes_to_read > h->segment_len - h->segment_pos)
        bytes_to_read =  h->segment_len - h->segment_pos;
      result = bgav_http_read(h->stream_socket, data + bytes_read, bytes_to_read);
      }
    if(result <= 0)
      return bytes_read;

    bytes_read += result;
    h->segment_pos += result;
    }

  return bytes_read;
  }

void bgav_hls_close(bgav_hls_t * h)
  {
  free_urls(h);

  if(h->urls)
    free(h->urls);

  gavl_dictionary_free(&h->stream_header);
  gavl_dictionary_free(&h->m3u8_header);
  
  if(h->stream_socket)
    bgav_http_close(h->stream_socket);
  if(h->m3u8_socket)
    bgav_http_close(h->m3u8_socket);
  
  free(h);
  }

