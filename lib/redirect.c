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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define LOG_DOMAIN "redirector"

extern const bgav_redirector_t bgav_redirector_asx;
extern const bgav_redirector_t bgav_redirector_m3u;
extern const bgav_redirector_t bgav_redirector_pls;
extern const bgav_redirector_t bgav_redirector_ref;
extern const bgav_redirector_t bgav_redirector_smil;
extern const bgav_redirector_t bgav_redirector_rtsptext;
extern const bgav_redirector_t bgav_redirector_qtl;

void bgav_redirectors_dump()
  {
  gavl_dprintf( "<h2>Redirectors</h2>\n");
  gavl_dprintf( "<ul>\n");
  gavl_dprintf( "<li>%s\n", bgav_redirector_asx.name);
  gavl_dprintf( "<li>%s\n", bgav_redirector_m3u.name);
  gavl_dprintf( "<li>%s\n", bgav_redirector_pls.name);
  gavl_dprintf( "<li>%s\n", bgav_redirector_ref.name);
  gavl_dprintf( "<li>%s\n", bgav_redirector_smil.name);
  gavl_dprintf( "<li>%s\n", bgav_redirector_rtsptext.name);
  gavl_dprintf( "<li>%s\n", bgav_redirector_qtl.name);
  gavl_dprintf( "</ul>\n");
  }

typedef struct
  {
  const bgav_redirector_t * r;
  char * format_name;
  } redir_t;


const redir_t redirectors[] =
  {
    { &bgav_redirector_asx, "asx" },
    { &bgav_redirector_pls, "pls" },
    { &bgav_redirector_ref, "MS Referece" },
    { &bgav_redirector_smil, "smil" },
    { &bgav_redirector_m3u, "m3u" },
    { &bgav_redirector_rtsptext, "rtsptext" },
    { &bgav_redirector_qtl, "qtl" },
  };

static const int num_redirectors = sizeof(redirectors)/sizeof(redirectors[0]);

const bgav_redirector_t *
bgav_redirector_probe(bgav_input_context_t * input)
  {
  int i;

  for(i = 0; i < num_redirectors; i++)
    {
    if(redirectors[i].r->probe(input))
      {
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
               "Detected %s redirector", redirectors[i].format_name);
      return redirectors[i].r;
      }
    }
  return NULL;
  }

int bgav_is_redirector(bgav_t * b)
  {
  const char * klass;
  
  if(b->tt && b->tt->num_tracks &&
     (klass = gavl_dictionary_get_string(b->tt->tracks[0]->metadata, GAVL_META_CLASS)) &&
     !strcmp(klass, GAVL_META_CLASS_LOCATION))
    return 1;
  return 0;
  }

