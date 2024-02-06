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

#include <string.h>
#include <stdlib.h>
#include <avdec_private.h>
#include <vorbis_comment.h>
#include <stdio.h>


/*
 *  Parsing code written straightforward after
 *  http://www.xiph.org/ogg/vorbis/doc/v-comment.html
 */

void bgav_vorbis_comment_dump(bgav_vorbis_comment_t * ret)
  {
  int i;
  gavl_dprintf( "Vorbis comment:\n");
  gavl_dprintf( "  Vendor string: %s\n", ret->vendor);
  
  for(i = 0; i < ret->num_user_comments; i++)
    {
    gavl_dprintf( "  %s\n", ret->user_comments[i]);
    }
  }

int bgav_vorbis_comment_read(bgav_vorbis_comment_t * ret,
                             bgav_input_context_t * input)
  {
  int i;
  uint32_t len;
  uint32_t num;
  
  // [vendor_length] = read an unsigned integer of 32 bits
  if(!bgav_input_read_32_le(input, &len))
    return 0;

  // [vendor_string] = read a UTF-8 vector as [vendor_length] octets
  ret->vendor = malloc(len+1);
  if(bgav_input_read_data(input, (uint8_t*)(ret->vendor), len) < len)
    return 0;
  ret->vendor[len] = '\0';

  // [user_comment_list_length] = read an unsigned integer of 32 bits
  if(!bgav_input_read_32_le(input, &num))
    return 0;

  ret->num_user_comments = num;
  ret->user_comments = calloc(ret->num_user_comments,
                              sizeof(*(ret->user_comments)));
  // iterate [user_comment_list_length] times
  for(i = 0; i < ret->num_user_comments; i++)
    {
    // [length] = read an unsigned integer of 32 bits
    if(!bgav_input_read_32_le(input, &len))
      return 0;

    
    ret->user_comments[i] = malloc(len + 1);
    if(bgav_input_read_data(input, (uint8_t*)(ret->user_comments[i]), len) < len)
      return 0;
    ret->user_comments[i][len] = '\0';
    }
  return 1;
  }

static char const * const artist_key = "ARTIST";
static char const * const author_key = "AUTHOR";
static char const * const album_key = "ALBUM";
static char const * const title_key = "TITLE";
// static char const * const _version_key = "VERSION=";
static char const * const track_number_key = "TRACKNUMBER";
// static char const * const _organization_key = "ORGANIZATION=";
static char const * const genre_key = "GENRE";
// static char const * const _description_key = "DESCRIPTION=";
static char const * const date_key = "DATE";
// static char const * const _location_key = "LOCATION=";
static char const * const copyright_key = "COPYRIGHT";

static char const * const albumartist1_key = "ALBUM ARTIST";
static char const * const albumartist2_key = "ALBUMARTIST";

const char *
bgav_vorbis_comment_get_field(bgav_vorbis_comment_t * vc, const char * key, int idx)
  {
  int i, j;
  int key_len = strlen(key);
  
  j = 0;

  for(i = 0; i < vc->num_user_comments; i++)
    {
    if(!strncasecmp(vc->user_comments[i], key, key_len) &&
        vc->user_comments[i][key_len] == '=')
      {
      if(idx == j)
        return vc->user_comments[i] + key_len + 1;
      else
        j++;
      }
    }
  return NULL;
  }

void bgav_vorbis_comment_2_metadata(bgav_vorbis_comment_t * comment,
                                    bgav_metadata_t * m)
  {
  const char * field;
  int j;

  j = 0;
  while((field = bgav_vorbis_comment_get_field(comment, artist_key, j)))
    {
    gavl_dictionary_append_string_array(m, GAVL_META_ARTIST, field);
    j++;
    }

  j = 0;
  while((field = bgav_vorbis_comment_get_field(comment, author_key, j)))
    {
    gavl_dictionary_append_string_array(m, GAVL_META_AUTHOR, field);
    j++;
    }

  if((field = bgav_vorbis_comment_get_field(comment, album_key, 0)))
    gavl_dictionary_set_string(m, GAVL_META_ALBUM, field);

  if((field = bgav_vorbis_comment_get_field(comment, title_key, 0)))
    gavl_dictionary_set_string(m, GAVL_META_TITLE, field);

  j = 0;
  while((field = bgav_vorbis_comment_get_field(comment, genre_key, j)))
    {
    gavl_dictionary_append_string_array(m, GAVL_META_GENRE, field);
    j++;
    }

  if((field = bgav_vorbis_comment_get_field(comment, date_key, 0)))
    {
    if(strlen(field) == 4)
      gavl_dictionary_set_string(m, GAVL_META_YEAR, field);
    else
      gavl_dictionary_set_string(m, GAVL_META_DATE, field);
    
    }

  if((field = bgav_vorbis_comment_get_field(comment, copyright_key, 0)))
    gavl_dictionary_set_string(m, GAVL_META_COPYRIGHT, field);

  if((field = bgav_vorbis_comment_get_field(comment, albumartist1_key, 0)))
    {
    gavl_dictionary_append_string_array(m, GAVL_META_ALBUMARTIST, field);    

    j = 1;
    while((field = bgav_vorbis_comment_get_field(comment, albumartist1_key, j)))
      {
      gavl_dictionary_append_string_array(m, GAVL_META_ALBUMARTIST, field);
      j++;
      }
    }
  else if((field = bgav_vorbis_comment_get_field(comment, albumartist2_key, 0)))
    {
    gavl_dictionary_append_string_array(m, GAVL_META_ALBUMARTIST, field);

    j = 1;
    while((field = bgav_vorbis_comment_get_field(comment, albumartist2_key, j)))
      {
      gavl_dictionary_append_string_array(m, GAVL_META_ALBUMARTIST, field);
      j++;
      }
    }
  
  if((field = bgav_vorbis_comment_get_field(comment, track_number_key, 0)))
    gavl_dictionary_set_int(m, GAVL_META_TRACKNUMBER, atoi(field));
  
  for(j = 0; j < comment->num_user_comments; j++)
    {
    if(!strchr(comment->user_comments[j], '='))
      {
      gavl_dictionary_set_string(m, GAVL_META_COMMENT, comment->user_comments[j]);
      break;
      }
    }
  }

#define MY_FREE(ptr) if(ptr) free(ptr);

void bgav_vorbis_comment_free(bgav_vorbis_comment_t * ret)
  {
  int i;
  MY_FREE(ret->vendor);

  for(i = 0; i < ret->num_user_comments; i++)
    {
    MY_FREE(ret->user_comments[i]);
    }
  MY_FREE(ret->user_comments);
  }

void bgav_vorbis_set_channel_setup(gavl_audio_format_t * format)
  {
  switch(format->num_channels)
    {
    case 1:
      format->channel_locations[0] = GAVL_CHID_FRONT_CENTER;
      break;
    case 2:
      format->channel_locations[0] = GAVL_CHID_FRONT_LEFT;
      format->channel_locations[1] = GAVL_CHID_FRONT_RIGHT;
      break;
    case 3:
      format->channel_locations[0] = GAVL_CHID_FRONT_LEFT;
      format->channel_locations[1] = GAVL_CHID_FRONT_CENTER;
      format->channel_locations[2] = GAVL_CHID_FRONT_RIGHT;
      break;
    case 4:
      format->channel_locations[0] = GAVL_CHID_FRONT_LEFT;
      format->channel_locations[1] = GAVL_CHID_FRONT_RIGHT;
      format->channel_locations[2] = GAVL_CHID_REAR_LEFT;
      format->channel_locations[3] = GAVL_CHID_REAR_RIGHT;
      break;
    case 5:
      format->channel_locations[0] =  GAVL_CHID_FRONT_LEFT;
      format->channel_locations[1] =  GAVL_CHID_FRONT_CENTER;
      format->channel_locations[2] =  GAVL_CHID_FRONT_RIGHT;
      format->channel_locations[3] =  GAVL_CHID_REAR_LEFT;
      format->channel_locations[4] =  GAVL_CHID_REAR_RIGHT;
      break;
    case 6:
      format->channel_locations[0] =  GAVL_CHID_FRONT_LEFT;
      format->channel_locations[1] =  GAVL_CHID_FRONT_CENTER;
      format->channel_locations[2] =  GAVL_CHID_FRONT_RIGHT;
      format->channel_locations[3] =  GAVL_CHID_REAR_LEFT;
      format->channel_locations[4] =  GAVL_CHID_REAR_RIGHT;
      format->channel_locations[5] =  GAVL_CHID_LFE;
      break;
    case 7:
      format->channel_locations[0] =  GAVL_CHID_FRONT_LEFT;
      format->channel_locations[1] =  GAVL_CHID_FRONT_CENTER;
      format->channel_locations[2] =  GAVL_CHID_FRONT_RIGHT;
      format->channel_locations[3] =  GAVL_CHID_SIDE_LEFT;
      format->channel_locations[4] =  GAVL_CHID_SIDE_RIGHT;
      format->channel_locations[5] =  GAVL_CHID_REAR_CENTER;
      format->channel_locations[6] =  GAVL_CHID_LFE;
      break;
    case 8:
      format->channel_locations[0] =  GAVL_CHID_FRONT_LEFT;
      format->channel_locations[1] =  GAVL_CHID_FRONT_CENTER;
      format->channel_locations[2] =  GAVL_CHID_FRONT_RIGHT;
      format->channel_locations[3] =  GAVL_CHID_SIDE_LEFT;
      format->channel_locations[4] =  GAVL_CHID_SIDE_RIGHT;
      format->channel_locations[5] =  GAVL_CHID_REAR_LEFT;
      format->channel_locations[6] =  GAVL_CHID_REAR_RIGHT;
      format->channel_locations[7] =  GAVL_CHID_LFE;
      break;
      
    }

  }
