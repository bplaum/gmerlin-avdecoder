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
#include <stdio.h>
#include <string.h>

#include <avdec_private.h>
#include <parser.h>
#include <bsf.h>
#include <mpeg4_header.h>

// #define DUMP_TIMESTAMPS

#define LOG_DOMAIN "video"

int bgav_num_video_streams(bgav_t *  bgav, int track)
  {
  return bgav->tt->tracks[track]->num_video_streams;
  }

const gavl_video_format_t * bgav_get_video_format(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  
  return s->data.video.format;
  }

const gavl_video_format_t * bgav_get_video_format_t(bgav_t * bgav, int t, int stream)
  {
  bgav_stream_t * s = bgav_track_get_video_stream(bgav->tt->tracks[t], stream);

  return s->data.video.format;
  }

const bgav_metadata_t *
bgav_get_video_metadata(bgav_t * b, int stream)
  {
  bgav_stream_t * s = bgav_track_get_video_stream(b->tt->cur, stream);
  return s->m;
  }

const bgav_metadata_t *
bgav_get_video_metadata_t(bgav_t * b, int t, int stream)
  {
  bgav_stream_t * s = bgav_track_get_video_stream(b->tt->tracks[t], stream);
  return s->m;
  }

int bgav_set_video_stream(bgav_t * b, int stream, bgav_stream_action_t action)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_video_stream(b->tt->cur, stream)))
    return 0;
  s->action = action;
  return 1;
  }

static int check_still(bgav_stream_t * s)
  {
  if(!STREAM_IS_STILL(s))
    return 1;
  if(s->flags & STREAM_HAVE_FRAME)
    return 1;
  if(bgav_stream_peek_packet_read(s, NULL, 0) == GAVL_SOURCE_OK)
    return 1;
  else if(s->flags & STREAM_EOF_D)
    return 1;
  
  return 0;
  
  }

int bgav_video_has_still(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s;
  s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  return check_still(s);
  }

static gavl_source_status_t
read_video_nocopy(void * sp,
                  gavl_video_frame_t ** frame)
  {
  gavl_source_status_t st;
  bgav_stream_t * s = sp;
  //  fprintf(stderr, "Read video nocopy\n");
  if(!check_still(s))
    return GAVL_SOURCE_AGAIN;
  if((st = s->data.video.decoder->decode(sp, NULL)) != GAVL_SOURCE_OK)
    {
    // fprintf(stderr, "EOF :)\n");
    if(st == GAVL_SOURCE_EOF)
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Detected EOF 1");
    return st;
    }
  if(frame)
    *frame = s->vframe;
#ifdef DUMP_TIMESTAMPS
  bgav_dprintf("Video timestamp: %"PRId64"\n", s->vframe->timestamp);
#endif    
  s->out_time = s->vframe->timestamp + s->vframe->duration;
  s->flags &= ~STREAM_HAVE_FRAME;
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t read_video_copy(void * sp,
                                            gavl_video_frame_t ** frame)
  {
  gavl_source_status_t st;
  bgav_stream_t * s = sp;
  if(!check_still(s))
    return GAVL_SOURCE_AGAIN;
  
  if(frame)
    {
    if((st = s->data.video.decoder->decode(sp, *frame)) != GAVL_SOURCE_OK)
      {
      if(st == GAVL_SOURCE_EOF)
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Detected EOF 2");
      return st;
      }
    s->out_time = (*frame)->timestamp + (*frame)->duration;
    }
  else
    {
    if((st = s->data.video.decoder->decode(sp, NULL)) != GAVL_SOURCE_OK)
      {
      if(st == GAVL_SOURCE_EOF)
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Detected EOF 3");
      return st;
      }
    }
#ifdef DUMP_TIMESTAMPS
  if(frame)
    bgav_dprintf("Video timestamp: %"PRId64"\n", (*frame)->timestamp);
#endif    
  s->flags &= ~STREAM_HAVE_FRAME;
  return GAVL_SOURCE_OK;
  }

int bgav_video_start(bgav_stream_t * s)
  {
  int result;
  int src_flags;
  bgav_video_decoder_t * dec;

  if(!s->timescale && s->data.video.format->timescale)
    s->timescale = s->data.video.format->timescale;

  //  if(s->fourcc == BGAV_MK_FOURCC('a', 'v', 'c', '1'))
  //    s->flags |= STREAM_FILTER_PACKETS;
  
  /* Some streams need to be parsed generically for extracting
     format values and/or timecodes */
  
  
  if(!(s->flags & STREAM_STANDALONE))
    {
    if(bgav_check_fourcc(s->fourcc, bgav_dv_fourccs) ||
       bgav_check_fourcc(s->fourcc, bgav_png_fourccs) ||
       (s->fourcc == BGAV_MK_FOURCC('a', 'v', 'c', '1')) ||
       (s->fourcc == BGAV_MK_FOURCC('V', 'P', '8', '0')) ||
       (s->fourcc == BGAV_MK_FOURCC('V', 'P', '9', '0')))
      s->flags |= STREAM_PARSE_FRAME;
    }
  
  if((s->flags & (STREAM_PARSE_FULL|STREAM_PARSE_FRAME)) &&
     !s->data.video.parser)
    {
    s->data.video.parser = bgav_video_parser_create(s);
    if(!s->data.video.parser)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "No video parser found for fourcc %c%c%c%c (0x%08x)",
               (s->fourcc & 0xFF000000) >> 24,
               (s->fourcc & 0x00FF0000) >> 16,
               (s->fourcc & 0x0000FF00) >> 8,
               (s->fourcc & 0x000000FF),
               s->fourcc);
      return 0;
      }
    
    /* Get the first packet to garantuee that the parser is fully initialized */
    if(bgav_stream_peek_packet_read(s, NULL, 1) != GAVL_SOURCE_OK)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "EOF while initializing video parser");
      return 0;
      }
    s->index_mode = INDEX_MODE_SIMPLE;
    }
  /* Frametype detector */
  if((s->flags & STREAM_NEED_FRAMETYPES) && !s->fd)
    {
    s->fd = bgav_frametype_detector_create(s);
    if(bgav_stream_peek_packet_read(s, NULL, 1) != GAVL_SOURCE_OK)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "EOF while initializing frametype detector");
      return 0;
      }
    }
  
  /* Packet timer */
  if((s->flags & (STREAM_NO_DURATIONS|STREAM_DTS_ONLY)) &&
     !s->pt)
    {
    s->pt = bgav_packet_timer_create(s);

    if(!bgav_stream_peek_packet_read(s, NULL, 1))
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "EOF while initializing packet timer");
      return 0;
      }
    s->index_mode = INDEX_MODE_SIMPLE;
    }

  //  if((s->action == BGAV_STREAM_READRAW) &&
  //     (s->flags & STREAM_FILTER_PACKETS))
  if(s->flags & STREAM_FILTER_PACKETS)
    {
    s->bsf = bgav_bsf_create(s);
    if(bgav_stream_peek_packet_read(s, NULL, 1) != GAVL_SOURCE_OK)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "EOF while initializing bitstream filter");
      return 0;
      }
    }
  
  if(s->stats.pts_start == GAVL_TIME_UNDEFINED)
    {
    bgav_packet_t * p = NULL;
    char tmp_string[128];
    
    if(bgav_stream_peek_packet_read(s, &p, 1) != GAVL_SOURCE_OK)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "EOF while getting start time");
      }
    s->stats.pts_start = p->pts;
    s->out_time = s->stats.pts_start;

    sprintf(tmp_string, "%" PRId64, s->out_time);
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
             "Got initial video timestamp: %s",
             tmp_string);
    }
  
  if((s->action == BGAV_STREAM_PARSE) &&
     ((s->data.video.format->framerate_mode == GAVL_FRAMERATE_VARIABLE) ||
      (s->data.video.format->interlace_mode == GAVL_INTERLACE_MIXED)))
    {
    s->data.video.ft = bgav_video_format_tracker_create(s);
    }

  /*
   *  Set max ref frames. Needs to be set before the decoder is started.
   *  Multiple references should already be set by the H.264 parser
   */
  
  if(!s->ci->max_ref_frames)
    {
    if(s->ci->flags & GAVL_COMPRESSION_HAS_B_FRAMES)
      s->ci->max_ref_frames = 2;
    else if(s->ci->flags & GAVL_COMPRESSION_HAS_P_FRAMES)
      s->ci->max_ref_frames = 1;
    }
  
  if(s->action == BGAV_STREAM_DECODE)
    {
    dec = bgav_find_video_decoder(s->fourcc);
    if(!dec)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "No video decoder found for fourcc %c%c%c%c (0x%08x)",
               (s->fourcc & 0xFF000000) >> 24,
               (s->fourcc & 0x00FF0000) >> 16,
               (s->fourcc & 0x0000FF00) >> 8,
               (s->fourcc & 0x000000FF),
               s->fourcc);
      return 0;
      }
    s->data.video.decoder = dec;
    
    result = dec->init(s);
    if(!result)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Initializig decoder failed");

      gavl_dictionary_dump(s->info, 2);
      
      return 0;
      }
#if 0
    else
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Initialized decoder");
      gavl_dictionary_dump(s->info, 2);
      }
#endif
    if(s->data.video.format->interlace_mode == GAVL_INTERLACE_UNKNOWN)
      s->data.video.format->interlace_mode = GAVL_INTERLACE_NONE;
    if(s->data.video.format->framerate_mode == GAVL_FRAMERATE_UNKNOWN)
      s->data.video.format->framerate_mode = GAVL_FRAMERATE_CONSTANT;

    src_flags = s->src_flags;
    
    if(s->vframe)
      src_flags |= GAVL_SOURCE_SRC_ALLOC;

    if(!s->data.video.vsrc)
      {
      if(src_flags & GAVL_SOURCE_SRC_ALLOC)
        s->data.video.vsrc_priv =
          gavl_video_source_create(read_video_nocopy,
                                   s, src_flags,
                                   s->data.video.format);
      else
        s->data.video.vsrc_priv =
          gavl_video_source_create(read_video_copy,
                                   s, 0,
                                   s->data.video.format);

      s->data.video.vsrc = s->data.video.vsrc_priv;
      
      }
    
    }
  else if(s->action == BGAV_STREAM_READRAW)
    {
    if(s->data.video.format->interlace_mode == GAVL_INTERLACE_UNKNOWN)
      s->data.video.format->interlace_mode = GAVL_INTERLACE_NONE;
    if(s->data.video.format->framerate_mode == GAVL_FRAMERATE_UNKNOWN)
      s->data.video.format->framerate_mode = GAVL_FRAMERATE_CONSTANT;
    
    s->psrc =
      gavl_packet_source_create_video(bgav_stream_read_packet_func, // get_packet,
                                      s, GAVL_SOURCE_SRC_ALLOC, s->ci, s->data.video.format);
    }

  if(s->codec_bitrate)
    gavl_dictionary_set_int(s->m, GAVL_META_BITRATE,
                          s->codec_bitrate);
  else if(s->container_bitrate)
    gavl_dictionary_set_int(s->m, GAVL_META_BITRATE,
                          s->container_bitrate);
  
  if(s->data.video.format->framerate_mode == GAVL_FRAMERATE_STILL)
    s->flags = STREAM_DISCONT;

  return 1;
  }

const char * bgav_get_video_description(bgav_t * b, int stream)
  {
  bgav_stream_t * s = bgav_track_get_video_stream(b->tt->cur, stream);
  
  return gavl_dictionary_get_string(s->m, GAVL_META_FORMAT);
  }

static int bgav_video_decode(bgav_stream_t * s,
                             gavl_video_frame_t* frame)
  {
  gavl_source_status_t result;
  result = gavl_video_source_read_frame(s->data.video.vsrc,
                                        frame ? &frame : NULL);
  return (result == GAVL_SOURCE_OK) ? 1 : 0;
  }

int bgav_read_video(bgav_t * b, gavl_video_frame_t * frame, int stream)
  {
  bgav_stream_t * s;
  
  if(b->flags & BGAV_FLAG_EOF)
    return 0;

  s = bgav_track_get_video_stream(b->tt->cur, stream);
  
  return bgav_video_decode(s, frame);
  }

void bgav_video_dump(bgav_stream_t * s)
  {
  bgav_dprintf("  Depth:             %d\n", s->data.video.depth);
  bgav_dprintf("Format:\n");
  gavl_video_format_dump(s->data.video.format);
  }

void bgav_video_stop(bgav_stream_t * s)
  {
  if(s->bsf)
    {
    bgav_bsf_destroy(s->bsf);
    s->bsf = NULL;
    }

  if(s->data.video.parser)
    {
    bgav_video_parser_destroy(s->data.video.parser);
    s->data.video.parser = NULL;
    }
  if(s->pt)
    {
    bgav_packet_timer_destroy(s->pt);
    s->pt = NULL;
    }
  if(s->fd)
    {
    bgav_frametype_detector_destroy(s->fd);
    s->fd = NULL;
    }
  if(s->data.video.ft)
    {
    bgav_video_format_tracker_destroy(s->data.video.ft);
    s->data.video.ft = NULL;
    }

  if(s->data.video.vsrc_priv)
    {
    gavl_video_source_destroy(s->data.video.vsrc_priv);
    s->data.video.vsrc_priv = NULL;
    }
  s->data.video.vsrc = NULL;


  if(s->data.video.decoder)
    {
    s->data.video.decoder->close(s);
    s->data.video.decoder = NULL;
    }
  /* Clear still mode flag (it will be set during reinit) */
  s->flags &= ~(STREAM_STILL_SHOWN  | STREAM_HAVE_FRAME);
  
  if(s->data.video.kft)
    {
    bgav_keyframe_table_destroy(s->data.video.kft);
    s->data.video.kft = NULL;
    }
  }

void bgav_video_resync(bgav_stream_t * s)
  {
  if(s->out_time == GAVL_TIME_UNDEFINED)
    {
    s->out_time =
      gavl_time_rescale(s->timescale,
                        s->data.video.format->timescale,
                        STREAM_GET_SYNC(s));
    }

  s->flags &= ~STREAM_HAVE_FRAME;
  
  if(s->data.video.parser)
    {
    bgav_video_parser_reset(s->data.video.parser,
                            GAVL_TIME_UNDEFINED, s->out_time);
    }

  if(s->pt)
    bgav_packet_timer_reset(s->pt);
  if(s->fd)
    bgav_frametype_detector_reset(s->fd);
  
  if(s->data.video.vsrc)
    gavl_video_source_reset(s->data.video.vsrc);
  
  /* If the stream has keyframes, skip until the next one */

  if(s->ci->flags & GAVL_COMPRESSION_HAS_P_FRAMES)
    {
    bgav_packet_t * p;
    gavl_source_status_t st;
    while(1)
      {
      /* Skip pictures until we have the next keyframe */
      p = NULL;
      if((st = bgav_stream_peek_packet_read(s, &p, 1)) != GAVL_SOURCE_OK)
        return;

      if(PACKET_GET_KEYFRAME(p))
        {
        s->out_time = p->pts;
        break;
        }
      /* Skip this packet */
      gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Skipping packet while waiting for keyframe");
      p = NULL;
      bgav_stream_get_packet_read(s, &p);
      bgav_stream_done_packet_read(s, p);
      }
    }
  
  if(s->data.video.decoder && s->data.video.decoder->resync)
    s->data.video.decoder->resync(s);
  }

/* Skipping to a specified time can happen in 3 ways:
   
   1. For intra-only streams, we can just skip the packets, completely
      bypassing the codec.

   2. For codecs with keyframes but without delay, we call the decode
      method until the next packet (as seen by
      bgav_stream_peek_packet_read()) is the right one.

   3. Codecs with delay (i.e. with B-frames) must have a skipto method
      we'll call
*/

int bgav_video_skipto(bgav_stream_t * s, int64_t * time, int scale)
  {
  bgav_packet_t * p; 
  gavl_source_status_t st;
  
  int result;
  int64_t time_scaled;
  
  time_scaled =
    gavl_time_rescale(scale, s->data.video.format->timescale, *time);
  
  if(STREAM_IS_STILL(s))
    {
    /* Nothing to do */
    return 1;
    }
  else if(s->out_time > time_scaled)
    {
    char tmp_string1[128];
    char tmp_string2[128];
    char tmp_string3[128];
    sprintf(tmp_string1, "%" PRId64, s->out_time);
    sprintf(tmp_string2, "%" PRId64, time_scaled);
    sprintf(tmp_string3, "%" PRId64, time_scaled - s->out_time);
    
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "Cannot skip backwards: Stream time: %s skip time: %s difference: %s",
             tmp_string1, tmp_string2, tmp_string3);
    return 1;
    }
  
  /* Easy case: Intra only streams */
  else if(!(s->ci->flags & GAVL_COMPRESSION_HAS_P_FRAMES))
    {
    while(1)
      {
      p = NULL;
      if((st = bgav_stream_peek_packet_read(s, &p, 1)) != GAVL_SOURCE_OK)
        return 0;
      
      if(p->pts + p->duration > time_scaled)
        {
        s->out_time = p->pts;
        return 1;
        }
      p = NULL;
      bgav_stream_get_packet_read(s, &p);
      bgav_stream_done_packet_read(s, p);
      }
    *time = gavl_time_rescale(s->data.video.format->timescale, scale, s->out_time);
    return 1;
    }
  
  if(s->data.video.decoder->skipto)
    {
    if(!s->data.video.decoder->skipto(s, time_scaled))
      return 0;
    }
  else
    {
    while(1)
      {
      p = NULL;
      if(bgav_stream_peek_packet_read(s, &p, 1) != GAVL_SOURCE_OK)
        {
        s->out_time = GAVL_TIME_UNDEFINED;
        return 0;
        }
      
      //      fprintf(stderr, "Peek packet: %ld %ld %ld\n",
      //              p->pts, p->duration, time_scaled);
      
      if(p->pts + p->duration > time_scaled)
        {
        s->out_time = p->pts;
        return 1;
        }
      result = bgav_video_decode(s, NULL);
      if(!result)
        {
        s->out_time = GAVL_TIME_UNDEFINED;
        return 0;
        }
      }
    }

  *time = gavl_time_rescale(s->data.video.format->timescale, scale, s->out_time);

  //  fprintf(stderr, "video resync %"PRId64"\n", s->out_time);
  
  return 1;
  }

void bgav_skip_video(bgav_t * bgav, int stream,
                     int64_t * time, int scale,
                     int exact)
  {
  bgav_stream_t * s;

  s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  bgav_video_skipto(s, time, scale);
  }

gavl_video_source_t * bgav_get_video_source(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s;
  s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  return s->data.video.vsrc;
  }

static void frame_table_append_frame(gavl_frame_table_t * t,
                                     int64_t time,
                                     int64_t * last_time)
  {
  if(*last_time != GAVL_TIME_UNDEFINED)
    gavl_frame_table_append_entry(t, time - *last_time);
  *last_time = time;
  }

/* Create frame table from file index */

static gavl_frame_table_t * create_frame_table_fi(bgav_stream_t * s)
  {
  gavl_frame_table_t * ret;
  int i;
  int last_non_b_index = -1;
  bgav_file_index_t * fi = s->file_index;
  
  int64_t last_time = GAVL_TIME_UNDEFINED;
  
  ret = gavl_frame_table_create();
  ret->offset = s->stats.pts_start;
  
  for(i = 0; i < fi->num_entries; i++)
    {
    if((fi->entries[i].flags & 0xff) == BGAV_CODING_TYPE_B)
      {
      frame_table_append_frame(ret,
                               fi->entries[i].pts,
                               &last_time);
      }
    else
      {
      if(last_non_b_index >= 0)
        frame_table_append_frame(ret,
                                 fi->entries[last_non_b_index].pts,
                                 &last_time);
      last_non_b_index = i;
      }
    }

  /* Flush last non B-frame */
  
  if(last_non_b_index >= 0)
    {
    frame_table_append_frame(ret,
                             fi->entries[last_non_b_index].pts,
                             &last_time);
    }

  /* Flush last frame */
  gavl_frame_table_append_entry(ret, s->stats.pts_end - last_time);

  for(i = 0; i < fi->tt.num_entries; i++)
    {
    gavl_frame_table_append_timecode(ret,
                                     fi->tt.entries[i].pts,
                                     fi->tt.entries[i].timecode);
    }
  
  return ret;
  }

/* Create frame table from superindex */

static gavl_frame_table_t *
create_frame_table_si(bgav_stream_t * s, bgav_superindex_t * si)
  {
  int i;
  gavl_frame_table_t * ret;
  int last_non_b_index = -1;
  
  ret = gavl_frame_table_create();

  for(i = 0; i < si->num_entries; i++)
    {
    if(si->entries[i].stream_id == s->stream_id)
      {
      if((si->entries[i].flags & 0xff) == BGAV_CODING_TYPE_B)
        {
        gavl_frame_table_append_entry(ret, si->entries[i].duration);
        }
      else
        {
        if(last_non_b_index >= 0)
          gavl_frame_table_append_entry(ret, si->entries[last_non_b_index].duration);
        last_non_b_index = i;
        }
      }
    }

  if(last_non_b_index >= 0)
    gavl_frame_table_append_entry(ret, si->entries[last_non_b_index].duration);

  /* Maybe we have timecodes in the timecode table */

  if(s->timecode_table)
    {
    for(i = 0; i < s->timecode_table->num_entries; i++)
      {
      gavl_frame_table_append_timecode(ret,
                                       s->timecode_table->entries[i].pts,
                                       s->timecode_table->entries[i].timecode);
      }
    }
  
  return ret;
  }

gavl_frame_table_t * bgav_get_frame_table(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s;
  s = bgav_track_get_video_stream(bgav->tt->cur, stream);

  if(s->file_index)
    {
    return create_frame_table_fi(s);
    }
  else if(bgav->demuxer->si)
    {
    return create_frame_table_si(s, bgav->demuxer->si);
    }
  else
    return NULL;
  }

const uint32_t bgav_png_fourccs[] =
  {
    BGAV_MK_FOURCC('p', 'n', 'g', ' '),
    BGAV_MK_FOURCC('M', 'P', 'N', 'G'),
    0x00
  };

static uint32_t jpeg_fourccs[] =
  {
    BGAV_MK_FOURCC('j', 'p', 'e', 'g'),
    0x00
  };

static uint32_t tiff_fourccs[] =
  {
    BGAV_MK_FOURCC('t', 'i', 'f', 'f'),
    0x00
  };

static uint32_t tga_fourccs[] =
  {
    BGAV_MK_FOURCC('t', 'g', 'a', ' '),
    0x00
  };

static uint32_t mpeg1_fourccs[] =
  {
    BGAV_MK_FOURCC('m', 'p', 'v', '1'),
    0x00
  };

static uint32_t mpeg2_fourccs[] =
  {
    BGAV_MK_FOURCC('m', 'p', 'v', '2'),
    0x00
  };

static uint32_t theora_fourccs[] =
  {
    BGAV_MK_FOURCC('T','H','R','A'),
    0x00
  };

static uint32_t dirac_fourccs[] =
  {
    BGAV_MK_FOURCC('d','r','a','c'),
    0x00
  };

static uint32_t h264_fourccs[] =
  {
    BGAV_MK_FOURCC('H','2','6','4'),
    BGAV_MK_FOURCC('h','2','6','4'),
    0x00
  };

static uint32_t avc1_fourccs[] =
  {
    BGAV_MK_FOURCC('a','v','c','1'),
    0x00
  };

static uint32_t mpeg4_fourccs[] =
  {
    BGAV_MK_FOURCC('m','p','4','v'),
    0x00
  };

static uint32_t vp8_fourccs[] =
  {
    BGAV_MK_FOURCC('V','P','8','0'),
    0x00
  };

static uint32_t d10_fourccs[] =
  {
    BGAV_MK_FOURCC('m', 'x', '5', 'p'),
    BGAV_MK_FOURCC('m', 'x', '4', 'p'),
    BGAV_MK_FOURCC('m', 'x', '3', 'p'),
    BGAV_MK_FOURCC('m', 'x', '5', 'n'),
    BGAV_MK_FOURCC('m', 'x', '4', 'n'),
    BGAV_MK_FOURCC('m', 'x', '3', 'n'),
    0x00,
  };

const uint32_t bgav_dv_fourccs[] =
  {
    BGAV_MK_FOURCC('d', 'v', 's', 'd'), 
    BGAV_MK_FOURCC('D', 'V', 'S', 'D'), 
    BGAV_MK_FOURCC('d', 'v', 'h', 'd'), 
    BGAV_MK_FOURCC('d', 'v', 's', 'l'), 
    BGAV_MK_FOURCC('d', 'v', '2', '5'),
    /* Generic DV */
    BGAV_MK_FOURCC('D', 'V', ' ', ' '),

    BGAV_MK_FOURCC('d', 'v', 'c', 'p') , /* DV PAL */
    BGAV_MK_FOURCC('d', 'v', 'c', ' ') , /* DV NTSC */
    BGAV_MK_FOURCC('d', 'v', 'p', 'p') , /* DVCPRO PAL produced by FCP */
    BGAV_MK_FOURCC('d', 'v', '5', 'p') , /* DVCPRO50 PAL produced by FCP */
    BGAV_MK_FOURCC('d', 'v', '5', 'n') , /* DVCPRO50 NTSC produced by FCP */
    BGAV_MK_FOURCC('A', 'V', 'd', 'v') , /* AVID DV */
    BGAV_MK_FOURCC('A', 'V', 'd', '1') , /* AVID DV */
    BGAV_MK_FOURCC('d', 'v', 'h', 'q') , /* DVCPRO HD 720p50 */
    BGAV_MK_FOURCC('d', 'v', 'h', 'p') , /* DVCPRO HD 720p60 */
    BGAV_MK_FOURCC('d', 'v', 'h', '5') , /* DVCPRO HD 50i produced by FCP */
    BGAV_MK_FOURCC('d', 'v', 'h', '6') , /* DVCPRO HD 60i produced by FCP */
    BGAV_MK_FOURCC('d', 'v', 'h', '3') , /* DVCPRO HD 30p produced by FCP */
    0x00,
  };

const uint32_t div3_fourccs[] =
  {
    BGAV_MK_FOURCC('D', 'I', 'V', '3'),
    BGAV_MK_FOURCC('M', 'P', '4', '3'), 
    BGAV_MK_FOURCC('M', 'P', 'G', '3'), 
    BGAV_MK_FOURCC('D', 'I', 'V', '5'), 
    BGAV_MK_FOURCC('D', 'I', 'V', '6'), 
    BGAV_MK_FOURCC('D', 'I', 'V', '4'), 
    BGAV_MK_FOURCC('A', 'P', '4', '1'),
    BGAV_MK_FOURCC('C', 'O', 'L', '1'),
    BGAV_MK_FOURCC('C', 'O', 'L', '0'),
    0x00
  };

int bgav_get_video_compression_info(bgav_t * bgav, int stream,
                                    gavl_compression_info_t * ret)
  {
  gavl_codec_id_t id;
  bgav_stream_t * s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  int need_bitrate = 0;
  bgav_bsf_t * bsf = NULL;

  if(ret)
    memset(ret, 0, sizeof(*ret));
  
  if(s->flags & STREAM_GOT_CI)
    {
    if(ret)
      gavl_compression_info_copy(ret, s->ci);
    return 1;
    }
  else if(s->flags & STREAM_GOT_NO_CI)
    return 0;

  bgav_track_get_compression(bgav->tt->cur);
  
  if(bgav_check_fourcc(s->fourcc, bgav_png_fourccs))
    id = GAVL_CODEC_ID_PNG;
  else if(bgav_check_fourcc(s->fourcc, jpeg_fourccs))
    id = GAVL_CODEC_ID_JPEG;
  else if(bgav_check_fourcc(s->fourcc, tiff_fourccs))
    id = GAVL_CODEC_ID_TIFF;
  else if(bgav_check_fourcc(s->fourcc, tga_fourccs))
    id = GAVL_CODEC_ID_TGA;
  else if(bgav_check_fourcc(s->fourcc, mpeg1_fourccs))
    id = GAVL_CODEC_ID_MPEG1;
  else if(bgav_check_fourcc(s->fourcc, mpeg2_fourccs))
    id = GAVL_CODEC_ID_MPEG2;
  else if(bgav_check_fourcc(s->fourcc, theora_fourccs))
    id = GAVL_CODEC_ID_THEORA;
  else if(bgav_check_fourcc(s->fourcc, dirac_fourccs))
    id = GAVL_CODEC_ID_DIRAC;
  else if(bgav_check_fourcc(s->fourcc, h264_fourccs))
    id = GAVL_CODEC_ID_H264;
  else if(bgav_check_fourcc(s->fourcc, mpeg4_fourccs))
    id = GAVL_CODEC_ID_MPEG4_ASP;
  else if(bgav_check_fourcc(s->fourcc, bgav_dv_fourccs))
    id = GAVL_CODEC_ID_DV;
  else if(bgav_check_fourcc(s->fourcc, vp8_fourccs))
    id = GAVL_CODEC_ID_VP8;
  else if(bgav_check_fourcc(s->fourcc, div3_fourccs))
    id = GAVL_CODEC_ID_DIV3;
  else if(bgav_check_fourcc(s->fourcc, avc1_fourccs))
    {
    id = GAVL_CODEC_ID_H264;
    s->flags |= STREAM_FILTER_PACKETS;
    }
  else if(bgav_check_fourcc(s->fourcc, d10_fourccs))
    {
    id = GAVL_CODEC_ID_MPEG2;
    need_bitrate = 1;
    }
  else if(bgav_video_is_divx4(s->fourcc))
    id = GAVL_CODEC_ID_MPEG4_ASP;
  else
    {
    s->flags |= STREAM_GOT_NO_CI;
    return 0;
    }
  if(gavl_compression_need_pixelformat(id) &&
     s->data.video.format->pixelformat == GAVL_PIXELFORMAT_NONE)
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "Video compression format needs pixelformat for compressed output");
    s->flags |= STREAM_GOT_NO_CI;
    return 0;
    }
  
  s->ci->id = id;

  /* Create the filtered extradata */
  if((s->flags & STREAM_FILTER_PACKETS) && !s->bsf)
    bsf = bgav_bsf_create(s);
  
  if((s->ci->global_header_len) && (bgav_video_is_divx4(s->fourcc)))
    {
    bgav_mpeg4_remove_packed_flag(s->ci->global_header,
                                  &s->ci->global_header_len,
                                  &s->ci->global_header_len);
    }
  
  /* Restore everything */
  if(bsf)
    bgav_bsf_destroy(bsf);
  
  if(s->codec_bitrate)
    s->ci->bitrate = s->codec_bitrate;
  else if(s->container_bitrate)
    s->ci->bitrate = s->container_bitrate;
  
  if(need_bitrate && !s->ci->bitrate)
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "Video compression format needs bitrate for compressed output");
    s->flags |= STREAM_GOT_NO_CI;
    return 0;
    }
  
  if(ret)
    gavl_compression_info_copy(ret, s->ci);
  s->flags |= STREAM_GOT_CI;
  
  return 1;
  }


int bgav_read_video_packet(bgav_t * bgav, int stream, gavl_packet_t * p)
  {
  bgav_stream_t * s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  return (gavl_packet_source_read_packet(s->psrc, &p) == GAVL_SOURCE_OK);
#if 0
  bgav_packet_t * bp;
  
  bp = bgav_stream_get_packet_read(s);
  if(!bp)
    return 0;
  
  gavl_packet_alloc(p, bp->data_size);
  memcpy(p->data, bp->data, bp->data_size);
  p->data_len = bp->data_size;

  bgav_packet_2_gavl(bp, p);

  bgav_stream_done_packet_read(s, bp);
#endif
  return 1;
  }

/* Set frame metadata from packet */

void bgav_set_video_frame_from_packet(const bgav_packet_t * p,
                                      gavl_video_frame_t * f)
  {
  f->timestamp = p->pts;
  f->duration = p->duration;
  f->timecode = p->tc;

  f->dst_x = p->dst_x;
  f->dst_y = p->dst_y;
  gavl_rectangle_i_copy(&f->src_rect, &p->src_rect);
  }

gavl_packet_source_t * bgav_get_video_packet_source(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  return s->psrc;
  }
