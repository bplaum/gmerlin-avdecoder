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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <glob.h>
#include <errno.h>

#include <avdec_private.h>
#include <pes_header.h>

static int glob_errfunc(const char *epath, int eerrno)
  {
  fprintf(stderr, "glob error: Cannot access %s: %s\n",
          epath, strerror(eerrno));
  return 0;
  }

/* SRT format */

#define LOG_DOMAIN "srt"

static int probe_srt(char * line, bgav_input_context_t * ctx)
  {
  int a1,a2,a3,a4,b1,b2,b3,b4,i;
  if(sscanf(line, "%d:%d:%d%[,.:]%d --> %d:%d:%d%[,.:]%d",
            &a1,&a2,&a3,(char *)&i,&a4,&b1,&b2,&b3,(char *)&i,&b4) == 10)
    return 1;
  return 0;
  }

static int init_srt(bgav_stream_t * s)
  {
  bgav_subtitle_reader_context_t * ctx;
  s->timescale = 1000;
  ctx = s->data.subtitle.subreader;
  ctx->scale_num = 1;
  ctx->scale_den = 1;
  return 1;
  }

static gavl_source_status_t read_srt(bgav_stream_t * s, bgav_packet_t * p)
  {
  int lines_read;
  uint32_t line_len;
  int a1,a2,a3,a4,b1,b2,b3,b4;
  int i,len;
  bgav_subtitle_reader_context_t * ctx;
  gavl_time_t start, end;
  
  ctx = s->data.subtitle.subreader;
  
  /* Read lines */
  while(1)
    {
    if(!bgav_input_read_convert_line(ctx->input, &ctx->line,
                                     &ctx->line_alloc, &line_len))
      return GAVL_SOURCE_EOF;

    // fprintf(stderr, "Line: %s (%c)\n", ctx->line, ctx->line[0]);
    
    if(ctx->line[0] == '@')
      {
      if(!strncasecmp(ctx->line, "@OFF=", 5))
        {
        ctx->time_offset += (int)(atof(ctx->line+5) * 1000);
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
                 "new time offset: %"PRId64, ctx->time_offset);
        }
      else if(!strncasecmp(ctx->line, "@SCALE=", 7))
        {
        sscanf(ctx->line + 7, "%d:%d", &ctx->scale_num, &ctx->scale_den);
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
                 "new scale factor: %d:%d", ctx->scale_num, ctx->scale_den);
        

        //        fprintf(stderr, "new scale factor: %d:%d\n",
        //                ctx->scale_num, ctx->scale_den);
        }
      continue;
      }
    
    
    else if((len=sscanf (ctx->line,
                         "%d:%d:%d%[,.:]%d --> %d:%d:%d%[,.:]%d",
                         &a1,&a2,&a3,(char *)&i,&a4,
                         &b1,&b2,&b3,(char *)&i,&b4)) == 10)
      {
      break;
      }
    }

  start  = a1;
  start *= 60;
  start += a2;
  start *= 60;
  start += a3;
  start *= 1000;
  start += a4;

  end  = b1;
  end *= 60;
  end += b2;
  end *= 60;
  end += b3;
  end *= 1000;
  end += b4;
  
  p->pts = start + ctx->time_offset;
  p->duration = end - start;

  p->pts = gavl_time_rescale(ctx->scale_den,
                             ctx->scale_num,
                             p->pts);

  p->duration = gavl_time_rescale(ctx->scale_den,
                                  ctx->scale_num,
                                  p->duration);
  
  p->data_size = 0;
  
  /* Read lines until we are done */

  lines_read = 0;
  while(1)
    {
    if(!bgav_input_read_convert_line(ctx->input, &ctx->line, &ctx->line_alloc,
                                     &line_len))
      {
      line_len = 0;
      if(!lines_read)
        return GAVL_SOURCE_EOF;
      }
    
    if(!line_len)
      {
      /* Zero terminate */
      if(lines_read)
        {
        p->data[p->data_size] = '\0';
        // Terminator doesn't count for data size
        // p->data_size++;
        }
      return GAVL_SOURCE_OK;
      }
    if(lines_read)
      {
      p->data[p->data_size] = '\n';
      p->data_size++;
      }
    
    lines_read++;
    bgav_packet_alloc(p, p->data_size + line_len + 2);
    memcpy(p->data + p->data_size, ctx->line, line_len);
    p->data_size += line_len;
    }
  
  return GAVL_SOURCE_EOF;
  }

#undef LOG_DOMAIN

/* MPSub */

typedef struct
  {
  int frame_based;
  int64_t frame_duration;

  gavl_time_t last_end_time;
  } mpsub_priv_t;

static int probe_mpsub(char * line, bgav_input_context_t * ctx)
  {
  float f;
  while(isspace(*line) && (*line != '\0'))
    line++;
  
  if(!strncmp(line, "FORMAT=TIME", 11) ||
     (sscanf(line, "FORMAT=%f", &f) == 1))
    return 1;
  return 0;
  }

static int init_mpsub(bgav_stream_t * s)
  {
  uint32_t line_len;
  double framerate;
  char * ptr;
  bgav_subtitle_reader_context_t * ctx;
  mpsub_priv_t * priv = calloc(1, sizeof(*priv));
  ctx = s->data.subtitle.subreader;
  ctx->priv = priv;
  s->timescale = GAVL_TIME_SCALE;
  while(1)
    {
    if(!bgav_input_read_line(ctx->input, &ctx->line,
                             &ctx->line_alloc, 0, &line_len))
      return 0;

    ptr = ctx->line;
    
    while(isspace(*ptr) && (*ptr != '\0'))
      ptr++;

    if(!strncmp(ptr, "FORMAT=TIME", 11))
      return 1;
    else if(sscanf(ptr, "FORMAT=%lf", &framerate))
      {
      priv->frame_duration = gavl_seconds_to_time(1.0 / framerate);
      priv->frame_based = 1;
      return 1;
      }
    }
  return 0;
  }

static gavl_source_status_t read_mpsub(bgav_stream_t * s, bgav_packet_t * p)
  {
  int i1, i2;
  double d1, d2;
  gavl_time_t t1 = 0, t2 = 0;
  
  uint32_t line_len;
  int lines_read;
  bgav_subtitle_reader_context_t * ctx;
  mpsub_priv_t * priv;
  char * ptr;
  
  ctx = s->data.subtitle.subreader;
  priv = ctx->priv;
    
  while(1)
    {
    if(!bgav_input_read_line(ctx->input, &ctx->line,
                             &ctx->line_alloc, 0, &line_len))
      return GAVL_SOURCE_EOF;

    ptr = ctx->line;
    
    while(isspace(*ptr) && (*ptr != '\0'))
      ptr++;
    
    /*
     * The following will reset last_end_time whenever we
     * cross a "FORMAT=" line
     */
    
    if(!strncmp(ptr, "FORMAT=", 7))
      {
      priv->last_end_time = 0;
      continue;
      }
    
    if(priv->frame_based)
      {
      if(sscanf(ptr, "%d %d\n", &i1, &i2) == 2)
        {
        t1 = i1 * priv->frame_duration;
        t2 = i2 * priv->frame_duration;
        break;
        }
      }
    else if(sscanf(ptr, "%lf %lf\n", &d1, &d2) == 2)
      {
      t1 = gavl_seconds_to_time(d1);
      t2 = gavl_seconds_to_time(d2);
      break;
      }
    }

  /* Set times */

  p->pts = priv->last_end_time + t1;
  p->duration  = t2;
  
  priv->last_end_time = p->pts + p->duration;
  
  /* Read the actual stuff */
  p->data_size = 0;
  
  /* Read lines until we are done */

  lines_read = 0;

  while(1)
    {
    if(!bgav_input_read_convert_line(ctx->input, &ctx->line, &ctx->line_alloc,
                                     &line_len))
      {
      line_len = 0;
      if(!lines_read)
        return GAVL_SOURCE_EOF;
      }
    
    if(!line_len)
      {
      /* Zero terminate */
      if(lines_read)
        {
        // Terminator doesn't count to data size
        p->data[p->data_size] = '\0';
        //        p->data_size++;
        }
      return GAVL_SOURCE_OK;
      }
    if(lines_read)
      {
      p->data[p->data_size] = '\n';
      p->data_size++;
      }
    
    lines_read++;
    bgav_packet_alloc(p, p->data_size + line_len + 2);
    memcpy(p->data + p->data_size, ctx->line, line_len);
    p->data_size += line_len;
    }
  return GAVL_SOURCE_EOF;
  }

static void close_mpsub(bgav_stream_t * s)
  {
  bgav_subtitle_reader_context_t * ctx;
  mpsub_priv_t * priv;
  
  ctx = s->data.subtitle.subreader;
  priv = ctx->priv;
  free(priv);
  }

/* vobsub */

#define LOG_DOMAIN "vobsub"

static int64_t vobsub_parse_pts(const char * str)
  {
  int h, m, s, ms;
  int sign = 0;
  int64_t ret;
  
  if(sscanf(str, "%d:%d:%d:%d", &h, &m, &s, &ms) < 4)
    return GAVL_TIME_UNDEFINED;
  
  if(h < 0)
    {
    h = -h;
    sign = 1;
    }

  ret = h;
  ret *= 60;
  ret += m;
  ret *= 60;
  ret += s;
  ret *= 1000;
  ret += ms;

  if(sign)
    ret = -ret;
  return ret;
  }

static char * find_vobsub_file(const char * idx_file)
  {
  char * pos;
  char * ret = NULL;
  glob_t glob_buf;
  int i;
  char * pattern = gavl_strdup(idx_file);

  memset(&glob_buf, 0, sizeof(glob_buf));
  
  if(!(pos = strrchr(pattern, '.')))
    goto end;

  pos++;
  if(*pos == '\0')
    goto end;

  *pos = '*';
  pos++;
  *pos = '\0';

  /* Look for all files with the same name base */

  pattern = gavl_escape_string(pattern, "[]?");
  
  if(glob(pattern, 0, glob_errfunc, &glob_buf))
    {
    // fprintf(stderr, "glob returned %d\n", result);
    goto end;
    }
  
  for(i = 0; i < glob_buf.gl_pathc; i++)
    {
    pos = strrchr(glob_buf.gl_pathv[i], '.');
    if(pos && !strcasecmp(pos, ".sub"))
      {
      ret = gavl_strdup(glob_buf.gl_pathv[i]);
      break;
      }
    }
  end:
  
  if(pattern)
    free(pattern);
  globfree(&glob_buf);
  return ret;
  }

typedef struct
  {
  bgav_input_context_t * sub;
  } vobsub_priv_t;

static int probe_vobsub(char * line, bgav_input_context_t * ctx)
  {
  int ret = 0;
  char * str = NULL;
  uint32_t str_alloc = 0;
  uint32_t str_len;
  if(!strncasecmp(line, "# VobSub index file, v7", 23) &&
     (str = find_vobsub_file(ctx->filename)))
    {
    free(str);
    str = NULL;
    /* Need to count the number of streams */
    while(bgav_input_read_convert_line(ctx, &str, &str_alloc, &str_len))
      {
      if(!strncmp(str, "id:", 3) &&
         !strstr(str, "--") && strstr(str, "index:"))
        ret++;
      }
    if(str)
      free(str);
    }
  if(ret)
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
             "Detected VobSub subtitles, %d streams", ret);
 
  return ret;
  }

static int setup_stream_vobsub(bgav_stream_t * s)
  {
  bgav_input_context_t * input = NULL;
  bgav_subtitle_reader_context_t * ctx;
  char *line = NULL;
  uint32_t line_alloc = 0;
  uint32_t line_len = 0;
  int idx = 0;
  int ret = 0;
  
  ctx = s->data.subtitle.subreader;
  
  /* Open file */
  input = bgav_input_create(NULL, s->opt);
  if(!bgav_input_open(input, ctx->filename))
    goto fail;
  
  /* Read lines */
  while(bgav_input_read_line(input, &line,
                             &line_alloc, 0, &line_len))
    {
    if(*line == '#')
      continue;
    else if(!strncmp(line, "size:", 5)) // size: 720x576
      {
      sscanf(line + 5, "%dx%d",
             &s->data.subtitle.video.format->image_width,
             &s->data.subtitle.video.format->image_height);
      s->data.subtitle.video.format->frame_width =
        s->data.subtitle.video.format->image_width;
      s->data.subtitle.video.format->frame_height =
        s->data.subtitle.video.format->image_height;
      }
    else if(!strncmp(line, "palette:", 8)) // palette: 000000, 828282...
      {
      if((s->ci->global_header = (uint8_t*)bgav_get_vobsub_palette(line + 8)))
        {
        s->ci->global_header_len = 16 * 4;
        s->fourcc = BGAV_MK_FOURCC('D', 'V', 'D', 'S');
        }
      }
    else if(!strncmp(line, "id:", 3) &&
            !strstr(line, "--") && strstr(line, "index:"))
      {
      if(idx == ctx->stream)
        {
        char * pos;
        char language_2cc[3];
        const char * language_3cc;
        
        pos = line + 3;
        while(isspace(*pos) && (*pos != '\0'))
          pos++;

        if(*pos != '\0')
          {
          language_2cc[0] = pos[0];
          language_2cc[1] = pos[1];
          language_2cc[2] = '\0';
          if((language_3cc = bgav_lang_from_twocc(language_2cc)))
            gavl_dictionary_set_string(s->m, GAVL_META_LANGUAGE, language_3cc);
          }

        ctx->data_start = input->position;
        break;
        }
      else
        idx++;
      }
    }

  ret = 1;
  fail:
  
  if(line)
    free(line);
  if(input)
    bgav_input_destroy(input);
  return ret;
  }

static int init_vobsub(bgav_stream_t * s)
  {
  bgav_subtitle_reader_context_t * ctx;
  vobsub_priv_t * priv;
  char * sub_file = NULL;
  int ret = 0;

  s->timescale = 1000;
  s->flags |= STREAM_PARSE_FRAME;
  priv = calloc(1, sizeof(*priv));
  
  ctx = s->data.subtitle.subreader;
  ctx->priv = priv;

  sub_file = find_vobsub_file(ctx->filename);
  
  priv->sub = bgav_input_create(NULL, s->opt);

  if(!bgav_input_open(priv->sub, sub_file))
    goto fail;

  bgav_input_seek(ctx->input, ctx->data_start, SEEK_SET);
  
  ret = 1;
  
  fail:

  if(sub_file)
    free(sub_file);
  
  return ret;
  }

static gavl_source_status_t
read_vobsub(bgav_stream_t * s, bgav_packet_t * p)
  {
  bgav_subtitle_reader_context_t * ctx;
  vobsub_priv_t * priv;
  uint32_t line_len;
  const char * pos;
  int64_t pts, position;
  bgav_pes_header_t pes_header;
  bgav_pack_header_t pack_header;
  uint32_t header;
  int packet_size = 0;
  int substream_id = -1;
  uint8_t byte;
  
  ctx = s->data.subtitle.subreader;
  priv = ctx->priv;

  while(1)
    {
    if(!bgav_input_read_line(ctx->input, &ctx->line,
                             &ctx->line_alloc, 0, &line_len))
      return GAVL_SOURCE_EOF; // EOF

    if(gavl_string_starts_with(ctx->line, "id:"))
      return GAVL_SOURCE_EOF; // Next stream

    if(gavl_string_starts_with(ctx->line, "timestamp:") &&
       (pos = strstr(ctx->line, "filepos:")))
      break;
    
    }
  
  if((pts = vobsub_parse_pts(ctx->line + 10)) == GAVL_TIME_UNDEFINED)
    return GAVL_SOURCE_EOF;

  position = strtoll(pos + 8, NULL, 16);

  //  fprintf(stderr, "Pos: %"PRId64", PTS: %"PRId64"\n", position, pts);

  bgav_input_seek(priv->sub, position, SEEK_SET);
  
  while(1)
    {
    if(!bgav_input_get_32_be(priv->sub, &header))
      return GAVL_SOURCE_EOF;
    switch(header)
      {
      case START_CODE_PACK_HEADER:
        if(!bgav_pack_header_read(priv->sub, &pack_header))
          return GAVL_SOURCE_EOF;
        //        bgav_pack_header_dump(&pack_header);
        break;
      case 0x000001bd: // Private stream 1
        if(!bgav_pes_header_read(priv->sub, &pes_header))
          return GAVL_SOURCE_EOF;
        //        bgav_pes_header_dump(&pes_header);

        if(!bgav_input_read_8(priv->sub, &byte))
          return GAVL_SOURCE_EOF;

        pes_header.payload_size--;
        
        if(substream_id < 0)
          substream_id = byte;
        else if(substream_id != byte)
          {
          bgav_input_skip(priv->sub, pes_header.payload_size);
          continue;
          }

        bgav_packet_alloc(p, p->data_size + pes_header.payload_size);
        if(bgav_input_read_data(priv->sub,
                                p->data + p->data_size,
                                pes_header.payload_size) <
           pes_header.payload_size)
          return GAVL_SOURCE_EOF;
        
        if(!packet_size)
          {
          packet_size = GAVL_PTR_2_16BE(p->data);
          //          fprintf(stderr, "packet_size: %d\n", packet_size);
          }
        p->data_size += pes_header.payload_size;
        break;
      default:
        fprintf(stderr, "Unknown startcode %08x\n", header);
        return GAVL_SOURCE_EOF;
      }
    
    if((packet_size > 0) && (p->data_size >= packet_size))
      break;
    }
  p->pts = pts;
  
  return GAVL_SOURCE_OK;
  }

static void close_vobsub(bgav_stream_t * s)
  {
  bgav_subtitle_reader_context_t * ctx;
  vobsub_priv_t * priv;
  
  ctx = s->data.subtitle.subreader;
  priv = ctx->priv;

  if(priv->sub)
    bgav_input_destroy(priv->sub);
  free(priv);
  }

static void seek_vobsub(bgav_stream_t * s, int64_t time1, int scale)
  {
  //  bgav_subtitle_reader_context_t * ctx;
  //  ctx = s->data.subtitle.subreader;
  
  }

#undef LOG_DOMAIN

/* Spumux */



#ifdef HAVE_LIBPNG

#define LOG_DOMAIN "spumux"

#include <pngreader.h>

typedef struct
  {
  bgav_yml_node_t * yml;
  bgav_yml_node_t * cur;
  gavl_video_format_t format;
  int have_header;
  int need_header;
  } spumux_t;

static int probe_spumux(char * line, bgav_input_context_t * ctx)
  {
  if(!strncasecmp(line, "<subpictures>", 13))
    return 1;
  return 0;
  }

static int init_current_spumux(bgav_stream_t * s)
  {
  bgav_subtitle_reader_context_t * ctx;
  spumux_t * priv;
  ctx = s->data.subtitle.subreader;
  priv = ctx->priv;
  
  priv->cur = priv->yml->children;
  while(priv->cur && (!priv->cur->name || strcasecmp(priv->cur->name, "stream")))
    {
    priv->cur = priv->cur->next;
    }
  if(!priv->cur)
    return 0;
  priv->cur = priv->cur->children;
  while(priv->cur && (!priv->cur->name || strcasecmp(priv->cur->name, "spu")))
    {
    priv->cur = priv->cur->next;
    }
  if(!priv->cur)
    return 0;
  return 1;
  }

static int advance_current_spumux(bgav_stream_t * s)
  {
  bgav_subtitle_reader_context_t * ctx;
  spumux_t * priv;
  ctx = s->data.subtitle.subreader;
  priv = ctx->priv;

  priv->cur = priv->cur->next;
  while(priv->cur && (!priv->cur->name || strcasecmp(priv->cur->name, "spu")))
    {
    priv->cur = priv->cur->next;
    }
  if(!priv->cur)
    return 0;
  return 1;
  }

static gavl_time_t parse_time_spumux(const char * str,
                                     int timescale, int frame_duration)
  {
  int h, m, s, f;
  gavl_time_t ret;
  if(sscanf(str, "%d:%d:%d.%d", &h, &m, &s, &f) < 4)
    return GAVL_TIME_UNDEFINED;
  ret = h;
  ret *= 60;
  ret += m;
  ret *= 60;
  ret += s;
  ret *= GAVL_TIME_SCALE;
  if(f)
    ret += gavl_frames_to_time(timescale, frame_duration, f);
  return ret;
  }


static gavl_source_status_t read_spumux(bgav_stream_t * s, bgav_packet_t * p)
  {
  const char * filename;
  const char * start_time;
  const char * tmp;
  
  bgav_subtitle_reader_context_t * ctx;
  spumux_t * priv;
  ctx = s->data.subtitle.subreader;
  priv = ctx->priv;

  if(!priv->cur)
    return 0;

  //  bgav_yml_dump(priv->cur);

  start_time = bgav_yml_get_attribute_i(priv->cur, "start");
  if(!start_time)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "yml node has no start attribute");
    return 0;
    }
  
  if(!priv->have_header)
    {
    filename = bgav_yml_get_attribute_i(priv->cur, "image");
    if(!filename)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "yml node has no filename attribute");
      return 0;
      }
    if(!bgav_slurp_file(filename, p, s->opt))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Reading file %s failed", filename);
      return 0;
      }
    }
  
  p->pts =
    parse_time_spumux(start_time, s->data.subtitle.video.format->timescale,
                      s->data.subtitle.video.format->frame_duration);
  
  if(p->pts == GAVL_TIME_UNDEFINED)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Parsing time string %s failed", start_time);
    return 0;
    }
  tmp = bgav_yml_get_attribute_i(priv->cur, "end");
  if(tmp)
    {
    p->duration =
      parse_time_spumux(tmp,
                        s->data.subtitle.video.format->timescale,
                        s->data.subtitle.video.format->frame_duration);
    if(p->duration == GAVL_TIME_UNDEFINED)
      return 0;
    p->duration -= p->pts;
    }
  else
    {
    p->duration = -1;
    }
  
  tmp = bgav_yml_get_attribute_i(priv->cur, "xoffset");
  if(tmp)
    p->dst_x = atoi(tmp);
  else
    p->dst_x = 0;
  
  tmp = bgav_yml_get_attribute_i(priv->cur, "yoffset");
  if(tmp)
    p->dst_y = atoi(tmp);
  else
    p->dst_y = 0;
  
  p->src_rect.x = 0;
  p->src_rect.y = 0;
  
  /* Will be set by the parser */
  p->src_rect.w = 0;
  p->src_rect.h = 0;
  
  priv->have_header = 0;
  advance_current_spumux(s);
  
  return 1;
  }

static int init_spumux(bgav_stream_t * s)
  {
  const char * tmp;
  bgav_subtitle_reader_context_t * ctx;
  spumux_t * priv;
  bgav_stream_t * vs;

  ctx = s->data.subtitle.subreader;
  s->timescale = GAVL_TIME_SCALE;
  s->fourcc    = BGAV_MK_FOURCC('p', 'n', 'g', ' ');
  s->flags |= STREAM_PARSE_FRAME;
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
    
  priv->yml = bgav_yml_parse(ctx->input);
  if(!priv->yml)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Parsing spumux file failed");
    return 0;
    }
  if(!priv->yml->name || strcasecmp(priv->yml->name, "subpictures"))
    return 0;

  /* Get duration */
  if(!init_current_spumux(s))
    return 0;
  
  do{
    tmp = bgav_yml_get_attribute_i(priv->cur, "end");
    s->stats.pts_end = parse_time_spumux(tmp,
                                         s->data.subtitle.video.format->timescale,
                                         s->data.subtitle.video.format->frame_duration);
    } while(advance_current_spumux(s));
  
  if(!init_current_spumux(s))
    return 0;

  if((vs = bgav_track_get_video_stream(s->track, 0)))  
    gavl_video_format_copy(s->data.subtitle.video.format,
                           vs->data.video.format);
  
  s->data.subtitle.video.format->pixelformat = GAVL_PIXELFORMAT_NONE;
  s->data.subtitle.video.format->timescale   = GAVL_TIME_SCALE;
  
  return 1;
  }

static void seek_spumux(bgav_stream_t * s, int64_t time1, int scale)
  {
  const char * start_time, * end_time;
  bgav_subtitle_reader_context_t * ctx;
  spumux_t * priv;
  gavl_time_t start, end = 0;

  gavl_time_t time = gavl_time_unscale(scale, time1);
  
  ctx = s->data.subtitle.subreader;
  priv = ctx->priv;
  init_current_spumux(s);
  
  while(1)
    {
    start_time = bgav_yml_get_attribute_i(priv->cur, "start");
    if(!start_time)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "yml node has no start attribute");
      return;
      }
    end_time = bgav_yml_get_attribute_i(priv->cur, "end");

    start = parse_time_spumux(start_time,
                              s->data.subtitle.video.format->timescale,
                              s->data.subtitle.video.format->frame_duration);
    if(start == GAVL_TIME_UNDEFINED)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Error parsing start attribute");
      return;
      }
    
    if(end_time)
      end = parse_time_spumux(end_time,
                              s->data.subtitle.video.format->timescale,
                              s->data.subtitle.video.format->frame_duration);
    
    if(end == GAVL_TIME_UNDEFINED)
      {
      if(end > time)
        break;
      }
    else
      {
      if(start > time)
        break;
      }
    advance_current_spumux(s);
    }
  priv->have_header = 0;
  }

static void close_spumux(bgav_stream_t * s)
  {
  bgav_subtitle_reader_context_t * ctx;
  spumux_t * priv;
  ctx = s->data.subtitle.subreader;
  priv = ctx->priv;

  if(priv->yml)
    bgav_yml_free(priv->yml);
  free(priv);
  }

#undef LOG_DOMAIN

#endif // HAVE_LIBPNG


static const bgav_subtitle_reader_t subtitle_readers[] =
  {
    {
      .type = GAVL_STREAM_TEXT,
      .name = "Subrip (srt)",
      .init =               init_srt,
      .probe =              probe_srt,
      .read_packet =        read_srt,
    },
    {
      .type = GAVL_STREAM_TEXT,
      .name = "Mplayer mpsub",
      .init =               init_mpsub,
      .probe =              probe_mpsub,
      .read_packet =        read_mpsub,
      .close =              close_mpsub,
    },
    {
      .type = GAVL_STREAM_OVERLAY,
      .name = "vobsub",
      .setup_stream =       setup_stream_vobsub,
      .init =               init_vobsub,
      .probe =              probe_vobsub,
      .read_packet =        read_vobsub,
      .seek =               seek_vobsub,
      .close =              close_vobsub,
    },

#ifdef HAVE_LIBPNG
    {
      .type = GAVL_STREAM_OVERLAY,
      .name = "Spumux (xml/png)",
      .probe =                 probe_spumux,
      .init =                  init_spumux,
      .read_packet =           read_spumux,
      .seek =                  seek_spumux,
      .close =                 close_spumux,
    },
#endif // HAVE_LIBPNG
    { /* End of subtitle readers */ }
    
  };

void bgav_subreaders_dump()
  {
  int i = 0;
  bgav_dprintf( "<h2>Subtitle readers</h2>\n<ul>\n");
  while(subtitle_readers[i].name)
    {
    bgav_dprintf( "<li>%s\n", subtitle_readers[i].name);
    i++;
    }
  bgav_dprintf( "</ul>\n");
  }

static char const * const extensions[] =
  {
    "srt",
    "sub",
    "xml",
    "idx",
    NULL
  };

static const bgav_subtitle_reader_t *
find_subtitle_reader(const char * filename,
                     const bgav_options_t * opt,
                     char ** charset, int * num_streams)
  {
  int i;
  bgav_input_context_t * input;
  const char * extension;
  const bgav_subtitle_reader_t* ret = NULL;
  
  char * line = NULL;
  uint32_t line_alloc = 0;
  uint32_t line_len;

  *num_streams = 0;
  
  /* 1. Check if we have a supported extension */
  extension = strrchr(filename, '.');
  if(!extension)
    return NULL;

  extension++;
  i = 0;
  while(extensions[i])
    {
    if(!strcasecmp(extension, extensions[i]))
      break;
    i++;
    }
  if(!extensions[i])
    return NULL;

  /* 2. Open the file and do autodetection */
  input = bgav_input_create(NULL, opt);
  if(!bgav_input_open(input, filename))
    {
    bgav_input_destroy(input);
    return NULL;
    }

  bgav_input_detect_charset(input);
  while(bgav_input_read_convert_line(input, &line, &line_alloc, &line_len))
    {
    i = 0;
    
    while(subtitle_readers[i].name)
      {
      if((*num_streams = subtitle_readers[i].probe(line, input)))
        {
        ret = &subtitle_readers[i];
        break;
        }
      i++;
      }
    if(ret)
      break;
    }

  if(ret && input->charset)
    *charset = gavl_strdup(input->charset);
  else
    *charset = NULL;
  
  bgav_input_destroy(input);
  free(line);
  
  return ret;
  }

extern bgav_input_t bgav_input_file;


bgav_subtitle_reader_context_t *
bgav_subtitle_reader_open(bgav_input_context_t * input_ctx)
  {
  char * pattern, * pos, * name;
  char * charset;
  const bgav_subtitle_reader_t * r;
  bgav_subtitle_reader_context_t * ret = NULL;
  bgav_subtitle_reader_context_t * end = NULL;
  bgav_subtitle_reader_context_t * new;
  glob_t glob_buf;
  int i, j, num_streams;
  int base_len;
  int result;
  
  /* Check if input is a regular file */
  if((input_ctx->input != &bgav_input_file) || !input_ctx->filename)
    return NULL;
  
  pattern = gavl_strdup(input_ctx->filename);
  pos = strrchr(pattern, '.');
  if(!pos)
    {
    free(pattern);
    return NULL;
    }

  base_len = pos - pattern;
  
  pos[0] = '*';
  pos[1] = '\0';

  //  fprintf(stderr, "Unescaped pattern: %s\n", pattern);

  pattern = gavl_escape_string(pattern, "[]?");

  //  fprintf(stderr, "Escaped pattern: %s\n", pattern);
  
  memset(&glob_buf, 0, sizeof(glob_buf));

  /* Pathnames are sorted to ensure reproducible orders */
  if((result = glob(pattern, 0,
                    glob_errfunc,
                    // NULL,
                    &glob_buf)))
    {
    // fprintf(stderr, "glob returned %d\n", result);
    return NULL;
    }
  free(pattern);
  
  for(i = 0; i < glob_buf.gl_pathc; i++)
    {
    if(!strcmp(glob_buf.gl_pathv[i], input_ctx->filename))
      continue;
    //    fprintf(stderr, "Found %s\n", glob_buf.gl_pathv[i]);

    r = find_subtitle_reader(glob_buf.gl_pathv[i],
                             input_ctx->opt, &charset, &num_streams);
    if(!r)
      continue;
    
    for(j = 0; j < num_streams; j++)
      {
      new = calloc(1, sizeof(*new));
      new->filename = gavl_strdup(glob_buf.gl_pathv[i]);
      new->input    = bgav_input_create(NULL, input_ctx->opt);
      new->reader   = r;
      new->charset  = gavl_strdup(charset);
      new->stream   = j;
      
      name = glob_buf.gl_pathv[i] + base_len;
    
      while(!isalnum(*name) && (*name != '\0'))
        name++;

      if(*name != '\0')
        {
        pos = strrchr(name, '.');
        if(pos)
          new->info = gavl_strndup(name, pos);
        else
          new->info = gavl_strdup(name);
        }
    
      /* Append to list */
      if(!ret)
        {
        ret = new;
        end = ret;
        }
      else
        {
        end->next = new;
        end = end->next;
        }

      }
    
    free(charset);
    }
  globfree(&glob_buf);
  return ret;
  }

void bgav_subtitle_reader_stop(bgav_stream_t * s)
  {
  bgav_subtitle_reader_context_t * ctx;
  ctx = s->data.subtitle.subreader;

  if(ctx->reader->close)
    ctx->reader->close(s);
  
  if(ctx->input)
    bgav_input_close(ctx->input);
  }

void bgav_subtitle_reader_destroy(bgav_stream_t * s)
  {
  bgav_subtitle_reader_context_t * ctx;
  ctx = s->data.subtitle.subreader;
  if(ctx->info)
    free(ctx->info);
  if(ctx->filename)
    free(ctx->filename);
  if(ctx->charset)
    free(ctx->charset);
  if(ctx->line)
    free(ctx->line);
  if(ctx->input)
    bgav_input_destroy(ctx->input);
  free(ctx);
  }

int bgav_subtitle_reader_start(bgav_stream_t * s)
  {
  bgav_subtitle_reader_context_t * ctx;
  ctx = s->data.subtitle.subreader;
  ctx->s = s;
  if(!bgav_input_open(ctx->input, ctx->filename))
    return 0;

  bgav_input_detect_charset(ctx->input);
  
  if(ctx->reader->init && !ctx->reader->init(s))
    return 0;

  /* Timescale is valid just now */
  gavl_dictionary_set_int(s->m, GAVL_META_STREAM_PACKET_TIMESCALE, s->timescale);
  gavl_dictionary_set_int(s->m, GAVL_META_STREAM_SAMPLE_TIMESCALE, s->timescale);
  
  return 1;
  }

void bgav_subtitle_reader_seek(bgav_stream_t * s,
                               int64_t time1, int scale)
  {
  bgav_subtitle_reader_context_t * ctx;
  int64_t time = gavl_time_rescale(scale, s->timescale, time1);
  
  ctx = s->data.subtitle.subreader;

  if(ctx->reader->seek)
    ctx->reader->seek(s, time, scale);
  
  else if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    {
    bgav_input_seek(ctx->input, ctx->data_start, SEEK_SET);
    
    ctx->time_offset = 0;

    if(!ctx->out_packet)
      ctx->out_packet = bgav_packet_pool_get(s->pp);
      
    while(ctx->reader->read_packet(s, ctx->out_packet))
      {
      if(ctx->out_packet->pts + ctx->out_packet->duration < time)
        continue;
      else
        break;
      }
    }
  }

/* Generic functions */

gavl_source_status_t
bgav_subtitle_reader_read_packet(void * subreader,
                                 bgav_packet_t ** p)
  {
  bgav_subtitle_reader_context_t * ctx = subreader;

  bgav_packet_t * ret;
  if(ctx->out_packet)
    {
    *p = ctx->out_packet;
    ctx->out_packet = NULL;
    return GAVL_SOURCE_OK;
    }
  ret = bgav_packet_pool_get(ctx->s->pp);
  
  if(ctx->reader->read_packet(ctx->s, ret))
    {
    *p = ret;
    return GAVL_SOURCE_OK;
    }
  else
    {
    bgav_packet_pool_put(ctx->s->pp, ret);
    return  GAVL_SOURCE_EOF;
    }
  
  }

gavl_source_status_t
bgav_subtitle_reader_peek_packet(void * subreader,
                                 bgav_packet_t ** p, int force)
  {
  bgav_subtitle_reader_context_t * ctx = subreader;

  if(!ctx->out_packet)
    {
    ctx->out_packet = bgav_packet_pool_get(ctx->s->pp);
    
    if(!ctx->reader->read_packet(ctx->s, ctx->out_packet))
      {
      bgav_packet_pool_put(ctx->s->pp, ctx->out_packet);
      ctx->out_packet = NULL;
      return GAVL_SOURCE_EOF;
      }
    }

  if(p)
    *p = ctx->out_packet;
  
  return GAVL_SOURCE_OK;
  }
