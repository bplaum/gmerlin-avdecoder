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
#include <string.h>

#include <avdec_private.h>


bgav_timecode_table_t *
bgav_timecode_table_create(int num)
  {
  bgav_timecode_table_t * ret;
  ret = calloc(1, sizeof(*ret));

  if(num)
    {
    ret->entries = calloc(num, sizeof(*ret->entries));
    ret->num_entries = num;
    ret->entries_alloc = num;
    }
  return ret;
  }

void
bgav_timecode_table_append_entry(bgav_timecode_table_t * tt,
                                 int64_t pts,
                                 gavl_timecode_t timecode)
  {
  if(tt->num_entries +1 > tt->entries_alloc)
    {
    tt->entries_alloc += 128;
    tt->entries = realloc(tt->entries,
                          tt->entries_alloc * sizeof(*tt->entries));
    memset(tt->entries + tt->num_entries, 0,
           (tt->entries_alloc - tt->num_entries) * sizeof(*tt->entries));
    }
  tt->entries[tt->num_entries].pts = pts;
  tt->entries[tt->num_entries].timecode = timecode;
  tt->num_entries++;
  }


void
bgav_timecode_table_destroy(bgav_timecode_table_t * table)
  {
  if(table->entries)
    free(table->entries);
  free(table);
  }

static int compare_func(const void * arg1_1, const void * arg2_1)
  {
  const bgav_timecode_table_entry_t * arg1;
  const bgav_timecode_table_entry_t * arg2;

  arg1 = arg1_1;
  arg2 = arg2_1;

  if(arg1->pts < arg2->pts)
    return -1;
  else if(arg1->pts == arg2->pts)
    return 0;
  else
    return 1;
  }

gavl_timecode_t
bgav_timecode_table_get_timecode(bgav_timecode_table_t * table,
                                 int64_t pts)
  {
  bgav_timecode_table_entry_t se;
  bgav_timecode_table_entry_t * e;

  /* Do a binary search in the table */
  se.pts = pts;

  e =
    (bgav_timecode_table_entry_t *)bsearch(&se, table->entries,
                                           table->num_entries,
                                           sizeof(*table->entries),
                                           compare_func);
  if(e)
    return e->timecode;
  else
    return GAVL_TIMECODE_UNDEFINED;
  }
