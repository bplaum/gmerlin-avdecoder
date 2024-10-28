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



#include <avdec_private.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define STREAM_ID 1

#define DO_BLOCK 0

#define LOG_DOMAIN "vtt"

typedef struct
  {
  gavl_time_t time_offset;

  gavl_buffer_t buf;
  int got_cr;
  } vtt_t;

static int read_line(bgav_demuxer_context_t * ctx, int block);
static void flush_line(bgav_demuxer_context_t * ctx);


static const char * webvtt_signatures[] =
  {
    (const char[]){ 0xEF, 0xBB, 0xBF, 0x57, 0x45, 0x42, 0x56, 0x54, 0x54, 0x00 },
    (const char[]){ 0x57, 0x45, 0x42, 0x56, 0x54, 0x54, 0x00  },
    NULL,
  };

#define MAX_SIG_LEN 9

static int is_vtt_sig(const void * str)
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

static int parse_time_map(const char * str, gavl_time_t * offset)
  {
  const char * pos;
  gavl_time_t mpeg_time = GAVL_TIME_UNDEFINED;
  gavl_time_t vtt_time = GAVL_TIME_UNDEFINED;

  if((pos = strstr(str, "MPEGTS:")))
    {
    mpeg_time = strtoll(pos + strlen("MPEGTS:"), NULL, 10);
    mpeg_time = gavl_time_unscale(90000, mpeg_time);
    }
  if((pos = strstr(str, "LOCAL:")))
    gavl_time_parse(pos + strlen("LOCAL:"), &vtt_time);

  if((mpeg_time != GAVL_TIME_UNDEFINED) &&
     (vtt_time != GAVL_TIME_UNDEFINED))
    {
    /* packet timestamp = vtt_time - vtt_offset + mpeg_offset; */
    *offset = mpeg_time - vtt_time;
    return 1;
    }
  else
    return 0;
  }


static int open_vtt(bgav_demuxer_context_t * ctx)
  {
  /* X-TIMESTAMP-MAP= */
  // #define PROBE_LEN 16
  //  char probe_buffer[PROBE_LEN+1];
  
  
  bgav_stream_t * s;
  vtt_t * priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
  
  ctx->flags |= BGAV_DEMUXER_DISCONT;
  
  ctx->tt = bgav_track_table_create(1);
  
  priv->time_offset = GAVL_TIME_UNDEFINED;
  
  s = bgav_track_add_text_stream(ctx->tt->cur, ctx->opt, GAVL_UTF8);
  s->stream_id = STREAM_ID;
  s->timescale = GAVL_TIME_SCALE;

  bgav_track_set_format(ctx->tt->cur, "WEBVTT", "text/vtt");

  /* Skip signature */
#if 0
  read_line(ctx, 1);  
  flush_line(ctx);
  
  probe_buffer[PROBE_LEN] = '\0';
  if(bgav_input_get_data(ctx->input, (uint8_t*)probe_buffer, PROBE_LEN) &&
     !strcmp(probe_buffer, "X-TIMESTAMP-MAP="))
    {
    read_line(ctx, 1);  
    parse_time_map((char*)priv->buf.buf, &priv->time_offset);
    flush_line(ctx);
    }
#endif
  
  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_TIME)
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
  
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

static void add_char(gavl_buffer_t * buf, uint8_t c)
  {
  gavl_buffer_append_data(buf, &c, 1);
  /* Ugly hack but should be quite robust */
  if(c == '\0')
    buf->len--;
  }

static void flush_line(bgav_demuxer_context_t * ctx)
  {
  vtt_t * priv = ctx->priv;
  gavl_buffer_reset(&priv->buf);
  priv->got_cr = 0;
  }

static int read_line(bgav_demuxer_context_t * ctx, int block)
  {
  char c;
  vtt_t * priv = ctx->priv;
  
  while(1)
    {
    if(block)
      {
      if(!bgav_input_read_data(ctx->input, (uint8_t*)(&c), 1))
        {
        //      return 0;
        add_char(&priv->buf, '\0');
        return !!priv->buf.len;
        break;
        }
      }
    else
      {
      /* TODO: Handle EOF */
      if(!bgav_input_read_nonblock(ctx->input, (uint8_t*)(&c), 1))
        return 0;
      }
    
    if(c == '\r')
      {
      priv->got_cr = 1;
      add_char(&priv->buf, '\0');
      return 1;
      }
    else if(c == '\n')
      {
      if(!priv->got_cr)
        {
        add_char(&priv->buf, '\0');
        return 1;
        }
      else
        priv->got_cr = 0;
      }
    else
      {
      add_char(&priv->buf, c);
      }
    }
  
  }

static const struct
  {
  const char * vtt_name;
  const char * pango_name;
  }
colors[] =
  {
    { "white",   "#FFFFFF" },
    { "lime",    "#00FF00" },
    { "green",   "#00FF00" },
    { "cyan",    "#00FFFF" },
    { "red",     "#FF0000" },
    { "yellow",  "#FFFF00" },
    { "magenta", "#FF00FF" },
    { "blue",    "#0000FF" },
    { "black",   "#000000" },
    { /* End */            },
  };

static const char * get_color(const char * c)
  {
  int i = 0;

  while(colors[i].vtt_name)
    {
    if(!strcmp(c, colors[i].vtt_name))
      return colors[i].pango_name;
    i++;
    }
  return NULL;
  }

static void append_payload_line(gavl_packet_t * p, gavl_buffer_t * buf)
  {
  const char * pos = (const char*)buf->buf;
  const char * end = (const char*)(buf->buf + buf->len);

  //  fprintf(stderr, "Append payload line: %s\n", pos);
  
  while(pos < end)
    {
    if(*pos == '<')
      {
      // Class
      if(gavl_string_starts_with((char*)pos, "<c."))
        {
        int idx;
        char * tag;
        char ** classes;
        char * tmp_string;
        
        const char * fg = NULL;
        const char * bg = NULL;
        const char * tag_end;
        const char * col = NULL;

        pos+=3;
        tag_end = strchr(pos, '>');
        
        tag = gavl_strndup(pos, tag_end);
        classes = gavl_strbreak(tag, '.');

        idx = 0;
        while(classes[idx])
          {
          if(gavl_string_starts_with(classes[idx], "bg_"))
            {
            if((col = get_color(classes[idx] + 3)))
              bg = col;
            }
          else
            {
            if((col = get_color(classes[idx])))
              fg = col;
            }
          idx++;
          }
        gavl_buffer_append_data(&p->buf, (const uint8_t*)"<span", 5);
        
        if(fg)
          {
          tmp_string = gavl_sprintf(" foreground=\"%s\"", fg);
          gavl_buffer_append_data(&p->buf, (const uint8_t*)tmp_string, strlen(tmp_string));
          free(tmp_string);
          }
        if(bg)
          {
          tmp_string = gavl_sprintf(" background=\"%s\"", bg);
          gavl_buffer_append_data(&p->buf, (const uint8_t*)tmp_string, strlen(tmp_string));
          free(tmp_string);
          }
        gavl_buffer_append_data(&p->buf, (const uint8_t*)">", 1);
        
        gavl_strbreak_free(classes);
        free(tag);
        
        pos = tag_end + 1;
        }
      else if(gavl_string_starts_with((char*)pos, "</c>"))
        {
        gavl_buffer_append_data(&p->buf, (const uint8_t*)"</span>", 7);
        pos+=4;
        }
      else if(gavl_string_starts_with((char*)pos, "<b>") ||
         gavl_string_starts_with((char*)pos, "<i>") ||
         gavl_string_starts_with((char*)pos, "<u>"))
        {
        gavl_buffer_append_data(&p->buf, (const uint8_t*)pos, 3);
        pos+=3;
        }
      else if(gavl_string_starts_with((char*)pos, "</b>") ||
              gavl_string_starts_with((char*)pos, "</i>") ||
              gavl_string_starts_with((char*)pos, "</u>"))
        {
        gavl_buffer_append_data(&p->buf, (const uint8_t*)pos, 4);
        pos+=4;
        }
      /* Escape unknown tags */
      else
        {
        gavl_buffer_append_data(&p->buf, (const uint8_t*)"&lt;", 4);
        pos++;
        }
      }
    else if(*pos == '>')
      {
      gavl_buffer_append_data(&p->buf, (const uint8_t*)"&gt;", 4);
      pos++;
      }
    else
      {
      gavl_buffer_append_data(&p->buf, (const uint8_t*)pos, 1);
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
  //  fprintf(stderr, "next_packet_vtt...");
  
  s = bgav_track_find_stream(ctx, STREAM_ID);
  
  if(!s)
    return GAVL_SOURCE_OK;
  
  while(1)
    {
    if(!read_line(ctx, DO_BLOCK))
      {
      //      fprintf(stderr, "got no line\n");
      return GAVL_SOURCE_AGAIN; // Non-Block
      }
    /* TODO: Detect EOF and errors */
    
    //      return GAVL_SOURCE_EOF;
    //    fprintf(stderr, "Got line: %s\n", (char*)priv->buf.buf);
    gavl_strtrim((char*)priv->buf.buf);
    priv->buf.len = strlen((char*)priv->buf.buf);
    
    if(priv->buf.buf[0] == '\0')
      {
      /* Termination */
      if(s->packet)
        {
        PACKET_SET_KEYFRAME(s->packet);
        
        //        fprintf(stderr, "Got packet\n");
        //        gavl_packet_dump(s->packet);
        
        bgav_stream_done_packet_write(s, s->packet);
        s->packet = NULL;

        flush_line(ctx);
        return GAVL_SOURCE_OK;
        }
      else
        {
        flush_line(ctx);
        continue;
        }
      }
    
    if(is_vtt_sig(priv->buf.buf))
      {
      flush_line(ctx);
      continue;
      }
    else if(gavl_string_starts_with((char*)priv->buf.buf, "X-TIMESTAMP-MAP="))
      {
      parse_time_map((char*)priv->buf.buf, &priv->time_offset);
      }
    else if((result = parse_time((char*)priv->buf.buf, &start, &end)))
      {
      /* TODO: Parse what comes after the times */
      
      start += priv->time_offset;
      end += priv->time_offset;
      
      s->packet           = bgav_stream_get_packet_write(s);
      s->packet->pts      = start;
      s->packet->duration = end - start;
      bgav_input_set_demuxer_pts(ctx->input, s->packet->pts, GAVL_TIME_SCALE);
      }
    else if(s->packet) /* Payload */
      {
      if(s->packet->buf.len)
        gavl_buffer_append_data(&s->packet->buf, (uint8_t*)"\n", 1);

      append_payload_line(s->packet, &priv->buf);
      
      }
    else
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Unknown line: %s", (char*)priv->buf.buf);
      }
    flush_line(ctx);
    }
  return GAVL_SOURCE_EOF;
  }

static void close_vtt(bgav_demuxer_context_t * ctx)
  {
  vtt_t * priv = ctx->priv;
  gavl_buffer_free(&priv->buf);
  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_vtt =
  {
    .probe =       probe_vtt,
    .open =        open_vtt,
    .next_packet = next_packet_vtt,
    .close =       close_vtt
  };
