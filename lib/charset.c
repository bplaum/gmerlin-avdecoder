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

#include <iconv.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#define LOG_DOMAIN "charset"

struct bgav_charset_converter_s
  {
  iconv_t cd;
  const bgav_options_t * opt;
  int utf_8_16;
  char * out_charset;
  };
  

bgav_charset_converter_t *
bgav_charset_converter_create(const bgav_options_t * opt,
                              const char * in_charset,
                              const char * out_charset)
  {
  bgav_charset_converter_t * ret = calloc(1, sizeof(*ret));

  if(!strcmp(in_charset, "bgav_unicode"))
    {
    ret->utf_8_16 = 1;
    ret->out_charset = gavl_strdup(out_charset);
    }
  else  
    ret->cd = iconv_open(out_charset, in_charset);
  ret->opt = opt;
  
  return ret;
  }

void bgav_charset_converter_destroy(bgav_charset_converter_t * cnv)
  {
  if(cnv->cd)
    iconv_close(cnv->cd);
  if(cnv->out_charset)
    free(cnv->out_charset);
  free(cnv);
  }

/*

void bgav_charset_converter_destroy(bgav_charset_converter_t *);

char * bgav_convert_string(bgav_charset_converter_t *,
                           const char * in_string, int in_len,
                           int * out_len);
*/

#define BYTES_INCREMENT 10

static int do_convert(bgav_charset_converter_t * cnv,
                      char * in_string, int len, uint32_t * out_len,
                      char ** ret, uint32_t * ret_alloc)
  {
  
  char *inbuf;
  char *outbuf;
  int output_pos;
  
  size_t inbytesleft;
  size_t outbytesleft;

  if(cnv->utf_8_16 && !cnv->cd)
    {
    /* Byte order Little Endian */
    if((len > 1) &&
       ((uint8_t)in_string[0] == 0xff) &&
       ((uint8_t)in_string[1] == 0xfe))
      cnv->cd = iconv_open(cnv->out_charset, "UTF-16LE");
    /* Byte order Big Endian */
    else if((len > 1) &&
            ((uint8_t)in_string[0] == 0xfe) &&
            ((uint8_t)in_string[1] == 0xff))
      cnv->cd = iconv_open(cnv->out_charset, "UTF-16BE");
    /* UTF-8 */
    else if(!strcmp(cnv->out_charset, BGAV_UTF8))
      {
      if(*ret_alloc < len+1)
        {
        *ret_alloc = len + BYTES_INCREMENT;
        *ret       = realloc(*ret, *ret_alloc);
        }
      strncpy(*ret, in_string, len);
      (*ret)[len] = '\0';
      if(out_len)
        *out_len = len;
      return 1;
      }
    else
      {
      cnv->cd = iconv_open(cnv->out_charset, BGAV_UTF8);
      }
    }
  
  if((*ret_alloc) < len + BYTES_INCREMENT)
    *ret_alloc = len + BYTES_INCREMENT;
  
  inbytesleft  = len;
  outbytesleft = *ret_alloc;

  *ret    = realloc(*ret, *ret_alloc);
  inbuf  = in_string;
  outbuf = *ret;
  
  while(1)
    {
    if(iconv(cnv->cd, &inbuf, &inbytesleft,
             &outbuf, &outbytesleft) == (size_t)-1)
      {
      switch(errno)
        {
        case E2BIG:
          output_pos = (int)(outbuf - *ret);

          *ret_alloc   += BYTES_INCREMENT;
          outbytesleft += BYTES_INCREMENT;

          *ret = realloc(*ret, *ret_alloc);
          outbuf = &((*ret)[output_pos]);
          break;
        case EILSEQ:
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                   "Invalid multibyte sequence");
          return 0;
          break;
        case EINVAL:
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                   "Incomplete multibyte sequence");
          return 0;
          break;
        }
      }
    if(!inbytesleft)
      break;
    }
  /* Zero terminate */

  output_pos = (int)(outbuf - *ret);
  
  if(outbytesleft < 2)
    {
    *ret_alloc+=2;
    *ret = realloc(*ret, *ret_alloc);
    outbuf = &((*ret)[output_pos]);
    }
  outbuf[0] = '\0';
  outbuf[1] = '\0';
  if(out_len)
    *out_len = outbuf - *ret;
  return 1;
  }

char * bgav_convert_string(bgav_charset_converter_t * cnv,
                           const char * str, int len,
                           uint32_t * out_len)
  {
  int result;
  char * ret = NULL;
  uint32_t ret_alloc = 0;
  char * tmp_string;

  if(len < 0)
    len = strlen(str);

  tmp_string = malloc(len+1);
  memcpy(tmp_string, str, len);
  tmp_string[len] = '\0';
  result = do_convert(cnv, tmp_string, len, out_len, &ret, &ret_alloc);
  free(tmp_string);
  
  if(!result)
    {
    if(ret)
      free(ret);
    return NULL;
    }
  return ret;
  }

int bgav_convert_string_realloc(bgav_charset_converter_t * cnv,
                                const char * str, int len,
                                uint32_t * out_len,
                                char ** ret, uint32_t * ret_alloc)
  {
  int result;
  char * tmp_string;
  if(len < 0)
    len = strlen(str);
  
  tmp_string = malloc(len+1);
  memcpy(tmp_string, str, len);
  tmp_string[len] = '\0';
  result = do_convert(cnv, tmp_string, len, out_len, ret, ret_alloc);
  free(tmp_string);
  
  return result;
  }

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
  char * line = NULL;
  uint32_t line_alloc = 0;
  
  int64_t old_position;
  uint8_t first_bytes[2];
  
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
    while(bgav_input_read_line(ctx, &line, &line_alloc, 0, NULL))
      {
      if(!bgav_utf8_validate((uint8_t*)line, NULL))
        {
        bgav_input_seek(ctx, old_position, SEEK_SET);
        if(line) free(line);
        return;
        }
      }
    ctx->charset = gavl_strdup(BGAV_UTF8);
    bgav_input_seek(ctx, old_position, SEEK_SET);
    if(line) free(line);
    return;
    }
  bgav_input_seek(ctx, old_position, SEEK_SET);
  if(line) free(line);
  }
