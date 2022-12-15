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
#include <stdlib.h>

#include <config.h>

#include <avdec_private.h>
#include <gavl/log.h>
#define LOG_DOMAIN "srt"

#define PROBE_LEN 64

#define STREAM_ID 1

typedef struct
  {
  int64_t time_offset;

  /* Timestamps are scaled with out_pts = pts * scale_num / scale_den */
  int scale_num;
  int scale_den;

  gavl_buffer_t line_buf;

  } srt_t;

static int probe_srt(bgav_input_context_t * input)
  {
  int i;
  int a1,a2,a3,a4,b1,b2,b3,b4;
  //  00:01:30,110 --> 00:01:36,352
  uint8_t test_data[PROBE_LEN];

  if(bgav_input_get_data(input, test_data, PROBE_LEN) < PROBE_LEN)
    return 0;

  if(memchr(test_data, '\n', PROBE_LEN) &&
     (gavl_string_starts_with((char*)test_data, "@OFF=") ||
      gavl_string_starts_with((char*)test_data, "@SCALE=") ||
      (sscanf((char*)test_data,
              "%d:%d:%d%[,.:]%d --> %d:%d:%d%[,.:]%d",
              &a1,&a2,&a3,(char *)&i,&a4,
              &b1,&b2,&b3,(char *)&i,&b4) == 10)))
    return 1;
  else
    return 0;
  }

static gavl_source_status_t next_packet_srt(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;
  
  int lines_read;
  int a1,a2,a3,a4,b1,b2,b3,b4;
  int i,len;
  srt_t * srt;
  gavl_time_t start, end;
  char * str;

  bgav_packet_t * p;
  
  srt = ctx->priv;

  s = bgav_track_find_stream(ctx, STREAM_ID);
  
  /* Read lines */
  while(1)
    {
    if(!bgav_input_read_convert_line(ctx->input, &srt->line_buf))
      return GAVL_SOURCE_EOF;
    str = (char*)srt->line_buf.buf;
    // fprintf(stderr, "Line: %s (%c)\n", srt->line, srt->line[0]);
    
    if(str[0] == '@')
      {
      if(!strncasecmp(str, "@OFF=", 5))
        {
        srt->time_offset += (int)(atof(str+5) * 1000);
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
                 "new time offset: %"PRId64, srt->time_offset);
        }
      else if(!strncasecmp(str, "@SCALE=", 7))
        {
        sscanf(str + 7, "%d:%d", &srt->scale_num, &srt->scale_den);
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
                 "new scale factor: %d:%d", srt->scale_num, srt->scale_den);
        

        //        fprintf(stderr, "new scale factor: %d:%d\n",
        //                srt->scale_num, srt->scale_den);
        }
      continue;
      }
    else if((len=sscanf (str,
                         "%d:%d:%d%[,.:]%d --> %d:%d:%d%[,.:]%d",
                         &a1,&a2,&a3,(char *)&i,&a4,
                         &b1,&b2,&b3,(char *)&i,&b4)) == 10)
      {
      break;
      }
    }

  //  p = 
  
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

  p = bgav_stream_get_packet_write(s);
  
  p->pts = start + srt->time_offset;
  p->duration = end - start;

  p->pts = gavl_time_rescale(srt->scale_den,
                             srt->scale_num,
                             p->pts);

  p->duration = gavl_time_rescale(srt->scale_den,
                                  srt->scale_num,
                                  p->duration);
  
  p->buf.len = 0;
  
  /* Read lines until we are done */

  lines_read = 0;
  while(1)
    {
    if(!bgav_input_read_convert_line(ctx->input, &srt->line_buf))
      {
      srt->line_buf.len = 0;
      if(!lines_read)
        return GAVL_SOURCE_EOF;
      }
    
    if(!srt->line_buf.len)
      {
      /* Zero terminate */
      if(lines_read)
        {
        p->buf.buf[p->buf.len] = '\0';
        // Terminator doesn't count for data size
        // p->data_size++;
        }
      return GAVL_SOURCE_OK;
      }
    if(lines_read)
      {
      p->buf.buf[p->buf.len] = '\n';
      p->buf.len++;
      }
    
    lines_read++;
    bgav_packet_alloc(p, p->buf.len + srt->line_buf.len + 2);
    gavl_buffer_append(&p->buf, &srt->line_buf);
    }

  bgav_stream_done_packet_write(s, p);
  
  return GAVL_SOURCE_OK;
  }

static int open_srt(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;

  bgav_input_detect_charset(ctx->input);
  
  ctx->tt = bgav_track_table_create(1);
  s = bgav_track_add_text_stream(ctx->tt->cur, ctx->opt, ctx->input->charset);
  s->stream_id = STREAM_ID;
  
  return 1;
  }

static void close_srt(bgav_demuxer_context_t * ctx)
  {
  
  }
  
const bgav_demuxer_t bgav_demuxer_srt =
  {
    .probe =       probe_srt,
    .open =        open_srt,
    .next_packet = next_packet_srt,
    .close =       close_srt
  };
