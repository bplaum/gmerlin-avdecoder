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

#include <libsmbclient.h>


#include <avdec_private.h>
#define LOG_DOMAIN "in_smb"

#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <pthread.h>


typedef struct
  {
  int fd;
  int64_t bytes_read;
  } smb_priv_t;


static bgav_input_context_t * auth_ctx;

static pthread_mutex_t auth_mutex = PTHREAD_MUTEX_INITIALIZER;

static void __cleanup() __attribute__ ((destructor));

static void __cleanup()
  {
  pthread_mutex_destroy(&auth_mutex);
  }

#define FREE(ptr) if(ptr) free(ptr);

static void smb_auth_fn(const char *server, const char *share,
             char *workgroup, int wgmaxlen, char *user, int unmaxlen,
             char *pass, int pwmaxlen)
  {
  char * server_share;
  char *password= NULL;
  char *username= NULL;

  server_share = bgav_sprintf("%s/%s", server, share);
  if(!auth_ctx->opt->user_pass_callback ||
     !auth_ctx->opt->user_pass_callback(auth_ctx->opt->user_pass_callback_data,
                                        server_share, &username, &password))
    {
    free(server_share);
    return;
    }
  if(username)
    strncpy(user, username, unmaxlen - 1);
  
  if(password)
    strncpy(pass, password, pwmaxlen - 1);

  FREE(username);
  FREE(password);
  free(server_share);
  }

/* Connect and open File */

static int open_smb(bgav_input_context_t * ctx, const char * url, char ** redirect_url)
  {
  smb_priv_t * p;
  uint64_t len;
  int err;

  p = calloc(1, sizeof(*p));
  ctx->priv = p;
  
  pthread_mutex_lock(&auth_mutex);
  auth_ctx = ctx;
  err = smbc_init(smb_auth_fn, 0);
  
  if (err < 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Initialization of samba failed (error: %d)", err);
    pthread_mutex_unlock(&auth_mutex);
    goto fail;
    }

  p->fd = smbc_open((char*)url, O_RDONLY, 0644);
  if (p->fd < 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Open file failed (error: %d)", p->fd);
    pthread_mutex_unlock(&auth_mutex);
    goto fail;
    }
  pthread_mutex_unlock(&auth_mutex);
    
  len = smbc_lseek(p->fd,0,SEEK_END);
  smbc_lseek (p->fd, 0, SEEK_SET);
 
  if (len <= 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Can't get filesize");
    goto fail;
    }
  
  ctx->total_bytes = len;
  ctx->flags |= BGAV_INPUT_CAN_PAUSE;

  return 1;
  
  fail:
  return 0;
  }

/* File read */

static int read_smb(bgav_input_context_t* ctx,
                    uint8_t * buffer, int len)
  {
  int len_read, err;
  smb_priv_t * p;
  p = ctx->priv;
  
  if(len + p->bytes_read > ctx->total_bytes)
    len = ctx->total_bytes - p->bytes_read;
  
  if(!len)
    {
    return 0;
    }
  err = smbc_read(p->fd, buffer, len);

  if(err <= 0)
    len_read = 0;
  else
    len_read = len;
  
  p->bytes_read += len_read;
  
  return len_read;
  }

/* File seek */

static int64_t seek_byte_smb(bgav_input_context_t * ctx,
                              int64_t pos, int whence)
  {
  smb_priv_t * p;
  int64_t len;
  p = ctx->priv;

  len = smbc_lseek(p->fd, pos, SEEK_SET);
  
  return len;
  }


/* close the smb connection */

static void    close_smb(bgav_input_context_t * ctx)
  {
  smb_priv_t * p;
  p = ctx->priv;
  smbc_close(p->fd);
  free(p);
  }


const bgav_input_t bgav_input_smb =
  {
    .name =      "samba",
    .open =      open_smb,
    .read =      read_smb,
    .seek_byte = seek_byte_smb,
    .close =     close_smb
  };

