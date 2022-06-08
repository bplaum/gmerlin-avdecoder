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
#include <string.h>

#include <gavl/trackinfo.h>

bgav_track_table_t * bgav_track_table_create(int num_tracks)
  {
  bgav_track_table_t * ret;
  int i;

  gavl_dictionary_t * m;
  
  gavl_dictionary_t track;

  gavl_dictionary_init(&track);

  
  ret = calloc(1, sizeof(*ret));
  
  gavl_dictionary_get_dictionary_create(&ret->info, GAVL_META_METADATA);
  
  m = gavl_dictionary_get_dictionary_create(&track, GAVL_META_METADATA);

  gavl_dictionary_set_int(m, GAVL_META_TOTAL, num_tracks);
  gavl_dictionary_set_string(m, GAVL_META_MEDIA_CLASS, GAVL_META_MEDIA_CLASS_ITEM);
  
  if(num_tracks)
    {
    ret->tracks = calloc(num_tracks, sizeof(*(ret->tracks)));
    /* Set all durations to undefined */
    for(i = 0; i < num_tracks; i++)
      {
      ret->tracks[i] = calloc(1, sizeof(*ret->tracks[i]));
      
      ret->tracks[i]->info = gavl_append_track(&ret->info, &track);
      ret->tracks[i]->metadata = gavl_track_get_metadata_nc(ret->tracks[i]->info);
      
      gavl_dictionary_set_int(ret->tracks[i]->metadata, GAVL_META_IDX, i);
      }
    ret->num_tracks = num_tracks;
    ret->cur = *ret->tracks;
    }

  gavl_dictionary_free(&track);
  
  ret->refcount = 1;
  return ret;
  }

bgav_track_t * bgav_track_table_append_track(bgav_track_table_t * t)
  {
  t->tracks = realloc(t->tracks, sizeof(*t->tracks) * (t->num_tracks+1));
  memset(&t->tracks[t->num_tracks], 0, sizeof(t->tracks[t->num_tracks]));

  t->tracks[t->num_tracks] = calloc(1, sizeof(*t->tracks[t->num_tracks]));
  
  t->tracks[t->num_tracks]->info = gavl_append_track(&t->info, NULL);
  t->tracks[t->num_tracks]->metadata = gavl_track_get_metadata_nc(t->tracks[t->num_tracks]->info);
  
  t->num_tracks++;
  
  return t->tracks[t->num_tracks-1];
  }

void bgav_track_table_remove_track(bgav_track_table_t * t, int idx)
  {
  if((idx < 0) || (idx >= t->num_tracks))
    return;
  bgav_track_free(t->tracks[idx]);
  free(t->tracks[idx]);
  
  memset(&t->tracks[idx], 0, sizeof(t->tracks[idx]));
  if(idx < t->num_tracks - 1)
    {
    memmove(t->tracks + idx, t->tracks + idx + 1,
            (t->num_tracks - 1 - idx) * sizeof(*t->tracks));
    }
  memset(&t->tracks[t->num_tracks - 1], 0, 
         sizeof(t->tracks[t->num_tracks - 1]));
  gavl_delete_track(&t->info, idx);
  t->num_tracks--;
  }

void bgav_track_table_ref(bgav_track_table_t * t)
  {
  t->refcount++;
  }
     
void bgav_track_table_unref(bgav_track_table_t * t)
  {
  int i;
  t->refcount--;
  if(t->refcount)
    return;
  for(i = 0; i < t->num_tracks; i++)
    {
    bgav_track_free(t->tracks[i]);
    free(t->tracks[i]);
    }
  free(t->tracks);
  gavl_dictionary_free(&t->info);
  free(t);
  }

void bgav_track_table_create_message_streams(bgav_track_table_t * t, const bgav_options_t * opt)
  {
  int i;
  for(i = 0; i < t->num_tracks; i++)
    bgav_track_add_msg_stream(t->tracks[i], opt, GAVL_META_STREAM_ID_MSG_PROGRAM);
  }

void bgav_track_table_select_track(bgav_track_table_t * t, int track)
  {
  /* Clear old track */
  if(t->cur)
    {
    bgav_track_stop(t->cur);
    bgav_track_clear(t->cur);
    }
  
  t->cur = t->tracks[track];
  bgav_track_mute(t->cur);
  bgav_track_clear(t->cur);
  }

void bgav_track_table_dump(bgav_track_table_t * t)
  {
  }

void bgav_track_table_merge_metadata(bgav_track_table_t*t,
                                     bgav_metadata_t * m)
  {
  int i;
  //  gavl_dictionary_t * edl;

  gavl_dictionary_t * src_t;
  const gavl_dictionary_t * src_ext;
  
  for(i = 0; i < t->num_tracks; i++)
    {
    gavl_dictionary_merge2(t->tracks[i]->metadata, m);

    src_ext = gavl_dictionary_get_src(m, GAVL_META_SRC, 0, NULL, NULL);
    src_t   = gavl_dictionary_get_src_nc(t->tracks[i]->metadata, GAVL_META_SRC, 0);
    
    if(src_ext && src_t)
      gavl_dictionary_merge2(src_t, src_ext);
#if 0
    fprintf(stderr, "bgav_track_table_merge_metadata\n");
    fprintf(stderr, "t->tracks[i]->metadata\n");
    gavl_dictionary_dump(t->tracks[i]->metadata, 2);

    fprintf(stderr, "m\n");
    gavl_dictionary_dump(m, 2);
#endif
    }
  
  }

void bgav_track_table_remove_unsupported(bgav_track_table_t * t)
  {
  int i;
  for(i = 0; i < t->num_tracks; i++)
    bgav_track_remove_unsupported(t->tracks[i]);
  }

void bgav_track_table_compute_info(bgav_track_table_t * t)
  {
  int i;
  gavl_dictionary_t * edl;

  for(i = 0; i < t->num_tracks; i++)
    bgav_track_compute_info(t->tracks[i]);

  if((edl = gavl_dictionary_get_dictionary_nc(&t->info, GAVL_META_EDL)))
    gavl_edl_finalize(edl);
  }
