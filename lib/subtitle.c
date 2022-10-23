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
#include <string.h>

#include <avdec_private.h>
#include <parser.h>

#define LOG_DOMAIN "subtitle"

// #define DUMP_TIMESTAMPS

int bgav_num_subtitle_streams(bgav_t * bgav, int track)
  {
  return bgav->tt->tracks[track]->num_text_streams +
    bgav->tt->tracks[track]->num_overlay_streams;
  }

int bgav_num_text_streams(bgav_t * bgav, int track)
  {
  return bgav->tt->tracks[track]->num_text_streams;
  }

int bgav_num_overlay_streams(bgav_t * bgav, int track)
  {
  return bgav->tt->tracks[track]->num_overlay_streams;
  }

int bgav_set_subtitle_stream(bgav_t * b, int stream,
                             bgav_stream_action_t action)
  {
  bgav_stream_t * s = bgav_track_get_subtitle_stream(b->tt->cur, stream);
  if(!s)
    return 0;
  s->action = action;
  return 1;
  }

int bgav_set_text_stream(bgav_t * b, int stream, bgav_stream_action_t action)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_text_stream(b->tt->cur, stream)))
    return 0;
  s->action = action;
  return 1;
  }

int bgav_set_overlay_stream(bgav_t * b, int stream, bgav_stream_action_t action)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_overlay_stream(b->tt->cur, stream)))
    return 0;
  s->action = action;
  return 1;
  }

const gavl_video_format_t * bgav_get_subtitle_format(bgav_t * b, int stream)
  {
  bgav_stream_t * s = bgav_track_get_subtitle_stream(b->tt->cur, stream);
  return s->data.subtitle.video.format;
  }

int bgav_subtitle_is_text(bgav_t * b, int stream)
  {
  bgav_stream_t * s = bgav_track_get_subtitle_stream(b->tt->cur, stream);
  if(s->type == GAVL_STREAM_TEXT)
    return 1;
  else
    return 0;
  }

const char * bgav_get_subtitle_language(bgav_t * b, int stream)
  {
  bgav_stream_t * s = bgav_track_get_subtitle_stream(b->tt->cur, stream);
  return gavl_dictionary_get_string(s->m, GAVL_META_LANGUAGE);
  }

/* LEGACY */
int bgav_read_subtitle_overlay(bgav_t * b, gavl_overlay_t * ovl, int stream)
  {
  bgav_stream_t * s = bgav_track_get_subtitle_stream(b->tt->cur, stream);
  
  if(bgav_has_subtitle(b, stream))
    {
    if(s->flags & STREAM_EOF_C)
      return 0;
    }
  else
    return 0;
  return gavl_video_source_read_frame(s->data.subtitle.video.vsrc, &ovl) == GAVL_SOURCE_OK;
  }

/* LEGACY */
int bgav_read_subtitle_text(bgav_t * b, char ** ret, int *ret_alloc,
                            int64_t * start_time, int64_t * duration,
                            int stream)
  {
  gavl_packet_t p;
  gavl_packet_t * pp;
  bgav_stream_t * s = bgav_track_get_subtitle_stream(b->tt->cur, stream);
  
  if(bgav_has_subtitle(b, stream))
    {
    if(s->flags & STREAM_EOF_C)
      return 0;
    }
  else
    return 0;

  gavl_packet_init(&p);
  
  p.buf.buf = (uint8_t*)(*ret);
  p.buf.alloc = *ret_alloc;

  pp = &p;
  
  if(gavl_packet_source_read_packet(s->psrc, &pp) != GAVL_SOURCE_OK)
    return 0;

  *ret = (char*)p.buf.buf;
  *ret_alloc = p.buf.alloc;
  
  *start_time = p.pts;
  *duration   = p.duration;
  
  return 1;
  }

int bgav_has_subtitle(bgav_t * b, int stream)
  {
  bgav_stream_t * s = bgav_track_get_subtitle_stream(b->tt->cur, stream);
  int force;
  
  if(s->flags & STREAM_HAS_SUBREADER)
    return 1;
  else
    {
    if(b->tt->cur->flags & TRACK_SAMPLE_ACCURATE)
      force = 1;
    else
      force = 0;
    
    if(bgav_stream_peek_packet_read(s, NULL, force) == GAVL_SOURCE_AGAIN)
      return 0;
    else
      return 1; 
    }
  }

void bgav_subtitle_dump(bgav_stream_t * s)
  {
  if(s->type == GAVL_STREAM_TEXT)
    {
    bgav_dprintf( "  Character set:     %s\n",
                  (s->data.subtitle.charset ? s->data.subtitle.charset :
                   BGAV_UTF8));
    }
  else
    {
    bgav_dprintf( "  Format:\n");
    gavl_video_format_dump(s->data.subtitle.video.format);
    }
  }

static gavl_source_status_t read_video_copy(void * sp,
                                            gavl_video_frame_t ** frame)
  {
  gavl_source_status_t st;
  bgav_stream_t * s = sp;

  if((st = bgav_stream_peek_packet_read(s, NULL, 0)) != GAVL_SOURCE_OK)
    return st;
  
  if(frame)
    {
    if((st = s->data.subtitle.video.decoder->decode(sp, *frame)) != GAVL_SOURCE_OK)
      return st;
    s->out_time = (*frame)->timestamp + (*frame)->duration;
    }
  else
    {
    if((st = s->data.subtitle.video.decoder->decode(sp, NULL)) != GAVL_SOURCE_OK)
      return st;
    }
#ifdef DUMP_TIMESTAMPS
  bgav_dprintf("Overlay timestamp: %"PRId64"\n", s->vframe->timestamp);
#endif
  // s->flags &= ~STREAM_HAVE_FRAME;
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t
read_video_nocopy(void * sp,
                  gavl_video_frame_t ** frame)
  {
  gavl_source_status_t st;
  bgav_stream_t * s = sp;
  //  fprintf(stderr, "Read video nocopy\n");
  if((st = bgav_stream_peek_packet_read(s, NULL, 0)) != GAVL_SOURCE_OK)
    return st;
  
  if((st = s->data.subtitle.video.decoder->decode(sp, NULL)) != GAVL_SOURCE_OK)
    return st;
  if(frame)
    *frame = s->vframe;
#ifdef DUMP_TIMESTAMPS
  bgav_dprintf("Overlay timestamp: %"PRId64"\n", s->vframe->timestamp);
#endif    
  s->out_time = s->vframe->timestamp + s->vframe->duration;
  //  s->flags &= ~STREAM_HAVE_FRAME;
  return GAVL_SOURCE_OK;
  }

int bgav_text_init(bgav_stream_t * s)
  {
  return 1;
  }

int bgav_text_start(bgav_stream_t * s)
  {
  s->flags &= ~(STREAM_EOF_C|STREAM_EOF_D);

  if(s->data.subtitle.subreader)
    if(!bgav_subtitle_reader_start(s))
      return 0;
  
  s->data.subtitle.cnv =
    bgav_subtitle_converter_create(s);
  
  s->psrc =
    gavl_packet_source_create_text(bgav_stream_read_packet_func, // get_packet,
                                   s, GAVL_SOURCE_SRC_ALLOC, s->timescale);
  return 1;
  }

const uint32_t bgav_dvdsub_fourccs[] =
  {
    BGAV_MK_FOURCC('D', 'V', 'D', 'S'),
    BGAV_MK_FOURCC('m', 'p', '4', 's'),
    0x00
  };

int bgav_overlay_init(bgav_stream_t * s)
  {
  if(s->data.subtitle.subreader)
    {
    if(!bgav_subtitle_reader_start(s))
      return 0;
    //    else
    //      return 1;
    }

  if(bgav_check_fourcc(s->fourcc, bgav_dvdsub_fourccs))
    s->flags |= STREAM_PARSE_FULL;
    
  if((s->flags & (STREAM_PARSE_FULL|STREAM_PARSE_FRAME)) &&
     !s->data.subtitle.video.parser)
    {
    s->data.subtitle.video.parser = bgav_video_parser_create(s);
    if(!s->data.subtitle.video.parser)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "No subtitle parser found for fourcc %c%c%c%c (0x%08x)",
               (s->fourcc & 0xFF000000) >> 24,
               (s->fourcc & 0x00FF0000) >> 16,
               (s->fourcc & 0x0000FF00) >> 8,
               (s->fourcc & 0x000000FF),
               s->fourcc);
      return 0;
      }
    s->index_mode = INDEX_MODE_SIMPLE;
    }
  return 1;
  }

int bgav_overlay_start(bgav_stream_t * s)
  {
  bgav_video_decoder_t * dec;
  
  s->flags &= ~(STREAM_EOF_C|STREAM_EOF_D);
  
  if(s->action == BGAV_STREAM_DECODE)
    {
    dec = bgav_find_video_decoder(s->fourcc);
    if(!dec)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "No subtitle decoder found for fourcc %c%c%c%c (0x%08x)",
               (s->fourcc & 0xFF000000) >> 24,
               (s->fourcc & 0x00FF0000) >> 16,
               (s->fourcc & 0x0000FF00) >> 8,
               (s->fourcc & 0x000000FF),
               s->fourcc);
      return 0;
      }
    s->data.subtitle.video.decoder = dec;
    
    if(!dec->init(s))
      return 0;

    s->data.subtitle.video.format->timescale = s->timescale;

    if(!s->data.subtitle.video.vsrc)
      {

      if(s->vframe)
        {
        s->data.subtitle.video.vsrc_priv =
          gavl_video_source_create(read_video_nocopy,
                                   s, GAVL_SOURCE_SRC_ALLOC | s->src_flags,
                                   s->data.subtitle.video.format);
        }
      else
        {
        s->data.subtitle.video.vsrc_priv =
          gavl_video_source_create(read_video_copy,
                                   s, s->src_flags,
                                   s->data.subtitle.video.format);
        }

      
      s->data.subtitle.video.vsrc = s->data.subtitle.video.vsrc_priv;
      }
    
    }
  else if(s->action == BGAV_STREAM_READRAW)
    {
    s->psrc =
      gavl_packet_source_create_video(bgav_stream_read_packet_func, // get_packet,
                                      s,
                                      GAVL_SOURCE_SRC_ALLOC,
                                      s->ci,
                                      s->data.subtitle.video.format, GAVL_STREAM_OVERLAY);
    }
  
  return 1;
  }

void bgav_subtitle_stop(bgav_stream_t * s)
  {
  if(s->data.subtitle.cnv)
    {
    bgav_subtitle_converter_destroy(s->data.subtitle.cnv);
    s->data.subtitle.cnv = NULL;
    }
  if(s->data.subtitle.subreader)
    {
    bgav_subtitle_reader_stop(s);
    }
  if(s->data.subtitle.video.decoder)
    {
    s->data.subtitle.video.decoder->close(s);
    s->data.subtitle.video.decoder = NULL;
    }
  if(s->data.subtitle.video.parser)
    {
    bgav_video_parser_destroy(s->data.subtitle.video.parser);
    s->data.subtitle.video.parser = NULL;
    }
  if(s->data.subtitle.video.vsrc_priv)
    {
    gavl_video_source_destroy(s->data.subtitle.video.vsrc_priv);
    s->data.subtitle.video.vsrc_priv = NULL;
    }
  s->data.subtitle.video.vsrc = NULL;
  }

void bgav_subtitle_resync(bgav_stream_t * s)
  {
  /* Nothing to do here */
  if(s->type == GAVL_STREAM_TEXT)
    return;

  if(s->data.subtitle.video.decoder &&
     s->data.subtitle.video.decoder->resync)
    s->data.subtitle.video.decoder->resync(s);

  if(s->data.subtitle.cnv)
    bgav_subtitle_converter_reset(s->data.subtitle.cnv);
  
  if(s->data.subtitle.video.vsrc)
    gavl_video_source_reset(s->data.subtitle.video.vsrc);
  }

int bgav_subtitle_skipto(bgav_stream_t * s, int64_t * time, int scale)
  {
  return 0;
  }

const char * bgav_get_subtitle_info(bgav_t * b, int stream)
  {
  bgav_stream_t * s = bgav_track_get_subtitle_stream(b->tt->cur, stream);
  return gavl_dictionary_get_string(s->m, GAVL_META_LABEL);
  }

const bgav_metadata_t *
bgav_get_subtitle_metadata(bgav_t * b, int stream)
  {
  bgav_stream_t * s = bgav_track_get_subtitle_stream(b->tt->cur, stream);
  return s->m;
  }

const bgav_metadata_t *
bgav_get_text_metadata(bgav_t * b, int stream)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_text_stream(b->tt->cur, stream)))
    return NULL;
  return s->m;
  }

const bgav_metadata_t *
bgav_get_text_metadata_t(bgav_t * b, int track, int stream)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_text_stream(b->tt->tracks[track], stream)))
    return NULL;
  return s->m;
  }

const bgav_metadata_t *
bgav_get_overlay_metadata(bgav_t * b, int stream)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_overlay_stream(b->tt->cur, stream)))
    return NULL;
  return s->m;
  }

const bgav_metadata_t *
bgav_get_overlay_metadata_t(bgav_t * b, int track, int stream)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_overlay_stream(b->tt->tracks[track], stream)))
    return NULL;
  return s->m;
  }

gavl_packet_source_t *
bgav_get_text_packet_source(bgav_t * b, int stream)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_text_stream(b->tt->cur, stream)))
    return NULL;

  return s->psrc;
  }

const gavl_video_format_t * bgav_get_overlay_format(bgav_t * b, int stream)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_overlay_stream(b->tt->cur, stream)))
    return NULL;

  return s->data.subtitle.video.format;
  }

int bgav_get_text_timescale(bgav_t * b, int stream)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_text_stream(b->tt->cur, stream)))
    return -1;

  return s->timescale;
  }

gavl_packet_source_t *
bgav_get_overlay_packet_source(bgav_t * b, int stream)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_overlay_stream(b->tt->cur, stream)))
    return NULL;
  
  return s->psrc;
  }

gavl_video_source_t *
bgav_get_overlay_source(bgav_t * b, int stream)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_overlay_stream(b->tt->cur, stream)))
    return NULL;

  return s->data.subtitle.video.vsrc;
  }

int bgav_get_overlay_compression_info(bgav_t * bgav, int stream,
                                    gavl_compression_info_t * info)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_overlay_stream(bgav->tt->cur, stream)))
    return 0;
  return gavl_stream_get_compression_info(s->info, info);
  
  }

int bgav_set_overlay_compression_info(bgav_stream_t * s)
  {
  gavl_codec_id_t id = GAVL_CODEC_ID_NONE;
  
  //  bgav_track_get_compression(b->tt->cur);
  
  if(bgav_check_fourcc(s->fourcc, bgav_png_fourccs))
    id = GAVL_CODEC_ID_PNG;
  else if(bgav_check_fourcc(s->fourcc, bgav_dvdsub_fourccs))
    id = GAVL_CODEC_ID_DVDSUB;
  
  s->ci->id = id;

  if(s->codec_bitrate)
    s->ci->bitrate = s->codec_bitrate;
  else if(s->container_bitrate)
    s->ci->bitrate = s->container_bitrate;
  
  s->flags |= STREAM_GOT_CI;

  bgav_set_video_compression_info(s);
  
  return 1;
  }
