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
#define LOG_DOMAIN "http"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

#include <http.h>

/* Creation/destruction of http header */

bgav_http_header_t * bgav_http_header_create()
  {
  bgav_http_header_t * ret = calloc(1, sizeof(*ret));
  return ret;
  }

void bgav_http_header_reset(bgav_http_header_t*h)
  {
  int i;
  for(i = 0; i < h->num_lines; i++)
    {
    if(h->lines[i])
      {
      free(h->lines[i]);
      h->lines[i] = NULL;
      }
    }
  h->num_lines = 0;
  }

void bgav_http_header_destroy(bgav_http_header_t * h)
  {
  int i;
  for(i = 0; i < h->lines_alloc; i++)
    {
    if(h->lines[i])
      free(h->lines[i]);
    }
  if(h->lines)
    free(h->lines);

  if(h->method)
    free(h->method);
  if(h->path)
    free(h->path);
  if(h->protocol)
    free(h->protocol);
  
  free(h);
  }

static void append_line(bgav_http_header_t * h)
  {
  if(h->lines_alloc < h->num_lines + 1)
    {
    h->lines_alloc += 8;
    h->lines = realloc(h->lines, h->lines_alloc * sizeof(*(h->lines)));
    memset(h->lines + h->num_lines, 0,
           (h->lines_alloc - h->num_lines) * sizeof(*(h->lines)));
    }
  }

void bgav_http_header_add_line(bgav_http_header_t * h, const char * line)
  {
  append_line(h);
  h->lines[h->num_lines] = gavl_strrep(h->lines[h->num_lines], line);
  h->num_lines++;
  }

void bgav_http_header_add_line_nocpy(bgav_http_header_t * h, char * line)
  {
  append_line(h);
  h->lines[h->num_lines] = line;
  h->num_lines++;
  }

void bgav_http_header_init(bgav_http_header_t * h, const char * method, const char * path,
                           const char * protocol)
  {
  h->method   = gavl_strrep(h->method, method);
  h->path     = gavl_strrep(h->path, path);
  h->protocol = gavl_strrep(h->protocol, protocol);
  }

char * bgav_http_header_to_string(const bgav_http_header_t * h1, const bgav_http_header_t * h2)
  {
  int i;
  int len = 0;
  char * ret = NULL;
  
  if(h1->method && h1->path && h1->protocol)
    len = strlen(h1->method) + strlen(h1->path) + strlen(h1->protocol) + 4;
  
  for(i = 0; i < h1->num_lines; i++)
    len += strlen(h1->lines[i]) + 2;

  if(h2)
    {
    for(i = 0; i < h2->num_lines; i++)
      len += strlen(h2->lines[i]) + 2;
    }
  
  len += 3; // \r\n\0

  ret = malloc(len);
  *ret = '\0';
  
  if(h1->method && h1->path && h1->protocol)
    sprintf(ret, "%s %s %s\r\n", h1->method, h1->path, h1->protocol);

  for(i = 0; i < h1->num_lines; i++)
    {
    strcat(ret, h1->lines[i]);
    strcat(ret, "\r\n");
    }

  if(h2)
    {
    for(i = 0; i < h2->num_lines; i++)
      {
      strcat(ret, h2->lines[i]);
      strcat(ret, "\r\n");
      }
    }
  
  strcat(ret, "\r\n");
  return ret;
  }

int bgav_http_header_send(const bgav_options_t * opt, const bgav_http_header_t * h1,
                          const bgav_http_header_t * h2,
                          int fd)
  {
  char * buf = bgav_http_header_to_string(h1, h2);
  
  if(buf)
    {
    if(!bgav_tcp_send(opt, fd, (uint8_t*)buf, strlen(buf)))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Remote end closed connection");
      free(buf);
      return 0;
      }
    free(buf);
    }
  
  return 1;
  }

/* Reading of http header */

int bgav_http_header_revc(const bgav_options_t * opt,
                          bgav_http_header_t * h, int fd)
  {
  int ret = 0;
  char * answer = NULL;
  int answer_alloc = 0;
  
  while(bgav_read_line_fd(opt, fd, &answer, &answer_alloc, opt->connect_timeout))
    {
    if(*answer == '\0')
      break;
    bgav_http_header_add_line(h, answer);
    ret = 1;
    }

  if(answer)
    free(answer);
  return ret;
  }

int bgav_http_header_status_code(bgav_http_header_t * h)
  {
  char * pos;
  if(!h->num_lines)
    return 0;

  pos = h->lines[0];
  while(!isspace(*pos) && (*pos != '\0'))
    pos++;

  while(isspace(*pos) && (*pos != '\0'))
    pos++;

  if(isdigit(*pos))
    return atoi(pos);
  return -1;
  }

const char * bgav_http_header_status_line(bgav_http_header_t * h)
  {
  if(!h->num_lines)
    return NULL;
  return h->lines[0];
  }

const char * bgav_http_header_get_var(bgav_http_header_t * h,
                                      const char * name)
  {
  int i;
  int name_len;
  const char * ret;
  name_len = strlen(name);
  for(i = 1; i < h->num_lines; i++)
    {
    if(!strncasecmp(h->lines[i], name, name_len) &&
       h->lines[i][name_len] == ':')
      {
      ret = &h->lines[i][name_len+1];
      while(isspace(*ret))
        ret++;
      return ret;
      }
    }
  return NULL;
  }

void bgav_http_header_dump(bgav_http_header_t*h)
  {
  int i;
  for(i = 0; i < h->num_lines; i++)
    gavl_dprintf( "  %s\n", h->lines[i]);
  }
