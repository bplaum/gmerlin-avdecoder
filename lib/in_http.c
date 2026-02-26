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
#include <stdio.h>
#include <unistd.h>
#include <avdec_private.h>
#include <hls.h>
#include <gavl/http.h>
#include <gavl/io.h>

#define LOG_DOMAIN "in_http"

/* Generic http input module */

/* Streams with a content-length smaller than this will be
   downloaded at once and buffered */
#define MAX_DOWNLOAD_SIZE (100*1024*1024)

typedef struct
  {
  int icy_metaint;
  int icy_bytes;

  gavl_io_t * io;
  
  gavl_charset_converter_t * charset_cnv;
  int64_t bytes_read;

  gavl_buffer_t buffer;
  
  } http_priv;

static void create_header(gavl_dictionary_t * ret, const bgav_options_t * opt)
  {
  gavl_dictionary_set_string(ret, "User-Agent", PACKAGE"/"VERSION);
  gavl_dictionary_set_string(ret, "Icy-MetaData", "1");
  gavl_dictionary_set_string(ret, "GetContentFeatures.DLNA.ORG", "1");
  }

static char const * const title_vars[] =
  {
    "icy-name",
    "ice-name",
    "x-audiocast-name",
    NULL
  };

static char const * const genre_vars[] =
  {
    "x-audiocast-genre",
    "icy-genre",
    "ice-genre",
    NULL
  };

static char const * const comment_vars[] =
  {
    "ice-description",
    "x-audiocast-description",
    NULL
  };

static char const * const url_vars[] =
  {
    "icy-url",
    NULL
  };

static void set_metadata_string(const gavl_dictionary_t * header,
                                char const * const vars[],
                                gavl_dictionary_t * m, const char * name)
  {
  const char * val;
  int i = 0;
  while(vars[i])
    {
    val = gavl_dictionary_get_string(header, vars[i]);
    if(val)
      {
      gavl_dictionary_set_string(m, name, val);
      return;
      }
    else
      i++;
    }
  }


static void set_metadata(const gavl_dictionary_t * header, gavl_dictionary_t * m)
  {
  int bitrate = 0;
  const char * var;
  gavl_dictionary_t * src;

  /* Get content type */

  if((src = gavl_metadata_get_src_nc(m, GAVL_META_SRC, 0)))
    {
    var = gavl_dictionary_get_string_i(header, "Content-Type");
    if(var)
      {
      /* Special hack for radio-browser.info */
      if(!strcasecmp(var, "application/octet-stream"))
        {
        if((var = gavl_dictionary_get_string_i(header, "Content-Disposition")) &&
           (gavl_string_starts_with(var, "attachment;")) &&
           (var = strstr(var, "filename=")) &&
           (var = strrchr(var, '.')))
          {
          if(!strcmp(var, ".pls"))
            gavl_dictionary_set_string(src, GAVL_META_MIMETYPE, "audio/x-scpls");
          else if(!strcmp(var, ".m3u"))
            gavl_dictionary_set_string(src, GAVL_META_MIMETYPE, "audio/x-mpegurl");
          }
        }
      else
        gavl_dictionary_set_string(src, GAVL_META_MIMETYPE, var);
      }
    else if(gavl_dictionary_get_string_i(header, "icy-notice1"))
      gavl_dictionary_set_string(src, GAVL_META_MIMETYPE, "audio/mpeg");
    }
  
  /* Get Metadata */
  
  set_metadata_string(header,
                      title_vars, m, GAVL_META_STATION);
  set_metadata_string(header,
                      genre_vars, m, GAVL_META_GENRE);
  set_metadata_string(header,
                      comment_vars, m, GAVL_META_COMMENT);
  set_metadata_string(header,
                      url_vars, m, GAVL_META_RELURL);

  /* Duration for lpcm streams from upnp servers */
  if((var = gavl_dictionary_get_string(header, "X-AvailableSeekRange")))
    {
    gavl_time_t start, end;
    int len;
    
    fprintf(stderr, "X-AvailableSeekRange: %s", var);
    
    if((var = strstr(var, "ntp=")) &&
       (var += 4) &&
       (len = gavl_time_parse(var, &start)) &&
       (var += len) &&
       (*var == '-') &&
       (var++) &&
       gavl_time_parse(var, &end))
      {
      if(start == 0)
        gavl_dictionary_set_long(m, GAVL_META_APPROX_DURATION, end);
      }
    
    }
  
  if((var = gavl_dictionary_get_string(header, "icy-br")))
    bitrate = atoi(var);
  else if((var = gavl_dictionary_get_string(header, "ice-audio-info")))
    {
    var = strstr(var, "bitrate=");
    if(var)
      bitrate = atoi(var + 8);
    }
  if(bitrate)
    gavl_dictionary_set_int(m, GAVL_META_BITRATE, bitrate * 1000);
  }


static int open_http(bgav_input_context_t * ctx, const char * url1, char ** r)
  {
  int ret = 0;
  const char * var;
  http_priv * p;

  gavl_dictionary_t * info;
  const gavl_dictionary_t * res;

  gavl_dictionary_t extra_header;
  gavl_dictionary_init(&extra_header);

  ctx->location = gavl_strdup(url1);
  
  p = calloc(1, sizeof(*p));
  ctx->priv = p;

  create_header(&extra_header, &ctx->opt);
  
  p->io = gavl_http_client_create();

  gavl_http_client_set_req_vars(p->io, &extra_header);

  if(!gavl_http_client_open(p->io, "GET", url1))
    goto fail;
  
  res = gavl_http_client_get_response(p->io);

  ctx->total_bytes = gavl_io_total_bytes(p->io);
  
  set_metadata(res, &ctx->m);

  info = gavl_io_info(p->io);
  if((var = gavl_dictionary_get_string(info, GAVL_META_REAL_URI)))
    gavl_dictionary_set_string(&ctx->m, GAVL_META_REAL_URI, var);
  
  var = gavl_dictionary_get_string(res, "icy-metaint");
  if(var)
    {
    p->icy_metaint = atoi(var);
    /* Then, we'll also need a charset converter */

    p->charset_cnv = gavl_charset_converter_create("ISO-8859-1",
                                                   GAVL_UTF8);
    }

  if(gavl_io_can_seek(p->io))
    ctx->flags |= (BGAV_INPUT_SEEK_SLOW | BGAV_INPUT_CAN_PAUSE);
  else
    ctx->flags &= ~BGAV_INPUT_CAN_SEEK_BYTE;
  
  ret = 1;
  fail:
  
  gavl_dictionary_free(&extra_header);
  
  return ret;
  }

static int64_t seek_byte_http(bgav_input_context_t * ctx,
                              int64_t pos, int whence)
  {
  http_priv * p = ctx->priv;
  //  fprintf(stderr, "seek_byte_http %"PRId64" %"PRId64"\n", pos, ctx->position);
  gavl_io_seek(p->io, pos, whence);
  return ctx->position;
  }

static void pause_http(bgav_input_context_t * ctx)
  {
  http_priv * p = ctx->priv;
  //  fprintf(stderr, "pause_http\n");
  gavl_http_client_pause(p->io);
  }

static void resume_http(bgav_input_context_t * ctx)
  {
  http_priv * p = ctx->priv;
  //  fprintf(stderr, "resume_http\n");
  gavl_http_client_resume(p->io);
  }

static int read_data(bgav_input_context_t* ctx,
                     uint8_t * buffer, int len)
  {
  int ret;
  http_priv * p = ctx->priv;
  ret = gavl_io_read_data(p->io, buffer, len);
  return ret;
  }

static void * memscan(void * mem_start, int size, void * key, int key_len)
  {
  void * mem = mem_start;


  while(mem - mem_start < size - key_len)
    {
    if(!memcmp(mem, key, key_len))
      return mem;
    mem++;
    }
  return NULL;
  }

static int read_shoutcast_metadata(bgav_input_context_t* ctx)
  {
  char * meta_buffer;
  
  const char * pos, *end_pos;
  uint8_t icy_len;
  int meta_bytes;
  http_priv * priv;
  
  priv = ctx->priv;
    
  if(!read_data(ctx, &icy_len, 1))
    {
    return 0;
    }
  meta_bytes = icy_len * 16;
  
  //  fprintf(stderr, "Got ICY metadata %d bytes\n", meta_bytes);
  
  if(meta_bytes)
    {
    meta_buffer = malloc(meta_bytes);
    
    /* Metadata block is read in blocking mode!! */
    
    if(read_data(ctx, (uint8_t*)meta_buffer, meta_bytes) < meta_bytes)
      return 0;

    //    fprintf(stderr, "Got metadata block\n");
    //    gavl_hexdump((uint8_t*)meta_buffer, meta_bytes, 16);
    
    if((ctx->tt) && (pos = memscan(meta_buffer, meta_bytes, "StreamTitle='", 13)))
      {
      pos+=13;
      end_pos = strchr(pos, ';');
      
      if(end_pos)
        {
        end_pos--; // ; -> '

        if(bgav_utf8_validate((const uint8_t*)pos, (const uint8_t *)end_pos))
          {
          gavl_dictionary_set_string_nocopy(ctx->tt->cur->metadata,
                                            GAVL_META_LABEL,
                                            gavl_strndup(pos, end_pos));
          }
        else
          {
          gavl_dictionary_set_string_nocopy(ctx->tt->cur->metadata,
                                            GAVL_META_LABEL,
                                            gavl_convert_string(priv->charset_cnv ,
                                                                pos, end_pos - pos,
                                                                NULL));
          }
        


        
        bgav_metadata_changed(ctx->b, ctx->tt->cur->metadata);

#if 0        
        fprintf(stderr, "Got ICY metadata: %s, %f\n",
                gavl_dictionary_get_string(ctx->tt->cur->metadata, GAVL_META_LABEL),
                gavl_time_to_seconds(timestamp));
#endif   
        }
      }
    free(meta_buffer);
    }
  return 1;
  }

static int do_read(bgav_input_context_t* ctx,
                   uint8_t * buffer, int len)
  {
  int bytes_to_read;
  int bytes_read = 0;

  int result;
  http_priv * p = ctx->priv;

  if(!p->icy_metaint) 
    return read_data(ctx, buffer, len);
  else
    {
    while(bytes_read < len)
      {
      /* Read data chunk */
      
      bytes_to_read = len - bytes_read;

      if(p->icy_bytes + bytes_to_read > p->icy_metaint)
        bytes_to_read = p->icy_metaint - p->icy_bytes;

      if(bytes_to_read)
        {
        result = read_data(ctx, buffer + bytes_read, bytes_to_read);
        bytes_read += result;
        p->icy_bytes += result;
        if(result < bytes_to_read)
          return bytes_read;
        }

      /* Read metadata */

      if(p->icy_bytes == p->icy_metaint)
        {
        if(!read_shoutcast_metadata(ctx))
          return bytes_read;
        else
          p->icy_bytes = 0;
        }
      }
    }
  return bytes_read;
  }

static int read_http(bgav_input_context_t* ctx,
                     uint8_t * buffer, int len)
  {
  return do_read(ctx, buffer, len);
  }


static void close_http(bgav_input_context_t * ctx)
  {
  http_priv * p = ctx->priv;

  if(p->io)
    gavl_io_destroy(p->io);
  
  if(p->charset_cnv)
    gavl_charset_converter_destroy(p->charset_cnv);
  free(p);
  }


const bgav_input_t bgav_input_http =
  {
    .name =          "http",
    .open =          open_http,
    .read =          read_http,
    .seek_byte     = seek_byte_http,

    .pause         = pause_http,
    .resume         = resume_http,
    
    .close =         close_http,
  };

