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
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define PROBE_BYTES 8

static int probe_rtsptext(bgav_input_context_t * input)
  {
  char probe_buffer[PROBE_BYTES];
  /* Most likely, we get this via http, so we can check the mimetype */

  if(bgav_input_get_data(input, (uint8_t*)probe_buffer, PROBE_BYTES) < PROBE_BYTES)
    return 0;
  
  if(!strncasecmp(probe_buffer, "rtsptext", PROBE_BYTES))
    return 1;
  return 0;
  }

static bgav_track_table_t * parse_rtsptext(bgav_input_context_t * input)
  {
  char * pos;
  bgav_track_table_t * ret;
  char * str;
  gavl_buffer_t line_buf;
  gavl_buffer_init(&line_buf);
  
  if(!bgav_input_read_line(input, &line_buf))
    return 0;
  str = (char*)line_buf.buf;
  
  pos = str + 8;

  while(isspace(*pos) && (*pos != '\0'))
    pos++;

  ret = bgav_track_table_create(1);
  
  if(*pos != '\0')
    {
    gavl_dictionary_set_string(ret->tracks[0]->metadata, GAVL_META_CLASS, GAVL_META_CLASS_LOCATION);
    gavl_metadata_add_src(ret->tracks[0]->metadata, GAVL_META_SRC, NULL, pos);
    }
  else
    {
    if(!bgav_input_read_line(input, &line_buf))
      return 0;
    str = (char*)line_buf.buf;

    gavl_dictionary_set_string(ret->tracks[0]->metadata, GAVL_META_CLASS, GAVL_META_CLASS_LOCATION);
    gavl_metadata_add_src(ret->tracks[0]->metadata, GAVL_META_SRC, NULL, str);
    }

  gavl_buffer_free(&line_buf);
  return ret;
  }

const bgav_redirector_t bgav_redirector_rtsptext = 
  {
    .name =  "rtsptext",
    .probe = probe_rtsptext,
    .parse = parse_rtsptext
  };
