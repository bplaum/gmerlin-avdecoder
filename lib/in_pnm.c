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


#include <stdio.h>
#include <stdlib.h>
#include <avdec_private.h>
#include "pnm.h"

typedef struct
  {
  char * url;
  int fd;
  int eof;
  pnm_t * s;
  } pnm_priv_t;

static int open_pnm(bgav_input_context_t * ctx, const char * url, char ** r)
  {
  int port;
  char * path = NULL;
  char * host = NULL;
  pnm_priv_t * priv = NULL;
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
  priv->url = bgav_sprintf("%s", url);
  
  if(!bgav_url_split(url, NULL,
                     NULL, /* User */
                     NULL, /* Pass */
                     &host, &port, &path))
    goto fail;
  
  /* Try to connect to host */

  if(port < 0)
    port = 7070;
  
  if((priv->fd = bgav_tcp_connect(ctx->opt, host, port)) == -1)
    return 0;

  priv->s = pnm_connect(priv->fd, path);

  if(!priv->s)
    {
    return 0;
    }

  if(host)
    free(host);
  if(path)
    free(path);

  ctx->url = gavl_strdup(url);
  
  return 1;

  fail:
  ctx->priv = NULL;
  if(host)
    free(host);
  if(path)
    free(path);
  
  if(priv)
    free(priv);
  return 0;
  }

static int     read_pnm(bgav_input_context_t* ctx,
                             uint8_t * buffer, int len)
  {
  int result;
  pnm_priv_t * priv = ctx->priv;

  if(priv->eof)
    return 0;

  result = pnm_read(priv->s, (char*)buffer, len);

  if(!result)
    priv->eof = 1;
  return result;
  }

static void    close_pnm(bgav_input_context_t * ctx)
  {
  pnm_priv_t * priv = ctx->priv;

  if(priv->url)
    free(priv->url);

  if(priv->s)
    pnm_close(priv->s);
  free(priv);
  }

const bgav_input_t bgav_input_pnm =
  {
    .name =      "Real pnm",
    .open =      open_pnm,
    .read =      read_pnm,
    .close =     close_pnm
  };

