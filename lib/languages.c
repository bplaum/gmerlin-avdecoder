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

#include <avdec_private.h>
#include <language_table.h>

static const int num_lang = sizeof(language_codes)/sizeof(language_codes[0]);

const char * bgav_lang_from_name(const char * name)
  {
  int i;
  for(i = 0; i < num_lang; i++)
    {
    if(!strcmp(name, language_codes[i].name))
      return language_codes[i].iso_639_b;
    }
  return NULL;
  }

const char * bgav_lang_from_twocc(const char * twocc)
  {
  int i;

  for(i = 0; i < num_lang; i++)
    {
    if(language_codes[i].iso_639_2 &&
       (language_codes[i].iso_639_2[0] == twocc[0]) &&
       (language_codes[i].iso_639_2[1] == twocc[1]))
      return language_codes[i].iso_639_b;
    }
  return NULL;
  }

const char * bgav_lang_name(const char * lang)
  {
  int i;

  for(i = 0; i < num_lang; i++)
    {
    if(language_codes[i].iso_639_b &&
       (language_codes[i].iso_639_b[0] == lang[0]) &&
       (language_codes[i].iso_639_b[1] == lang[1]) && 
       (language_codes[i].iso_639_b[2] == lang[2]))
      return language_codes[i].name;
    }
  return NULL;
  }

void bgav_correct_language(char * lang)
  {
  int i;
  char lang_save[4];
  memcpy(lang_save, lang, 3);
  lang_save[3] = '\0';
  memset(lang, 0, 4);
  
  for(i = 0; i < num_lang; i++)
    {
    if(language_codes[i].iso_639_b &&
       !strcmp(language_codes[i].iso_639_b, lang_save))
      {
      memcpy(lang, language_codes[i].iso_639_b, 3);
      break;
      }
    else if(language_codes[i].iso_639_t &&
            language_codes[i].iso_639_b &&
            !strcmp(language_codes[i].iso_639_t,lang_save ))
      {
      memcpy(lang, language_codes[i].iso_639_b, 3);
      break;
      }
    }
  }
