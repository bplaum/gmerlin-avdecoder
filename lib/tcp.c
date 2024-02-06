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


#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
                                                                               
#include <fcntl.h>
#include <sys/types.h>

#ifdef _WIN32
#include <bgavdefs.h>
#include <winsock2.h>
#include <ws2spi.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif


#include <avdec_private.h>
#define LOG_DOMAIN "tcp"

#if !HAVE_DECL_MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/* for MacOSX */
#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0
#endif

/* Utility functions */

static int create_socket(int domain, int type, int protocol)
  {
  int ret;
#if HAVE_DECL_SO_NOSIGPIPE // OSX
  int value = 1;
#endif

  ret = socket(domain, type, protocol);

#if HAVE_DECL_SO_NOSIGPIPE // OSX
  if(ret < 0)
    return ret;
  if(setsockopt(ret, SOL_SOCKET, SO_NOSIGPIPE, &value,
                sizeof(int)) == -1)
    return -1;
#endif
  return ret;
  }

static void address_set_port(struct addrinfo * info, int port)
  {
  while(info)
    {
    switch(info->ai_family)
      {
      case AF_INET:
        {
        struct sockaddr_in * addr;
        addr = (struct sockaddr_in*)info->ai_addr;
        addr->sin_port = htons(port);
        }
        break;
      case AF_INET6:
        {
        struct sockaddr_in6 * addr;
        addr = (struct sockaddr_in6*)info->ai_addr;
        addr->sin6_port = htons(port);
        }
        break;
      default:
        break;
      }
    info = info->ai_next;
    }
  }

struct addrinfo *
bgav_hostbyname(const bgav_options_t * opt,
                const char * hostname, int port, int socktype, int flags)
  {
  int err;
  struct in_addr ipv4_addr;
  
  struct addrinfo hints;
  struct addrinfo * ret;
  char * service = NULL;
  
  memset(&hints, 0, sizeof(hints));
  //  hints.ai_family   = AF_UNSPEC;
  hints.ai_family   = AF_INET;
  hints.ai_socktype = socktype; // SOCK_STREAM, SOCK_DGRAM
  hints.ai_protocol = 0; // PF_INET, PF_INET6
  hints.ai_flags    = flags;
  
  /* prevent DNS lookup for numeric IP addresses */
  
  if(hostname && bgav_inet_aton(hostname, &ipv4_addr))
    hints.ai_flags |= AI_NUMERICHOST;
  
  if(!hostname)
    {
    service = gavl_sprintf("%d", port);
    hints.ai_flags |= AI_NUMERICSERV;
    }
  
  if((err = getaddrinfo(hostname, service /* service */,
                        &hints, &ret)))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Cannot resolve address of %s: %s",
             hostname, gai_strerror(err));
    return NULL;
    }

  if(hostname)
    address_set_port(ret, port);
  
  if(service)
    free(service);
  
  return ret;
  }


/* Client connection (stream oriented) */
                                                                               
static int socket_connect_inet(const bgav_options_t * opt, struct addrinfo * addr)
  {
  int ret = -1;
  int err;
  socklen_t err_len;

  struct timeval timeout;
  fd_set write_fds;
#ifdef _WIN32
  unsigned long flags = 1;
#endif
  
  /* Create the socket */
  if((ret = create_socket(addr->ai_family, SOCK_STREAM, 0)) < 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Cannot create socket");
    return -1;
    }
  
  /* Set nonblocking mode */
#ifdef _WIN32
  if (ioctlsocket(ret,FIONBIO, &flags) == SOCKET_ERROR)
#else
  if(fcntl(ret, F_SETFL, O_NONBLOCK) < 0)
#endif   
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Cannot set nonblocking mode");
    return -1;
    }
  
  /* Connect the thing */
  if(connect(ret, addr->ai_addr, addr->ai_addrlen)<0)
    {
    if(errno == EINPROGRESS)
      {
      timeout.tv_sec = opt->connect_timeout / 1000;
      timeout.tv_usec = 1000 * (opt->connect_timeout % 1000);
      FD_ZERO (&write_fds);
      FD_SET (ret, &write_fds);
      if(!select(ret+1, NULL, &write_fds,NULL,&timeout))
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Connection timed out");
        return -1;
        }
      }
    else
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Connecting failed: %s", strerror(errno));
      return -1;
      }
    }

  /* Check for errors */
  err_len = sizeof(err);
  getsockopt(ret, SOL_SOCKET, SO_ERROR, &err, &err_len);

  if(err)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Connecting failed: %s",
             strerror(err));
    return -1;
    }
  
  /* Set back to blocking mode */
#ifdef _WIN32
  flags = 0;
  if (ioctlsocket(ret,FIONBIO, &flags) == SOCKET_ERROR)
#else
  if(fcntl(ret, F_SETFL, 0) < 0)
#endif   
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Cannot set blocking mode");
    return -1;
    }
  return ret;
  }

int bgav_tcp_connect(const bgav_options_t * opt,
                     const char * host, int port)
  {
  struct addrinfo * addr;
  int ret;
  
  addr = bgav_hostbyname(opt, host, port, SOCK_STREAM, 0);
  if(!addr)
    return -1;
  ret = socket_connect_inet(opt, addr);
  freeaddrinfo(addr);
  return ret;
  }

int bgav_tcp_send(const bgav_options_t * opt, int fd,
                  uint8_t * data, int len)
  {
  int result;
  result = send(fd, data, len, MSG_NOSIGNAL);
  if(result != len)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Could not send data: %s", strerror(errno));
    return 0;
    }
  return 1;
  }
