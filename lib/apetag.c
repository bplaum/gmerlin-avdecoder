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




#include <avdec_private.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>



struct bgav_ape_tag_s
  {
  uint32_t num_items;

  struct
    {
    char * key;
    char * str;
    } * items;

  };

int bgav_ape_tag_probe(bgav_input_context_t * input, int * tag_size)
  {
  uint32_t size, flags;
  
  uint8_t probe_data[32];
  uint8_t * ptr;
  
  if(bgav_input_read_data(input, probe_data, 32) < 32)
    return 0;

  if(strncmp((char*)probe_data, "APETAGEX", 8))
    return 0;

  /* Now, compute the size */ 

  ptr = &probe_data[12];
  
  size = GAVL_PTR_2_32LE(ptr);

  ptr = &probe_data[20];
  
  flags = GAVL_PTR_2_32LE(ptr);

  *tag_size = size;

  if(flags & 0x80000000)
    *tag_size += 32;
  
  return 1;
  }

bgav_ape_tag_t * bgav_ape_tag_read(bgav_input_context_t * input, int tag_size)
  {
  
  int i;
  bgav_ape_tag_t *ret;
  uint32_t flags, item_value_size;
  uint8_t * buffer, *ptr;

  ret = calloc(1, sizeof(*ret));
  
  buffer = malloc(tag_size);
  if(bgav_input_read_data(input, buffer, tag_size) < tag_size)
    return NULL;

  /* Go to start of footer */
  ptr = buffer + (tag_size - 16);
  
  ret->num_items = GAVL_PTR_2_32LE(ptr);ptr += 4;
  ret->items = calloc(ret->num_items, sizeof(*ret->items));

  flags = GAVL_PTR_2_32LE(ptr);

  ptr = buffer;
  if(flags & 0x80000000)
    ptr += 32;
  
  /* Goto start of the items */
  
  for(i = 0; i < ret->num_items; i++)
    {
    item_value_size = GAVL_PTR_2_32LE(ptr); ptr+=4;
    flags  = GAVL_PTR_2_32LE(ptr);          ptr+=4;

    ret->items[i].key = gavl_strdup((char*)ptr);
    ptr += strlen(ret->items[i].key) + 1;

    if((flags & 0x00000006) == 0) /* UTF-8 Data */
      {
      ret->items[i].str = gavl_strndup((char*)ptr, (char*)(ptr + item_value_size));
      ptr += item_value_size;
      }
    }
  //  bgav_ape_tag_dump(ret);
  return ret;
  }

void bgav_ape_tag_dump(bgav_ape_tag_t * tag)
  {
  int i;
  gavl_dprintf("APE Tag %d items\n", tag->num_items);

  for(i = 0; i < tag->num_items; i++)
    {
    gavl_dprintf("  Item %d\n", i+1);
    gavl_dprintf("    Key: %s\n", tag->items[i].key);

    if(tag->items[i].str)
      gavl_dprintf("    String: %s\n", tag->items[i].str);
    else
      gavl_dprintf("    No usable value\n");
    }
  }

#define STRVAL(k, gavl_key) \
  if(!strcasecmp(tag->items[i].key, k) && tag->items[i].str)  \
    gavl_dictionary_set_string(m, gavl_key, tag->items[i].str);

#define INTVAL(k, gavl_key) \
  if(!strcasecmp(tag->items[i].key, k) && tag->items[i].str)            \
    gavl_dictionary_set_int(m, gavl_key, atoi(tag->items[i].str));

void bgav_ape_tag_2_metadata(bgav_ape_tag_t * tag, gavl_dictionary_t * m)
  {
  int i;
  for(i = 0; i < tag->num_items; i++)
    {
    STRVAL("Genre",     GAVL_META_GENRE);
    STRVAL("Year",      GAVL_META_YEAR);
    STRVAL("Artist",    GAVL_META_ARTIST);
    STRVAL("Album",     GAVL_META_ALBUM);
    STRVAL("Title",     GAVL_META_TITLE);
    STRVAL("Comment",   GAVL_META_COMMENT);
    STRVAL("Composer",  GAVL_META_AUTHOR);
    STRVAL("Copyright", GAVL_META_COPYRIGHT);

    INTVAL("Track", GAVL_META_TRACKNUMBER);
    }
  }

#undef STRVAL
#undef INTVAL

#define FREE(p) if(p) free(p);

void bgav_ape_tag_destroy(bgav_ape_tag_t * tag)
  {
  int i;
  for(i = 0; i < tag->num_items; i++)
    {
    FREE(tag->items[i].key);
    FREE(tag->items[i].str);
    }
  FREE(tag->items);
  free(tag);
  }

#undef FREE
