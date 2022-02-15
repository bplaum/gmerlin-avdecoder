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
#include <ctype.h>
#include <avdec_private.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG_DOMAIN "r_asx"

static int probe_asx(bgav_input_context_t * input)
  {
  char * pos;
  char buf[16];
  int i;
  /* We accept all files, which end with .asx */

  if(input->filename)
    {
    pos = strrchr(input->filename, '.');
    if(pos)
      {
      pos++;
      if(!strcasecmp(pos, "asx"))
        return 1;
      }
    }
  if(bgav_input_get_data(input, (uint8_t*)buf, 16) < 16)
    return 0;
  for(i = 0; i < 16-4; i++)
    {
    if((buf[i+0] == '<') &&
       (tolower(buf[i+1]) == 'a') && 
       (tolower(buf[i+2]) == 's') && 
       (tolower(buf[i+3]) == 'x'))
      return 1;
    }
  return 0;
  }

static int sc(const char * str1, const char * str2)
  {
  if((!str1) || (!str2))
    return -1;
  return strcasecmp(str1, str2);
  }

static int count_urls(bgav_yml_node_t * n)
  {
  int ret = 0;

  while(n)
    {
    if(!sc(n->name, "Entry"))
      {
      ret++;
      }
    else if(!sc(n->name, "Repeat"))
      {
      ret += count_urls(n->children);
      }
    n = n->next;
    }
  return ret;
  }

static void get_url(bgav_yml_node_t * n, bgav_track_table_t * tt,
                    const char * title, int * index)
  {
  bgav_track_t * ret = tt->tracks[*index];
  while(n)
    {
    if(!sc(n->name, "Title") && n->children)
      {
      if(title)
        gavl_dictionary_set_string_nocopy(ret->metadata,
                                GAVL_META_LABEL,
                                bgav_sprintf("%s (%s)",
                                             title, n->children->str));
      else
        gavl_dictionary_set_string(ret->metadata,
                          GAVL_META_LABEL,
                          n->children->str);
      }
    else if(!sc(n->name, "Ref"))
      {
      gavl_dictionary_set_string(ret->metadata,
                        GAVL_META_REFURL,
                        bgav_yml_get_attribute_i(n, "href"));
      }
    n = n->next;
    }

  if(!gavl_dictionary_get_string(ret->metadata, GAVL_META_LABEL))
    {
    gavl_dictionary_set_string_nocopy(ret->metadata,
                            GAVL_META_LABEL,
                            bgav_sprintf("Stream %d (%s)",
                                         (*index)+1,
                                         gavl_dictionary_get_string(ret->metadata,
                                                           GAVL_META_REFURL)));
    }
  (*index)++;
  }

static int get_urls(bgav_yml_node_t * n,
                    bgav_track_table_t * ret,
                    const char * title, int * index)
  {
  while(n)
    {
    if(!sc(n->name, "Entry"))
      {
      get_url(n->children, ret, title, index);
      }
    else if(!sc(n->name, "Repeat"))
      {
      get_urls(n->children, ret, title, index);
      }
    n = n->next;
    }
  return 1;
  }

static bgav_track_table_t * xml_2_asx(bgav_yml_node_t * n)
  {
  int index;
  char * title;
  bgav_yml_node_t * node;
  int num_urls;
  bgav_track_table_t * ret;
  
  /* Count the entries and get the global title */

  n = bgav_yml_find_by_name(n, "ASX");
  
  if(!n)
    return 0;
  
  node = n->children;
  num_urls = 0;
  
  title = NULL;

  /* Try to fetch a title */
    
  while(node)
    {
    if(!sc(node->name, "Title") && node->children)
      {
      title = gavl_strdup(node->children->str);
      }
    node = node->next;
    }

  /* Count the entries */

  num_urls = count_urls(n->children);

  ret = bgav_track_table_create(num_urls);
  
  /* Now, loop through all streams and collect the values */
  
  index = 0;

  get_urls(n->children, ret, title, &index);
  
  if(title)
    free(title);
  return ret;
  }


static bgav_track_table_t * parse_asx(bgav_input_context_t * input)
  {
  bgav_track_table_t * result;
  bgav_yml_node_t * node;
  
  node = bgav_input_get_yml(input);
  if(!node)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Parse asx failed (yml error)");
    return 0;
    }

  result = xml_2_asx(node);
  bgav_yml_free(node);

  if(!result)
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Parse asx failed");

  return result;
  }

const bgav_redirector_t bgav_redirector_asx = 
  {
    .name =  "ASX (Windows Media)",
    .probe = probe_asx,
    .parse = parse_asx,
  };
