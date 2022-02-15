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

#define LOG_DOMAIN "in_mmsh"

typedef struct
  {
  bgav_http_t * h;

  int buffer_alloc;
  int buffer_size;
  uint8_t * buffer;
  uint8_t * pos;

  int have_header;
  } mmsh_priv;

extern bgav_demuxer_t bgav_demuxer_asf;

#define ASF_STREAMING_CLEAR     0x4324          // $C
#define ASF_STREAMING_DATA      0x4424          // $D
#define ASF_STREAMING_END_TRANS 0x4524          // $E
#define ASF_STREAMING_HEADER    0x4824          // $H

static int read_data(bgav_input_context_t* ctx,
                     uint8_t * buffer, int len, int block)
  {
  mmsh_priv * p = ctx->priv;
  return bgav_http_read(p->h, buffer, len, block);
  }

typedef struct
  {
  uint16_t type;
  uint16_t size;
  uint32_t sequence_number;
  uint16_t unknown;
  uint16_t size_confirm;
  } stream_chunck_t;

static int stream_chunk_read(bgav_input_context_t* ctx,
                             stream_chunck_t * ch, int block)
  {
  uint8_t buffer[12];
  uint8_t * pos = buffer;
  if(read_data(ctx, buffer, 12, block) < 12)
    return 0;
#ifdef WORDS_BIGENDIAN  
  ch->type            = GAVL_PTR_2_16BE(pos); pos+=2;
#else
  ch->type            = GAVL_PTR_2_16LE(pos); pos+=2;
#endif
  ch->size            = GAVL_PTR_2_16LE(pos); pos+=2;
  ch->sequence_number = GAVL_PTR_2_32LE(pos); pos+=4;
  ch->unknown         = GAVL_PTR_2_16LE(pos); pos+=2;
  ch->size_confirm    = GAVL_PTR_2_16LE(pos); pos+=2;
  return 1;
  }
#if 0
static void stream_chunk_dump(stream_chunck_t * ch)
  {
  bgav_dprintf("Type:        %c%c\n", ch->type >> 8, ch->type & 0xff);
  bgav_dprintf("Size:        %d\n",   ch->size);
  bgav_dprintf("Seq:         %d\n",   ch->sequence_number);
  bgav_dprintf("Unknown:     %d\n",   ch->unknown);
  bgav_dprintf("SizeConfirm: %d\n",   ch->size_confirm);
  }
#endif
static int open_mmsh(bgav_input_context_t * ctx, const char * url, char ** r)
  {
  const char * var;
  mmsh_priv * p;
  //  stream_chunck_t sc;
  
  gavl_dictionary_t extra_header;

  const gavl_dictionary_t * res;
  
  
  p = calloc(1, sizeof(*p));

  gavl_dictionary_init(&extra_header);

  gavl_dictionary_set_string(&extra_header, "Accept", "*/*");
  
  gavl_dictionary_set_string(&extra_header, "User-Agent", "NSPlayer/4.1.0.3856");
  gavl_dictionary_set_string(&extra_header, "Pragma", "xClientGUID={c77e7400-738a-11d2-9add-0020af0a3278}");
  gavl_dictionary_set_string(&extra_header, "Pragma", "no-cache,rate=1.000000,stream-time=0,stream-offset=0:0,request-context=1,max-duration=0");

  gavl_dictionary_set_string(&extra_header, "Connection", "Close");
  
  p->h = bgav_http_open(url, ctx->opt, r, header);
  
  if(!p->h)
    {
    free(p);
    return 0;
    }
  ctx->priv = p;
  
  bgav_http_header_destroy(header);
  
  header = bgav_http_get_header(p->h);
  
//  bgav_http_header_dump(header);

#if 0  
  var = bgav_http_header_get_var(header, "Content-Length");
  if(var)
    ctx->total_bytes = atoi(var);
#endif

  var = bgav_http_header_get_var(header, "Content-Type");
  if(var)
    gavl_dictionary_set_string(gavl_dictionary_get_src_nc(&ctx->m, GAVL_META_SRC, 0),
                               GAVL_META_MIMETYPE, var);
  
  ctx->url = gavl_strdup(url);

  ctx->demuxer = bgav_demuxer_create(ctx->b, &bgav_demuxer_asf, ctx);
  if(!bgav_demuxer_start(ctx->demuxer))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Initializing asf demuxer failed");
    return 0;
    }
  
  /* Close and reopen connection */
  bgav_http_close(p->h);
  
  //  bgav_http_header_destroy(header);
  header = bgav_http_header_create();
  
  bgav_http_header_add_line(header,
                            "Accept: */*");

  bgav_http_header_add_line(header,
                            "User-Agent: NSPlayer/4.1.0.3856");
  
  bgav_http_header_add_line(header,
                            "Pragma: xClientGUID={c77e7400-738a-11d2-9add-0020af0a3278}");
  bgav_http_header_add_line(header,
                            "Pragma: no-cache,rate=1.000000,stream-time=0,stream-offset=0:0,request-context=1,max-duration=0");
  bgav_http_header_add_line(header,
                            "Connection: Close");

  bgav_http_header_add_line(header,
                            "Pragma: xPlayStrm=1");
  bgav_http_header_add_line(header,
                            "Pragma: stream-switch-entry=");

  p->h = bgav_http_open(url, ctx->opt,
                        r, header);
  
  if(!p->h)
    {
    free(p);
    return 0;
    }

  if(p->buffer_alloc < ctx->demuxer->packet_size)
    {
    p->buffer_alloc = ctx->demuxer->packet_size;
    p->buffer = realloc(p->buffer, p->buffer_alloc);
    }
  p->pos = p->buffer;
  p->buffer_size = 0;
  
  //  header = bgav_http_get_header(p->h);
  
  //  bgav_http_header_dump(header);
  
  return 1;
  }

static int fill_buffer(bgav_input_context_t* ctx, int block)
  {
  stream_chunck_t ch;
  int len;
  mmsh_priv * p = ctx->priv;

  while(1)
    {
    if(!stream_chunk_read(ctx, &ch, block))
      return 0;
    //    stream_chunk_dump(&ch);

    len = ch.size - 8;
    
    if(p->buffer_alloc < len)
      {
      p->buffer_alloc = len + 1024;
      p->buffer = realloc(p->buffer, p->buffer_alloc);
      }
    if(read_data(ctx, p->buffer, len, 1) < len)
      return 0;
    p->pos = p->buffer;
    
    switch(ch.type)
      {
      case ASF_STREAMING_HEADER:
        if(!p->have_header)
          {
          p->buffer_size = len;
          p->have_header = 1;
          return 1;
          }
        break;
      case ASF_STREAMING_DATA:
        if(p->have_header)
          {
          if(ctx->demuxer->packet_size < len)
            return 0;
          
          /* Pad with zeros */
          if(ctx->demuxer->packet_size > len)
            memset(p->buffer + len, 0, ctx->demuxer->packet_size - len);
          p->buffer_size = ctx->demuxer->packet_size;
          return 1;
          }
        break;
      }
    
    }
  return 0;
  }

static int do_read(bgav_input_context_t* ctx,
                   uint8_t * buffer, int len, int block)
  {
  int bytes_read = 0;
  int bytes_to_copy;
  mmsh_priv * p = ctx->priv;
  
  while(bytes_read < len)
    {
    if(!p->buffer_size || (p->pos - p->buffer >= p->buffer_size))
      {
      if(!fill_buffer(ctx, block))
        return bytes_read;
      }
    bytes_to_copy = p->buffer_size - (p->pos - p->buffer);

    if(bytes_to_copy > len - bytes_read)
      bytes_to_copy = len - bytes_read;

    memcpy(buffer + bytes_read, p->pos, bytes_to_copy);
    p->pos += bytes_to_copy;
    bytes_read += bytes_to_copy;
    }
  return bytes_read;
  }

static int read_mmsh(bgav_input_context_t* ctx,
                     uint8_t * buffer, int len)
  {
  return do_read(ctx, buffer, len, 1);
  }

static int read_nonblock_mmsh(bgav_input_context_t * ctx,
                              uint8_t * buffer, int len)
  {
  return do_read(ctx, buffer, len, 0);
  }

static void close_mmsh(bgav_input_context_t * ctx)
  {
  mmsh_priv * p = ctx->priv;
  bgav_http_close(p->h);
  free(p);
  }

const bgav_input_t bgav_input_mmsh =
  {
    .name =          "mmsh",
    .open =          open_mmsh,
    .read =          read_mmsh,
    .read_nonblock = read_nonblock_mmsh,
    .close =         close_mmsh,
  };

