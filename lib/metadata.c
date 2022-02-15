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

#include <avdec_private.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


const char * bgav_metadata_get_author(const bgav_metadata_t*m)
  {
  return gavl_dictionary_get_string(m, GAVL_META_AUTHOR);
  }

const char * bgav_metadata_get_title(const bgav_metadata_t*m)
  {
  return gavl_dictionary_get_string(m, GAVL_META_TITLE);
  }

const char * bgav_metadata_get_comment(const bgav_metadata_t*m)
  {
  return gavl_dictionary_get_string(m, GAVL_META_COMMENT);
  }

const char * bgav_metadata_get_copyright(const bgav_metadata_t*m)
  {
  return gavl_dictionary_get_string(m, GAVL_META_COPYRIGHT);
  }

const char * bgav_metadata_get_album(const bgav_metadata_t*m)
  {
  return gavl_dictionary_get_string(m, GAVL_META_ALBUM);
  }

const char * bgav_metadata_get_artist(const bgav_metadata_t*m)
  {
  return gavl_dictionary_get_string(m, GAVL_META_ARTIST);
  }

const char * bgav_metadata_get_albumartist(const bgav_metadata_t*m)
  {
  return gavl_dictionary_get_string(m, GAVL_META_ALBUMARTIST);
  }

const char * bgav_metadata_get_genre(const bgav_metadata_t*m)
  {
  return gavl_dictionary_get_string(m, GAVL_META_GENRE);
  }

const char * bgav_metadata_get_date(const bgav_metadata_t*m)
  {
  const char * ret = gavl_dictionary_get_string(m, GAVL_META_DATE);
  if(!ret)
    ret = gavl_dictionary_get_string(m, GAVL_META_YEAR);
  return ret;
  }

int bgav_metadata_get_track(const bgav_metadata_t*m)
  {
  int ret;
  if(gavl_dictionary_get_int(m, GAVL_META_TRACKNUMBER, &ret))
    return ret;
  return 0;
  }
