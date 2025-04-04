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



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <avdec_private.h>
#include <parser.h>

static void bgav_stream_set_timing(bgav_stream_t * s);

// #define DUMP_IN_PACKETS

int bgav_stream_start(bgav_stream_t * stream)
  {
  int result = 1;
  
  switch(stream->type)
    {
    case GAVL_STREAM_VIDEO:
      result = bgav_video_start(stream);
      break;
    case GAVL_STREAM_AUDIO:
      result = bgav_audio_start(stream);
      break;
    case GAVL_STREAM_OVERLAY:
      result = bgav_overlay_start(stream);
      break;
    case GAVL_STREAM_TEXT:
      result = bgav_text_start(stream);
      break;
    default:
      break;
    }

  if(result)
    stream->flags |= STREAM_STARTED;
  return result;
  }

int bgav_stream_init_read(bgav_stream_t * stream)
  {
  int result = 1;
  
  if(!stream->parser)
    {
    gavl_stream_set_compression_info(stream->info, stream->ci);
    gavl_stream_set_compression_tag(stream->info, stream->fourcc);
    }
  
  switch(stream->type)
    {
    case GAVL_STREAM_VIDEO:
      result = bgav_video_init(stream);
      break;
    case GAVL_STREAM_AUDIO:
      result = bgav_audio_init(stream);
      break;
    case GAVL_STREAM_OVERLAY:
      result = bgav_overlay_init(stream);
      break;
    case GAVL_STREAM_TEXT:
      result = bgav_text_init(stream);
      break;
    default:
      break;
    }

  return result;
  }


int64_t bgav_stream_get_duration(bgav_stream_t * s)
  {
  int64_t ret;
  if(s->stats.pts_end == GAVL_TIME_UNDEFINED)
    return GAVL_TIME_UNDEFINED;
  
  ret = s->stats.pts_end;

  if(s->stats.pts_start != GAVL_TIME_UNDEFINED)
    ret -= s->stats.pts_start;
  return ret;
  }  

void bgav_stream_stop(bgav_stream_t * s)
  {
  if((s->action == BGAV_STREAM_DECODE) ||
     (s->action == BGAV_STREAM_PARSE) ||
     (s->action == BGAV_STREAM_READRAW))
    {
    switch(s->type)
      {
      case GAVL_STREAM_VIDEO:
        bgav_video_stop(s);
        break;
      case GAVL_STREAM_AUDIO:
        bgav_audio_stop(s);
        break;
      case GAVL_STREAM_TEXT:
      case GAVL_STREAM_OVERLAY:
        bgav_subtitle_stop(s);
      default:
        break;
      }
    }

  bgav_stream_clear(s);
  s->index_position = 0;
  
  }

static int create_parser(bgav_stream_t * s)
  {
  gavl_stream_set_compression_info(s->info, s->ci);
  gavl_stream_set_compression_tag(s->info, s->fourcc);

  if(s->timescale)
    gavl_dictionary_set_int(s->m, GAVL_META_STREAM_PACKET_TIMESCALE, s->timescale);
  
  /* Create parser */
  if((s->parser = bgav_packet_parser_create(s->info, s->flags, s->ci)) &&
     (s->psink = bgav_packet_parser_connect(s->parser, s->psink)))
    return 1;
  else
    return 0;
  }

int bgav_stream_set_parse_full(bgav_stream_t * s)
  {
  if(s->parser)
    return 1;
  
  s->flags |= STREAM_PARSE_FULL;
  return create_parser(s);
  }

int bgav_stream_set_parse_frame(bgav_stream_t * s)
  {
  if(s->parser)
    return 1;
  s->flags |= STREAM_PARSE_FRAME;
  return create_parser(s);
  }

void bgav_stream_flush(bgav_stream_t * s)
  {
  if(s->packet)
    {
    bgav_stream_done_packet_write(s, s->packet);
    s->packet = NULL;
    }
  if(s->parser)
    bgav_packet_parser_flush(s->parser);

  if(s->pbuffer)
    gavl_packet_buffer_flush(s->pbuffer);

#if 0
  switch(s->type)
    {
    case GAVL_STREAM_VIDEO:
      result = bgav_video_flush(stream);
      break;
    case GAVL_STREAM_AUDIO:
      result = bgav_audio_flush(stream);
      break;
    case GAVL_STREAM_OVERLAY:
      result = bgav_overlay_start(stream);
      break;
    case GAVL_STREAM_TEXT:
      result = bgav_text_start(stream);
      break;
    default:
      break;
    }
#endif
  }

void bgav_stream_clear(bgav_stream_t * s)
  {
  /* Clear possibly stored packets */
  if(s->pbuffer)
    gavl_packet_buffer_clear(s->pbuffer);

  if(s->parser)
    bgav_packet_parser_reset(s->parser);
  
  if(s->packet)
    s->packet = NULL;

  if(s->pf)
    bgav_packet_filter_reset(s->pf);
  
  if(s->psrc_priv)
    gavl_packet_source_reset(s->psrc_priv);
  
  s->in_position  = 0;
  s->out_time = GAVL_TIME_UNDEFINED;
  STREAM_UNSET_SYNC(s);
  s->flags &= ~(STREAM_EOF_C|STREAM_EOF_D);
  s->packet_seq = 0;

  //  if(s->flags & STREAM_NEED_START_PTS)
  //    s->stats.pts_start = GAVL_TIME_UNDEFINED;
  
  s->index_position  = -1;
  }

static gavl_source_status_t
read_packet_continuous(void * priv, bgav_packet_t ** ret)
  {
  gavl_source_status_t st;
  gavl_source_status_t st1;
  bgav_stream_t * s = priv;

  while((st = gavl_packet_source_read_packet(gavl_packet_buffer_get_source(s->pbuffer), ret))
        == GAVL_SOURCE_AGAIN)
    {
    bgav_demuxer_context_t * demuxer;

    demuxer = s->demuxer;
    
    if((s->flags & STREAM_DISCONT) && !(s->demuxer->flags & BGAV_DEMUXER_PEEK_FORCES_READ))
      return st;
    
    demuxer->request_stream = s;

    st1 = bgav_demuxer_next_packet(demuxer);
    demuxer->request_stream = NULL;
    
    if(st1 != GAVL_SOURCE_OK)
      return st1; // Return for now
    }
  
  if((st == GAVL_SOURCE_OK) && s->opt->dump_packets)
    {
    gavl_dprintf("Packet out (stream %d): ", s->stream_id);
    gavl_packet_dump(*ret);
    //    gavl_hexdump((*ret)->buf.buf, (*ret)->buf.len < 16 ? (*ret)->buf.len : 16, 16);
    }
  
  return st;
  }


void bgav_stream_create_packet_buffer(bgav_stream_t * stream)
  {
  stream->pbuffer = gavl_packet_buffer_create(stream->info);

  gavl_packet_buffer_set_calc_frame_durations(stream->pbuffer, 1);
  
  stream->psink   = gavl_packet_buffer_get_sink(stream->pbuffer);

  stream->psrc_priv = gavl_packet_source_create(read_packet_continuous,
                                                stream, GAVL_SOURCE_SRC_ALLOC, stream->info);
  stream->psrc    = stream->psrc_priv;
  }


void bgav_stream_init(bgav_stream_t * stream, const bgav_options_t * opt)
  {
  memset(stream, 0, sizeof(*stream));
  STREAM_UNSET_SYNC(stream);

  stream->index_position = -1;
  stream->opt = opt;

  /* the ci pointer might be changed by a bitstream filter */
  stream->ci = &stream->ci_orig;
  
  gavl_stream_stats_init(&stream->stats);
  }

void bgav_stream_free(bgav_stream_t * s)
  {
  /* Cleanup must be called as long as the other
     members are still functional */
  if(s->cleanup)
    s->cleanup(s);

  if(s->pbuffer)
    gavl_packet_buffer_destroy(s->pbuffer);
  
  if((s->type == GAVL_STREAM_TEXT) &&
     s->data.subtitle.charset)
    {
    free(s->data.subtitle.charset);
    }
  
  if(s->type == GAVL_STREAM_VIDEO)
    {
    if(s->data.video.pal)
      gavl_palette_destroy(s->data.video.pal);
    }
  
  if(s->timecode_table)
    bgav_timecode_table_destroy(s->timecode_table);

  if(s->parser)
    bgav_packet_parser_destroy(s->parser);
  if(s->pf)
    bgav_packet_filter_destroy(s->pf);

  if(s->psrc_priv)
    gavl_packet_source_destroy(s->psrc_priv);
  
  gavl_compression_info_free(&s->ci_orig);
  }

void bgav_stream_dump(bgav_stream_t * s)
  {
  switch(s->type)
    {
    case GAVL_STREAM_AUDIO:
      gavl_dprintf("============ Audio stream ============\n");
      break;
    case GAVL_STREAM_VIDEO:
      gavl_dprintf("============ Video stream ============\n");
      break;
    case GAVL_STREAM_TEXT:
      gavl_dprintf("=========== Text subtitles ===========\n");
      break;
    case GAVL_STREAM_OVERLAY:
      gavl_dprintf("========= Overlay subtitles ===========\n");
      break;
    case GAVL_STREAM_MSG:
      gavl_dprintf("==============  Messages  =============\n");
      break;
    case GAVL_STREAM_NONE:
      return;
    }

  gavl_dprintf("  Metadata:\n");
  gavl_dictionary_dump(s->m, 4);
  gavl_dprintf("\n");
  
  gavl_dprintf("  Fourcc:            ");
  bgav_dump_fourcc(s->fourcc);
  gavl_dprintf("\n");

  gavl_stream_stats_dump(&s->stats, 4);
  gavl_dprintf("\n");
 
  gavl_dprintf("  Stream ID:         %d (0x%x)\n",
          s->stream_id,
          s->stream_id);
  gavl_dprintf("  Codec bitrate:     ");
  if(s->codec_bitrate == GAVL_BITRATE_VBR)
    gavl_dprintf("Variable\n");
  else if(s->codec_bitrate)
    gavl_dprintf("%d\n", s->codec_bitrate);
  else
    gavl_dprintf("Unspecified\n");

  gavl_dprintf("  Container bitrate: ");
  if(s->container_bitrate == GAVL_BITRATE_VBR)
    gavl_dprintf("Variable\n");
  else if(s->container_bitrate)
    gavl_dprintf("%d\n", s->container_bitrate);
  else
    gavl_dprintf("Unspecified\n");

  gavl_dprintf("  Timescale:         %d\n", s->timescale);
  
  gavl_dprintf("  Compression info:\n");
  gavl_compression_info_dumpi(s->ci, 0);
  
  // gavl_dprintf("  Private data:      %p\n", s->priv);
  //  gavl_dprintf("  Codec header:      %d bytes\n", s->ci->codec_header.len);
  }

int bgav_stream_skipto(bgav_stream_t * s, gavl_time_t * time, int scale)
  {
  if(s->action != BGAV_STREAM_DECODE)
    return 1;
  
  switch(s->type)
    {
    case GAVL_STREAM_AUDIO:
      return bgav_audio_skipto(s, time, scale);
      break;
    case GAVL_STREAM_VIDEO:
      return bgav_video_skipto(s, time, scale);
      break;
    case GAVL_STREAM_TEXT:
    case GAVL_STREAM_OVERLAY:
      return bgav_subtitle_skipto(s, time, scale);
      break;
    case GAVL_STREAM_NONE:
    case GAVL_STREAM_MSG:
      break;
    }
  return 0;
  }

bgav_packet_t * bgav_stream_get_packet_write(bgav_stream_t * s)
  {
  //  if(s->type == GAVL_STREAM_VIDEO)
  //    fprintf(stderr, "bgav_stream_get_packet_write\n");
      
  return gavl_packet_sink_get_packet(s->psink);
  }

void bgav_stream_done_packet_write(bgav_stream_t * s, bgav_packet_t * p)
  {
#ifdef DUMP_IN_PACKETS
  gavl_dprintf("Packet in (stream %d): ", s->stream_id);
  gavl_packet_dump(p);
#endif

  p->id = s->stream_id;
  
  s->in_position++;

  if(!(s->flags & STREAM_WRITE_STARTED))
    {
    bgav_stream_set_timing(s);
    s->flags |= STREAM_WRITE_STARTED;
    }
  
  if(s->type == GAVL_STREAM_VIDEO)
    {
    /* If the stream has a constant framerate, all packets have the same
       duration */
    
    if((s->data.video.format->frame_duration) &&
       (s->data.video.format->framerate_mode == GAVL_FRAMERATE_CONSTANT) &&
       (p->duration <= 0))
      p->duration = s->data.video.format->frame_duration;

    /* Send palette */

    if(s->data.video.pal && !s->data.video.pal_sent)
      {
      gavl_palette_t * pal = gavl_packet_add_extradata(p, GAVL_PACKET_EXTRADATA_PALETTE);
      gavl_palette_alloc(pal, s->data.video.pal->num_entries);
      memcpy(pal->entries, s->data.video.pal->entries,
             s->data.video.pal->num_entries * sizeof(*pal->entries));
      s->data.video.pal_sent = 1;
      }
    }
  else // All non-video streams have only I-frames (hopefully)
    p->flags |= GAVL_PACKET_KEYFRAME;
  
  /* Padding (if fourcc != gavl) */
  if(p->buf.buf)
    {
    gavl_buffer_alloc(&p->buf, p->buf.len + GAVL_PACKET_PADDING);
    memset(p->buf.buf + p->buf.len, 0, GAVL_PACKET_PADDING);
    }
#if 1
  if((s->flags & STREAM_DTS_ONLY) && (p->pts != GAVL_TIME_UNDEFINED))
    {
    p->dts = p->pts;
    p->pts = GAVL_TIME_UNDEFINED;
    }
#endif

  //  if(s->type == GAVL_STREAM_VIDEO)
  //    fprintf(stderr, "bgav_stream_done_packet_write\n");
  
  gavl_packet_sink_put_packet(s->psink, p);
  }

static void set_sample_timescale(bgav_stream_t * s, int scale)
  {
  if(!s->timescale)
    s->timescale = scale;
  gavl_dictionary_set_int(s->m, GAVL_META_STREAM_SAMPLE_TIMESCALE, scale);
  }

static void bgav_stream_set_timing(bgav_stream_t * s)
  {
  //  int sample_timescale = 0;
  switch(s->type)
    {
    case GAVL_STREAM_AUDIO:
      if(s->data.audio.format->samplerate)
        set_sample_timescale(s, s->data.audio.format->samplerate);
      break;
    case GAVL_STREAM_VIDEO:
      if(s->data.video.format->timescale)
        set_sample_timescale(s, s->data.video.format->timescale);
      break;
    default:
      break;
    }
  
  if(s->timescale)
    gavl_dictionary_set_int(s->m, GAVL_META_STREAM_PACKET_TIMESCALE, s->timescale);
  
  }



gavl_source_status_t
bgav_stream_get_packet_read(bgav_stream_t * s, bgav_packet_t ** ret)
  {
  gavl_source_status_t st;

  //  fprintf(stderr, "bgav_stream_get_packet_read\n");
  
  if((st = gavl_packet_source_read_packet(s->psrc, ret)) != GAVL_SOURCE_OK)
    {
    // fprintf(stderr, "bgav_stream_get_packet_read returned %d\n", st);
    return st;
    }
  if(s->timecode_table)
    (*ret)->timecode =
      bgav_timecode_table_get_timecode(s->timecode_table,
                                       (*ret)->pts);
  
  //  if((*ret)->pts + (*ret)->duration > s->stats.pts_end)
  //    (*ret)->duration = s->stats.pts_end - (*ret)->pts;
  
  if((s->flags & STREAM_DEMUXER_SETS_PTS_END) &&
     ((*ret)->pts + (*ret)->duration > s->stats.pts_end))
    {
    (*ret)->duration = s->stats.pts_end - (*ret)->pts;
    gavl_dprintf("Limiting last duration: %"PRId64"\n", (*ret)->duration);
    }
  
  //  fprintf(stderr, "bgav_stream_get_packet_read %p\n", *ret);
  
  return GAVL_SOURCE_OK;
  }

gavl_source_status_t
bgav_stream_peek_packet_read(bgav_stream_t * s, bgav_packet_t ** p)
  {
  gavl_source_status_t st;

  //  fprintf(stderr, "bgav_stream_peek_packet_read\n");

  if((st = gavl_packet_source_peek_packet(s->psrc, p)) != GAVL_SOURCE_OK)
    {
    // fprintf(stderr, "bgav_stream_get_packet_read returned %d\n", st);
    return st;
    }
  if(!p)
    return st;
  
  if(s->timecode_table)
    (*p)->timecode =
      bgav_timecode_table_get_timecode(s->timecode_table,
                                       (*p)->pts);
  
  //  if((*p)->pts + (*p)->duration > s->stats.pts_end)
  //    (*p)->duration = s->stats.pts_end - (*p)->pts;
  //  fprintf(stderr, "bgav_stream_peek_packet_read %p\n", *p);
  
  return st;
  }

void bgav_stream_done_packet_read(bgav_stream_t * s, bgav_packet_t * p)
  {
  /* Nop */ 
  //  fprintf(stderr, "bgav_stream_done_packet_read %p\n", p);
  }


void bgav_stream_set_extradata(bgav_stream_t * s,
                               const uint8_t * data, int len)
  {
  if(len <= 0)
    return;
  
  gavl_buffer_reset(&s->ci->codec_header);
  gavl_buffer_append_data_pad(&s->ci->codec_header, data, len, GAVL_PACKET_PADDING);
  }

void bgav_stream_set_from_gavl(bgav_stream_t * s,
                               gavl_dictionary_t * dict)
  {
  s->info = dict;
  s->info_ext = dict;
  
  s->m = gavl_stream_get_metadata_nc(dict);

  gavl_stream_get_compression_info(dict, &s->ci_orig);
  s->ci = &s->ci_orig;
  
  switch(s->type)
    {
    case GAVL_STREAM_AUDIO:
      s->data.audio.format = gavl_stream_get_audio_format_nc(dict);
      gavl_stream_get_compression_info(dict, s->ci);
      s->timescale = s->data.audio.format->samplerate;
      break;
    case GAVL_STREAM_VIDEO:
      s->data.video.format = gavl_stream_get_video_format_nc(dict);
      s->timescale = s->data.video.format->timescale;

      gavl_stream_get_compression_info(dict, s->ci);
      break;
    case GAVL_STREAM_TEXT:
      break;
    case GAVL_STREAM_OVERLAY:
      s->data.subtitle.video.format = gavl_stream_get_video_format_nc(dict);
      gavl_stream_get_compression_info(dict, s->ci);
      s->timescale = s->data.subtitle.video.format->timescale;
      break;
    case GAVL_STREAM_MSG:
    case GAVL_STREAM_NONE:
      break;
    }
  
  s->fourcc = bgav_compression_id_2_fourcc(s->ci->id);
  s->container_bitrate = s->ci->bitrate;
  }

int bgav_streams_foreach(bgav_stream_t ** s, int num,
                         int (*action)(void * priv, bgav_stream_t * s), void * priv)
  {
  int i;
  for(i = 0; i < num; i++)
    {
    if(!action(priv, s[i]))
      return 0;
    }
  return 1;
  } 

gavl_sink_status_t bgav_stream_put_packet_get_duration(void * priv, gavl_packet_t * p)
  {
  bgav_stream_t * s = priv;

  if(!(s->flags & STREAM_DEMUXER_SETS_PTS_END))
    gavl_stream_stats_update_end(&s->stats, p);
  
  return GAVL_SINK_OK;
  }

gavl_sink_status_t bgav_stream_put_packet_parse(void * priv, gavl_packet_t * p)
  {
  bgav_stream_t * s = priv;
  gavl_stream_stats_update(&s->stats, p);
  
  gavl_packet_index_add_packet(s->demuxer->si, p);
  
  return GAVL_SINK_OK;
  }
