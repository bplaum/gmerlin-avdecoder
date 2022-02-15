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

#define PTS_CACHE_SIZE 32

typedef struct
  {
  int64_t pts;
  int duration;
  int used;
  gavl_timecode_t tc;
  } bgav_pts_cache_entry_t;

typedef struct
  {
  bgav_pts_cache_entry_t entries[PTS_CACHE_SIZE];
  } bgav_pts_cache_t;

void bgav_pts_cache_push(bgav_pts_cache_t * c, bgav_packet_t * p,
                         int * index,
                         bgav_pts_cache_entry_t ** e);

void bgav_pts_cache_clear(bgav_pts_cache_t * c);

/* Get the smallest timestamp */
int bgav_pts_cache_get_first(bgav_pts_cache_t * c, gavl_video_frame_t * f);
int bgav_pts_cache_peek_first(bgav_pts_cache_t * c, gavl_video_frame_t * f);


int64_t bgav_pts_cache_peek_last(bgav_pts_cache_t * c, int * duration);
