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


#include <avdec_private.h>

struct bgav_subtitle_converter_s
  {
  bgav_charset_converter_t * cnv;
  
  gavl_packet_source_t * src;
  gavl_packet_source_t * prev;
  };

/* Remove \r */

static void remove_cr(char * str, int * len_p)
  {
  char * dst;
  char * src;
  int i;
  uint32_t len = *len_p;
  dst = str;
  src = str;
  
  for(i = 0; i < len; i++)
    {
    if(*src != '\r')
      {
      if(dst != src)
        *dst = *src;
      dst++;
      }
    src++;
    }
  
  *len_p = (dst - str);
  
  /* New zero termination */
  if(*len_p != len)
    memset(str + *len_p, 0, len - *len_p);
  }

static gavl_source_status_t
source_func(void * priv, bgav_packet_t ** ret_p)
  {
  gavl_source_status_t st;
  bgav_packet_t * in_packet = NULL;
  bgav_subtitle_converter_t * cnv = priv;

  bgav_packet_t * ret;
  
  if((st = gavl_packet_source_read_packet(cnv->prev, &in_packet)) != GAVL_SOURCE_OK)
    return st;
  
  /* Make sure we have a '\0' at the end */

  if(in_packet->buf.len > 0)
    {
    int len;
    if(in_packet->buf.buf[in_packet->buf.len-1] != '\0')
      {
      uint8_t term = '\0';
      gavl_buffer_append_data(&in_packet->buf, &term, 1);
      }

    len = strlen((char*)in_packet->buf.buf);

    if(len < in_packet->buf.len)
      in_packet->buf.len = len;
    }
  
  if(cnv->cnv && in_packet->buf.len > 0)
    {
    
    ret = *ret_p;
    
    /* Convert character set */
    if(!bgav_convert_string_realloc(cnv->cnv,
                                    (const char *)in_packet->buf.buf,
                                    in_packet->buf.len,
                                    &ret->buf))
      return GAVL_SOURCE_EOF;
    
    gavl_packet_copy_metadata(ret, in_packet);
    }
  else
    {
    ret = in_packet;
    }
  /* Remove \r */

  remove_cr((char*)ret->buf.buf, &ret->buf.len);
  
  PACKET_SET_KEYFRAME(ret);

  if(ret_p)
    *ret_p = ret;
  
  return GAVL_SOURCE_OK;
  }

#if 0
static gavl_source_status_t get_packet(void * priv, bgav_packet_t ** ret)
  {
  bgav_subtitle_converter_t * cnv = priv;
  
  if(cnv->out_packet)
    {
    *ret = cnv->out_packet;
    cnv->out_packet = NULL;
    return GAVL_SOURCE_OK;
    }
  return next_packet(cnv, ret, 1);
  }

static gavl_source_status_t peek_packet(void * priv, bgav_packet_t ** ret,
                                        int force)
  {
  gavl_source_status_t st;
  bgav_subtitle_converter_t * cnv = priv;

  if(cnv->out_packet)
    {
    if(ret)
      *ret = cnv->out_packet;
    return GAVL_SOURCE_OK;
    }

  if((st = next_packet(cnv, &cnv->out_packet, force)) != GAVL_SOURCE_OK)
    return st;

  if(ret)
    *ret = cnv->out_packet;
  return GAVL_SOURCE_OK;
  }
#endif

bgav_subtitle_converter_t *
bgav_subtitle_converter_create(const char * charset)
  {
  bgav_subtitle_converter_t * ret;
  ret = calloc(1, sizeof(*ret));

  if(strcmp(charset, BGAV_UTF8))
    {
    ret->cnv = bgav_charset_converter_create(charset,
                                             BGAV_UTF8);
    }
  
  return ret;
  }

gavl_packet_source_t * bgav_subtitle_converter_connect(bgav_subtitle_converter_t * cnv, gavl_packet_source_t * src)
  {
  cnv->prev = src;

  if(cnv->cnv)
    cnv->src = gavl_packet_source_create(source_func, cnv, 0, gavl_packet_source_get_stream(src));
  else
    cnv->src = gavl_packet_source_create(source_func, cnv, GAVL_SOURCE_SRC_ALLOC,
                                         gavl_packet_source_get_stream(src));
  return cnv->src;
  }

#if 0
void
bgav_subtitle_converter_reset(bgav_subtitle_converter_t * cnv)
  {
  }
#endif

void
bgav_subtitle_converter_destroy(bgav_subtitle_converter_t * cnv)
  {
  if(cnv->cnv)
    bgav_charset_converter_destroy(cnv->cnv);
  if(cnv->src)
    gavl_packet_source_destroy(cnv->src);
  
  free(cnv);
  }
