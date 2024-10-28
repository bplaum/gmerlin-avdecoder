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

#include <iconv.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#define LOG_DOMAIN "charset"

/* Charset detection. This detects UTF-8 and UTF-16 for now */

int bgav_utf8_validate(const uint8_t * str, const uint8_t * end)
  {
  if(end == NULL)
    end = str + strlen((char*)str);
  
  while(1)
    {
    if(str == end)
      return 1;
    /* 0xxxxxxx */
    if(!(str[0] & 0x80))
      str++;

    /* 110xxxxx 10xxxxxx */
    else if((str[0] & 0xe0) == 0xc0)
      {
      if(end - str < 2)
        return 0;
      
      if((str[1] & 0xc0) == 0x80)
        str+=2;
      else
        return 0;
      }
    
    /* 1110xxxx 10xxxxxx 10xxxxxx */
    else if((str[0] & 0xf0) == 0xe0)
      {
      if(end - str < 3)
        return 0;

      if(((str[1] & 0xc0) == 0x80) &&
         ((str[2] & 0xc0) == 0x80))
        str+=3;
      else
        return 0;
      }
    /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */

    else if((str[0] & 0xf8) == 0xf0)
      {
      if(end - str < 4)
        return 0;
      
      if(((str[1] & 0xc0) == 0x80) &&
         ((str[2] & 0xc0) == 0x80) &&
         ((str[3] & 0xc0) == 0x80))
        str+=4;
      else
        return 0;
      }
    else
      return 0;
    }
  return 1;
  }

void bgav_input_detect_charset(bgav_input_context_t * ctx)
  {
  gavl_buffer_t line_buf;
  
  int64_t old_position;
  uint8_t first_bytes[2];

  gavl_buffer_init(&line_buf);
  
  /* We need byte accurate seeking */
  if(!(ctx->flags & BGAV_INPUT_CAN_SEEK_BYTE) || !ctx->total_bytes || ctx->charset)
    return;

  old_position = ctx->position;
  
  bgav_input_seek(ctx, 0, SEEK_SET);

  if(bgav_input_get_data(ctx, first_bytes, 2) < 2)
    return;

  if((first_bytes[0] == 0xff) && (first_bytes[1] == 0xfe))
    {
    ctx->charset = gavl_strdup("UTF-16LE");
    bgav_input_seek(ctx, old_position, SEEK_SET);
    return;
    }
  else if((first_bytes[0] == 0xfe) && (first_bytes[1] == 0xff))
    {
    ctx->charset = gavl_strdup("UTF-16BE");
    bgav_input_seek(ctx, old_position, SEEK_SET);
    return;
    }
  else
    {
    while(bgav_input_read_line(ctx, &line_buf))
      {
      if(!bgav_utf8_validate(line_buf.buf, NULL))
        {
        bgav_input_seek(ctx, old_position, SEEK_SET);
        gavl_buffer_free(&line_buf);
        return;
        }
      }
    ctx->charset = gavl_strdup(GAVL_UTF8);
    bgav_input_seek(ctx, old_position, SEEK_SET);
    gavl_buffer_free(&line_buf);
    return;
    }
  bgav_input_seek(ctx, old_position, SEEK_SET);
  gavl_buffer_free(&line_buf);
  }
