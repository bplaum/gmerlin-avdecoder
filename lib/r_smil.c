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

#define LOG_DOMAIN "r_smil"

static int probe_smil(bgav_input_context_t * input)
  {
  uint8_t buf[5];

  /* We accept all files, which end with .smil or .smi */

  if(input->location &&
     (gavl_string_ends_with_i(input->location, ".smil") ||
      gavl_string_ends_with_i(input->location, ".smi")))
    return 1;
  
  if(bgav_input_get_data(input, buf, 5) < 5)
    return 0;
  if((buf[0] == '<') &&
     (buf[1] == 's') && 
     (buf[2] == 'm') && 
     (buf[3] == 'i') &&
     (buf[4] == 'l'))
    return 1;

  
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
    if(!sc(n->name, "audio") || !sc(n->name, "video"))
      {
      ret++;
      }
    else if(n->children)
      {
      ret += count_urls(n->children);
      }
    n = n->next;
    }
  return ret;
  }

#if 1

static void get_url(bgav_yml_node_t * n, bgav_track_t * ret,
                    const char * title, const char * url_base, int * index)
  {
  const char * location;
  const char * bitrate;
  const char * language;

  int i;
  
  location =
    gavl_strdup(bgav_yml_get_attribute_i(n, "src"));
  language = 
    gavl_strdup(bgav_yml_get_attribute_i(n, "system-language"));
  bitrate = 
    gavl_strdup(bgav_yml_get_attribute_i(n, "system-bitrate"));

  
  if(!location)
    return;

  /* Set URL */

  gavl_dictionary_set_string(ret->metadata, GAVL_META_MEDIA_CLASS, GAVL_META_MEDIA_CLASS_LOCATION);
  
  if(!strstr(location, "://") && url_base)
    {
    char * uri;
    
    if(url_base[strlen(url_base)-1] == '/')
      uri = gavl_sprintf("%s%s", url_base, location);
    else
      uri = gavl_sprintf("%s/%s", url_base, location);

    gavl_metadata_add_src(ret->metadata, GAVL_META_SRC, NULL, uri);
    
    free(uri);
    }
  else
    {
    gavl_metadata_add_src(ret->metadata, GAVL_META_SRC, NULL, location);
    }
  /* Set name */

  if(title)
    gavl_dictionary_set_string_nocopy(ret->metadata, GAVL_META_LABEL,
                            gavl_sprintf("%s Stream %d", title, (*index)+1));
  else
    gavl_dictionary_set_string_nocopy(ret->metadata, GAVL_META_LABEL,
                            gavl_sprintf("%s Stream %d", location, (*index)+1));


  if(bitrate)
    {
    i = atoi(bitrate);
    gavl_dictionary_set_int(ret->metadata, GAVL_META_BITRATE, i);
    }

  if(language)
    gavl_dictionary_set_string(ret->metadata, GAVL_META_LANGUAGE,
                      bgav_lang_from_twocc(language));
  
  (*index)++;
  }

static int get_urls(bgav_yml_node_t * n,
                    bgav_track_table_t * tab,
                    const char * title, const char * url_base, int * index)
  {
  while(n)
    {
    if(!sc(n->name, "audio") || !sc(n->name, "video"))
      {
      get_url(n, tab->tracks[*index], title, url_base, index);
      }
    else if(n->children)
      {
      get_urls(n->children, tab, title, url_base, index);
      }
    n = n->next;
    }
  return 1;
  }

#endif

static bgav_track_table_t * xml_2_smil(bgav_input_context_t * input,
                                           bgav_yml_node_t * n)
  {
  int index;
  bgav_yml_node_t * node;
  bgav_yml_node_t * child_node;
  char * url_base = NULL;
  const char * title = NULL;
  char * pos;
  int num_urls;

  bgav_track_table_t * ret;

  
  n = bgav_yml_find_by_name(n, "smil");
  
  if(!n)
    return NULL;
  
  node = n->children;


  /* Get the header */
  
  while(node)
    {
    if(!sc(node->name, "head"))
      {
      child_node = node->children;
      //      title = gavl_strdup(node->children->str);

      while(child_node)
        {
        /* Parse meta info */
        
        if(!sc(child_node->name, "meta"))
          {
          
          /* Fetch url base */
          
          if(!url_base)
            {
            if(!sc(bgav_yml_get_attribute(child_node, "name"), "base"))
              {
              url_base =
                gavl_strdup(bgav_yml_get_attribute(child_node, "content"));
              }
            }

          /* Fetch title */

          if(!title)
            {
            if(!sc(bgav_yml_get_attribute(child_node, "name"), "title"))
              {
              title = bgav_yml_get_attribute(child_node, "content");
              }
            }


          }
        child_node = child_node->next;
        }
      }
    
    if(!sc(node->name, "body"))
      break;
    node = node->next;
    }

  if(!url_base && input->location)
    {
    pos = strrchr(input->location, '/');
    if(pos)
      {
      pos++;
      url_base = gavl_strndup(input->location, pos);
      }
    }
  
  /* Count the entries */

  
  num_urls = count_urls(node->children);
  ret = bgav_track_table_create(num_urls);
  
  /* Now, loop through all streams and collect the values */
  
  index = 0;

  get_urls(node->children, ret, title, url_base, &index);

  if(url_base)
    free(url_base);
  
  return ret;
  }


static bgav_track_table_t * parse_smil(bgav_input_context_t * input)
  {
  bgav_track_table_t * ret;
  bgav_yml_node_t * node;

  node = bgav_input_get_yml(input);
  
  if(!node)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Parse smil failed (yml error)");
    return 0;
    }

  //  bgav_yml_dump(node);
  
  ret = xml_2_smil(input, node);

  bgav_yml_free(node);
  if(!ret)
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Parse smil failed");
  return ret;
  }

const bgav_redirector_t bgav_redirector_smil = 
  {
    .name =  "smil",
    .probe = probe_smil,
    .parse = parse_smil
  };
