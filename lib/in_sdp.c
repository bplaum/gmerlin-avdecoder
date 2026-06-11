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

#include <string.h>
#include <stdlib.h>


#include <avdec_private.h>

extern const bgav_demuxer_t bgav_demuxer_ffmpeg;


static int read_sdp(bgav_input_context_t* ctx,
                    uint8_t * buffer, int len)
  {
  int bytes_to_read;
  int bytes_left;
  
  gavl_buffer_t * buf = ctx->priv;
  
  bytes_left = buf->len - buf->pos;
  bytes_to_read = (len < bytes_left) ? len : bytes_left;
  memcpy(buffer, buf->buf + buf->pos, bytes_to_read);
  buf->pos += bytes_to_read;
  return bytes_to_read;
  }

static int64_t seek_byte_sdp(bgav_input_context_t * ctx,
                             int64_t pos, int whence)
  {
  gavl_buffer_t * buf = ctx->priv;

  buf->pos = ctx->position;
  return buf->pos;
  }

static void    close_sdp(bgav_input_context_t * ctx)
  {
  gavl_buffer_t * buf = ctx->priv;
  gavl_buffer_free(buf);
  free(buf);
  }

static int    open_sdp(bgav_input_context_t * ctx, const char * url, char ** r)
  {
  gavl_buffer_t * buf = calloc(1, sizeof(*buf));
  gavl_base64_decode_data_urlsafe(url + 6, buf);
  
  ctx->location = gavl_strdup(url);
  ctx->total_bytes = buf->len;
  ctx->priv = buf;

  ctx->demuxer = bgav_demuxer_create(ctx->b, &bgav_demuxer_ffmpeg, NULL);

  if(!bgav_demuxer_start(ctx->demuxer))
    return 0;
  
  return 1;
  }
  
const bgav_input_t bgav_input_sdp =
  {
    .open =      open_sdp,
    .read =      read_sdp,
    .seek_byte = seek_byte_sdp,
    .close =     close_sdp
  };

