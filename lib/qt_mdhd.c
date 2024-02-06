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
#include <stdio.h>
#include <string.h>

#include <qt.h>

#define LOG_DOMAIN "qt_mdhd"

/*
typedef struct
  {
  qt_atom_header_t h;

  uint32_t creation_time;
  uint32_t modification_time;
  uint32_t time_scale;
  uint32_t duration;
  uint16_t language;
  uint16_t quality;
  } qt_mdhd_t;
*/

int bgav_qt_mdhd_get_language(qt_mdhd_t * m, char * ret)
  {
  if(!bgav_qt_get_language(m->language, ret))
    {
    ret[0] = ((m->language >> 10) & 0x1f) + 0x60;
    ret[1] = ((m->language >> 5) & 0x1f)  + 0x60;
    ret[2] = (m->language & 0x1f)         + 0x60;
    ret[3] = '\0';
    }
  bgav_correct_language(ret);
  return 1;
  }

void bgav_qt_mdhd_dump(int indent, qt_mdhd_t * m)
  {
  char lang_code[4];
  const char * language;
  const char * charset = NULL;
  memset(lang_code, 0, 4);

  bgav_qt_mdhd_get_language(m, lang_code);
  
  language = bgav_lang_name(lang_code);
  
  charset = bgav_qt_get_charset(m->language);
  if(!charset)
    charset = "UTF-8/16";
  
  gavl_diprintf(indent, "mdhd:\n");
  gavl_diprintf(indent+2, "version:           %d\n", m->version);
  gavl_diprintf(indent+2, "flags:             %06xd\n", m->flags);
  
  gavl_diprintf(indent+2, "creation_time:     %"PRId64"\n", m->creation_time);
  gavl_diprintf(indent+2, "modification_time: %"PRId64"\n", m->modification_time);
  gavl_diprintf(indent+2, "time_scale:        %d\n", m->time_scale);
  gavl_diprintf(indent+2, "duration:          %"PRId64"\n", m->duration);
  gavl_diprintf(indent+2, "language:          %d (%s, charset: %s)\n",
                m->language, language, charset);
  gavl_diprintf(indent+2, "quality:           %d\n", m->quality);
  gavl_diprintf(indent, "end of mdhd\n");

  }


int bgav_qt_mdhd_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_mdhd_t * ret)
  {
  //   int result;
  READ_VERSION_AND_FLAGS;
  memcpy(&ret->h, h, sizeof(*h));

  if(ret->version == 0)
    {
    uint32_t dummy1, dummy2;
    if(!bgav_input_read_32_be(input, &dummy1) ||
       !bgav_input_read_32_be(input, &dummy2))
      return 0;
    ret->creation_time     = dummy1;
    ret->modification_time = dummy2;
    }
  else if(ret->version == 1)
    {
    if(!bgav_input_read_64_be(input, &ret->creation_time) ||
       !bgav_input_read_64_be(input, &ret->modification_time))
      return 0;
    }
  else
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Unsupported version %d in mdhd",
             ret->version);
    return 0;
    }
  
  if(!bgav_input_read_32_be(input, &ret->time_scale))
    return 0;
  
  if(ret->version == 0)
    {
    uint32_t dummy;
    if(!bgav_input_read_32_be(input, &dummy))
      return 0;
    ret->duration = dummy;
    }
  else if(ret->version == 1)
    {
    if(!bgav_input_read_64_be(input, &ret->duration))
      return 0;
    }

  if(!bgav_input_read_16_be(input, &ret->language) ||
     !bgav_input_read_16_be(input, &ret->quality))
    return 0;

  return 1;
  }

void bgav_qt_mdhd_free(qt_mdhd_t * c)
  {
  
  }

