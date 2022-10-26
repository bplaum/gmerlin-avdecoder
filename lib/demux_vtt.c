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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define STREAM_ID 0

#define LOG_DOMAIN "vtt"

typedef struct
  {
  gavl_time_t time_offset;

  char * buf;
  uint32_t buf_alloc;
  } vtt_t;

#define STREAM_ID 0

static const char * webvtt_signatures[] =
  {
    (const char[]){ 0xEF, 0xBB, 0xBF, 0x57, 0x45, 0x42, 0x56, 0x54, 0x54, 0x00 },
    (const char[]){ 0x57, 0x45, 0x42, 0x56, 0x54, 0x54, 0x00  },
    NULL,
  };

#define MAX_SIG_LEN 9

static int is_vtt_sig(const char * str)
  {
  int idx = 0;

  while(webvtt_signatures[idx])
    {
    if(gavl_string_starts_with(str, webvtt_signatures[idx]))
      return 1;
    idx++;
    }
  return 0;
  }

static int probe_vtt(bgav_input_context_t * input)
  {
  uint8_t data[MAX_SIG_LEN+1];
  if(bgav_input_get_data(input, data, MAX_SIG_LEN) < MAX_SIG_LEN)
    return 0;
  data[MAX_SIG_LEN] = '\0';
  return is_vtt_sig((char*)data);
  }

static int open_vtt(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;
  vtt_t * priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
  
  ctx->flags |= BGAV_DEMUXER_DISCONT;
  
  ctx->tt = bgav_track_table_create(1);

  s = bgav_track_add_text_stream(ctx->tt->cur, ctx->opt, BGAV_UTF8);
  s->stream_id = STREAM_ID;
  s->timescale = GAVL_TIME_SCALE;

  bgav_track_set_format(ctx->tt->cur, "WEBVTT", "text/vtt");
  return 1;
  }

static int parse_time(const char * str_start, gavl_time_t * start, gavl_time_t * end)
  {
  int result;
  const char * str = str_start;
  
  result = gavl_time_parse(str, start);

  if(!result)
    return 0;

  str += result;

  while(isspace(*str) && (*str != '\0'))
    str++;

  if(*str == '\0')
    return 0;

  if(!gavl_string_starts_with(str, "-->"))
    return 0;

  str += 3;

  while(isspace(*str) && (*str != '\0'))
    str++;

  if(*str == '\0')
    return 0;

  result = gavl_time_parse(str, end);
  
  if(!result)
    return 0;

  str += result;
  
  return str - str_start;
  }

#define ALLOC_SIZE 128

static void add_char(char ** buffer, uint32_t * buffer_alloc,
                     int pos, char c)
  {
  if(pos + 1 > *buffer_alloc)
    {
    while(pos + 1 > *buffer_alloc)
      (*buffer_alloc) += ALLOC_SIZE;
    *buffer = realloc(*buffer, *buffer_alloc);
    }
  (*buffer)[pos] = c;
  }

static int read_line(bgav_demuxer_context_t * ctx)
  {
  int pos = 0;
  char c;
  vtt_t * priv = ctx->priv;

  while(1)
    {
    if(!bgav_input_read_data(ctx->input, (uint8_t*)(&c), 1))
      {
      //      return 0;
      add_char(&priv->buf, &priv->buf_alloc, pos, '\0');
      return !!pos;
      break;
      }

    else if(c == '\r')
      {
      if(bgav_input_get_data(ctx->input, (uint8_t*)(&c), 1) && (c == '\n'))
        bgav_input_read_data(ctx->input, (uint8_t*)(&c), 1);
      add_char(&priv->buf, &priv->buf_alloc, pos, '\0');
      return 1;
      }
    else if(c == '\n')
      {
      add_char(&priv->buf, &priv->buf_alloc, pos, '\0');
      return 1;
      }
    else
      {
      add_char(&priv->buf, &priv->buf_alloc, pos, c);
      pos++;
      }
    }
  
  }

static gavl_source_status_t next_packet_vtt(bgav_demuxer_context_t * ctx)
  {
  int result;
  bgav_stream_t * s;
  gavl_time_t start = GAVL_TIME_UNDEFINED;
  gavl_time_t end = GAVL_TIME_UNDEFINED;
  vtt_t * priv = ctx->priv;
  
  s = bgav_track_find_stream(ctx, STREAM_ID);
  
  if(!s)
    return GAVL_SOURCE_OK;
  
  while(1)
    {
    if(!read_line(ctx))
      return GAVL_SOURCE_EOF;

    gavl_strtrim(priv->buf);
    if(priv->buf[0] == '\0')
      continue;

    fprintf(stderr, "Got line: %s\n", priv->buf);
    
    if(is_vtt_sig(priv->buf))
      continue;

    else if(gavl_string_starts_with(priv->buf, "X-TIMESTAMP-MAP="))
      {
      const char * pos;
      int64_t mpeg_time = GAVL_TIME_UNDEFINED;
      gavl_time_t vtt_time = GAVL_TIME_UNDEFINED;

      if((pos = strstr(priv->buf, "MPEGTS:")))
        {
        mpeg_time = strtoll(pos + strlen("MPEGTS:"), NULL, 10);
        mpeg_time = gavl_time_unscale(90000, mpeg_time);
        }
      if((pos = strstr(priv->buf, "LOCAL:")))
        gavl_time_parse(pos + strlen("LOCAL:"), &vtt_time);

      if((mpeg_time != GAVL_TIME_UNDEFINED) &&
         (vtt_time != GAVL_TIME_UNDEFINED))
        {
        /* packet timestamp = vtt_time - vtt_offset + mpeg_offset; */
        priv->time_offset = mpeg_time - vtt_time;
        }
      }
    else if((result = parse_time(priv->buf, &start, &end)))
      {
      bgav_packet_t * p;
      start += priv->time_offset;
      end += priv->time_offset;

      p = bgav_stream_get_packet_write(s);
      p->pts = start;
      p->duration = end - start;

      /* TODO: Parse what comes after the times */

      /* Read payload */
      while(1)
        {
        int len ;
        if(!read_line(ctx))
          return GAVL_SOURCE_EOF;
        
        gavl_strtrim(priv->buf);
        if(priv->buf[0] == '\0')
          break;

        len = strlen(priv->buf);
        
        if(p->buf.len)
          {
          bgav_packet_alloc(p, p->buf.len + len + 1);
          p->buf.buf[p->buf.len] = '\n';
          p->buf.len++;
          memcpy(p->buf.buf + p->buf.len, priv->buf, len);
          p->buf.len += len;
          }
        else
          {
          bgav_packet_alloc(p, p->buf.len + len);
          memcpy(p->buf.buf + p->buf.len, priv->buf, len);
          p->buf.len += len;
          }
        }

      if(!p->buf.len)
        return GAVL_SOURCE_EOF;
      
      PACKET_SET_KEYFRAME(p);
      bgav_stream_done_packet_write(s, p);
      
      return GAVL_SOURCE_OK;
      }
    else
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Unknown line: %s", priv->buf);
      }
    
    }
  return GAVL_SOURCE_EOF;
  }

static void close_vtt(bgav_demuxer_context_t * ctx)
  {
  vtt_t * priv = ctx->priv;

  if(priv->buf)
    free(priv->buf);
  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_vtt =
  {
    .probe =       probe_vtt,
    .open =        open_vtt,
    .next_packet = next_packet_vtt,
    .close =       close_vtt
  };
