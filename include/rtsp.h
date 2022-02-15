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

#include <sdp.h>

/* rtsp.c */

typedef struct bgav_rtsp_s bgav_rtsp_t;

/* Housekeeping */

bgav_rtsp_t * bgav_rtsp_create(const bgav_options_t * opt)
  __attribute__ ((visibility("default")));


/* Open the URL and send the OPTIONS request */

int bgav_rtsp_open(bgav_rtsp_t *, const char * url, int * got_redirected)
  __attribute__ ((visibility("default")));

int bgav_rtsp_reopen(bgav_rtsp_t * rtsp);


void bgav_rtsp_close(bgav_rtsp_t *, int teardown);

/* Return raw socket */

int bgav_rtsp_get_fd(bgav_rtsp_t *);

/* Append a field to the next request header */

void bgav_rtsp_schedule_field(bgav_rtsp_t *, const char * field);

/* Get a field from the last answer */

const char * bgav_rtsp_get_answer(bgav_rtsp_t *, const char * name);

/* Now, the requests follow */

int bgav_rtsp_request_describe(bgav_rtsp_t *, int * got_redirected);

/* Get the redirection URL */

const char * bgav_rtsp_get_url(bgav_rtsp_t *);

/* After a successful describe request, the session description is there */

bgav_sdp_t * bgav_rtsp_get_sdp(bgav_rtsp_t *);

/* Issue a SETUP */

int bgav_rtsp_request_setup(bgav_rtsp_t *, const char * arg);

/* Set Parameter */

int bgav_rtsp_request_setparameter(bgav_rtsp_t *);

/* Play */

int bgav_rtsp_request_play(bgav_rtsp_t *);

