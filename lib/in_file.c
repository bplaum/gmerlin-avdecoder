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
#include <md5.h>


#define LOG_DOMAIN "in_file"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdio.h>

/* stat */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#ifdef _WIN32
#define BGAV_FSEEK(a,b,c) fseeko64(a,b,c)
#define BGAV_FTELL(a) ftello64(a)
#else
#ifdef HAVE_FSEEKO
#define BGAV_FSEEK fseeko
#else
#define BGAV_FSEEK fseek
#endif

#ifdef HAVE_FTELLO
#define BGAV_FTELL ftello
#else
#define BGAV_FTELL ftell
#endif

#endif

static int open_file(bgav_input_context_t * ctx, const char * url, char ** r)
  {
  gavl_dictionary_t * dict;
  FILE * f;
  struct stat st;
  uint8_t md5sum[16];
  if(!strncmp(url, "file://", 7))
    url += 7;
  
  f = fopen(url, "rb");
  if(!f)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Cannot open %s: %s",
             url, strerror(errno));
    return 0;
    }
  ctx->priv = f;

  fstat(fileno(f), &st);
  dict = gavl_dictionary_get_src_nc(&ctx->m, GAVL_META_SRC, 0);

  gavl_dictionary_set_long(dict, GAVL_META_MTIME, st.st_mtime);
  
  
  BGAV_FSEEK((FILE*)(ctx->priv), 0, SEEK_END);
  ctx->total_bytes = BGAV_FTELL((FILE*)(ctx->priv));

  gavl_dictionary_set_long(dict, GAVL_META_TOTAL_BYTES, ctx->total_bytes);
  
  BGAV_FSEEK((FILE*)(ctx->priv), 0, SEEK_SET);
  
  ctx->filename = gavl_strdup(url);
  
  bgav_md5_buffer(ctx->filename, strlen(ctx->filename),
                  md5sum);
  
  ctx->index_file = bgav_sprintf("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                 md5sum[0], md5sum[1], md5sum[2], md5sum[3], 
                                 md5sum[4], md5sum[5], md5sum[6], md5sum[7], 
                                 md5sum[8], md5sum[9], md5sum[10], md5sum[11], 
                                 md5sum[12], md5sum[13], md5sum[14], md5sum[15]);
  ctx->flags |= BGAV_INPUT_CAN_PAUSE;
  return 1;
  }

static int     read_file(bgav_input_context_t* ctx,
                         uint8_t * buffer, int len)
  {
  return fread(buffer, 1, len, (FILE*)(ctx->priv)); 
  }

static int64_t seek_byte_file(bgav_input_context_t * ctx,
                              int64_t pos, int whence)
  {
  BGAV_FSEEK((FILE*)(ctx->priv), ctx->position, SEEK_SET);
  return BGAV_FTELL((FILE*)(ctx->priv));
  }

static void close_file(bgav_input_context_t * ctx)
  {
  if(ctx->priv)
    fclose((FILE*)(ctx->priv));
  }

static int open_stdin(bgav_input_context_t * ctx, const char * url, char ** r)
  {
  ctx->priv = stdin;
  return 1;
  }

static void close_stdin(bgav_input_context_t * ctx)
  {
  /* Nothing to do here */
  }

const bgav_input_t bgav_input_file =
  {
    .name =      "file",
    .open =      open_file,
    .read =      read_file,
    .seek_byte = seek_byte_file,
    .close =     close_file
  };

const bgav_input_t bgav_input_stdin =
  {
    .name =      "stdin",
    .open =      open_stdin,
    .read =      read_file,
    .close =     close_stdin
  };

