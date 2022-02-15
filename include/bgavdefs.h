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

#ifndef BGAVDEFS_H_INCLUDED
#define BGAVDEFS_H_INCLUDED

#ifdef DLL_EXPORT  
#define BGAV_PUBLIC __declspec(dllexport)
#else
#define BGAV_PUBLIC __attribute__ ((visibility("default")))
#endif


#ifdef _WIN32

/* need to declare that we are XP or newer 
 * otherwise, getaddrinfo and other things 
 * won't work */
#define WINVER 0x501

#ifndef EINVAL
#define EINVAL WSAEINVAL
#endif

#ifndef EINPROGRESS
#define EINPROGRESS WSAEINPROGRESS
#endif

/* for MacOSX AND mingw */
#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0
#endif

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#endif


#endif // BGAVDEFS_H_INCLUDED

