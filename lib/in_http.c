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
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <avdec_private.h>
#include <http.h>
#include <hls.h>

#define NUM_REDIRECTIONS 5

#define LOG_DOMAIN "in_http"

/* Generic http input module */

typedef struct
  {
  int64_t total_bytes_read;

  int icy_metaint;
  int icy_bytes;
  bgav_http_t * h;

  bgav_charset_converter_t * charset_cnv;

  //  bgav_hls_t * hls;

  int64_t bytes_read;
  
  } http_priv;

static void create_header(gavl_dictionary_t * ret, const bgav_options_t * opt)
  {
  gavl_dictionary_set_string(ret, "User-Agent", PACKAGE"/"VERSION);
  gavl_dictionary_set_string(ret, "Accept", "*");
  
  if(opt->http_shoutcast_metadata)
    gavl_dictionary_set_string(ret, "Icy-MetaData", "1");

  gavl_dictionary_set_string(ret, "GetContentFeatures.DLNA.ORG", "1");
  }

static int open_http(bgav_input_context_t * ctx, const char * url, char ** r)
  {
  const char * var;
  http_priv * p;

  const gavl_dictionary_t * res;
  
  gavl_dictionary_t extra_header;
  gavl_dictionary_init(&extra_header);
  
  p = calloc(1, sizeof(*p));
  
  create_header(&extra_header, ctx->opt);
  
  p->h = bgav_http_open(url, ctx->opt, r, &extra_header);

  gavl_dictionary_free(&extra_header);
  
  if(!p->h)
    {
    free(p);
    return 0;
    }
  
  ctx->priv = p;

  ctx->total_bytes = bgav_http_total_bytes(p->h);
  
  res = bgav_http_get_header(p->h);
  bgav_http_set_metadata(p->h, &ctx->m);
  
  //  bgav_http_header_dump(header);
  
  var = gavl_dictionary_get_string(res, "icy-metaint");
  if(var)
    {
    p->icy_metaint = atoi(var);
    //    p->icy_bytes = p->icy_metaint;
    /* Then, we'll also need a charset converter */

    p->charset_cnv = bgav_charset_converter_create(ctx->opt,
                                                   "ISO-8859-1",
                                                   BGAV_UTF8);
    }

  var = gavl_dictionary_get_string(res, "Accept-Ranges");
  if(!var || strcasecmp(var, "bytes"))
    ctx->flags &= ~BGAV_INPUT_CAN_SEEK_BYTE;
  else
    ctx->flags |= (BGAV_INPUT_SEEK_SLOW | BGAV_INPUT_CAN_PAUSE);
  
  //  ctx->flags |= BGAV_INPUT_DO_BUFFER;

  ctx->url = gavl_strdup(url);
  return 1;
  }

static int64_t seek_byte_http(bgav_input_context_t * ctx,
                              int64_t pos, int whence)
  {
  http_priv * p = ctx->priv;

  gavl_dictionary_t extra_header;
  gavl_dictionary_init(&extra_header);

  if(p->h)
    {
    bgav_http_close(p->h);
    p->h = NULL;
    }
  
  create_header(&extra_header, ctx->opt);

  gavl_dictionary_set_string_nocopy(&extra_header, "Range",
                                    bgav_sprintf("bytes=%"PRId64"-", ctx->position));
  
  p->h = bgav_http_open(ctx->url, ctx->opt, NULL, &extra_header);

  gavl_dictionary_free(&extra_header);

  p->bytes_read = ctx->position;
  
  return ctx->position;
  }

static void pause_http(bgav_input_context_t * ctx)
  {
  http_priv * p = ctx->priv;
  bgav_http_close(p->h);
  p->h = NULL;
  }

static void resume_http(bgav_input_context_t * ctx)
  {
  gavl_dictionary_t extra_header;

  http_priv * p = ctx->priv;
  gavl_dictionary_init(&extra_header);

  create_header(&extra_header, ctx->opt);

  gavl_dictionary_set_string_nocopy(&extra_header, "Range",
                                    bgav_sprintf("bytes=%"PRId64"-", p->bytes_read));

  p->h = bgav_http_open(ctx->url, ctx->opt, NULL, &extra_header);
  gavl_dictionary_free(&extra_header);
  }

static int read_data(bgav_input_context_t* ctx,
                     uint8_t * buffer, int len)
  {
  http_priv * p = ctx->priv;
#if 0  
  if(p->hls)
    return bgav_hls_read(p->hls, buffer, len);
  else
    {
#endif
    int ret = bgav_http_read(p->h, buffer, len);
    //    fprintf(stderr, "
    p->bytes_read += ret;

    if((ret < len) && (ctx->total_bytes > 0) && (p->bytes_read < ctx->total_bytes))
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Premature end of stream, reconnecting...");

      pause_http(ctx);
      resume_http(ctx);

      ret = bgav_http_read(p->h, buffer, len);
      p->bytes_read += ret;
      }
    
    return ret;
#if 0  

    }
#endif

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
                                            bgav_convert_string(priv->charset_cnv ,
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
        p->total_bytes_read += result;
        
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

  if(p->h)
    bgav_http_close(p->h);
  
  if(p->charset_cnv)
    bgav_charset_converter_destroy(p->charset_cnv);
#if 0
  if(p->hls)
    bgav_hls_close(p->hls);
#endif
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

