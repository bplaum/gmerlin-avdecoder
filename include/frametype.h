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



#ifndef BGAV_FRAMETYPE_H_INCLUDED
#define BGAV_FRAMETYPE_H_INCLUDED

/* Detect frametypes (especially if B-frames can occur)
   from timestamps */

typedef struct bgav_frametype_detector_s bgav_frametype_detector_t;

bgav_frametype_detector_t *
bgav_frametype_detector_create(bgav_stream_t * s);

void bgav_frametype_detector_destroy(bgav_frametype_detector_t *);
void bgav_frametype_detector_reset(bgav_frametype_detector_t *);

#endif // BGAV_FRAMETYPE_H_INCLUDED

