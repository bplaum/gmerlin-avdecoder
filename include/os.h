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

/* Functions which miss on some systems */

#ifndef OS_H
#define OS_H

#include <config.h>

#ifdef _WIN32
#include <winsock2.h> //needed for in_addr
#endif

#ifndef HAVE_CLOSESOCKET
#define closesocket(fd) close(fd)
#else

#endif // HAVE_CLOSESOCKET

/* poll  */
#ifndef HAVE_POLL
typedef unsigned long nfds_t;
struct pollfd {
    int fd;
    short events;  /* events to look for */
    short revents; /* events that occured */
};

/* events & revents */
#define POLLIN     0x0001  /* any readable data available */
#define POLLOUT    0x0002  /* file descriptor is writeable */
#define POLLRDNORM POLLIN
#define POLLWRNORM POLLOUT
#define POLLRDBAND 0x0008  /* priority readable data */
#define POLLWRBAND 0x0010  /* priority data can be written */
#define POLLPRI    0x0020  /* high priority readable data */

/* revents only */
#define POLLERR    0x0004  /* errors pending */
#define POLLHUP    0x0080  /* disconnected */
#define POLLNVAL   0x1000  /* invalid file descriptor */

int bgav_poll(struct pollfd *fds, nfds_t numfds, int timeout);

#else

#define bgav_poll(fds, numfds, timeout) poll(fds, numfds, timeout) 

#endif // poll

#ifndef HAVE_INET_ATON
int bgav_inet_aton(const char *cp, struct in_addr * addr);
#else
#define bgav_inet_aton(cp, addr) inet_aton(cp, addr) 
#endif

#endif
