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



#ifndef BGAV_HTTP_H_INCLUDED
#define BGAV_HTTP_H_INCLUDED

/* http support */

typedef struct bgav_http_s bgav_http_t;

bgav_http_t * bgav_http_open(const char * url,
                             const bgav_options_t * opt,
                             char ** redirect_url,
                             const gavl_dictionary_t * extra_header);

bgav_http_t * bgav_http_reopen(bgav_http_t * ret,
                               const char * url, const bgav_options_t * opt,
                               char ** redirect_url,
                               const gavl_dictionary_t * extra_header);

void bgav_http_close(bgav_http_t *);

gavl_dictionary_t * bgav_http_get_header(bgav_http_t *);

int bgav_http_is_keep_alive(bgav_http_t *);

int bgav_http_read(bgav_http_t * h, uint8_t * data, int len);

int64_t bgav_http_total_bytes(bgav_http_t * h);
int bgav_http_can_seek(bgav_http_t * h);

#endif // BGAV_HTTP_H_INCLUDED

