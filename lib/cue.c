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
#include <cue.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <gavl/trackinfo.h>
#include <gavl/metatags.h>


#define SAMPLES_PER_FRAME 588

#define MY_FREE(ptr) \
  if(ptr) free(ptr);


typedef struct
  {
  uint32_t frame;
  int number;
  } cue_index_t;


typedef struct
  {
  int number;
  char * mode;

  char * performer;
  char * songwriter;
  char * title;
  
  int num_indices;
  cue_index_t * indices;
  } cue_track_t;

struct bgav_cue_s
  {
  char * performer;
  char * songwriter;
  char * title;
  
  char ** comments;
  int num_comments;
  
  cue_track_t * tracks;
  int num_tracks;

  };

static bgav_input_context_t *
get_cue_file(bgav_input_context_t * audio_file)
  {
  char * pos;
  char * tmp_string;
  char * filename;
  bgav_input_context_t * ret;
  
  /* We take local files only */
  if(strcmp(audio_file->input->name, "file"))
    return NULL;
  
  tmp_string = gavl_strdup(audio_file->filename);
  
  pos = strrchr(tmp_string, '.');
  if(!pos)
    {
    free(tmp_string);
    return NULL;
    }
  *pos = '\0';
  
  filename = bgav_sprintf("%s.%s", tmp_string, "cue");
  free(tmp_string);
  
  if(access(filename, R_OK))
    {
    free(filename);
    return NULL;
    }
  ret = bgav_input_create(audio_file->b, NULL);

  /* Adding file:// prevents the .cue files being
     opened with the vcd module */
  tmp_string = bgav_sprintf("file://%s", filename);
  
  if(!bgav_input_open(ret, tmp_string))
    {
    bgav_input_destroy(ret);
    free(tmp_string);
    free(filename);
    return NULL;
    }
  free(tmp_string);
  free(filename);
  return ret;
  }

static const char * skip_space(const char * ptr)
  {
  while(isspace(*ptr) && (*ptr != '\0'))
    ptr++;
  if(*ptr == '\0')
    return NULL;
  return ptr;
  }

static char * get_string(const char * pos)
  {
  const char * end;
  pos = skip_space(pos);
  
  if(!pos)
    return NULL;
  if(*pos == '"')
    {
    pos++;
    end = strrchr(pos, '"');
    if(end - pos > 1)
      return gavl_strndup(pos, end);
    else
      return NULL;
    }
  else
    {
    return gavl_strdup(pos);
    }
  }

static uint32_t get_frame(const char * pos)
  {
  int minutes, seconds, frames;
  if(sscanf(pos, "%d:%d:%d", &minutes, &seconds, &frames) < 3)
    return 0;
  return frames + 75 * (seconds + 60 * minutes);
  }

bgav_cue_t *
bgav_cue_read(bgav_input_context_t * audio_file)
  {
  bgav_cue_t * ret = NULL;
  char * line = NULL;
  uint32_t line_alloc = 0;
  const char * pos;
  char * rest;
  cue_track_t * cur = NULL;
  
  bgav_input_context_t * ctx = get_cue_file(audio_file);

  if(!ctx)
    goto fail;
  
  ret = calloc(1, sizeof(*ret));

  while(1)
    {
    if(!bgav_input_read_line(ctx, &line, &line_alloc, 0, NULL))
      break;
    pos = skip_space(line);

    if(!pos)
      continue;
    
    /* Track */
    if(!strncasecmp(pos, "TRACK ", 6))
      {
      pos += 6;
      ret->tracks = realloc(ret->tracks,
                            (ret->num_tracks+1) *
                            sizeof(*ret->tracks));
      cur = ret->tracks + ret->num_tracks;
      ret->num_tracks++;
      memset(cur, 0, sizeof(*cur));

      
      /* Number */
      cur->number = strtol(pos, &rest, 10);
      pos = rest;
      pos = skip_space(pos);

      /* Mode */
      if(pos)
        cur->mode = gavl_strdup(pos);
      }
    /* Comment */
    else if(!strncasecmp(pos, "REM ", 4))
      {
      pos += 4;
      pos = skip_space(pos);

      if(!cur && pos)
        {
        ret->comments =
          realloc(ret->comments,
                  (ret->num_comments+1) *
                  sizeof(*ret->comments));
        ret->comments[ret->num_comments] =
          gavl_strdup(pos);
        ret->num_comments++;
        }
      }
    else if(!strncasecmp(pos, "PERFORMER ", 10))
      {
      pos += 10;

      if(cur)
        cur->performer = get_string(pos);
      else
        ret->performer = get_string(pos);
        
      }
    else if(!strncasecmp(pos, "TITLE ", 6))
      {
      pos += 6;
      if(cur)
        cur->title = get_string(pos);
      else
        ret->title = get_string(pos);
      
      }
    else if(!strncasecmp(pos, "SONGWRITER ", 11))
      {
      pos += 11;
      if(cur)
        cur->songwriter = get_string(pos);
      else
        ret->songwriter = get_string(pos);
      }
    else if(!strncasecmp(pos, "INDEX ", 6))
      {
      pos += 6;
      if(cur)
        {
        cur->indices = realloc(cur->indices,
                               (cur->num_indices+1) *
                               sizeof(*cur->indices));
        
        cur->indices[cur->num_indices].number =
          strtol(pos, &rest, 10);
        pos = rest;
        pos = skip_space(pos);

        if(!pos)
          cur->indices[cur->num_indices].frame = 0;
        else
          cur->indices[cur->num_indices].frame = get_frame(pos);
        cur->num_indices++;
        }
      }
    }

  bgav_input_close(ctx);
  bgav_input_destroy(ctx);
  MY_FREE(line);
  
  return ret;

  fail:
  if(ret)
    bgav_cue_destroy(ret);
  if(ctx)
    {
    bgav_input_close(ctx);
    bgav_input_destroy(ctx);
    }
  MY_FREE(line);
  return NULL;
  
  }

gavl_dictionary_t * bgav_cue_get_edl(bgav_cue_t * cue,
                                     int64_t total_samples,
                                     gavl_dictionary_t * parent)
  {
  int i, j;
  gavl_dictionary_t * stream;
  gavl_dictionary_t * track;
  gavl_dictionary_t * seg;
  gavl_dictionary_t * last_seg = NULL;
  gavl_dictionary_t * mp;

  const char * pos;
  gavl_dictionary_t m;
  gavl_dictionary_t * ret = gavl_edl_create(parent);
  
  int64_t last_time = GAVL_TIME_UNDEFINED;
  int64_t seg_time  = GAVL_TIME_UNDEFINED;
  
  
  /* Create common metadata entries */
  memset(&m , 0, sizeof(m));
  if(cue->title)
    gavl_dictionary_set_string(&m, GAVL_META_ALBUM, cue->title);
  if(cue->performer)
    gavl_dictionary_set_string(&m, GAVL_META_ALBUMARTIST, cue->performer);
  
  for(i = 0; i < cue->num_comments; i++)
    {
    pos = cue->comments[i];
    if(!strncasecmp(pos, "GENRE ", 6))
      {
      pos += 6;
      pos = skip_space(pos);
      if(pos)
        gavl_dictionary_set_string_nocopy(&m, GAVL_META_GENRE, get_string(pos));
      }
    if(!strncasecmp(pos, "DATE ", 5))
      {
      pos += 5;
      pos = skip_space(pos);
      if(pos)
        gavl_dictionary_set_string_nocopy(&m, GAVL_META_YEAR, get_string(pos));
      }
    if(!strncasecmp(pos, "COMMENT ", 8))
      {
      pos += 8;
      pos = skip_space(pos);
      if(pos)
        gavl_dictionary_set_string_nocopy(&m, GAVL_META_COMMENT, get_string(pos));
      }
    }

  /* Loop through tracks */
  for(i = 0; i < cue->num_tracks; i++)
    {
    if(!strcmp(cue->tracks[i].mode, "AUDIO"))
      {
      gavl_dictionary_t * tm;
      track = gavl_append_track(ret, NULL);
      
      tm = gavl_track_get_metadata_nc(track);
      gavl_dictionary_copy(tm, &m);
      
      if(cue->tracks[i].performer)
        gavl_dictionary_set_string(tm, GAVL_META_ARTIST,
                                   cue->tracks[i].performer);
      if(cue->tracks[i].title)
        gavl_dictionary_set_string(tm, GAVL_META_TITLE,
                                   cue->tracks[i].title);

      gavl_dictionary_set_int(tm, GAVL_META_TRACKNUMBER,
                              cue->tracks[i].number);

      if(cue->tracks[i].performer &&
         cue->tracks[i].title)
        {
        gavl_dictionary_set_string_nocopy(tm, GAVL_META_LABEL,
                                          bgav_sprintf("%02d %s - %s",
                                                       cue->tracks[i].number,
                                                       cue->tracks[i].performer,
                                                       cue->tracks[i].title));
        }
      else if(cue->tracks[i].title)
        {
        gavl_dictionary_set_string_nocopy(tm, GAVL_META_LABEL,
                                          bgav_sprintf("%02d %s",
                                                       cue->tracks[i].number,
                                                       cue->tracks[i].title));
        }
      else
        {
        gavl_dictionary_set_string_nocopy(tm, GAVL_META_LABEL,
                                          bgav_sprintf("Track %02d",
                                                       cue->tracks[i].number));
        }
      
      stream = gavl_track_append_audio_stream(track);

      mp = gavl_stream_get_metadata_nc(stream);
      gavl_dictionary_set_int(mp, GAVL_META_STREAM_SAMPLE_TIMESCALE, 44100);
      
      seg = gavl_edl_add_segment(stream);
      
      for(j = 0; j < cue->tracks[i].num_indices; j++)
        {
        if(j == 0)
          {
          /* End time of previous track */
          if(last_seg)
            {
            gavl_edl_segment_set(last_seg,
                                 0, 0, 44100,
                                 last_time,
                                 0,
                                 cue->tracks[i].indices[j].frame * SAMPLES_PER_FRAME - last_time);
            
            last_time = GAVL_TIME_UNDEFINED;
            last_seg = NULL;
            }
          }
        if((cue->tracks[i].indices[j].number == 1) ||
           (cue->tracks[i].num_indices == 1))
          {
          /* Start time of this track */
          seg_time = cue->tracks[i].indices[j].frame * SAMPLES_PER_FRAME;
          }
        }

      } // Audio
    else
      track = NULL;
    
    if(seg)
      {
      last_seg = seg;
      last_time = seg_time;
      seg = NULL;
      }

    //    if(track)
    //      gavl_track_finalize(track);
    
    } // End of tracks loop

  /* Get duration of the last track */
  if(last_seg)
    {
    gavl_edl_segment_set(last_seg,
                         0, 0, 44100,
                         last_time,
                         0,
                         total_samples - last_time);
    }
  
  gavl_dictionary_free(&m);

  gavl_edl_finalize(ret);
  
  return ret;
  }

void bgav_demuxer_init_cue(bgav_demuxer_context_t * ctx)
  {
  bgav_cue_t * cue = bgav_cue_read(ctx->input);
  if(cue)
    {
    gavl_dictionary_t * edl;
    bgav_stream_t * s;

    if(!(s = bgav_track_get_audio_stream(ctx->tt->cur, 0)))
      return;
    
    edl = bgav_cue_get_edl(cue, bgav_stream_get_duration(s), &ctx->tt->info);
    
    bgav_cue_destroy(cue);
    gavl_dictionary_set_string(edl, GAVL_META_URI, ctx->input->filename);
    }
  }


void bgav_cue_destroy(bgav_cue_t * cue)
  {
  int i;
  MY_FREE(cue->performer);
  MY_FREE(cue->songwriter);
  MY_FREE(cue->title);

  for(i = 0; i < cue->num_comments; i++)
    free(cue->comments[i]);
  MY_FREE(cue->comments);
  
  for(i = 0; i < cue->num_tracks; i++)
    {
    MY_FREE(cue->tracks[i].performer);
    MY_FREE(cue->tracks[i].songwriter);
    MY_FREE(cue->tracks[i].mode);
    MY_FREE(cue->tracks[i].title);
    MY_FREE(cue->tracks[i].indices);
    }
  MY_FREE(cue->tracks);
  free(cue);
  }

#if 0 
void bgav_cue_dump(bgav_cue_t * cue)
  {
  
  }
#endif
