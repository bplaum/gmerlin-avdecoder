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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glob.h>

#define LOG_DOMAIN "cuedemux"

extern bgav_input_t bgav_input_file;

static int glob_errfunc(const char *epath, int eerrno)
  {
  fprintf(stderr, "glob error: Cannot access %s: %s\n",
          epath, strerror(eerrno));
  return 0;
  }

static int probe_cue(bgav_input_context_t * input)
  {
  const char * loc = NULL;
  uint8_t test_data[4];

  /* Need to call glob() later on */
  if(input->input != &bgav_input_file)
    return 0;
  
  /* Check for .cue or .CUE in the location  */
  if(!gavl_dictionary_get_src(&input->m, GAVL_META_SRC, 0,
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

static int open_cue(bgav_demuxer_context_t * ctx)
  {
  int num, i;
  int ret = 0;
  char * pattern;
  char * pos;
  /* Search for audio file */
  const char * loc = NULL;
  gavl_dictionary_t * edl;
  glob_t glob_buf;

  memset(&glob_buf, 0, sizeof(glob_buf));

  
  gavl_dictionary_get_src(&ctx->input->m, GAVL_META_SRC, 0, NULL, &loc);
  pattern = gavl_strdup(loc);
  pos = strrchr(pattern, '.');
  pos++;
  *pos = '*';
  pos++;
  *pos = '\0';

  pattern = gavl_escape_string(pattern, "[]?");

  if(glob(pattern, 0, glob_errfunc, &glob_buf))
    {
    // fprintf(stderr, "glob returned %d\n", result);
    goto fail;
    }
  
  /* We simply open the audio file, which will load the cue file in turn.
     Then we copy the EDL and forget the rest */

  ctx->tt = bgav_track_table_create(0);
  edl = gavl_edl_create(&ctx->tt->info);
  
  if(!load_edl(edl, &glob_buf, ".wav") &&
     !load_edl(edl, &glob_buf, ".flac") &&
     !load_edl(edl, &glob_buf, ".ape") &&
     !load_edl(edl, &glob_buf, ".wv"))
    goto fail;

  num = gavl_get_num_tracks(edl);
  
  for(i = 0; i < num; i++)
    {
    gavl_dictionary_t * track;
    gavl_dictionary_t * m;
    gavl_dictionary_t * src;
    const gavl_dictionary_t * src_input;
    
    track = gavl_get_track_nc(edl, i);
    m = gavl_track_get_metadata_nc(track);

    gavl_dictionary_set(m, GAVL_META_SRC, NULL);
    
    src = gavl_dictionary_get_dictionary_create(m, GAVL_META_SRC);

    gavl_dictionary_set_string(src, GAVL_META_FORMAT, "CUE");
    gavl_dictionary_set_string(src, GAVL_META_MIMETYPE, "application/x-cue");

    if((src_input = gavl_dictionary_get_dictionary(&ctx->input->m, GAVL_META_SRC)))
      gavl_dictionary_merge2(src, src_input);

    /* Set samplerate and channels */

    gavl_dictionary_set_int(m, GAVL_META_AUDIO_SAMPLERATE, 44100);
    gavl_dictionary_set_int(m, GAVL_META_AUDIO_CHANNELS, 2);

    gavl_dictionary_set_int(m, GAVL_META_SAMPLE_ACCURATE, 1);


    }
  
  //      src = gavl_dictionary_get_dictionary_create(tm, GAVL_META_SRC);
      

     
  ret = 1;
  
  fail:

  if(pattern)
    free(pattern);
  globfree(&glob_buf);
    
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
