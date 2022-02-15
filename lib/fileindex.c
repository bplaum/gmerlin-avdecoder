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

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <avdec_private.h>
#include <md5.h>
#include <limits.h>

#include <dirent.h>
#include <ctype.h>

#define LOG_DOMAIN "fileindex"

#define INDEX_SIGNATURE "BGAVINDEX"

/* Version must be increased each time the fileformat
   changes */
#define INDEX_VERSION 10

static void dump_index(bgav_stream_t * s)
  {
  int i;
  gavl_timecode_t tc;
  
  if(s->type == GAVL_STREAM_VIDEO)
    {
    for(i = 0; i < s->file_index->num_entries; i++)
      {
      bgav_dprintf("      K: %d, P: %"PRId64", T: %"PRId64" CT: %s ",
                   !!(s->file_index->entries[i].flags & GAVL_PACKET_KEYFRAME),
                   s->file_index->entries[i].position,
                   s->file_index->entries[i].pts,
                   bgav_coding_type_to_string(s->file_index->entries[i].flags));
      
      if(i < s->file_index->num_entries-1)
        bgav_dprintf("posdiff: %"PRId64,
                     s->file_index->entries[i+1].position-s->file_index->entries[i].position
                     );
      
      tc = bgav_timecode_table_get_timecode(&s->file_index->tt,
                                            s->file_index->entries[i].pts);

      if(tc != GAVL_TIME_UNDEFINED)
        {
        int year, month, day, hours, minutes, seconds, frames;

        gavl_timecode_to_ymd(tc, &year, &month, &day);
        gavl_timecode_to_hmsf(tc, &hours,
                              &minutes, &seconds, &frames);

        bgav_dprintf(" tc: ");
        if(month && day)
          bgav_dprintf("%04d-%02d-%02d ", year, month, day);
        
        bgav_dprintf("%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
        }
      
      bgav_dprintf("\n");
      }
    }
  else
    {
    for(i = 0; i < s->file_index->num_entries; i++)
      {
      bgav_dprintf("      K: %d, P: %"PRId64", T: %"PRId64,
                   !!(s->file_index->entries[i].flags & GAVL_PACKET_KEYFRAME),
                   s->file_index->entries[i].position,
                   s->file_index->entries[i].pts);
      
      if(i < s->file_index->num_entries-1)
        bgav_dprintf(" D: %"PRId64" posdiff: %"PRId64"\n",
                     s->file_index->entries[i+1].pts-s->file_index->entries[i].pts,
                     s->file_index->entries[i+1].position-s->file_index->entries[i].position
                     );
      else
        bgav_dprintf(" D: %"PRId64"\n", s->stats.pts_end - s->file_index->entries[i].pts);
      }
    }
  }

void bgav_file_index_dump(bgav_t * b)
  {
  int i, j;
  bgav_stream_t * s;
  if(!(b->tt->tracks[0]->flags & TRACK_HAS_FILE_INDEX))
    {
    bgav_dprintf("No index available\n");
    return;
    }

  bgav_dprintf("Generated index table(s)\n");
  for(i = 0; i < b->tt->num_tracks; i++)
    {
    bgav_dprintf(" Track %d\n", i+1);
    
    for(j = 0; j < b->tt->tracks[i]->num_audio_streams; j++)
      {
      s = bgav_track_get_audio_stream(b->tt->tracks[i], j);
      if(!s->file_index)
        continue;
      bgav_dprintf("   Audio stream %d [ID: %08x, Timescale: %d, PTS offset: %"PRId64"]\n", j+1,
                   s->stream_id, s->data.audio.format->samplerate,
                   s->stats.pts_start);
      bgav_dprintf("   Maximum packet size: %d\n",
                   s->ci->max_packet_size);
      bgav_dprintf("   Duration: %"PRId64", entries: %d\n",
                   bgav_stream_get_duration(s),
                   s->file_index->num_entries);
      
      dump_index(s);
      }
    for(j = 0; j < b->tt->tracks[i]->num_video_streams; j++)
      {
      s = bgav_track_get_video_stream(b->tt->tracks[i], j);
      if(!s->file_index)
        continue;
      bgav_dprintf("   Video stream %d [ID: %08x, Timescale: %d, PTS offset: %"PRId64"]\n", j+1,
                   s->stream_id, s->data.video.format->timescale,
                   s->stats.pts_start);
      bgav_dprintf("   Maximum packet size: %d\n",
                   s->ci->max_packet_size);
      bgav_dprintf("   Interlace mode: %s\n",
                   gavl_interlace_mode_to_string(s->data.video.format->interlace_mode));
      bgav_dprintf("   Framerate mode: %s\n",
                   gavl_framerate_mode_to_string(s->data.video.format->framerate_mode));
      if(s->data.video.format->framerate_mode == GAVL_FRAMERATE_CONSTANT)
        bgav_dprintf("   Frame Duration: %d\n", s->data.video.format->frame_duration);
      
      bgav_dprintf("   Duration: %"PRId64", entries: %d\n",
                   bgav_stream_get_duration(s),
                   s->file_index->num_entries);
      dump_index(s);
      }
    for(j = 0; j < b->tt->tracks[i]->num_text_streams; j++)
      {
      s = bgav_track_get_text_stream(b->tt->tracks[i], j);
      if(!s->file_index)
        continue;
      bgav_dprintf("   Text stream %d [ID: %08x, Timescale: %d, PTS offset: %"PRId64"]\n", j+1,
                   s->stream_id, s->timescale,
                   s->stats.pts_start);
      bgav_dprintf("   Maximum packet size: %d\n",
                   s->ci->max_packet_size);
      bgav_dprintf("   Duration: %"PRId64"\n", bgav_stream_get_duration(s));
      dump_index(s);
      }
    for(j = 0; j < b->tt->tracks[i]->num_overlay_streams; j++)
      {
      s = bgav_track_get_overlay_stream(b->tt->tracks[i], j);
      if(!s->file_index)
        continue;
      bgav_dprintf("   Overlay stream %d [ID: %08x, Timescale: %d, PTS offset: %"PRId64"]\n", j+1,
                   s->stream_id, s->timescale,
                   s->stats.pts_start);
      bgav_dprintf("   Maximum packet size: %d\n",
                   s->ci->max_packet_size);
      bgav_dprintf("   Duration: %"PRId64"\n", bgav_stream_get_duration(s));
      dump_index(s);
      }
    }
  }

bgav_file_index_t * bgav_file_index_create()
  {
  bgav_file_index_t * ret = calloc(1, sizeof(*ret));
  return ret;
  }

void bgav_file_index_destroy(bgav_file_index_t * idx)
  {
  if(idx->entries)
    free(idx->entries);
  free(idx);
  }

void
bgav_file_index_append_packet(bgav_file_index_t * idx,
                              int64_t position,
                              int64_t time,
                              int flags, gavl_timecode_t tc)
  {
  if(idx->num_entries >= idx->entries_alloc)
    {
    idx->entries_alloc += 512;
    idx->entries = realloc(idx->entries,
                           idx->entries_alloc * sizeof(*idx->entries));
    }
  /* First frame is always a keyframe */
  if(!idx->num_entries)
    flags |= GAVL_PACKET_KEYFRAME;
    
  idx->entries[idx->num_entries].position = position;
  idx->entries[idx->num_entries].pts     = time;
  idx->entries[idx->num_entries].flags    = flags;
  idx->num_entries++;

  /* Timecode */
  if(tc != GAVL_TIMECODE_UNDEFINED)
    {
    bgav_timecode_table_append_entry(&idx->tt,
                                     time, tc);
    }
  }

/*
 * File I/O.
 *
 * Format:
 *
 * All multibyte numbers are big endian
 * (network byte order)
 *
 * - Signature "BGAVINDEX <version>\n"
 *    (Version is the INDEX_VERSION defined above)
 * - Filename terminated with \n
 * - File time (st_mtime returned by stat(2)) (64)
 * - Number of tracks (32)
 * - Tracks consising of
 *    - Number of streams (32)
 *    - Stream entries consisting of
 *        - Stream ID     (32)
 *        - StreamType    (32)
 *        - Fourcc        (32)
 *        - MaxPacketSize (32)
 *        - Timescale     (32)
 *        if(StreamType == GAVL_STREAM_VIDEO)
 *          - InterlaceMode (32)
 *          - FramerateMode (32)
 *          if(FramerateMode == GAVL_FRAMERATE_CONSTANT)
 *            - FrameDuration (32)
 *        - Timestamp offset (64)
 *        - Duration (64)
 *        - Number of entries (32)
 *          - Index entries consiting of
 *            - packet flags (32)
 *            - position (64)
 *            - time (64)
 *        if(StreamType == GAVL_STREAM_VIDEO)
 *          - number of timecodes (32)
 *          - Timecodes consisting of
 *            - pts (64)
 *            - timecode (64)
 *        
 */


int bgav_file_index_read_header(const char * filename,
                                bgav_input_context_t * input,
                                int * num_tracks)
  {
  int ret = 0;
  uint64_t file_time;
  char * line = NULL;
  uint32_t line_alloc = 0;
  uint32_t ntracks;
  struct stat stat_buf;
  int sig_len;
  sig_len = strlen(INDEX_SIGNATURE);
  
  if(!bgav_input_read_line(input, &line, &line_alloc, 0, NULL))
    goto fail;
  /* Check signature */
  if(strncmp(line, INDEX_SIGNATURE, sig_len))
    goto fail;
  /* Check version */
  if((strlen(line) < sig_len + 2) || !isdigit(*(line + sig_len + 1)))
    goto fail;
  if(atoi(line + sig_len + 1) != INDEX_VERSION)
    goto fail;
  
  if(!bgav_input_read_line(input, &line, &line_alloc, 0, NULL))
    goto fail;
  /* Check filename */
  if(strcmp(line, filename))
    goto fail;
  if(!bgav_input_read_64_be(input, &file_time))
    goto fail;
  
  /* Don't do this check if we have uuid's as names */
  
  if(filename[0] == '/')
    {
    if(stat(filename, &stat_buf))
      goto fail;
    if(file_time != stat_buf.st_mtime)
      goto fail;
    }
  if(!bgav_input_read_32_be(input, &ntracks))
    goto fail;
  
  *num_tracks = ntracks;
  ret = 1;
  fail:
  if(line)
    free(line);
  return ret;
  }

static void write_64(FILE * out, uint64_t i)
  {
  uint8_t buf[8];
  GAVL_64BE_2_PTR(i, buf);
  fwrite(buf, 8, 1, out);
  }

static void write_32(FILE * out, uint32_t i)
  {
  uint8_t buf[4];
  GAVL_32BE_2_PTR(i, buf);
  fwrite(buf, 4, 1, out);
  }
#if 0
static void write_8(FILE * out, uint8_t i)
  {
  fwrite(&i, 1, 1, out);
  }
#endif
void bgav_file_index_write_header(const char * filename,
                                  FILE * output,
                                  int num_tracks)
  {
  uint64_t file_time = 0;
  struct stat stat_buf;

  fprintf(output, "%s %d\n", INDEX_SIGNATURE, INDEX_VERSION);
  fprintf(output, "%s\n", filename);

  if(filename[0] == '/')
    {
    if(stat(filename, &stat_buf))
      return;
    file_time = stat_buf.st_mtime;
    }
  write_64(output, file_time);
  write_32(output, num_tracks);
  }

static bgav_file_index_t *
file_index_read_stream(bgav_input_context_t * input, bgav_stream_t * s)
  {
  int i;
  uint32_t tmp_32;
  bgav_file_index_t * ret = calloc(1, sizeof(*ret));

  switch(s->type)
    {
    case GAVL_STREAM_AUDIO:
      if(!bgav_input_read_32_be(input, &tmp_32))
        return NULL;
      s->data.audio.format->samplerate = tmp_32;
      break;
    case GAVL_STREAM_VIDEO:
      if(!bgav_input_read_32_be(input, &tmp_32))
        return NULL;
      s->data.video.format->timescale = tmp_32;
      
      if(!bgav_input_read_32_be(input, &tmp_32))
        return NULL;
      s->data.video.format->interlace_mode = tmp_32;
      
      if(!bgav_input_read_32_be(input, &tmp_32))
        return NULL;
      s->data.video.format->framerate_mode = tmp_32;

      if(s->data.video.format->framerate_mode == GAVL_FRAMERATE_CONSTANT)
        {
        if(!bgav_input_read_32_be(input, &tmp_32))
          return NULL;
        s->data.video.format->frame_duration = tmp_32;
        }
      
      break;
    case GAVL_STREAM_TEXT:
    case GAVL_STREAM_OVERLAY:
    case GAVL_STREAM_NONE:
      if(!bgav_input_read_32_be(input, (uint32_t*)&s->timescale))
        return NULL;
      break;
    case GAVL_STREAM_MSG:
      break;
    }
  
#if 0
  write_32(output, s->stats.size_min);
  write_32(output, s->stats.size_max);
  
  write_64(output, s->stats.duration_min);
  write_64(output, s->stats.duration_max);
  
  write_64(output, s->stats.pts_start);
  write_64(output, s->stats.pts_end);

  write_64(output, s->stats.total_bytes);
  write_64(output, s->stats.total_packets);
#endif

  if(!bgav_input_read_32_be(input, (uint32_t*)&s->stats.size_min))
    return NULL;
  if(!bgav_input_read_32_be(input, (uint32_t*)&s->stats.size_max))
    return NULL;

  if(!bgav_input_read_64_be(input, (uint64_t*)&s->stats.duration_min))
    return NULL;
  if(!bgav_input_read_64_be(input, (uint64_t*)&s->stats.duration_max))
    return NULL;
  
  if(!bgav_input_read_64_be(input, (uint64_t*)&s->stats.pts_start))
    return NULL;
  if(!bgav_input_read_64_be(input, (uint64_t*)&s->stats.pts_end))
    return NULL;

  if(!bgav_input_read_64_be(input, (uint64_t*)&s->stats.total_bytes))
    return NULL;
  if(!bgav_input_read_64_be(input, (uint64_t*)&s->stats.total_packets))
    return NULL;
  
  if(!bgav_input_read_32_be(input, &ret->num_entries))
    return NULL;
  
  ret->entries = calloc(ret->num_entries, sizeof(*ret->entries));

  for(i = 0; i < ret->num_entries; i++)
    {
    if(!bgav_input_read_32_be(input, &ret->entries[i].flags) ||
       !bgav_input_read_64_be(input, &ret->entries[i].position) ||
       !bgav_input_read_64_be(input, (uint64_t*)(&ret->entries[i].pts)))
      return NULL;
    }

  if(s->type == GAVL_STREAM_VIDEO)
    {
    if(!bgav_input_read_32_be(input, (uint32_t*)&ret->tt.num_entries))
      return NULL;

    if(ret->tt.num_entries)
      {
      ret->tt.entries_alloc = ret->tt.num_entries;
      ret->tt.entries = calloc(ret->tt.num_entries, sizeof(*ret->tt.entries));

      for(i = 0; i < ret->tt.num_entries; i++)
        {
        if(!bgav_input_read_64_be(input, (uint64_t*)&ret->tt.entries[i].pts) ||
           !bgav_input_read_64_be(input, &ret->tt.entries[i].timecode))
          return NULL;
        }
      }
    
    }
  
  return ret;
  }

static void
file_index_write_stream(FILE * output,
                        bgav_file_index_t * idx, bgav_stream_t * s)
  {
  int i;
  
  write_32(output, s->stream_id);
  write_32(output, s->type);
  write_32(output, s->fourcc);
  write_32(output, s->ci->max_packet_size);
  
  switch(s->type)
    {
    case GAVL_STREAM_AUDIO:
      write_32(output, s->data.audio.format->samplerate);
      break;
    case GAVL_STREAM_VIDEO:
      write_32(output, s->data.video.format->timescale);
      write_32(output, s->data.video.format->interlace_mode);
      write_32(output, s->data.video.format->framerate_mode);
      if(s->data.video.format->framerate_mode == GAVL_FRAMERATE_CONSTANT)
        write_32(output, s->data.video.format->frame_duration);
      break;
    case GAVL_STREAM_TEXT:
    case GAVL_STREAM_OVERLAY:
    case GAVL_STREAM_NONE:
      write_32(output, s->timescale);
      break;
    case GAVL_STREAM_MSG:
      break;
    }

  write_32(output, s->stats.size_min);
  write_32(output, s->stats.size_max);
  
  write_64(output, s->stats.duration_min);
  write_64(output, s->stats.duration_max);
  
  write_64(output, s->stats.pts_start);
  write_64(output, s->stats.pts_end);

  write_64(output, s->stats.total_bytes);
  write_64(output, s->stats.total_packets);
  
  write_32(output, idx->num_entries);

  for(i = 0; i < idx->num_entries; i++)
    {
    write_32(output, idx->entries[i].flags);
    write_64(output, idx->entries[i].position);
    write_64(output, idx->entries[i].pts);
    }

  if(s->type == GAVL_STREAM_VIDEO)
    {
    write_32(output, idx->tt.num_entries);
    
    for(i = 0; i < idx->tt.num_entries; i++)
      {
      write_64(output, idx->tt.entries[i].pts);
      write_64(output, idx->tt.entries[i].timecode);
      }
    }
  }

static void set_has_file_index_s(bgav_stream_t * s,
                                 bgav_demuxer_context_t * demuxer)
  {
  if(!s->file_index || demuxer->si)
    return;
  s->first_index_position = 0;
  s->last_index_position = s->file_index->num_entries-1;

  
  
  }

static void set_has_file_index(bgav_t * b)
  {
  int i, j;
  bgav_stream_t * s;
  
  for(i = 0; i < b->tt->num_tracks; i++)
    {
    b->tt->tracks[i]->flags |= (TRACK_HAS_FILE_INDEX|TRACK_SAMPLE_ACCURATE);
    
    //    if(b->tt->tracks[i].duration == GAVL_TIME_UNDEFINED)
    //      {
    
    for(j= 0; j <b->tt->tracks[i]->num_streams; j++)
      {
      s = &b->tt->tracks[i]->streams[j];

      if(!(s->flags & STREAM_EXTERN))
        set_has_file_index_s(s, b->demuxer);
      }
    }
  b->demuxer->flags |= BGAV_DEMUXER_CAN_SEEK;
  }

int bgav_read_file_index(bgav_t * b)
  {
  int i, j;
  bgav_input_context_t * input = NULL;
  int num_tracks;
  uint32_t num_streams;
  uint32_t stream_id;
  uint32_t stream_type;
  char * filename;
  bgav_stream_t * s;
  uint32_t u_tmp;
  
  /* Check if we already have a file index */

  if(!b->tt->tracks || (b->tt->tracks[0]->flags & TRACK_HAS_FILE_INDEX))
    return 1;
  
  /* Check if the input provided an index filename */
  if(!b->input->index_file || !b->input->filename)
    return 0;

  filename =
    bgav_search_file_read(&b->opt,
                          "indices", b->input->index_file);
  if(!filename)
    goto fail;
  
  input = bgav_input_create(b, NULL);
  if(!bgav_input_open(input, filename))
    goto fail;

  if(!bgav_file_index_read_header(b->input->filename,
                                  input, &num_tracks))
    goto fail;

  if(num_tracks != b->tt->num_tracks)
    goto fail;

  for(i = 0; i < num_tracks; i++)
    {
    if(!bgav_input_read_32_be(input, &num_streams))
      goto fail;
    
    for(j = 0; j < num_streams; j++)
      {
      if(!bgav_input_read_32_be(input, &stream_id))
        goto fail;
      s = bgav_track_find_stream_all(b->tt->tracks[i], stream_id);
      if(!s)
        {
        /* Create stream */
        if(!bgav_input_read_32_be(input, &stream_type))
          goto fail;
        
        switch(stream_type)
          {
          case GAVL_STREAM_AUDIO:
            s = bgav_track_add_audio_stream(b->tt->tracks[i], &b->opt);
            break;
          case GAVL_STREAM_VIDEO:
            s = bgav_track_add_video_stream(b->tt->tracks[i], &b->opt);
            break;
            /* Passing NULL as encoding might break when we have MPEG-like formats with
               text subtitles */
          case GAVL_STREAM_TEXT:
            s = bgav_track_add_text_stream(b->tt->tracks[i], &b->opt, NULL);
            break;
          case GAVL_STREAM_OVERLAY:
            s = bgav_track_add_overlay_stream(b->tt->tracks[i], &b->opt);
            break;
          }
        
        /* Fourcc */
        if(!bgav_input_read_32_be(input, &s->fourcc) ||
           !bgav_input_read_32_be(input, &u_tmp))
          goto fail;
        s->ci->max_packet_size = u_tmp;
        s->stream_id = stream_id;
        }
      else
        {
        bgav_input_skip(input, 8); /* Stream type + fourcc */
        
        if(!bgav_input_read_32_be(input, &u_tmp))
          goto fail;
        s->ci->max_packet_size = u_tmp;
        }
      s->file_index = file_index_read_stream(input, s);
      if(!s->file_index)
        {
        goto fail;
        }
      }
    }
  bgav_input_destroy(input);
  set_has_file_index(b);
  free(filename);
  return 1;
  fail:

  if(input)
    bgav_input_destroy(input);
  
  if(filename)
    free(filename);
  return 0;
  }

typedef struct
  {
  char * name;
  off_t size;
  time_t time;
  } index_file_t;

static void purge_cache(const char * filename,
                        int max_size, const bgav_options_t * opt)
  {
  int num_files;
  int files_alloc;
  index_file_t * files = NULL;
  int64_t total_size;
  int64_t max_total_size;
  time_t time_min;
  int index;
  char * directory, *pos;
  DIR * dir;
  struct dirent * res;
  struct stat st;
  int i;
  directory = gavl_strdup(filename);
  pos = strrchr(directory, '/');
  if(!pos)
    {
    free(directory);
    return;
    }
  *pos = '\0';

  dir = opendir(directory);
  if(!dir)
    {
    free(directory);
    return;
    }
  /* Get all index files with their sizes and mtimes */
  num_files = 0;
  files_alloc = 0;
  total_size = 0;
  while( (res=readdir(dir)) )
    {
    if(!res)
      break;
#ifdef _WIN32
    stat(res->d_name, &st);
    if( S_ISDIR(st.st_mode  ))
#else    
    if(res->d_type == DT_REG)
#endif
      {
      if(num_files + 1 > files_alloc)
        {
        files_alloc += 128;
        files = realloc(files, files_alloc * sizeof(*files));
        memset(files + num_files, 0,
               (files_alloc - num_files) * sizeof(*files));
        }
      files[num_files].name = bgav_sprintf("%s/%s", directory, res->d_name);

      if(!stat(files[num_files].name, &st))
        {
        files[num_files].time = st.st_mtime;
        files[num_files].size = st.st_size;
        }
      total_size += files[num_files].size;
      num_files++;
      }
    }
  closedir(dir);
  
  max_total_size = (int64_t)max_size * 1024 * 1024;

  while(total_size > max_total_size)
    {
    /* Look for the file to be deleted */
    time_min = 0;
    index = -1;
    for(i = 0; i < num_files; i++)
      {
      if(files[i].time &&
         ((files[i].time < time_min) || !time_min))
        {
        time_min = files[i].time;
        index = i;
        }
      }
    if(index == -1)
      break;
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
             "Removing %s to keep maximum cache size", files[index].name);
    remove(files[index].name);
    files[index].time = 0;
    total_size -= files[index].size;
    }
  
  for(i = 0; i < num_files; i++)
    {
    if(files[i].name) free(files[i].name);
    }
  free(files);
  free(directory);
  }

void bgav_write_file_index(bgav_t * b)
  {
  int i, j;
  FILE * output;
  char * filename;
  int num_streams;
  bgav_stream_t * s;
  /* Check if the input provided an index filename */
  if(!b->input->index_file || !b->input->filename)
    return;
  
  filename = 
    bgav_search_file_write(&b->opt,
                           "indices", b->input->index_file);
  
  output = fopen(filename, "w");
  
  bgav_file_index_write_header(b->input->filename,
                               output,
                               b->tt->num_tracks);
  for(i = 0; i < b->tt->num_tracks; i++)
    {
    num_streams = 0;
    for(j = 0; j < b->tt->tracks[i]->num_streams; j++)
      {
      s = &b->tt->tracks[i]->streams[j];
      if(s->file_index)
        num_streams++;
      }
    
    write_32(output, num_streams);
    
    for(j = 0; j < b->tt->tracks[i]->num_streams; j++)
      {
      s = &b->tt->tracks[i]->streams[j];
      if(s->file_index)
        file_index_write_stream(output, s->file_index, s);
      }
    }
  fclose(output);
  
  if(b->opt.cache_size > 0)
    purge_cache(filename, b->opt.cache_size, &b->opt);
  
  free(filename);

  }

/*
 *  Top level packets contain complete frames of one elemtary stream
 */

static void flush_stream_simple(bgav_stream_t * s, int force)
  {
  bgav_packet_t * p;
  int64_t t;
  
  while(bgav_stream_peek_packet_read(s, NULL, force) == GAVL_SOURCE_OK)
    {
    p = NULL;
    bgav_stream_get_packet_read(s, &p);
    
    t = p->pts - s->stats.pts_start;
    
#if 0
    fprintf(stderr, "flush_stream_simple ID: %d Force: %d ", s->stream_id, force);
    bgav_packet_dump(p);
#endif
    if(p->pts != GAVL_TIME_UNDEFINED)
      {
      bgav_file_index_append_packet(s->file_index,
                                    p->position, t, p->flags, p->tc);
      if(t + p->duration >= s->stats.pts_end)
        s->stats.pts_end = t + p->duration;
      }
    bgav_stream_done_packet_read(s, p);
    }

  if(force) // EOF
    {
    if(!s->ci->max_packet_size)
      s->ci->max_packet_size = s->max_packet_size_tmp;
    }
  }

static int build_file_index_simple(bgav_t * b)
  {
  int j;
  int64_t old_position;
  bgav_stream_t * s;

  old_position = b->input->position;
  
  while(1)
    {
    if(!bgav_demuxer_next_packet(b->demuxer))
      break;
    
    for(j = 0; j < b->tt->cur->num_streams; j++)
      {
      if(!(b->tt->cur->streams[j].flags & STREAM_EXTERN))
        flush_stream_simple(&b->tt->cur->streams[j], 0);
      }
    }

  for(j = 0; j < b->tt->cur->num_streams; j++)
    {
    s = &b->tt->cur->streams[j];
    if(!(s->flags & STREAM_EXTERN))
      flush_stream_simple(s, 1);
    }
  bgav_input_seek(b->input, old_position, SEEK_SET);
  return 1;
  }

static int bgav_build_file_index_parseall(bgav_t * b)
  {
  int i, j;
  int ret = 0;
  
  for(i = 0; i < b->tt->num_tracks; i++)
    {
    bgav_select_track(b, i);
    b->demuxer->flags |= BGAV_DEMUXER_BUILD_INDEX;
    for(j = 0; j < b->tt->cur->num_streams; j++)
      {
      if(b->tt->cur->streams[i].flags & STREAM_EXTERN)
        continue;

      b->tt->cur->streams[j].file_index = bgav_file_index_create();
      b->tt->cur->streams[j].action = BGAV_STREAM_PARSE;
      gavl_stream_stats_init(&b->tt->cur->streams[j].stats);
      }

    if(!bgav_start(b))
      return 0;
    
    build_file_index_simple(b);
    
    b->demuxer->flags &= ~BGAV_DEMUXER_BUILD_INDEX;
    
    bgav_stop(b);
    
    /* Switch off streams again */
    for(j = 0; j < b->tt->cur->num_streams; j++)
      {
      if(b->tt->cur->streams[i].flags & STREAM_EXTERN)
        continue;
      b->tt->cur->streams[j].action = BGAV_STREAM_MUTE;
      }
    }
  ret = 1;
  return ret;
  }

static int build_file_index_si_parse_audio(bgav_t * b, int track, int stream)
  {
  bgav_stream_t * s;
  bgav_select_track(b, track);

  s = bgav_track_get_audio_stream(b->tt->cur, stream);
  
  s->file_index = bgav_file_index_create();
  bgav_set_audio_stream(b, stream, BGAV_STREAM_PARSE);
  gavl_stream_stats_init(&s->stats);
  bgav_start(b);
  b->demuxer->request_stream = s;
  s->stats.pts_end = 0; 

  if(s->index_mode == INDEX_MODE_SIMPLE)
    {
    while(1)
      {
      if(!bgav_demuxer_next_packet(b->demuxer))
        break;
      flush_stream_simple(s, 0);
      b->demuxer->request_stream = s;
      }
    flush_stream_simple(s, 1);
    }
  
  bgav_stop(b);
  bgav_set_audio_stream(b, stream, BGAV_STREAM_MUTE);
  return 1;
  }

static int build_file_index_si_parse_video(bgav_t * b, int track, int stream)
  {
  bgav_stream_t * s;

  bgav_select_track(b, track);

  s = bgav_track_get_video_stream(b->tt->cur, stream);
  
  s->file_index = bgav_file_index_create();
  bgav_set_video_stream(b, stream, BGAV_STREAM_PARSE);
  gavl_stream_stats_init(&s->stats);
  bgav_start(b);
  b->demuxer->request_stream = s;
  
  if(s->index_mode == INDEX_MODE_SIMPLE)
    {
    while(1)
      {
      if(!bgav_demuxer_next_packet(b->demuxer))
        break;
      flush_stream_simple(s, 0);
      b->demuxer->request_stream = s;
      }
    flush_stream_simple(s, 1);
    }
  bgav_stop(b);
  bgav_set_video_stream(b, stream, BGAV_STREAM_MUTE);
  return 1;
  }

static int bgav_build_file_index_si_parse(bgav_t * b)
  {
  int i, j;
  bgav_stream_t * s;
  
  for(i = 0; i < b->tt->num_tracks; i++)
    {
    b->demuxer->flags |= BGAV_DEMUXER_BUILD_INDEX;
    for(j = 0; j < b->tt->cur->num_audio_streams; j++)
      {
      s = bgav_track_get_audio_stream(b->tt->cur, j);
      
      if(!s->index_mode)
        continue;

      if(!build_file_index_si_parse_audio(b, i, j))
        return 0;
      }
    for(j = 0; j < b->tt->cur->num_video_streams; j++)
      {
      s = bgav_track_get_video_stream(b->tt->cur, j);

      if(!s->index_mode)
        continue;

      if(!build_file_index_si_parse_video(b, i, j))
        return 0;
      }
    b->demuxer->flags &= ~BGAV_DEMUXER_BUILD_INDEX;
    }
  return 1;
  }

int bgav_build_file_index(bgav_t * b, gavl_time_t * time_needed)
  {
  int ret;
  
  gavl_timer_t * timer = gavl_timer_create();

  gavl_timer_start(timer);
  
  switch(b->demuxer->index_mode)
    {
    case INDEX_MODE_SIMPLE:
    case INDEX_MODE_MIXED:
      ret = bgav_build_file_index_parseall(b);
      break;
    case INDEX_MODE_SI_PARSE:
      ret = bgav_build_file_index_si_parse(b);
      break;
    default:
      ret = 0;
      break; 
    }
  if(!ret)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Building file index failed");
    gavl_timer_destroy(timer);
    return 0;
    }
  *time_needed = gavl_timer_get(timer);
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
           "Built file index in %.2f seconds",
           gavl_time_to_seconds(*time_needed));
  gavl_timer_destroy(timer);
  set_has_file_index(b);
  return 1;
  }

int bgav_demuxer_next_packet_fileindex(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s = ctx->request_stream;
  int new_pos;

  /* Check for EOS */
  if(s->index_position >= s->file_index->num_entries)
    return 0;
  
  /* Seek to right position */
  if(s->file_index->entries[s->index_position].position !=
     ctx->input->position)
    bgav_input_seek(ctx->input,
                    s->file_index->entries[s->index_position].position,
                    SEEK_SET);

  /* Advance next index position until we have a new file position */
  new_pos = s->index_position + 1;
  
  while((new_pos < s->file_index->num_entries) &&
        (s->file_index->entries[s->index_position].position ==
         s->file_index->entries[new_pos].position))
    {
    new_pos++;
    }
  /* Tell the demuxer where to stop */

  if(new_pos >= s->file_index->num_entries)
    ctx->next_packet_pos = 0x7FFFFFFFFFFFFFFFLL;
  else 
    ctx->next_packet_pos = s->file_index->entries[new_pos].position;

  if(!ctx->demuxer->next_packet(ctx))
    return 0;

  s->index_position = new_pos;
  
  return 1;
  }


