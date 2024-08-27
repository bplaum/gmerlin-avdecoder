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
#include <stdio.h>
#include <string.h>
#include <glob.h>

#include <cue.h>

#define LOG_DOMAIN "cuedemux"

extern bgav_input_t bgav_input_file;

#if 0
static int glob_errfunc(const char *epath, int eerrno)
  {
  fprintf(stderr, "glob error: Cannot access %s: %s\n",
          epath, strerror(eerrno));
  return 0;
  }
#endif

static int probe_cue(bgav_input_context_t * input)
  {
  const char * loc = NULL;
  uint8_t test_data[4];

  /* Need to call glob() later on */
  if(input->input != &bgav_input_file)
    return 0;
  
  /* Check for .cue or .CUE in the location  */
  if(!gavl_metadata_get_src(&input->m, GAVL_META_SRC, 0,
                              NULL, &loc) ||
     !loc ||
     (!gavl_string_ends_with(loc, ".cue") &&
      !gavl_string_ends_with(loc, ".CUE")))
    return 0;
  
  if(bgav_input_get_data(input, test_data, 4) < 4)
    return 0;
  if((test_data[0] == 'R') &&
     (test_data[1] == 'E') &&
     (test_data[2] == 'M') &&
     (test_data[3] == ' '))
    return 1;
  return 0;
  }

#if 0
static int load_edl(gavl_dictionary_t * ret, const glob_t * g, const char * ext)
  {
  int result = 0;
  const gavl_dictionary_t * edl;
  bgav_t  * b;
  int i;
  const char * loc = NULL;
  const char * pos;
  
  /* Search for the filename */

  for(i = 0; i < g->gl_pathc; i++)
    {
    pos = strrchr(g->gl_pathv[i], '.');
    if(pos && !strcasecmp(pos, ext))
      {
      loc = g->gl_pathv[i];
      break;
      }
    }
  
  if(!loc)
    return 0;
  
  b = bgav_create();
  if(!bgav_open(b, loc))
    {
    bgav_close(b);
    return 0;
    }
  
  if((edl = bgav_get_edl(b)))
    {
    gavl_dictionary_copy(ret, edl);
    result = 1;
    }
  
  bgav_close(b);
  
  return result;
  }
#endif

static int open_cue(bgav_demuxer_context_t * ctx)
  {
  int ret = 0;
  /* Search for audio file */
  const char * loc = NULL;

  bgav_cue_t * cue = NULL;

  if(!gavl_metadata_get_src(&ctx->input->m, GAVL_META_SRC, 0, NULL, &loc))
    goto fail;
  
  if(!(cue = bgav_cue_read(ctx->input)))
    goto fail;

  ctx->tt = bgav_track_table_create(0);

  bgav_cue_get_edl(cue, &ctx->tt->info, loc);

  ret = 1;
  fail:
  
  bgav_cue_destroy(cue);
  return ret;
  }

static void close_cue(bgav_demuxer_context_t * ctx)
  {

  }

const bgav_demuxer_t bgav_demuxer_cue =
  {
    .probe =       probe_cue,
    .open =        open_cue,
    .close =       close_cue
  };
