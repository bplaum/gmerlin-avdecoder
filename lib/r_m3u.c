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

#include <avdec_private.h>

#define PROBE_BYTES 10

#define LOG_DOMAIN "m3u"

static int probe_m3u(bgav_input_context_t * input)
  {
  char probe_buffer[PROBE_BYTES];
  char * pos;
  const char * mimetype = NULL;
  int result = 0;
  
  /* Most likely, we get this via http, so we can check the mimetype */
  if(gavl_dictionary_get_src(&input->m, GAVL_META_SRC, 0, &mimetype, NULL) && mimetype)
    {
    if(strcasecmp(mimetype, "audio/x-pn-realaudio-plugin") &&
       strcasecmp(mimetype, "video/x-pn-realvideo-plugin") &&
       strcasecmp(mimetype, "audio/x-pn-realaudio") &&
       strcasecmp(mimetype, "video/x-pn-realvideo") &&
       strcasecmp(mimetype, "audio/x-mpegurl") &&
       strcasecmp(mimetype, "audio/mpegurl") &&
       strcasecmp(mimetype, "audio/m3u") &&
       strncasecmp(mimetype, "application/x-mpegurl", 21) && // HLS
       strncasecmp(mimetype, "application/vnd.apple.mpegurl", 29)) // HLS
      return 0;
    }
  else if(input->filename)
    {
    pos = strrchr(input->filename, '.');
    if(!pos)
      return 0;
    if(strcasecmp(pos, ".m3u") &&
       strcasecmp(pos, ".m3u8") &&
       strcasecmp(pos, ".ram"))
      return 0;
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

static bgav_track_t * append_track(bgav_track_table_t * tt)
  {
  bgav_track_t * ret = bgav_track_table_append_track(tt);
  gavl_dictionary_set_string(ret->metadata, GAVL_META_MEDIA_CLASS, GAVL_META_MEDIA_CLASS_LOCATION);
  return ret;
  }

static bgav_track_table_t * parse_m3u(bgav_input_context_t * input)
  {
  char * buffer = NULL;
  uint32_t buffer_alloc = 0;
  char * pos;
  bgav_track_table_t * tt;
  bgav_track_t * t = NULL;

  int hls = 0;

  /* hls data */
  int width = 0;
  int height = 0;
  int bitrate = 0;
  
  tt = bgav_track_table_create(0);
  
  while(1)
    {
    if(!bgav_input_read_line(input, &buffer, &buffer_alloc, 0, NULL))
      break;
    pos = strip_spaces(buffer);

    if(!strcmp(pos, "--stop--"))
      break;

    if(*pos == '\0') // Empty line
      continue;
    
    if(*pos == '#')
      {
      // m3u8
      // http://tools.ietf.org/html/draft-pantos-http-live-streaming-12#section-3.4.10

      if(!strncasecmp(pos, "#EXT-X-MEDIA-SEQUENCE:", 22))
        {
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Detected HLS segment list");
        
        if(input->url)
          {
          char * hls_uri;

          if(!t)
            t = append_track(tt);

          if(gavl_string_starts_with(input->url, "https://"))
            hls_uri = gavl_sprintf("hlss://%s", input->url + 8);
          else if(gavl_string_starts_with(input->url, "http://"))
            hls_uri = gavl_sprintf("hls://%s", input->url + 7);
          else
            hls_uri = gavl_strdup(input->url);
        
          gavl_metadata_add_src(t->metadata, GAVL_META_SRC, NULL, hls_uri);
          free(hls_uri);
          return tt;
          }
        
        }
      
      if(!strncasecmp(pos, "#EXT-X-VERSION:", 15))
        {
        hls = 1;
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Detected HLS");
        }
      
      if(!strncasecmp(pos, "#EXT-X-STREAM-INF:", 18)) 
        {
        int idx = 0;
        char ** attrs;
        if(!t)
          t = append_track(tt);

        attrs = gavl_strbreak(pos + 18, ',');

        while(attrs[idx])
          {
          if(!strncasecmp(attrs[idx], "BANDWIDTH=", 10))
            bitrate = atoi(attrs[idx] + 10);

          else if(!strncasecmp(attrs[idx], "RESOLUTION=", 11))
            sscanf(attrs[idx] + 11, "%dx%d", &width, &height);
          
          idx++;
          }
        
        gavl_strbreak_free(attrs);
        }
      // Extended m3u
      else if(!strncasecmp(pos, "#EXTINF:", 8))
        {
        char * comma;
        gavl_time_t duration;

        comma = strchr(pos, ',');

        if(comma)
          {
          *comma = '\0';
          if(!t)
            t = append_track(tt);

          duration = gavl_seconds_to_time(strtod(pos, NULL));
          if(duration > 0)
            gavl_dictionary_set_long(t->metadata, GAVL_META_APPROX_DURATION, duration);
          comma++;

          while(isspace(*comma) && (*comma != '\0'))
            comma++;

          if(*comma != '\0')
            gavl_dictionary_set_string(t->metadata, GAVL_META_LABEL, comma);
          }
        }
      }
    else
      {
      char * uri;
      char * hls_uri;
      gavl_dictionary_t * src;
      if(!t)
        t = append_track(tt);
   
      
      uri = bgav_input_absolute_url(input, pos);

      if(hls)
        {
        if(gavl_string_starts_with(uri, "https://"))
          hls_uri = gavl_sprintf("hlss://%s", uri + 8);
        else if(gavl_string_starts_with(uri, "http://"))
          hls_uri = gavl_sprintf("hls://%s", uri + 7);
        else
          hls_uri = gavl_strdup(uri);
        
        src = gavl_metadata_add_src(t->metadata, GAVL_META_SRC, NULL, hls_uri);
        free(hls_uri);
        }
      else
        src = gavl_metadata_add_src(t->metadata, GAVL_META_SRC, NULL, uri);
      
      free(uri);

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
      
      if(!hls)
        t = NULL;
      }
    }

  if(buffer)
    free(buffer);
  return tt;
  }

const bgav_redirector_t bgav_redirector_m3u = 
  {
    .name =  "m3u/ram",
    .probe = probe_m3u,
    .parse = parse_m3u
  };
