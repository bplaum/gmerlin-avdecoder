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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// #define DUMP_IN_PACKETS

int bgav_stream_start(bgav_stream_t * stream)
  {
  int result = 1;

  gavl_stream_set_stats(stream->info, &stream->stats);
  
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
    stream->initialized = 1;
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
  if(s->packet_buffer)
    bgav_packet_buffer_clear(s->packet_buffer);

  /* Clear possibly stored packets */
  if(s->packet)
    {
    bgav_packet_pool_put(s->pp, s->packet);
    s->packet = NULL;
    }
  if(s->out_packet_b)
    {
    bgav_packet_pool_put(s->pp, s->out_packet_b);
    s->out_packet_b = NULL;
    }
  if(s->psrc)
    {
    gavl_packet_source_destroy(s->psrc);
    s->psrc = NULL;
    }
  
  s->index_position = s->first_index_position;
  s->in_position = 0;
  s->out_time = 0;
  s->packet_seq = 0;

  if(s->demuxer)
    {
    s->src.data = s;
    s->src.get_func = bgav_demuxer_get_packet_read;
    s->src.peek_func = bgav_demuxer_peek_packet_read;
    }
  
  s->flags &= ~(STREAM_EOF_C|STREAM_EOF_D);
  
  STREAM_UNSET_SYNC(s);
  
  }

void bgav_stream_create_packet_buffer(bgav_stream_t * stream)
  {
  stream->packet_buffer = bgav_packet_buffer_create(stream->pp);
  }

void bgav_stream_create_packet_pool(bgav_stream_t * stream)
  {
  stream->pp = bgav_packet_pool_create();
  }

void bgav_stream_init(bgav_stream_t * stream, const bgav_options_t * opt)
  {
  memset(stream, 0, sizeof(*stream));
  STREAM_UNSET_SYNC(stream);
  stream->first_index_position = INT_MAX;

  /* need to set this to -1 so we know, if this stream has packets at all */
  stream->last_index_position = -1; 
  stream->index_position = -1;
  stream->opt = opt;

  /* the ci pointer might be changed by a bitstream filter */
  stream->ci = &stream->ci_orig;
  
  /* Better to have everything zero for a while */
  gavl_stream_stats_init(&stream->stats);
  }

void bgav_stream_free(bgav_stream_t * s)
  {
  /* Cleanup must be called as long as the other
     members are still functional */
  if(s->cleanup)
    s->cleanup(s);
  
  if(s->file_index)
    bgav_file_index_destroy(s->file_index);
  
  if(s->packet_buffer)
    bgav_packet_buffer_destroy(s->packet_buffer);

  if(((s->type == GAVL_STREAM_TEXT) ||
      (s->type == GAVL_STREAM_OVERLAY)) &&
     s->data.subtitle.subreader)
    bgav_subtitle_reader_destroy(s);

  if((s->type == GAVL_STREAM_TEXT) &&
     s->data.subtitle.charset)
    {
    free(s->data.subtitle.charset);
    }
  
  if(s->type == GAVL_STREAM_VIDEO)
    {
    if(s->data.video.pal.entries)
      free(s->data.video.pal.entries);
    }
  
  if(s->timecode_table)
    bgav_timecode_table_destroy(s->timecode_table);
  if(s->pp)
    bgav_packet_pool_destroy(s->pp);

  gavl_compression_info_free(&s->ci_orig);
  }

void bgav_stream_dump(bgav_stream_t * s)
  {
  switch(s->type)
    {
    case GAVL_STREAM_AUDIO:
      bgav_dprintf("============ Audio stream ============\n");
      break;
    case GAVL_STREAM_VIDEO:
      bgav_dprintf("============ Video stream ============\n");
      break;
    case GAVL_STREAM_TEXT:
      bgav_dprintf("=========== Text subtitles ===========\n");
      break;
    case GAVL_STREAM_OVERLAY:
      bgav_dprintf("========= Overlay subtitles ===========\n");
      break;
    case GAVL_STREAM_MSG:
      bgav_dprintf("==============  Messages  =============\n");
      break;
    case GAVL_STREAM_NONE:
      return;
    }

  bgav_dprintf("  Metadata:\n");
  gavl_dictionary_dump(s->m, 4);
  bgav_dprintf("\n");
  
  bgav_dprintf("  Fourcc:            ");
  bgav_dump_fourcc(s->fourcc);
  bgav_dprintf("\n");

  gavl_stream_stats_dump(&s->stats, 4);
  bgav_dprintf("\n");
 
  bgav_dprintf("  Stream ID:         %d (0x%x)\n",
          s->stream_id,
          s->stream_id);
  bgav_dprintf("  Codec bitrate:     ");
  if(s->codec_bitrate == GAVL_BITRATE_VBR)
    bgav_dprintf("Variable\n");
  else if(s->codec_bitrate)
    bgav_dprintf("%d\n", s->codec_bitrate);
  else
    bgav_dprintf("Unspecified\n");

  bgav_dprintf("  Container bitrate: ");
  if(s->container_bitrate == GAVL_BITRATE_VBR)
    bgav_dprintf("Variable\n");
  else if(s->container_bitrate)
    bgav_dprintf("%d\n", s->container_bitrate);
  else
    bgav_dprintf("Unspecified\n");

  bgav_dprintf("  Timescale:         %d\n", s->timescale);
  bgav_dprintf("  MaxPacketSize:     ");
  if(s->ci->max_packet_size)
    bgav_dprintf("%d\n", s->ci->max_packet_size);
  else
    bgav_dprintf("Unknown\n");
  
  // bgav_dprintf("  Private data:      %p\n", s->priv);
  bgav_dprintf("  Codec header:      %d bytes\n", s->ci->global_header_len);
  }

void bgav_stream_clear(bgav_stream_t * s)
  {
  if(s->packet_buffer)
    bgav_packet_buffer_clear(s->packet_buffer);
  if(s->packet)
    {
    bgav_packet_pool_put(s->pp, s->packet);
    s->packet = NULL;
    }
  if(s->out_packet_b)
    {
    bgav_packet_pool_put(s->pp, s->out_packet_b);
    s->out_packet_b = NULL;
    }
  
  s->in_position  = 0;
  s->out_time = GAVL_TIME_UNDEFINED;
  STREAM_UNSET_SYNC(s);
  s->flags &= ~(STREAM_EOF_C|STREAM_EOF_D);

  s->index_position  = -1;
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
  return bgav_packet_pool_get(s->pp);
  }

void bgav_stream_done_packet_write(bgav_stream_t * s, bgav_packet_t * p)
  {
#ifdef DUMP_IN_PACKETS
  bgav_dprintf("Packet in (stream %d): ", s->stream_id);
  bgav_packet_dump(p);
#endif
  s->in_position++;

  /* If the stream has a constant framerate, all packets have the same
     duration */
  if(s->type == GAVL_STREAM_VIDEO)
    {
    if((s->data.video.format->frame_duration) &&
       (s->data.video.format->framerate_mode == GAVL_FRAMERATE_CONSTANT) &&
       !p->duration)
      p->duration = s->data.video.format->frame_duration;

    if(s->data.video.pal.size && !s->data.video.pal.sent)
      {
      bgav_packet_alloc_palette(p, s->data.video.pal.size);
      memcpy(p->palette, s->data.video.pal.entries,
             s->data.video.pal.size * sizeof(*p->palette));
      s->data.video.pal.sent = 1;
      }
    }
  /* Padding (if fourcc != gavl) */
  if(p->data)
    memset(p->data + p->data_size, 0, GAVL_PACKET_PADDING);

  /* Set timestamps from file index because the
     demuxer might have them messed up */
  if((s->action != BGAV_STREAM_PARSE) && s->file_index)
    {
    p->position = s->index_position;
    s->index_position++;
    }
  
  bgav_packet_buffer_append(s->packet_buffer, p);
  }

int bgav_stream_get_index(bgav_stream_t * s)
  {
  int i;
  int ret = 0;

  for(i = 0; i < s->track->num_streams; i++)
    {
    if(s->track->streams[i].type == s->type)
      {
      if(&s->track->streams[i]== s)
        return ret;
      else
        ret++;
      }
    }
  
  return -1;
  }

gavl_source_status_t
bgav_stream_get_packet_read(bgav_stream_t * s, bgav_packet_t ** ret)
  {
  bgav_packet_t * p = NULL;
  gavl_source_status_t st;
  
  if((st = s->src.get_func(s->src.data, &p)) != GAVL_SOURCE_OK)
    {
    // fprintf(stderr, "bgav_stream_get_packet_read returned %d\n", st);
    return st;
    }
  if(s->timecode_table)
    p->tc =
      bgav_timecode_table_get_timecode(s->timecode_table,
                                       p->pts);
  if(s->opt->dump_packets)
    {
    bgav_dprintf("Packet out (stream %d): ", s->stream_id);
    bgav_packet_dump(p);
    gavl_hexdump(p->data, p->data_size < 16 ? p->data_size : 16, 16);
    }
  
  if(s->max_packet_size_tmp < p->data_size)
    s->max_packet_size_tmp = p->data_size;
  
  *ret = p;

  if(s->action == BGAV_STREAM_PARSE)
    {
    gavl_stream_stats_update_params(&s->stats,
                                    p->pts, p->duration, p->data_size,
                                    p->flags & 0x0000ffff);
    }
    
  return GAVL_SOURCE_OK;
  }

gavl_source_status_t
bgav_stream_peek_packet_read(bgav_stream_t * s, bgav_packet_t ** p,
                             int force)
  {
  return s->src.peek_func(s->src.data, p, force);
  }

void bgav_stream_done_packet_read(bgav_stream_t * s, bgav_packet_t * p)
  {
  /* If no packet pool is there, we assume the packet will be
     freed somewhere else */
  if(s->pp)
    bgav_packet_pool_put(s->pp, p);
  }

/* Read one packet from an A/V stream */

gavl_source_status_t
bgav_stream_read_packet_func(void * sp, gavl_packet_t ** p)
  {
  gavl_source_status_t st;
  bgav_stream_t * s = sp;
  
  if(s->out_packet_b)
    {
    bgav_stream_done_packet_read(s, s->out_packet_b);
    s->out_packet_b = NULL;
    }
  
  if(s->flags & STREAM_DISCONT)
    {
    /* Check if we have a packet at all */
    if((st = bgav_stream_peek_packet_read(s, NULL, 0)) != GAVL_SOURCE_OK)
      {
      if(s->flags & STREAM_EOF_D)
        st = GAVL_SOURCE_EOF;        
      return st;
      }
    }

  if((st = bgav_stream_get_packet_read(s, &s->out_packet_b)) != GAVL_SOURCE_OK)
    return st;
  
  bgav_packet_2_gavl(s->out_packet_b, &s->out_packet_g);

  //  gavl_packet_dump(&s->out_packet_g);
  //  bgav_packet_dump(s->out_packet_b);
  

  *p = &s->out_packet_g;
  return GAVL_SOURCE_OK;
  }

void bgav_stream_set_extradata(bgav_stream_t * s,
                               const uint8_t * data, int len)
  {
  if(len <= 0)
    return;

  s->ci->global_header_len = len;
  s->ci->global_header = malloc(s->ci->global_header_len+16);
  
  memcpy(s->ci->global_header, data, len);
  memset(s->ci->global_header + len, 0, 16);
  }

void bgav_stream_set_from_gavl(bgav_stream_t * s,
                               gavl_dictionary_t * dict)
  {
  s->info = dict;
  s->m = gavl_stream_get_metadata_nc(dict);
  
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
                         
int bgav_streams_foreach(bgav_stream_t * s, int num,
                         int (*action)(void * priv, bgav_stream_t * s), void * priv)
  {
  int i;
  for(i = 0; i < num; i++)
    {
    if(!action(priv, s + i))
      return 0;
    }
  return 1;
  } 
