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

/* http support */

typedef struct bgav_http_s bgav_http_t;

#if 0

typedef struct
  {
  int num_lines;
  int lines_alloc;

  char * method;
  char * protocol;
  char * path;
  
  char ** lines;
  } bgav_http_header_t;


/* Creation/destruction of http header */

bgav_http_header_t * bgav_http_header_create();

void bgav_http_header_reset(bgav_http_header_t*);
void bgav_http_header_destroy(bgav_http_header_t*);

void bgav_http_header_add_line(bgav_http_header_t*, const char * line);
void bgav_http_header_add_line_nocpy(bgav_http_header_t * h, char * line);

void bgav_http_header_init(bgav_http_header_t*, const char * method, const char * path,
                           const char * protocol);

int bgav_http_header_send(const bgav_options_t * opt, const bgav_http_header_t * h1,
                          const bgav_http_header_t * h2,
                          int fd);

char * bgav_http_header_to_string(const bgav_http_header_t * h1, const bgav_http_header_t * h2);

int bgav_http_header_status_code(bgav_http_header_t * h);
const char * bgav_http_header_status_line(bgav_http_header_t * h);

/* Reading of http header */

int bgav_http_header_revc(const bgav_options_t * opt,
                          bgav_http_header_t*, int fd);

const char * bgav_http_header_get_var(bgav_http_header_t*,
                                      const char * name);


void bgav_http_header_dump(bgav_http_header_t*);

#endif

/* http connection */

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

void bgav_http_set_metadata(bgav_http_t * h, gavl_dictionary_t * m);
