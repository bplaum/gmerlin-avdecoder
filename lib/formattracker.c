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

#include <stdlib.h>
#include <avdec_private.h>

#define TRACK_INTERLACING (1<<0)
#define TRACK_FRAMERATE   (1<<1)

struct bgav_video_format_tracker_s
  {
  bgav_stream_t * s;
  int do_track;
  
  bgav_packet_source_t src;

  bgav_packet_t * output_packet;
  int eof;
  
  gavl_framerate_mode_t fps_mode;
  int last_frame_duration;

  /* Interlacing flags */
  int have_top;
  int have_bottom;
  int have_progressive;
  };

static void set_eof(bgav_video_format_tracker_t * ft)
  {
  if(ft->eof)
    return;

  if(ft->do_track & TRACK_FRAMERATE)
    {
    ft->s->data.video.format->framerate_mode = ft->fps_mode;
    if(ft->fps_mode == GAVL_FRAMERATE_CONSTANT)
      ft->s->data.video.format->frame_duration = ft->last_frame_duration;
    }
  
  if(ft->do_track & TRACK_INTERLACING)
    {
    if(ft->have_progressive)
      {
      if(ft->have_top)
        {
        if(ft->have_bottom)
          {
          /* progressive + top + bottom  */
          ft->s->data.video.format->interlace_mode = GAVL_INTERLACE_MIXED;
          }
        else
          {
          /* progressive + top + !bottom  */
          ft->s->data.video.format->interlace_mode = GAVL_INTERLACE_MIXED_TOP;
          }
        }
      else /* !ft->have_top */
        {
        if(ft->have_bottom)
          {
          /* progressive + !top + bottom  */
          ft->s->data.video.format->interlace_mode = GAVL_INTERLACE_MIXED_BOTTOM;
          }
        else
          {
          /* progressive + !top + !bottom  */
          ft->s->data.video.format->interlace_mode = GAVL_INTERLACE_NONE;
          }
        }
      }
    else
      {
      if(ft->have_top)
        {
        if(ft->have_bottom)
          {
          /* !progressive + top + bottom  */
          ft->s->data.video.format->interlace_mode = GAVL_INTERLACE_MIXED;
          }
        else
          {
          /* !progressive + top + !bottom  */
          ft->s->data.video.format->interlace_mode = GAVL_INTERLACE_TOP_FIRST;
          }
        }
      else /* !ft->have_top */
        {
        if(ft->have_bottom)
          {
          /* !progressive + !top + bottom  */
          ft->s->data.video.format->interlace_mode = GAVL_INTERLACE_BOTTOM_FIRST;
          }
        else
          {
          /* !progressive + !top + !bottom  */
          ft->s->data.video.format->interlace_mode = GAVL_INTERLACE_MIXED;
          }
        }
      }
    }
  ft->do_track = 0;
  ft->eof = 1;
  }

static gavl_source_status_t
peek_func(void * ft1, bgav_packet_t ** ret, int force)
  {
  gavl_source_status_t st;
  bgav_video_format_tracker_t * ft = ft1;
  
  st = ft->src.peek_func(ft->src.data, ret, force);
  
  if(st == GAVL_SOURCE_EOF)
    set_eof(ft);
  return st;
  }

static gavl_source_status_t
get_func(void * ft1, bgav_packet_t ** ret)
  {
  gavl_source_status_t st;
  bgav_video_format_tracker_t * ft = ft1;

  st = ft->src.get_func(ft->src.data, ret);

  if(st == GAVL_SOURCE_EOF)
    {
    set_eof(ft);
    return st;
    }

  if(st != GAVL_SOURCE_OK)
    return st;
  
  /* Check what we have */
  
  if(ft->do_track & TRACK_FRAMERATE)
    {
    if((ft->last_frame_duration >= 0) &&
       ((*ret)->duration != ft->last_frame_duration))
      ft->fps_mode = GAVL_FRAMERATE_VARIABLE;
    ft->last_frame_duration = (*ret)->duration;
    }
  
  if(ft->do_track & TRACK_INTERLACING)
    {
    switch((*ret)->interlace_mode)
      {
      case GAVL_INTERLACE_NONE:
        ft->have_progressive = 1;
        break;
      case GAVL_INTERLACE_TOP_FIRST:
        ft->have_top = 1;
        break;
      case GAVL_INTERLACE_BOTTOM_FIRST:
        ft->have_bottom = 1;
        break;
      default:
        break;
      }
    }
  return GAVL_SOURCE_OK;
  }

bgav_video_format_tracker_t *
bgav_video_format_tracker_create(bgav_stream_t * s)
  {
  bgav_video_format_tracker_t * ret;
  ret = calloc(1, sizeof(*ret));

  ret->s = s;

  bgav_packet_source_copy(&ret->src, &s->src);

  s->src.get_func = get_func;
  s->src.peek_func = peek_func;
  s->src.data = ret;
  
  switch(s->data.video.format->interlace_mode)
    {
    case GAVL_INTERLACE_MIXED:        /*!< Use interlace_mode of the frames */
    case GAVL_INTERLACE_MIXED_TOP:    /*!< Progressive + top    */
    case GAVL_INTERLACE_MIXED_BOTTOM: /*!< Progressive + bottom */
      ret->do_track |= TRACK_INTERLACING;
      break;
    default:
      break;
    }

  if(s->data.video.format->framerate_mode == GAVL_FRAMERATE_VARIABLE)
    {
    ret->do_track |= TRACK_FRAMERATE;
    ret->last_frame_duration = -1;
    ret->fps_mode = GAVL_FRAMERATE_CONSTANT;
    }

  

  return ret;
  }

void bgav_video_format_tracker_destroy(bgav_video_format_tracker_t *ft)
  {
  free(ft);
  }
