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

#include <stdlib.h>

#include <avdec_private.h>

const gavl_chapter_list_t * bgav_get_chapter_list(bgav_t * bgav, int track)
  {
  const gavl_chapter_list_t * ret;
  
  if((track >= bgav->tt->num_tracks) || (track < 0) ||
     !(ret = gavl_dictionary_get_chapter_list(bgav->tt->tracks[track]->metadata)))
    return NULL;
  return ret;
  }

int bgav_get_num_chapters(bgav_t * bgav, int track, int * timescale)
  {
  const gavl_chapter_list_t * list;
  list = bgav_get_chapter_list(bgav, track);
  if(!list)
    {
    *timescale = 0;
    return 0;
    }
  *timescale = gavl_chapter_list_get_timescale(list);
  return gavl_chapter_list_get_num(list);
  }

const char *
bgav_get_chapter_name(bgav_t * bgav, int track, int chapter)
  {
  const gavl_chapter_list_t * list;
  list = bgav_get_chapter_list(bgav, track);
  
  if(!list)
    return NULL;
  return gavl_chapter_list_get_label(list, chapter);
  }

int64_t bgav_get_chapter_time(bgav_t * bgav, int track, int chapter)
  {
  const gavl_chapter_list_t * list;
  list = bgav_get_chapter_list(bgav, track);
  if(!list)
    return 0;
  
  return gavl_chapter_list_get_time(list, chapter);
  }
