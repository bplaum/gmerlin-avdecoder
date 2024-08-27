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


                                                                               
#include <fcntl.h>
#include <sys/types.h>

#include <errno.h>
#include <string.h>

#ifdef _WIN32
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

#define LOG_DOMAIN "udp"

int bgav_udp_open(const bgav_options_t * opt, int port)
  {
  int ret;
  size_t tmp = 0;
  struct addrinfo * addr;
  addr = bgav_hostbyname(opt, NULL, port, SOCK_DGRAM, AI_PASSIVE);

  /* Create the socket */
  if((ret = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Cannot create socket");
    return -1;
    }
  
  /* Give the socket a name. */
  
  if(bind(ret, addr->ai_addr, addr->ai_addrlen) < 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Cannot bind inet socket: %s", strerror(errno));
    return -1;
    }

  //  getsockopt(ret, SOL_SOCKET, SO_RCVBUF, &tmp, &optlen);
  tmp = 65536;
  setsockopt(ret, SOL_SOCKET, SO_RCVBUF, &tmp, sizeof(tmp));
  
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
           "UDP Socket bound on port %d\n", port);
  
  freeaddrinfo(addr);
  return ret;
  }

int bgav_udp_read(int fd, uint8_t * data, int len)
  {
  int bytes_read;
  for(;;)
    {
    bytes_read = recv(fd, data, len, 0);
    if (bytes_read < 0)
      {
      if((errno == EAGAIN) ||
         (errno == EINTR))
        return -1;
      }
    else
      break;
    }
  return bytes_read;
  }

int bgav_udp_write(const bgav_options_t * opt,
                   int fd, uint8_t * data, int len,
                   struct addrinfo * addr)
  {
  for(;;)
    {
    if(sendto(fd, data, len, 0, addr->ai_addr, addr->ai_addrlen) < len)
      {
      if((errno == EAGAIN) ||
         (errno == EINTR))
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Sending UDP packet failed: %s\n", strerror(errno));
        return -1;
        }
      }
    }
  return len;
  }
