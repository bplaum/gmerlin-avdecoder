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

#include <stdlib.h>

#include <avdec_private.h>

// #define DUMP_TABLE

#ifdef DUMP_TABLE
static void bgav_keyframe_table_dump(bgav_keyframe_table_t* tab)
  {
  int i;
  bgav_dprintf("keyframe table:\n");
  for(i = 0; i < tab->num_entries; i++)
    {
    bgav_dprintf("  Pos: %d, pts: %ld\n", tab->entries[i].pos, tab->entries[i].pts);
    }
  }
#endif

static void append_entry(bgav_keyframe_table_t * tab, int * allocated)
  {
  if(tab->num_entries >= *allocated)
    {
    *allocated += 1024;
    tab->entries = realloc(tab->entries, *allocated * sizeof(*tab->entries));
    }
  tab->num_entries++;
  }

bgav_keyframe_table_t * bgav_keyframe_table_create_fi(bgav_file_index_t * fi)
  {
  int allocated = 0;
  int i;
  bgav_keyframe_table_t * ret = calloc(1, sizeof(*ret));

  for(i = 0; i < fi->num_entries; i++)
    {
    if(fi->entries[i].flags & GAVL_PACKET_KEYFRAME)
      {
      append_entry(ret, &allocated);
      ret->entries[ret->num_entries-1].pos = i;
      ret->entries[ret->num_entries-1].pts = fi->entries[i].pts;
      }
    }
#ifdef DUMP_TABLE
  bgav_keyframe_table_dump(ret);
#endif
  return ret;
  }

bgav_keyframe_table_t * bgav_keyframe_table_create_si(bgav_superindex_t * si,
                                                      bgav_stream_t * s)
  {
  int allocated = 0;
  int i;
  bgav_keyframe_table_t * ret = calloc(1, sizeof(*ret));

  for(i = s->first_index_position; i <= s->last_index_position; i++)
    {
    if((si->entries[i].stream_id == s->stream_id) &&
       (si->entries[i].flags & GAVL_PACKET_KEYFRAME))
      {
      append_entry(ret, &allocated);
      ret->entries[ret->num_entries-1].pos = i;
      ret->entries[ret->num_entries-1].pts = si->entries[i].pts;
      }
      
    }
#ifdef DUMP_TABLE
  bgav_keyframe_table_dump(ret);
#endif
  return ret;
  }

void bgav_keyframe_table_destroy(bgav_keyframe_table_t * tab)
  {
  if(tab->entries)
    free(tab->entries);
  free(tab);
  }

/* Returns the index position */
int bgav_keyframe_table_seek(bgav_keyframe_table_t * tab,
                             int64_t  seek_pts,
                             int64_t * kf_pts)
  {
#if 0
  int pos1, pos2, ret, mid;
  pos1 = 0;
  pos2 = tab->num_entries-1;

  if(tab->entries[pos1].pts > seek_pts)
    ret = pos1;
  else if(tab->entries[pos2].pts < seek_pts)
    ret = pos2;
  else
    {
    while(pos2 - pos1 > 1)
      {
      /* Do a fast bisection search */
      mid = (pos1 + pos2) >> 1;

      if(tab->entries[mid].pts < seek_pts)
        pos1 = mid;
      else
        pos2 = mid;
      }
    
    }
    
  return 0;
#else
  int i;
  for(i = tab->num_entries-1; i >= 0; i--)
    {
    if(tab->entries[i].pts <= seek_pts)
      {
      if(kf_pts) *kf_pts = tab->entries[i].pts;
      return tab->entries[i].pos;
      }
    }
  
  if(kf_pts)
    *kf_pts = tab->entries[0].pts;
  return 0;
#endif
  }

