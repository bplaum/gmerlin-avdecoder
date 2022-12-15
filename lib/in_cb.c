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
#include <utils.h>
#include <md5.h>

typedef struct
  {
  void * priv;
  int   (*read_callback)(void * priv, uint8_t * data, int len);
  int64_t (*seek_callback)(void * priv, int64_t pos, int whence);
  } cb_t;

static int     read_callbacks(bgav_input_context_t* ctx,
                         uint8_t * buffer, int len)
  {
  cb_t * c = ctx->priv;
  return c->read_callback(c->priv, buffer, len);
  }

static int64_t seek_byte_callbacks(bgav_input_context_t * ctx,
                              int64_t pos, int whence)
  {
  cb_t * c = ctx->priv;
  return c->seek_callback(c->priv, pos, whence);
  }

static void    close_callbacks(bgav_input_context_t * ctx)
  {
  cb_t * c = ctx->priv;
  if(c)
    free(c);
  }

const bgav_input_t bgav_input_callbacks =
  {
    .name =      "callbacks",
    .read =      read_callbacks,
    .seek_byte = seek_byte_callbacks,
    .close =     close_callbacks
  };

const bgav_input_t bgav_input_callbacks_noseek =
  {
    .name =      "callbacks_noseek",
    .read =      read_callbacks,
    .close =     close_callbacks
  };

static
bgav_input_context_t *
bgav_input_open_callbacks(int (*read_callback)(void * priv, uint8_t * data, int len),
                          int64_t (*seek_callback)(void * priv, int64_t pos, int whence),
                          void * priv,
                          const char * filename, const char * mimetype, int64_t total_bytes,
                          bgav_t * b)
  {
  bgav_input_context_t * ret;
  cb_t * c = calloc(1, sizeof(*c));
  c->read_callback = read_callback;
  c->priv = priv;
  
  ret = bgav_input_create(b, NULL);
  if(seek_callback)
    {
    ret->input = &bgav_input_callbacks;
    c->seek_callback = seek_callback;
    ret->total_bytes = c->seek_callback(c->priv, 0, SEEK_END);
    c->seek_callback(c->priv, 0, SEEK_SET);
    }
  else
    {
    ret->input = &bgav_input_callbacks_noseek;
    }
  ret->priv = c;
  ret->location = gavl_strdup(filename);
  if(mimetype)
    gavl_dictionary_set_string(gavl_dictionary_get_src_nc(&ret->m, GAVL_META_SRC, 0),
                               GAVL_META_MIMETYPE, mimetype);
  ret->total_bytes = total_bytes;
  
  if(ret->location)
    {
    uint8_t md5sum[16];
    bgav_md5_buffer(ret->location, strlen(ret->location),
                    md5sum);
    ret->index_file =
      bgav_sprintf("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                   md5sum[0], md5sum[1], md5sum[2], md5sum[3], 
                   md5sum[4], md5sum[5], md5sum[6], md5sum[7], 
                   md5sum[8], md5sum[9], md5sum[10], md5sum[11], 
                   md5sum[12], md5sum[13], md5sum[14], md5sum[15]);
    }
  
  return ret;
  }

int bgav_open_callbacks(bgav_t * b,
                        int (*read_callback)(void * priv, uint8_t * data, int len),
                        int64_t (*seek_callback)(void * priv, int64_t pos, int whence),
                        void * priv,
                        const char * filename, const char * mimetype, int64_t total_bytes)
  {
  bgav_codecs_init(&b->opt);
  b->input = bgav_input_open_callbacks(read_callback, seek_callback,
                                       priv, filename, mimetype, total_bytes,
                                       b);
  if(!b->input)
    return 0;
  if(!bgav_init(b))
    goto fail;
  return 1;
  fail:
  return 0;
  }
