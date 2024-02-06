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

#include <rtsp.h>
#include <sdp.h>

#include <http.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>



/* Real specific stuff */

#include <rmff.h>

#define LOG_DOMAIN "rtsp"

struct bgav_rtsp_s
  {
  int fd;
  int cseq;
  
  char * session;
  char * url;

  bgav_http_header_t * res;
  bgav_http_header_t * req;
  
  bgav_sdp_t sdp;

  const bgav_options_t * opt;
  
  };


#define DUMP_REQUESTS

static int rtsp_send_request(bgav_rtsp_t * rtsp,
                             const char * command,
                             const char * what,
                             int * got_redirected)
  {
  const char * var;
  int status;
  char * line = NULL;
  char * request;
  int i;
  
  /* We must send the whole request at once so that
     RTSP aware NATs/firewalls recognize our requests and
     open the UDP ports for us
  */
  
  rtsp->cseq++;

  

  request = gavl_sprintf("%s %s RTSP/1.0\r\n", command, what);
  
  for(i = 0; i < rtsp->req->num_lines; i++)
    {
    request = gavl_strcat(request, rtsp->req->lines[i]);
    request = gavl_strcat(request, "\r\n");
    }

  if(rtsp->session)
    {
    line = gavl_sprintf("Session: %s\r\n", rtsp->session);
    request = gavl_strcat(request, line);
    free(line);
    }
  
  line = gavl_sprintf("CSeq: %u\r\n", rtsp->cseq);
  request = gavl_strcat(request, line);
  free(line);

  request = gavl_strcat(request, "\r\n");

#ifdef DUMP_REQUESTS
  bgav_dprintf("Sending request:\n%s", request);
#endif  
  
  if(!bgav_tcp_send(rtsp->opt, rtsp->fd, (uint8_t*)request, strlen(request)))
    {
    free(request);
    goto fail;
    }
  free(request);
  
  bgav_http_header_reset(rtsp->req);
  
  /* Read answers */
  bgav_http_header_reset(rtsp->res);
  if(!bgav_http_header_revc(rtsp->opt, rtsp->res, rtsp->fd))
    return 0;
  
  /* Handle redirection */
  
  if(strstr(rtsp->res->lines[0], "REDIRECT"))
    {
    free(rtsp->url);
    rtsp->url =
      gavl_strdup(bgav_http_header_get_var(rtsp->res,"Location"));
    if(got_redirected)
      *got_redirected = 1;
#if 1
    /* Redirection means new session */
    if(rtsp->session)
      {
      free(rtsp->session);
      rtsp->session = NULL;
      }
#endif
    return 1;
    }

  /* Get the server status */
  status = bgav_http_header_status_code(rtsp->res);

#ifdef DUMP_REQUESTS
  bgav_dprintf("Got answer %d:\n", status);
  bgav_http_header_dump(rtsp->res);
#endif  

  if(status != 200)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "%s", bgav_http_header_status_line(rtsp->res));
    goto fail;
    }
  var = bgav_http_header_get_var(rtsp->res, "Session");
  if(var && !(rtsp->session)) 
    rtsp->session = gavl_strdup(var);
  return 1;
  
  fail:
  return 0;
  }

void bgav_rtsp_schedule_field(bgav_rtsp_t * rtsp, const char * field)
  {
  bgav_http_header_add_line(rtsp->req, field);
  }

const char * bgav_rtsp_get_answer(bgav_rtsp_t * rtsp, const char * name)
  {
  return bgav_http_header_get_var(rtsp->res, name);
  }

int bgav_rtsp_request_describe(bgav_rtsp_t *rtsp, int * got_redirected)
  {
  int content_length;
  const char * var;
  char * buf = NULL;

  /* Send the "DESCRIBE" request */
  
  if(!rtsp_send_request(rtsp, "DESCRIBE", rtsp->url, got_redirected))
    goto fail;

  if(got_redirected && *got_redirected)
    {
    return 1;
    }
  var = bgav_http_header_get_var(rtsp->res, "Content-Length");
  if(!var)
    goto fail;
  
  content_length = atoi(var);
  
  buf = malloc(content_length+1);
  
  if(bgav_read_data_fd(rtsp->opt, rtsp->fd, (uint8_t*)buf,
                       content_length, rtsp->opt->read_timeout) <
     content_length)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Reading session description failed");
    goto fail;
    }

  buf[content_length] = '\0';
  
  if(!bgav_sdp_parse(rtsp->opt, buf, &rtsp->sdp))
    goto fail;

  //  bgav_sdp_dump(&rtsp->sdp);
  
  
  free(buf);

  return 1;
  
  fail:

  if(buf)
    free(buf);
  return 0;
  }

int bgav_rtsp_request_setup(bgav_rtsp_t *r, const char *what)
  {
  return rtsp_send_request(r,"SETUP",what, NULL);
  }

int bgav_rtsp_request_setparameter(bgav_rtsp_t * r)
  {
  return rtsp_send_request(r,"SET_PARAMETER",r->url, NULL);
  }

int bgav_rtsp_request_play(bgav_rtsp_t * r)
  {
  return rtsp_send_request(r,"PLAY",r->url, NULL);
  }

/*
 *  Open connection to the server, get options and handle redirections
 *
 *  Return FALSE if error, TRUE on success.
 *  If we got redirectred, TRUE is returned and got_redirected
 *  is set to 1. The new URL is copied to the rtsp structure
 */

static int do_connect(bgav_rtsp_t * rtsp,
                      int * got_redirected, int get_options)
  {
  int port = -1;
  char * host = NULL;
  char * protocol = NULL;
  int ret = 0;
  if(!gavl_url_split(rtsp->url,
                     &protocol,
                     NULL, /* User */
                     NULL, /* Pass */
                     &host,
                     &port, NULL))
    goto done;
  
  if(strcmp(protocol, "rtsp"))
    goto done;

  if(port == -1)
    port = 554;

  //  rtsp->cseq = 1;
  rtsp->fd = bgav_tcp_connect(rtsp->opt, host, port);
  if(rtsp->fd < 0)
    goto done;

  if(get_options)
    {
    if(!rtsp_send_request(rtsp, "OPTIONS", rtsp->url, got_redirected))
      goto done;
    }
  
  ret = 1;

  done:

  if(!ret && (rtsp->fd >= 0))
    closesocket(rtsp->fd);
  
  if(host)
    free(host);
  if(protocol)
    free(protocol);
  return ret;
  }

bgav_rtsp_t * bgav_rtsp_create(const bgav_options_t * opt)
  {
  bgav_rtsp_t * ret = NULL;
  ret = calloc(1, sizeof(*ret));
  ret->opt = opt;
  ret->res = bgav_http_header_create();
  ret->req = bgav_http_header_create();
  ret->fd = -1;
  return ret;
  }

int bgav_rtsp_open(bgav_rtsp_t * rtsp, const char * url,
                   int * got_redirected)
  {
  if(url)
    rtsp->url = gavl_strdup(url);
  return do_connect(rtsp, got_redirected, 1);
  }

int bgav_rtsp_reopen(bgav_rtsp_t * rtsp)
  {
  int got_redirected = 0;
  if(rtsp->fd >= 0)
    closesocket(rtsp->fd);
  return do_connect(rtsp, &got_redirected, 0);
  }

bgav_sdp_t * bgav_rtsp_get_sdp(bgav_rtsp_t * r)
  {
  return &r->sdp;
  }

void bgav_rtsp_close(bgav_rtsp_t * r, int teardown)
  {
  if(teardown && (r->fd >= 0))
    rtsp_send_request(r,"TEARDOWN",r->url, NULL);
  
  bgav_http_header_destroy(r->res);
  bgav_http_header_destroy(r->req);
  bgav_sdp_free(&r->sdp);
  if(r->url) free(r->url);
  if(r->session) free(r->session);

  if(r->fd > 0)
    closesocket(r->fd);
  
  free(r);
  }

int bgav_rtsp_get_fd(bgav_rtsp_t * r)
  {
  return r->fd;
  }

const char * bgav_rtsp_get_url(bgav_rtsp_t * r)
  {
  return r->url;
  }
