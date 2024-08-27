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



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <avdec_private.h>

typedef struct
  {
  int fd;
  } fd_priv_t;

static int read_fd(bgav_input_context_t* ctx,
                    uint8_t * buffer, int len)
  {
  fd_priv_t * priv = ctx->priv;
  return read(priv->fd, buffer, len);
  }

static void    close_fd(bgav_input_context_t * ctx)
  {
  fd_priv_t * priv = ctx->priv;
  free(priv);
  }

static const bgav_input_t bgav_input_fd =
  {
    .open =   NULL, /* Not needed */
    .read =   read_fd,
    .close =  close_fd
  };

bgav_input_context_t * bgav_input_open_fd(int fd, int64_t total_bytes, const char * mimetype)
  {
  bgav_input_context_t * ret;
  fd_priv_t * priv;
  
  ret = calloc(1, sizeof(*ret));
  priv = calloc(1, sizeof(*priv));
  ret->priv = priv;
  ret->input = &bgav_input_fd;

  priv->fd     = fd;
  ret->total_bytes = total_bytes;

  if(mimetype)
    gavl_dictionary_set_string(&ret->metadata, GAVL_META_MIMETYPE, mimetype);
  return ret;
  }
