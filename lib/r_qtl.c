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

static int probe_yml_qtl(bgav_input_context_t * input)
  {
  bgav_yml_node_t * node;
  if((node = bgav_input_get_yml(input)) &&
     bgav_yml_find_by_pi(node, "quicktime") &&
     bgav_yml_find_by_name(node, "embed"))
    return 1;
  return 0;
  }

static bgav_track_table_t * parse_qtl(bgav_input_context_t * input)
  {
  const char * url;
  bgav_yml_node_t * node;
  bgav_track_table_t * ret;
  
  if(!(node = bgav_input_get_yml(input)) ||
     !(node = bgav_yml_find_by_name(node, "embed")) ||
     !(url = bgav_yml_get_attribute(node, "src")))
    return NULL;
  ret = bgav_track_table_create(1);

  gavl_dictionary_set_string(ret->tracks[0]->metadata, GAVL_META_MEDIA_CLASS, GAVL_META_MEDIA_CLASS_LOCATION);
  
  gavl_metadata_add_src(ret->tracks[0]->metadata, GAVL_META_SRC, NULL, url);
  return ret;
  }

const bgav_redirector_t bgav_redirector_qtl = 
  {
    .name =  "qtl",
    .probe = probe_yml_qtl,
    .parse = parse_qtl,
  };
