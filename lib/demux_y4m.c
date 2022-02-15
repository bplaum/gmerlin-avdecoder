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

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

// #include <yuv4mpeg.h>

#define LOG_DOMAIN "demux_y4m"

typedef struct
  {
  
  uint8_t * tmp_planes[4]; /* For YUVA4444 */
  int64_t pts;

  uint32_t line_alloc;
  char * line;
  
  int buf_size;
  } y4m_t;

static int probe_y4m(bgav_input_context_t * input)
  {
  uint8_t probe_data[9];

  if(bgav_input_get_data(input, probe_data, 9) < 9)
    return 0;

  if(!strncmp((char*)probe_data, "YUV4MPEG2", 9))
    return 1;
  
  return 0;
  }

#if 0

/* For debugging only (incomplete) */
static void dump_stream_header(y4m_stream_info_t * si)
  {
  bgav_dprintf( "YUV4MPEG2 stream header\n");
  bgav_dprintf( "  Image size: %dx%d\n", y4m_si_get_width(si), y4m_si_get_height(si));
  bgav_dprintf( "  Interlace mode: %d\n", y4m_si_get_interlace(si));
  bgav_dprintf( "  Chroma: %d\n", y4m_si_get_chroma(si));
  }
#endif

static const char * next_tag(const char * old)
  {
  while(!isspace(*old) && (*old != '\0'))
    old++;
  if(*old == '\0')
    return NULL;

  while(isspace(*old) && (*old != '\0'))
    old++;

  if(*old == '\0')
    return NULL;
  
  return old;
  }

static int open_y4m(bgav_demuxer_context_t * ctx)
  {
  y4m_t * priv;
  bgav_stream_t * s;
  int ext_len, w, h;
  const char * pos;
  const char * end;
  const char * format = NULL;
  
  /* Allocate private data */
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
  
  /* Read the stream header */

  if(!bgav_input_read_line(ctx->input,
                           &priv->line, &priv->line_alloc,
                           0, NULL))
    return 0;
  
  /* Create track table */

  ctx->tt = bgav_track_table_create(1);
  
  /* Set up the stream */
  s = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);

  pos = next_tag(priv->line);

  while(1)
    {
    switch(*pos)
      {
      case 'W':
        s->data.video.format->image_width = atoi(pos+1);
        s->data.video.format->frame_width  = s->data.video.format->image_width;
        break;
      case 'H':
        s->data.video.format->image_height = atoi(pos+1);
        s->data.video.format->frame_height  = s->data.video.format->image_height;
        break;
      case 'C':
        format = pos+1;
        break;
      case 'I':
        switch(pos[1])
          {
          case 'p':
          case '?':
            s->data.video.format->interlace_mode = GAVL_INTERLACE_NONE;
            break;
          case 't':
            s->data.video.format->interlace_mode = GAVL_INTERLACE_TOP_FIRST;
            break;
          case 'b':
            s->data.video.format->interlace_mode = GAVL_INTERLACE_BOTTOM_FIRST;
            break;
          case 'm':
            s->data.video.format->interlace_mode = GAVL_INTERLACE_MIXED;
            break;
          }
        break;
      case 'F':
        if(sscanf(pos+1, "%d:%d",
                  &s->data.video.format->timescale,
                  &s->data.video.format->frame_duration) < 2)
          return 0;
        break;
      case 'A':
        if(sscanf(pos+1, "%d:%d",
                  &s->data.video.format->pixel_width,
                  &s->data.video.format->pixel_height) < 2)
          return 0;
        break;
      default:
        break;
      }
    pos = next_tag(pos);
    if(!pos)
      break;
    }

  /* Set default values */
  if(!s->data.video.format->timescale ||
     !s->data.video.format->frame_duration)
    {
    s->data.video.format->timescale = 25; // Completely random
    s->data.video.format->frame_duration = 1;
    }
  
  if(!s->data.video.format->pixel_width ||
     !s->data.video.format->pixel_height)
    {
    s->data.video.format->pixel_width = 1;
    s->data.video.format->pixel_height = 1;
    }
  
  if(s->data.video.format->interlace_mode == GAVL_INTERLACE_MIXED)
    {
    s->data.video.format->interlace_mode = GAVL_INTERLACE_MIXED;
    s->data.video.format->timescale *= 2;
    s->data.video.format->frame_duration *= 2;
    s->data.video.format->framerate_mode = GAVL_FRAMERATE_VARIABLE;
    }
  
  s->ci->flags &= ~GAVL_COMPRESSION_HAS_B_FRAMES;
  
  if(!format)
    format = "420jpeg";

  end = strchr(format, ' ');
  if(end)
    ext_len = end - format;
  else
    ext_len = strlen(format);

  bgav_stream_set_extradata(s, (const uint8_t*)format, ext_len);
  
  w = s->data.video.format->image_width;
  h = s->data.video.format->image_height;
  
  if(!strncmp(format, "420", 3))
    priv->buf_size = w * h + (w/2) * (h/2) + (w/2) * (h/2);

  else if(!strcmp(format, "411"))
    priv->buf_size = w * h + (w/4) *  h + (w/4) *  h;

  else if(!strcmp(format, "422"))
    priv->buf_size = w * h + (w/2) *  h + (w/2) *  h;

  else if(!strcmp(format, "444"))
    priv->buf_size = w * h +  w * h +  w * h;

  else if(!strcmp(format, "444alpha"))
    priv->buf_size = w * h +  w * h +  w * h +  w * h;

  else if(!strcmp(format, "mono"))
    priv->buf_size = w * h;
  
  s->fourcc = BGAV_MK_FOURCC('y','4','m',' ');

  bgav_track_set_format(ctx->tt->cur, "yuv4mpeg", NULL);
  
  ctx->index_mode = INDEX_MODE_SIMPLE;
  return 1;
  }

static int select_track_y4m(bgav_demuxer_context_t * ctx, int track)
  {
  y4m_t * priv;
  priv = ctx->priv;
  priv->pts = 0;
  return 1;
  }


static int next_packet_y4m(bgav_demuxer_context_t * ctx)
  {
  bgav_packet_t * p;
  bgav_stream_t * s;
  y4m_t * priv;
  const char * pos;
  
  priv = ctx->priv;
  
  s = bgav_track_get_video_stream(ctx->tt->cur, 0);
  
  p = bgav_stream_get_packet_write(s);
  p->position = ctx->input->position;

  if(!bgav_input_read_line(ctx->input,
                           &priv->line, &priv->line_alloc,
                           0, NULL))
    return 0;

  if(strncmp(priv->line, "FRAME", 5))
    return 0;
  
  bgav_packet_alloc(p, priv->buf_size);

  if(bgav_input_read_data(ctx->input, p->data, priv->buf_size) < priv->buf_size)
    return 0;

  p->data_size = priv->buf_size;
  
  p->pts = priv->pts;
  
  PACKET_SET_KEYFRAME(p);
  p->duration = s->data.video.format->frame_duration;
  
  pos = next_tag(priv->line);
  while(pos)
    {
    switch(pos[0])
      {
      case 'I':
        switch(pos[1])
          {
          case 't':
            p->ilace = GAVL_INTERLACE_TOP_FIRST;
            p->duration = s->data.video.format->frame_duration;
            break;
          case 'T':
            p->ilace = GAVL_INTERLACE_TOP_FIRST;
            p->duration = (s->data.video.format->frame_duration*3)/2;
            break;
          case 'b':
            p->ilace = GAVL_INTERLACE_BOTTOM_FIRST;
            p->duration = s->data.video.format->frame_duration;
            break;
          case 'B':
            p->ilace = GAVL_INTERLACE_BOTTOM_FIRST;
            p->duration = (s->data.video.format->frame_duration*3)/2;
            break;
          case '1':
            p->ilace = GAVL_INTERLACE_NONE;
            p->duration = s->data.video.format->frame_duration;
            break;
          case '2':
            p->ilace = GAVL_INTERLACE_NONE;
            p->duration = 2 * s->data.video.format->frame_duration;
            break;
          case '3':
            p->ilace = GAVL_INTERLACE_NONE;
            p->duration = 3 * s->data.video.format->frame_duration;
            break;
          }
        break;
      default:
        break;
      }
    pos = next_tag(priv->line);
    }
  
  priv->pts += p->duration;
  bgav_stream_done_packet_write(s, p);
  return 1;
  }

static void resync_y4m(bgav_demuxer_context_t * ctx, bgav_stream_t * s)
  {
  y4m_t * priv;
  priv = ctx->priv;
  priv->pts = STREAM_GET_SYNC(s);
  }

static void close_y4m(bgav_demuxer_context_t * ctx)
  {
  y4m_t * priv;
  priv = ctx->priv;

  if(priv->line)
    free(priv->line);
  
  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_y4m =
  {
    .probe        = probe_y4m,
    .open         = open_y4m,
    .select_track = select_track_y4m,
    .next_packet = next_packet_y4m,
    .resync      = resync_y4m,
    .close =       close_y4m
  };
