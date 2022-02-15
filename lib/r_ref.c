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
#include <ctype.h>

#include <avdec_private.h>


static int probe_ref(bgav_input_context_t * input)
  {
  char probe_data[11];
  if(bgav_input_get_data(input, (uint8_t*)probe_data, 11) < 11)
    return 0;

  if(!strncasecmp(probe_data, "[Reference]", 11))
    return 1;
  return 0;
  }

static bgav_track_table_t * parse_ref(bgav_input_context_t * input)
  {
  char * buffer = NULL;
  uint32_t buffer_alloc = 0;
  char * pos;
  bgav_track_table_t * ret;
  bgav_track_t * t;
  
  if(!bgav_input_read_line(input, &buffer, &buffer_alloc, 0, NULL))
    return NULL;

  if(strncasecmp(buffer, "[Reference]", 11))
    return NULL;

  ret = bgav_track_table_create(0);
  
  while(1)
    {
    if(!bgav_input_read_line(input, &buffer, &buffer_alloc, 0, NULL))
      break;

    if(!strncasecmp(buffer, "ref", 3) && isdigit(buffer[3]))
      {
      pos = strchr(buffer, '=');
      if(pos)
        {
        pos++;

        t = bgav_track_table_append_track(ret);
        
        gavl_dictionary_set_string_nocopy(t->metadata,
                                GAVL_META_LABEL,
                                bgav_sprintf("Stream %d (%s)",
                                             ret->num_tracks,
                                             pos));
        
        if(!strncasecmp(pos, "http://", 7))
          gavl_dictionary_set_string_nocopy(t->metadata,
                                  GAVL_META_REFURL,
                                  bgav_sprintf("mmsh%s", pos+4));
        else
          gavl_dictionary_set_string(t->metadata,
                            GAVL_META_REFURL,
                            pos);
        }
      }
    }
  return ret;
  }

const bgav_redirector_t bgav_redirector_ref = 
  {
    .name =  "[Reference]",
    .probe = probe_ref,
    .parse = parse_ref
  };
