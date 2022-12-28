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
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <gavl/log.h>
#include <gavl/http.h>

#include <avdec_private.h>

#define PROBE_BYTES 10

#define LOG_DOMAIN "m3u"

#define META_AUDIO_GROUP    "audio-group"
#define META_SUBTITLE_GROUP "subtitle-group"

static int probe_m3u(bgav_input_context_t * input)
  {
  char probe_buffer[PROBE_BYTES];
  const char * mimetype = NULL;
  int result = 0;

  const char * real_uri = gavl_dictionary_get_string(&input->m, GAVL_META_REAL_URI);

  if(real_uri &&
     (gavl_string_ends_with(real_uri, ".m3u") ||
      gavl_string_ends_with(real_uri, ".m3u8")))
    return 1;
  
  /* Most likely, we get this via http, so we can check the mimetype */
  if(input->location)
    {
    if(gavl_metadata_get_src(&input->m, GAVL_META_SRC, 0, &mimetype, NULL) && mimetype)
      {
      if(strcasecmp(mimetype, "audio/x-pn-realaudio-plugin") &&
         strcasecmp(mimetype, "video/x-pn-realvideo-plugin") &&
         strcasecmp(mimetype, "audio/x-pn-realaudio") &&
         strcasecmp(mimetype, "video/x-pn-realvideo") &&
         strcasecmp(mimetype, "audio/x-mpegurl") &&
         strcasecmp(mimetype, "audio/mpegurl") &&
         strcasecmp(mimetype, "audio/m3u") &&
         strncasecmp(mimetype, "application/x-mpegurl", 21) && // HLS
         strncasecmp(mimetype, "application/vnd.apple.mpegurl", 29) && // HLS
         (!gavl_string_ends_with_i(input->location, ".m3u") &&
          !gavl_string_ends_with_i(input->location, ".m3u8")))
        return 0;
      }
    else // No mimetype
      {
      if(!gavl_string_ends_with(input->location, ".m3u")  &&
         !gavl_string_ends_with(input->location, ".m3u8") &&
         !gavl_string_ends_with(input->location, ".ram"))
        return 0;
      }
    }
  
  if(bgav_input_get_data(input, (uint8_t*)probe_buffer,
                         PROBE_BYTES) < PROBE_BYTES)
    goto end;
  
  /* Some streams with the above mimetype are in realtiy
     different streams, so we check this here */
  if(strncmp(probe_buffer, "mms://", 6) &&
     strncmp(probe_buffer, "http://", 7) &&
     strncmp(probe_buffer, "https://", 8) &&
     strncmp(probe_buffer, "rtsp://", 7) &&
     (probe_buffer[0] != '#'))
    goto end;
  result = 1;
  end:
  return result;
  
  }

static char * strip_spaces(char * str)
  {
  char * pos = str;

  while(isspace(*pos) && *pos != '\0')
    pos++;

  if((*pos == '\0'))
    {
    *str = '\0';
    return str;
    }
  
  if(pos > str)
    memmove(str, pos, strlen(pos)+1);

  pos = str + (strlen(str)-1);
  while(isspace(*pos))
    pos--;

  pos++;
  *pos = '\0';
  return str;
  }

static char * make_hls_uri(const char * uri)
  {
  char * ret;
  if(gavl_string_starts_with_i(uri, "https://"))
    ret = gavl_sprintf("hlss://%s", uri + 8);
  else if(gavl_string_starts_with_i(uri, "http://"))
    ret = gavl_sprintf("hls://%s", uri + 7);
  else
    ret = gavl_strdup(uri);
  
  return ret;
  }

static bgav_track_t * append_track(bgav_track_table_t * tt)
  {
  bgav_track_t * ret = bgav_track_table_append_track(tt);
  gavl_dictionary_set_string(ret->metadata, GAVL_META_MEDIA_CLASS, GAVL_META_MEDIA_CLASS_LOCATION);
  return ret;
  }

static void parse_ext_x_media(bgav_input_context_t * input, const char * pos, gavl_dictionary_t * ret)
  {
  char * pos1;
  char * pos2;

  char * type = NULL;
  char * group_id = NULL;
      
  if((pos1 = strstr(pos, "TYPE=")))
    {
    pos1 += strlen("TYPE=");
          
    if((pos2 = strchr(pos1, ',')))
      type = gavl_strndup(pos1, pos2);
    else
      type = gavl_strdup(pos1);
    }

  if((pos1 = strstr(pos, "GROUP-ID=\"")))
    {
    pos1 += strlen("GROUP-ID=\"");
          
    if((pos2 = strchr(pos1, '"')))
      group_id = gavl_strndup(pos1, pos2);
    else
      group_id = gavl_strdup(pos1);
    }

  if(type && group_id)
    {
    gavl_value_t val;
    gavl_array_t * arr;
    gavl_dictionary_t * dict;
          
    gavl_value_init(&val);
          
    dict = gavl_dictionary_get_dictionary_create(ret, type);
    arr = gavl_dictionary_get_array_create(dict, group_id);
          
    dict = gavl_value_set_dictionary(&val);
    gavl_array_splice_val_nocopy(arr, -1, 0, &val);
          
    if((pos1 = strstr(pos, "NAME=\"")))
      {
      pos1 += strlen("NAME=\"");
            
      if((pos2 = strchr(pos1, '"')))
        gavl_dictionary_set_string_nocopy(dict, GAVL_META_LABEL, gavl_strndup(pos1, pos2));
      }

    if((pos1 = strstr(pos, "URI=\"")))
      {
      char * tmp_string;
          
      pos1 += strlen("URI=\"");
            
      if((pos2 = strchr(pos1, '"')))
        {
        tmp_string = gavl_strndup(pos1, pos2);
        gavl_dictionary_set_string_nocopy(dict, GAVL_META_URI, bgav_input_absolute_url(input, tmp_string));
        free(tmp_string);
        }
      }

    if((pos1 = strstr(pos, "LANGUAGE=\"")))
      {
      pos1 += strlen("LANGUAGE=\"");
            
      if((pos2 = strchr(pos1, '"')))
        gavl_dictionary_set_string_nocopy(dict, GAVL_META_LANGUAGE, gavl_strndup(pos1, pos2));
      }
          
    }
        
  if(type)
    free(type);
  if(group_id)
    free(group_id);
  
  }

static bgav_track_table_t * parse_m3u(bgav_input_context_t * input)
  {
  int i;
  gavl_buffer_t line_buf;
  const char * pos;
  char * str;
  
  bgav_track_table_t * tt;
  bgav_track_t * t = NULL;

  int hls = 0;

  /* hls data */
  int width = 0;
  int height = 0;
  int bitrate = 0;
  double framerate = 0.0;
  gavl_array_t lines;
  
  char * audio = NULL;
  char * subtitles = NULL;
  
  gavl_dictionary_t ext_x_media;
  gavl_dictionary_t metadata;
  gavl_dictionary_t http_vars;
  gavl_dictionary_t http_vars_global;

  gavl_buffer_init(&line_buf);

  gavl_dictionary_init(&ext_x_media);
  gavl_dictionary_init(&metadata);
  gavl_dictionary_init(&http_vars);
  gavl_dictionary_init(&http_vars_global);
  gavl_array_init(&lines);

  if(input->location)
    {
    char * tmp_string = gavl_strdup(input->location);
    tmp_string = gavl_url_extract_http_vars(tmp_string, &http_vars_global);
    free(tmp_string);
    }

  //  fprintf(stderr, "Got global http vars %s:\n", input->url);
  //  gavl_dictionary_dump(&http_vars_global, 2);
  
  tt = bgav_track_table_create(0);

  /* Read input and split into lines */

  /* First pass: read lines, process #EXT-X-MEDIA lines */
  while(1)
    {
    if(!bgav_input_read_line(input, &line_buf))
      break;
    str = (char*)line_buf.buf;
    pos = strip_spaces(str);
    if(*pos == '\0') // Empty line
      continue;

    if(gavl_string_starts_with(pos, "#EXT-X-MEDIA:"))
      {
      parse_ext_x_media(input, pos, &ext_x_media);
      }
    else
      {
      /* Save for second pass */
      gavl_string_array_insert_at(&lines, -1, pos);
      }
    }

  for(i = 0; i < lines.num_entries; i++)
    {
    pos = gavl_string_array_get(&lines, i);
    
    if(!strcmp(pos, "--stop--"))
      break;

    if(*pos == '\0') // Empty line
      continue;
    
    if(*pos == '#')
      {
      // m3u8
      // http://tools.ietf.org/html/draft-pantos-http-live-streaming-12#section-3.4.10

      if(gavl_string_starts_with(pos, "#EXT-X-MEDIA-SEQUENCE:"))
        {
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Detected HLS segment list");
        
        if(input->location)
          {
          char * hls_uri;

          if(!t)
            t = append_track(tt);

          hls_uri = make_hls_uri(input->location);

          gavl_dictionary_merge2(&http_vars, &http_vars_global);
          hls_uri = gavl_url_append_http_vars(hls_uri, &http_vars);
          gavl_dictionary_reset(&http_vars);
          
          gavl_metadata_add_src(t->metadata, GAVL_META_SRC, NULL, hls_uri);
          free(hls_uri);
          return tt;
          }
        
        }
      
      if(gavl_string_starts_with(pos, "#EXT-X-VERSION:") ||
         gavl_string_starts_with(pos, "#EXT-X-INDEPENDENT-SEGMENTS"))
        {
        hls = 1;
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Detected HLS");
        }
      
      
      if(gavl_string_starts_with(pos, "#EXT-X-STREAM-INF:")) 
        {
        char * pos1, * pos2;

        if(!hls)
          {
          hls = 1;
          gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Detected HLS");
          }
        
        //        int idx = 0;

        //        fprintf(stderr, "Got #EXT-X-STREAM-INF\n");
        //        gavl_dictionary_dump(&ext_x_media, 2);
        
        
        //        if(!t)
        //          t = append_track(tt);

        if((pos1 = strstr(pos, "BANDWIDTH=")))
          bitrate = atoi(pos1 + strlen("BANDWIDTH="));

        if((pos1 = strstr(pos, "FRAME-RATE=")))
          framerate = strtod(pos1 + strlen("FRAME-RATE="), NULL);
        
        if((pos1 = strstr(pos, "RESOLUTION=")))
          sscanf(pos1 + strlen("RESOLUTION="), "%dx%d", &width, &height);
        
        if((pos1 = strstr(pos, "AUDIO=\"")))
          {
          pos1 += strlen("AUDIO=\"");
          pos2 = strchr(pos1, '"');

          if(pos2)
            audio = gavl_strndup(pos1, pos2);
          
          }
        if((pos1 = strstr(pos, "SUBTITLES=\"")))
          {
          pos1 += strlen("SUBTITLES=\"");
          pos2 = strchr(pos1, '"');

          if(pos2)
            subtitles = gavl_strndup(pos1, pos2);
          }
        
        }
      // Extended m3u
      else if(gavl_string_starts_with(pos, "#EXTINF:"))
        {
        char * comma;
        gavl_time_t duration;
        const char * pos1, *pos2;
        
        comma = strrchr(pos, ',');

        if(comma)
          {
          *comma = '\0';
          
          duration = gavl_seconds_to_time(strtod(pos, NULL));
          if(duration > 0)
            gavl_dictionary_set_long(&metadata, GAVL_META_APPROX_DURATION, duration);
          comma++;

          while(isspace(*comma) && (*comma != '\0'))
            comma++;

          if(*comma != '\0')
            gavl_dictionary_set_string(&metadata, GAVL_META_LABEL, comma);

          if((pos1 = strstr(pos, "tvg-logo=\"")))
            {
            pos1 += strlen("tvg-logo=\"");

            if((pos2 = strchr(pos1, '"')) && (pos2 > pos1))
              {
              gavl_dictionary_set_string_nocopy(&metadata, GAVL_META_LOGO_URL, gavl_strndup(pos1, pos2));
              }
            }

          if((pos1 = strstr(pos, "tvg-country=\"")))
            {
            pos1 += strlen("tvg-country=\"");

            if((pos2 = strchr(pos1, '"')) && (pos2 > pos1))
              {
              char * pos;
              const char * label;
              char * tmp_string = gavl_strndup(pos1, pos2);

              if(strchr(tmp_string, ';'))
                {
                int idx = 0;
                char ** arr = gavl_strbreak(tmp_string, ';');

                while(arr[idx])
                  {
                  if((pos = strchr(arr[idx], '-')))
                    *pos = '\0';
                  if((label = gavl_get_country_label(arr[idx])))
                    gavl_dictionary_append_string_array(&metadata, GAVL_META_COUNTRY, label);
                  idx++;
                  }
                gavl_strbreak_free(arr);
                free(tmp_string);
                }
              else
                {
                if((pos = strchr(tmp_string, '-')))
                  *pos = '\0';

                if((label = gavl_get_country_label(tmp_string)))
                  gavl_dictionary_set_string(&metadata, GAVL_META_COUNTRY, label);
                free(tmp_string);
                }
              
              
              }
            }

          if((pos1 = strstr(pos, "tvg-language=\"")))
            {
            pos1 += strlen("tvg-language=\"");

            if((pos2 = strchr(pos1, '"')) && (pos2 > pos1))
              {
              char * tmp_string = gavl_strndup(pos1, pos2);

              if(strchr(tmp_string, ';'))
                {
                int idx = 0;
                char ** arr = gavl_strbreak(tmp_string, ';');

                while(arr[idx])
                  {
                  gavl_dictionary_append_string_array(&metadata, GAVL_META_AUDIO_LANGUAGES, arr[idx]);
                  idx++;
                  }
                gavl_strbreak_free(arr);
                free(tmp_string);
                }
              else
                gavl_dictionary_set_string_nocopy(&metadata, GAVL_META_AUDIO_LANGUAGES,
                                                  tmp_string);
              }
            }
          if((pos1 = strstr(pos, "group-title=\"")))
            {
            pos1 += strlen("group-title=\"");

            if((pos2 = strchr(pos1, '"')) && (pos2 > pos1))
              {
              char * tmp_string = gavl_strndup(pos1, pos2);

              if(strchr(tmp_string, ';'))
                {
                int idx = 0;
                char ** arr = gavl_strbreak(tmp_string, ';');

                while(arr[idx])
                  {
                  gavl_dictionary_append_string_array(&metadata, GAVL_META_CATEGORY, arr[idx]);
                  idx++;
                  }
                gavl_strbreak_free(arr);
                free(tmp_string);
                }
              else
                gavl_dictionary_set_string_nocopy(&metadata, GAVL_META_CATEGORY,
                                                  tmp_string);
              }
            }
          
          
          }
        }
      else if(gavl_string_starts_with(pos, "#EXTVLCOPT:"))
        {
        const char * pos1;
        pos1 = pos + strlen("#EXTVLCOPT:");

        if(gavl_string_starts_with(pos1, "http-referrer="))
          {
          pos1 += strlen("http-referrer=");
          gavl_dictionary_set_string(&http_vars, "Referer", pos1);
          }
        else if(gavl_string_starts_with(pos1, "http-user-agent="))
          {
          pos1 += strlen("http-user-agent=");
          gavl_dictionary_set_string(&http_vars, "User-Agent", pos1);
          }
        }
      }
    else // Got URI
      {
      char * uri;
      char * hls_uri;
      const gavl_array_t * arr;
      const gavl_dictionary_t * dict;

      const char * stream_uri;

      uri = bgav_input_absolute_url(input, pos);

      if(hls)
        {
        gavl_dictionary_t * variant;
        gavl_dictionary_t * variant_m;
        
        if(!t)
          t = append_track(tt);

        hls_uri = make_hls_uri(uri);
        
        gavl_dictionary_merge2(&http_vars, &http_vars_global);
        
        /* Append URI variables */
        hls_uri = gavl_url_append_http_vars(hls_uri, &http_vars);
        gavl_dictionary_reset(&http_vars);
        
        gavl_dictionary_merge2(t->metadata, &metadata);
        gavl_dictionary_reset(&metadata);

        variant = gavl_track_append_variant(t->info, NULL, hls_uri);
        free(hls_uri);
        
        variant_m = gavl_track_get_metadata_nc(variant);
        if(framerate != 0.0)
          {
          gavl_dictionary_set_float(variant_m, GAVL_META_FRAMERATE, framerate);
          framerate = 0.0;
          }

        if(bitrate)
          {
          gavl_dictionary_set_int(variant_m, GAVL_META_BITRATE, bitrate);
          bitrate = 0;
          }
        if(width)
          {
          gavl_dictionary_set_int(variant_m, GAVL_META_WIDTH, width);
          width = 0;
          }
        if(height)
          {
          gavl_dictionary_set_int(variant_m, GAVL_META_HEIGHT, height);
          height = 0;
          }
        
        /* Check for separate audio streams */
        if(audio && (dict = gavl_dictionary_get_dictionary_nc(&ext_x_media, "AUDIO")) &&
           (arr = gavl_dictionary_get_array(dict, audio)))
          {
          /* Attach external audio streams */
          gavl_dictionary_t * ext_stream;
          gavl_dictionary_t * ext_m;
          int i;
          for(i = 0; i < arr->num_entries; i++)
            {
            if((dict = gavl_value_get_dictionary(&arr->entries[i])) &&
               (stream_uri = gavl_dictionary_get_string(dict, GAVL_META_URI)))
              {
              hls_uri = make_hls_uri(stream_uri);
              
              ext_stream = gavl_track_append_external_stream(variant, GAVL_STREAM_AUDIO,
                                                             NULL, hls_uri);
              ext_m = gavl_stream_get_metadata_nc(ext_stream);
              gavl_dictionary_set(ext_m, GAVL_META_LANGUAGE, gavl_dictionary_get(dict, GAVL_META_LANGUAGE));
              gavl_dictionary_set(ext_m, GAVL_META_LABEL, gavl_dictionary_get(dict, GAVL_META_LABEL));
              free(hls_uri);
              }
            }
          }
#if 0
        /* Check for separate subtitle streams */
        if(subtitles && (dict = gavl_dictionary_get_dictionary_nc(&ext_x_media, "SUBTITLES")) &&
           (arr = gavl_dictionary_get_array(dict, subtitles)))
          {
          /* Attach external subtitle streams */
          gavl_dictionary_t * ext_stream;
          gavl_dictionary_t * ext_m;
          int i;
          for(i = 0; i < arr->num_entries; i++)
            {
            if((dict = gavl_value_get_dictionary(&arr->entries[i])) &&
               (stream_uri = gavl_dictionary_get_string(dict, GAVL_META_URI)))
              {
              hls_uri = make_hls_uri(stream_uri);

              ext_stream = gavl_track_append_external_stream(variant, GAVL_STREAM_TEXT,
                                                             NULL, hls_uri);
              ext_m = gavl_stream_get_metadata_nc(ext_stream);

              gavl_dictionary_set(ext_m, GAVL_META_LANGUAGE, gavl_dictionary_get(dict, GAVL_META_LANGUAGE));
              gavl_dictionary_set(ext_m, GAVL_META_LABEL, gavl_dictionary_get(dict, GAVL_META_LABEL));
              free(hls_uri);
              }
            }
          }
#endif
        }
      else
        {
        char * tmp_string = gavl_strdup(uri);
        if(!t)
          t = append_track(tt);

        gavl_dictionary_merge2(&http_vars, &http_vars_global);
        
        tmp_string = gavl_url_append_http_vars(tmp_string, &http_vars);
        gavl_dictionary_reset(&http_vars);

        
        gavl_dictionary_merge2(t->metadata, &metadata);
        gavl_dictionary_reset(&metadata);
        
        gavl_metadata_add_src(t->metadata, GAVL_META_SRC, NULL, tmp_string);
        free(tmp_string);
        }
      free(uri);
#if 0
      if(src)
        {
        if(bitrate)
          {
          gavl_dictionary_set_int(src, GAVL_META_BITRATE, bitrate);
          bitrate = 0;
          }
        if(width)
          {
          gavl_dictionary_set_int(src, GAVL_META_WIDTH, width);
          width = 0;
          }
        if(height)
          {
          gavl_dictionary_set_int(src, GAVL_META_HEIGHT, height);
          height = 0;
          }

        }
#endif
      
      if(audio)
        {
        free(audio);
        audio = NULL;
        }

      if(subtitles)
        {
        free(subtitles);
        subtitles = NULL;
        }
      
      if(!hls)
        t = NULL;
      }
    }
  gavl_dictionary_free(&ext_x_media);
  gavl_dictionary_free(&http_vars_global);
  gavl_dictionary_free(&http_vars);
  gavl_array_free(&lines);
  
  gavl_buffer_free(&line_buf);
  
  return tt;
  }

const bgav_redirector_t bgav_redirector_m3u = 
  {
    .name =  "m3u/ram",
    .probe = probe_m3u,
    .parse = parse_m3u
  };
