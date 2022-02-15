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

typedef struct bgav_mms_s bgav_mms_t;

/* Open an mms url, return opaque structure */

bgav_mms_t * bgav_mms_open(const bgav_options_t * opt, const char * url)
  __attribute__ ((visibility("default")));

/* After a successful open call, the ASF header can obtained
   with the following function */

uint8_t * bgav_mms_get_header(bgav_mms_t * mms, int * len);

/* Select the streams, right now, all streams MUST be switched on */

int bgav_mms_select_streams(bgav_mms_t * mms,
                            int * stream_ids, int num_streams);

/*
 *  This reads data (usually one asf packet)
 *  NULL is returned on EOF 
 */

uint8_t * bgav_mms_read_data(bgav_mms_t * mms, int * len, int block);

void bgav_mms_close(bgav_mms_t*);

