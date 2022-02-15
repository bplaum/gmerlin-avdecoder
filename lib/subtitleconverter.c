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
  bgav_packet_t * out_packet;
  bgav_stream_t * s;
  
  bgav_packet_source_t src;
  };

/* Remove \r */

static void remove_cr(char * str, uint32_t * len_p)
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
next_packet(bgav_subtitle_converter_t * cnv,
            bgav_packet_t ** ret_p, int force)
  {
  gavl_source_status_t st;
  bgav_packet_t * in_packet;
  char * pos;
  int in_len;
  bgav_packet_t * ret;
  
  if(!force && ((st = cnv->src.peek_func(cnv->src.data, NULL, 0)) != GAVL_SOURCE_OK))
    return st;
  
  in_packet = NULL;
  if((st = cnv->src.get_func(cnv->src.data, &in_packet)) != GAVL_SOURCE_OK)
    return st;
  
  /* Make sure we have a '\0' at the end */

  if(in_packet->data_size > 0)
    {
    pos = (char*)&in_packet->data[in_packet->data_size-1];
    in_len = in_packet->data_size;

    while(*pos == '\0')
      {
      pos--;
      in_len--;

      if(in_len < 0)
        break;
      }

    if(in_len < 0)
      in_len = 0;
    }
  else
    in_len = 0;
  
  if(cnv->cnv && in_len)
    {
    ret = bgav_packet_pool_get(cnv->s->pp);

    /* Convert character set */
    if(!bgav_convert_string_realloc(cnv->cnv,
                                    (const char *)in_packet->data,
                                    in_len,
                                    &ret->data_size,
                                    (char **)&ret->data, &ret->data_alloc))
      {
      bgav_packet_pool_put(cnv->s->pp, in_packet);
      bgav_packet_pool_put(cnv->s->pp, ret);
      return GAVL_SOURCE_EOF;
      }
    
    bgav_packet_copy_metadata(ret, in_packet);
    bgav_packet_pool_put(cnv->s->pp, in_packet);
    }
  else
    {
    ret = in_packet;
    ret->data_size = in_len;
    }
  /* Remove \r */

  remove_cr((char*)ret->data, &ret->data_size);
  
  PACKET_SET_KEYFRAME(ret);

  if(ret_p)
    *ret_p = ret;
  
  return GAVL_SOURCE_OK;
  }

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

bgav_subtitle_converter_t *
bgav_subtitle_converter_create(bgav_stream_t * s)
  {
  bgav_subtitle_converter_t * ret;
  ret = calloc(1, sizeof(*ret));

  if(strcmp(s->data.subtitle.charset, BGAV_UTF8))
    {
    ret->cnv = bgav_charset_converter_create(s->opt,
                                             s->data.subtitle.charset,
                                             BGAV_UTF8);
    }

  bgav_packet_source_copy(&ret->src, &s->src);
  s->src.get_func = get_packet;
  s->src.peek_func = peek_packet;
  s->src.data = ret;

  ret->s = s;
  
  return ret;
  }

void
bgav_subtitle_converter_reset(bgav_subtitle_converter_t * cnv)
  {
  if(cnv->out_packet)
    {
    bgav_packet_pool_put(cnv->s->pp, cnv->out_packet);
    cnv->out_packet = NULL;
    }
  }

void
bgav_subtitle_converter_destroy(bgav_subtitle_converter_t * cnv)
  {
  if(cnv->out_packet)
    bgav_packet_pool_put(cnv->s->pp, cnv->out_packet);
  if(cnv->cnv)
    bgav_charset_converter_destroy(cnv->cnv);
  free(cnv);
  }
