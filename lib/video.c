/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
 *
 * Copyright (c) 2001 - 2024 Members of the Gmerlin project
 * http://github.com/bplaum
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
#include <gavl/msg.h>

// #define DUMP_TIMESTAMPS

#define LOG_DOMAIN "video"

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

/* DIVX (maybe with B-frames) requires special attention */

static const uint32_t video_codecs_divx[] =
  {
    BGAV_MK_FOURCC('D', 'I', 'V', 'X'),
    BGAV_MK_FOURCC('d', 'i', 'v', 'x'),
    BGAV_MK_FOURCC('D', 'X', '5', '0'),
    BGAV_MK_FOURCC('X', 'V', 'I', 'D'),
    BGAV_MK_FOURCC('x', 'v', 'i', 'd'),
    BGAV_MK_FOURCC('F', 'M', 'P', '4'),
    BGAV_MK_FOURCC('f', 'm', 'p', '4'),    
    0x00,
  };

int bgav_video_is_divx4(uint32_t fourcc)
  {
  int i = 0;
  while(video_codecs_divx[i])
    {
    if(video_codecs_divx[i] == fourcc)
      return 1;
    i++;
    }
  return 0;
  }



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
  if(bgav_stream_peek_packet_read(s, NULL) == GAVL_SOURCE_OK)
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
  gavl_dprintf("Video timestamp: %"PRId64"\n", s->vframe->timestamp);
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
    gavl_dprintf("Video timestamp: %"PRId64"\n", (*frame)->timestamp);
#endif    
  s->flags &= ~STREAM_HAVE_FRAME;
  return GAVL_SOURCE_OK;
  }

int bgav_video_init(bgav_stream_t * s)
  {
  bgav_set_video_compression_info(s);
  
  if(!s->timescale && s->data.video.format->timescale)
    s->timescale = s->data.video.format->timescale;

  if(!(s->flags & STREAM_STANDALONE))
    {
    /* TODO: Avoid this when not really needed (i.e. when
       packets are decoded via ffmpeg */
    if(bgav_check_fourcc(s->fourcc, avc1_fourccs))
      s->flags |= STREAM_FILTER_PACKETS;
    }
  
  //  if((s->action == BGAV_STREAM_READRAW) &&
  //     (s->flags & STREAM_FILTER_PACKETS))
  if((s->flags & STREAM_FILTER_PACKETS) && !s->pf)
    {
    gavl_stream_set_compression_info(s->info, s->ci);
    
    s->pf = bgav_packet_filter_create(s->fourcc);
    s->psrc = bgav_packet_filter_connect(s->pf, s->psrc);
    }

  /* Now the whole format should be avaibable */

  gavl_packet_source_peek_packet(s->psrc, NULL);
  
  gavl_dictionary_reset(s->info_ext);
  gavl_dictionary_copy(s->info_ext, gavl_packet_source_get_stream(s->psrc));
  
  gavl_compression_info_free(s->ci);
  gavl_compression_info_init(s->ci);
  gavl_stream_get_compression_info(s->info_ext, s->ci);
  gavl_stream_set_stats(s->info_ext, &s->stats);
  
  if(s->stats.pts_start == GAVL_TIME_UNDEFINED)
    {
    bgav_packet_t * p = NULL;
    char tmp_string[128];
    
    if(bgav_stream_peek_packet_read(s, &p) != GAVL_SOURCE_OK)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "EOF while getting start time");
      return 0;
      }
    s->stats.pts_start = p->pts;
    s->out_time = s->stats.pts_start;

    sprintf(tmp_string, "%" PRId64, s->out_time);
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
             "Got initial video timestamp: %s",
             tmp_string);
    }

  bgav_set_video_compression_info(s);
  return 1;
  }

int bgav_video_start(bgav_stream_t * s)
  {
  int result;
  int src_flags;
  bgav_video_decoder_t * dec;
  
  if(s->action == BGAV_STREAM_DECODE)
    {
    dec = bgav_find_video_decoder(s->fourcc, s->info);
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
    else // Decoder created source already, update format
      {
      gavl_video_format_copy(s->data.video.format, gavl_video_source_get_src_format(s->data.video.vsrc));
      }
    
    }
  else if(s->action == BGAV_STREAM_READRAW)
    {
    if(s->data.video.format->interlace_mode == GAVL_INTERLACE_UNKNOWN)
      s->data.video.format->interlace_mode = GAVL_INTERLACE_NONE;
    if(s->data.video.format->framerate_mode == GAVL_FRAMERATE_UNKNOWN)
      s->data.video.format->framerate_mode = GAVL_FRAMERATE_CONSTANT;
    }

  if(s->codec_bitrate)
    gavl_dictionary_set_int(s->m, GAVL_META_BITRATE,
                          s->codec_bitrate);
  else if(s->container_bitrate)
    gavl_dictionary_set_int(s->m, GAVL_META_BITRATE,
                          s->container_bitrate);
  
  if(s->data.video.format->framerate_mode == GAVL_FRAMERATE_STILL)
    s->flags |= STREAM_DISCONT;

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
  gavl_dprintf("  Depth:             %d\n", s->data.video.depth);
  gavl_dprintf("Format:\n");
  gavl_video_format_dump(s->data.video.format);
  }

void bgav_video_stop(bgav_stream_t * s)
  {
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

  if(s->data.video.frame_table)
    {
    gavl_packet_index_destroy(s->data.video.frame_table);
    s->data.video.frame_table = NULL;
    }
  }
  
void bgav_video_resync(bgav_stream_t * s)
  {
  if(s->out_time == GAVL_TIME_UNDEFINED)
    s->out_time = STREAM_GET_SYNC(s);
  
  s->flags &= ~STREAM_HAVE_FRAME;
  
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
      if((st = bgav_stream_peek_packet_read(s, &p)) != GAVL_SOURCE_OK)
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
  
  //  fprintf(stderr, "video resync %"PRId64"\n", gavl_time_unscale(s->data.video.format->timescale, s->out_time));
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
  
  int64_t time_scaled;
  
  time_scaled =
    gavl_time_rescale(scale, s->data.video.format->timescale, *time);
  
  if(STREAM_IS_STILL(s))
    {
    /* Nothing to do */
    return 1;
    }

  if(s->out_time > time_scaled)
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
  if(!(s->ci->flags & GAVL_COMPRESSION_HAS_P_FRAMES))
    {
    while(1)
      {
      p = NULL;
      if((st = bgav_stream_peek_packet_read(s, &p)) != GAVL_SOURCE_OK)
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
    return s->data.video.decoder->skipto(s, time_scaled);
  
  while(1)
    {
    gavl_video_frame_t * f;

    f = NULL;
      
    if(gavl_video_source_read_frame(s->data.video.vsrc, &f) != GAVL_SOURCE_OK)
      {
      s->out_time = GAVL_TIME_UNDEFINED;
      return 0;
      }
    if(f->duration <= 0)
      {
      if(f->timestamp >= time_scaled)
        {
        s->out_time = f->timestamp;
        goto end;
        }
      }
    else
      {
      if(f->timestamp + f->duration >= time_scaled)
        {
        s->out_time = f->timestamp + f->duration;
        goto end;
        }
      }
    }

  end:
  
  *time = gavl_time_rescale(s->data.video.format->timescale, scale, s->out_time);

  //  fprintf(stderr, "video resync %"PRId64"\n", s->out_time);
  
  return 1;
  }

int bgav_skip_video(bgav_t * bgav, int stream,
                    int64_t * time, int scale,
                    int exact)
  {
  bgav_stream_t * s;

  s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  return bgav_video_skipto(s, time, scale);
  }

gavl_video_source_t * bgav_get_video_source(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s;
  s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  return s->data.video.vsrc;
  }


/* Create frame table from superindex */

static gavl_frame_table_t *
create_frame_table_si(bgav_stream_t * s, gavl_packet_index_t * si)
  {
  int i;
  gavl_frame_table_t * ret;
  int last_non_b_index = -1;
  
  ret = gavl_frame_table_create();

  for(i = 0; i < si->num_entries; i++)
    {
    if(si->entries[i].stream_id == s->stream_id)
      {
      if((si->entries[i].flags & 0xff) == GAVL_PACKET_TYPE_B)
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

  if(bgav->demuxer->si)
    {
    return create_frame_table_si(s, bgav->demuxer->si);
    }
  else
    return NULL;
  }


int bgav_get_video_compression_info(bgav_t * bgav, int stream,
                                    gavl_compression_info_t * info)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_video_stream(bgav->tt->cur, stream)))
    return 0;
  return gavl_stream_get_compression_info(s->info, info);
  
  }


int bgav_set_video_compression_info(bgav_stream_t * s)
  {
  gavl_codec_id_t id;
  uint32_t codec_tag = 0;
  int need_bitrate = 0;

  if(s->flags & (STREAM_GOT_CI || STREAM_GOT_NO_CI))
    return 1;

  //  fprintf(stderr, "bgav_set_video_compression_info\n");
  
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
    id = GAVL_CODEC_ID_H264;
  else if(bgav_check_fourcc(s->fourcc, d10_fourccs))
    {
    id = GAVL_CODEC_ID_MPEG2;
    need_bitrate = 1;
    }
  else if(bgav_video_is_divx4(s->fourcc))
    id = GAVL_CODEC_ID_MPEG4_ASP;
  else
    {
    id = GAVL_CODEC_ID_EXTENDED;
    codec_tag = s->fourcc;
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
  s->ci->codec_tag = codec_tag;
  
  if((s->ci->codec_header.len) && (bgav_video_is_divx4(s->fourcc)))
    {
    bgav_mpeg4_remove_packed_flag(&s->ci->codec_header);
    }
  
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
  
  s->flags |= STREAM_GOT_CI;

  //  fprintf(stderr, "Setting compression info: %d\n", s->ci->id);
  
  gavl_stream_set_compression_info(s->info, s->ci);
  return 1;
  }


int bgav_read_video_packet(bgav_t * bgav, int stream, gavl_packet_t * p)
  {
  bgav_stream_t * s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  return (gavl_packet_source_read_packet(s->psrc, &p) == GAVL_SOURCE_OK);
  }

/* Set frame metadata from packet */

void bgav_set_video_frame_from_packet(const bgav_packet_t * p,
                                      gavl_video_frame_t * f)
  {
  f->timestamp = p->pts;
  f->duration = p->duration;
  f->timecode = p->timecode;

  f->dst_x = p->dst_x;
  f->dst_y = p->dst_y;
  gavl_rectangle_i_copy(&f->src_rect, &p->src_rect);
  }

gavl_packet_source_t * bgav_get_video_packet_source(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s = bgav_track_get_video_stream(bgav->tt->cur, stream);
  return s->psrc;
  }

int bgav_video_ensure_frame_table(bgav_t * b, int stream)
  {
  bgav_stream_t * s = bgav_track_get_video_stream(b->tt->cur, stream);

  if(s->data.video.frame_table)
    return 1;
  
  if(!bgav_ensure_index(b))
    return 0;
      
  s->data.video.frame_table = gavl_packet_index_create(0);
      
  gavl_packet_index_extract_stream(b->demuxer->si,
                                   s->data.video.frame_table,
                                   s->stream_id);
  gavl_packet_index_sort_by_pts(s->data.video.frame_table);
  gavl_packet_index_set_stream_stats(s->data.video.frame_table,
                                     s->stream_id, &s->stats);
  return 1;
  }

int64_t bgav_get_num_video_frames(bgav_t * b, int stream)
  {
  bgav_stream_t * s = bgav_track_get_video_stream(b->tt->cur, stream);

  if(!s->stats.total_packets)
    {
    if(!bgav_video_ensure_frame_table(b, stream))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Couldn't get total number of frames");
      }
    }

  return s->stats.total_packets;
  }

void bgav_set_video_skip_mode(bgav_t * bgav, int stream,
                              int mode)
  {
  bgav_stream_t * s = bgav_track_get_video_stream(bgav->tt->cur, stream);

  if(s->data.video.skip_mode != mode)
    {
    s->data.video.skip_mode = mode;
    s->flags |= STREAM_SKIP_MODE_CHANGED;
    }
  }

int bgav_video_packet_skip(gavl_packet_t * p, int skip_mode)
  {
  switch(skip_mode)
    {
    case GAVL_MSG_SRC_SKIP_NONE:
      return 0;
      break;
    case GAVL_MSG_SRC_SKIP_NONREF:
      if(((p->flags & GAVL_PACKET_TYPE_MASK) == GAVL_PACKET_TYPE_B) &&
         !(p->flags & GAVL_PACKET_REF))
        return 1;
      else
        return 0;
      break;
    case GAVL_MSG_SRC_SKIP_NONKEY:
      if(p->flags & GAVL_PACKET_KEYFRAME)
        return 0;
      else
        return 1;
    }
  return 0;
  }

void bgav_video_compute_info(bgav_stream_t * s)
  {
  gavl_stream_stats_apply_video(&s->stats, s->data.video.format,
                                s->ci, s->m);
  gavl_dictionary_set_int(s->m, GAVL_META_STREAM_PACKET_TIMESCALE, s->timescale);
  gavl_dictionary_set_int(s->m, GAVL_META_STREAM_SAMPLE_TIMESCALE, s->data.video.format->timescale);
  }
