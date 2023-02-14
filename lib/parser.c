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
#include <string.h>


#include <config.h>
#include <avdec_private.h>
#include <parser.h>

#define LOG_DOMAIN "packetparser"


static const struct
  {
  uint32_t fourcc;
  void (*func)(bgav_packet_parser_t * parser);
  }
parsers[] =
  {
    /* Audio */
    
    { BGAV_WAVID_2_FOURCC(0x0050), bgav_packet_parser_init_mpeg },
    { BGAV_WAVID_2_FOURCC(0x0055), bgav_packet_parser_init_mpeg },
    { BGAV_MK_FOURCC('.','m','p','1'), bgav_packet_parser_init_mpeg },
    { BGAV_MK_FOURCC('.','m','p','2'), bgav_packet_parser_init_mpeg },
    { BGAV_MK_FOURCC('.','m','p','3'), bgav_packet_parser_init_mpeg },
    { BGAV_MK_FOURCC('m','p','g','a'), bgav_packet_parser_init_mpeg },
    { BGAV_MK_FOURCC('L','A','M','E'), bgav_packet_parser_init_mpeg },
    { BGAV_WAVID_2_FOURCC(0x2000), bgav_packet_parser_init_a52 },
    { BGAV_MK_FOURCC('.','a','c','3'), bgav_packet_parser_init_a52 },
    { BGAV_MK_FOURCC('a','c','-','3'), bgav_packet_parser_init_a52 },
#ifdef HAVE_DCA
    { BGAV_MK_FOURCC('d','t','s',' '), bgav_packet_parser_init_dca },
#endif
#ifdef HAVE_VORBIS
    { BGAV_MK_FOURCC('V','B','I','S'), bgav_packet_parser_init_vorbis },
#endif
    { BGAV_MK_FOURCC('F','L','A','C'), bgav_packet_parser_init_flac },
    { BGAV_MK_FOURCC('F','L','C','N'), bgav_packet_parser_init_flac },
#ifdef HAVE_OPUS
    { BGAV_MK_FOURCC('O','P','U','S'), bgav_packet_parser_init_opus },
#endif
#ifdef HAVE_SPEEX
    { BGAV_MK_FOURCC('S','P','E','X'), bgav_packet_parser_init_speex },
#endif
#ifdef HAVE_THEORADEC
    { BGAV_MK_FOURCC('T','H','R','A'), bgav_packet_parser_init_theora },
#endif
    { BGAV_MK_FOURCC('A','D','T','S'), bgav_packet_parser_init_adts },
    { BGAV_MK_FOURCC('A','A','C','P'), bgav_packet_parser_init_adts },

    /* Video */
    
    { BGAV_MK_FOURCC('H', '2', '6', '4'), bgav_packet_parser_init_h264 },
    { BGAV_MK_FOURCC('a', 'v', 'c', '1'), bgav_packet_parser_init_h264 },
    { BGAV_MK_FOURCC('m', 'p', 'g', 'v'), bgav_packet_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'p', 'v', '1'), bgav_packet_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'p', 'v', '2'), bgav_packet_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'x', '3', 'p'), bgav_packet_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'x', '4', 'p'), bgav_packet_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'x', '5', 'p'), bgav_packet_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'x', '3', 'n'), bgav_packet_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'x', '4', 'n'), bgav_packet_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'x', '5', 'n'), bgav_packet_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'p', '4', 'v'), bgav_packet_parser_init_mpeg4 },
    /* DivX Variants */
    { BGAV_MK_FOURCC('D', 'I', 'V', 'X'), bgav_packet_parser_init_mpeg4 },
    { BGAV_MK_FOURCC('d', 'i', 'v', 'x'), bgav_packet_parser_init_mpeg4 },
    { BGAV_MK_FOURCC('D', 'X', '5', '0'), bgav_packet_parser_init_mpeg4 },
    { BGAV_MK_FOURCC('X', 'V', 'I', 'D'), bgav_packet_parser_init_mpeg4 },
    { BGAV_MK_FOURCC('x', 'v', 'i', 'd'), bgav_packet_parser_init_mpeg4 },
    { BGAV_MK_FOURCC('F', 'M', 'P', '4'), bgav_packet_parser_init_mpeg4 },
    { BGAV_MK_FOURCC('f', 'm', 'p', '4'), bgav_packet_parser_init_mpeg4 },
    /* */
    { BGAV_MK_FOURCC('C', 'A', 'V', 'S'), bgav_packet_parser_init_cavs },
    { BGAV_MK_FOURCC('V', 'C', '-', '1'), bgav_packet_parser_init_vc1 },
    { BGAV_MK_FOURCC('d', 'r', 'a', 'c'), bgav_packet_parser_init_dirac },
    { BGAV_MK_FOURCC('m', 'j', 'p', 'a'), bgav_packet_parser_init_mjpa },
    { BGAV_MK_FOURCC('j', 'p', 'e', 'g'), bgav_packet_parser_init_jpeg },
    { BGAV_MK_FOURCC('B', 'B', 'C', 'D'), bgav_packet_parser_init_dirac },
    { BGAV_MK_FOURCC('V', 'P', '8', '0'), bgav_packet_parser_init_vp8 },
    { BGAV_MK_FOURCC('V', 'P', '9', '0'), bgav_packet_parser_init_vp9 },
    
    { BGAV_MK_FOURCC('D', 'V', 'D', 'S'), bgav_packet_parser_init_dvdsub },
    { BGAV_MK_FOURCC('m', 'p', '4', 's'), bgav_packet_parser_init_dvdsub },
    { /* End */ }
  };

static void parser_flush_bytes(bgav_packet_parser_t * p)
  {
  int i;
  int num_del = 0;
  int bytes = p->buf.pos;
  
  if(!bytes)
    return;
  
  gavl_buffer_flush(&p->buf, bytes);

  for(i = 0; i < p->num_packets; i++)
    {
    if(p->packets[i].size > bytes)
      {
      p->packets[i].size -= bytes;
      break;
      }
    else
      {
      bytes -= p->packets[i].size;
      p->packets[i].size = 0;
      num_del++;
      if(!bytes)
        break;
      }
    }

  if(num_del > 0)
    {
    if(num_del < p->num_packets)
      memmove(p->packets, p->packets + num_del, sizeof(*p->packets) * (p->num_packets - num_del));
    p->num_packets -= num_del;
    }
  
  }

static int do_parse_frame(bgav_packet_parser_t * p, gavl_packet_t * pkt)
  {
  if(!p->parse_frame(p, pkt))
    return 0;
  
  /* Set format and compression */
  if(!(p->parser_flags & PARSER_HAS_HEADER))
    {
    gavl_stream_set_compression_info(p->info, &p->ci);
    gavl_stream_set_default_packet_timescale(p->info);
    gavl_stream_set_sample_timescale(p->info);
    
    p->parser_flags |= PARSER_HAS_HEADER;
    }

  /* Set keyframe flag */
  if(PACKET_GET_CODING_TYPE(pkt) == BGAV_CODING_TYPE_I)
    PACKET_SET_KEYFRAME(pkt);

  fprintf(stderr, "Parsed frame\n");
  gavl_packet_dump(pkt);
  
  return 1;
  }


/* Parse frame */

static gavl_packet_t * sink_get_func_frame(void * priv)
  {
  bgav_packet_parser_t * p = priv;
  return gavl_packet_sink_get_packet(p->next);
  }

static gavl_sink_status_t sink_put_func_frame(void * priv, gavl_packet_t * pkt)
  {
  bgav_packet_parser_t * p = priv;
  
  if(!do_parse_frame(p, pkt))
    return GAVL_SINK_ERROR;
  
  return gavl_packet_sink_put_packet(p->next, pkt);
  }

/* Parse full */
static gavl_packet_t * sink_get_func_full(void * priv)
  {
  bgav_packet_parser_t * p = priv;
  return &p->in_packet;
  }

static gavl_sink_status_t sink_put_func_full(void * priv, gavl_packet_t * pkt)
  {
  int skip = 0;
  packet_info_t * pi;
  bgav_packet_parser_t * p = priv;
  
  /* Append packet */
  gavl_buffer_append(&p->buf, &pkt->buf);

  if(p->num_packets == p->packets_alloc)
    {
    p->packets_alloc += 16;
    p->packets = realloc(p->packets, sizeof(*p->packets) * p->packets_alloc);
    memset(p->packets + p->num_packets, 0,
           sizeof(*p->packets) * (p->packets_alloc - p->num_packets));
    }
  pi = p->packets + p->num_packets;
  p->num_packets++;

  if(pkt->pts != GAVL_TIME_UNDEFINED)
    pi->pts = pkt->pts;
  else if(pkt->pes_pts != GAVL_TIME_UNDEFINED)
    pi->pts = pkt->pes_pts;
  
  pi->position = pkt->position;
  pi->size = pkt->buf.len;

  /*
   *   Check if we have a chance to find a frame boundary
   *
   *   The mpegaudio parser e.g. knows the size of the full frame
   *   already after reading the first 4 bytes.
   */
  
  if(p->buf.pos >= p->buf.len)
    return GAVL_SINK_OK;
  
  /* Try to flush packets */
  
  while(p->find_frame_boundary(p, &skip))
    {
    if(!(p->parser_flags & PARSER_HAS_SYNC))
      {
      /* Skip undecodeable bytes */
      p->parser_flags |= PARSER_HAS_SYNC;
      }
    else
      {
      /* Get new packet */
      gavl_packet_t * pkt = gavl_packet_sink_get_packet(p->next);
      gavl_buffer_append_data_pad(&pkt->buf, p->buf.buf, p->buf.pos, GAVL_PACKET_PADDING);
      
      /* Set pts */
      if(p->packets[0].pts != GAVL_TIME_UNDEFINED)
        {
        pkt->pes_pts = p->packets[0].pts;
        pkt->position = p->packets[0].position;

        /* Don't use this pts for other frames */
        p->packets[0].pts = GAVL_TIME_UNDEFINED;
        p->packets[0].position = -1;
        }

      /* Parse frame (must be done *after* pes_pts is set) */
      if(!do_parse_frame(p, pkt))
        return GAVL_SINK_ERROR;
      
      if(gavl_packet_sink_put_packet(p->next, pkt) != GAVL_SINK_OK)
        return GAVL_SINK_ERROR;
      }
    parser_flush_bytes(p);
    p->buf.pos = skip;
    }
  
  return GAVL_SINK_OK;
  }

/* */

gavl_packet_sink_t * bgav_packet_parser_connect(bgav_packet_parser_t * p,
                                                gavl_packet_sink_t * dst)
  {
  p->next =dst;
  return p->sink;
  }

bgav_packet_parser_t * bgav_packet_parser_create(gavl_dictionary_t * stream_info,
                                                 int stream_flags)
  {
  bgav_packet_parser_t * ret = NULL;
  int idx = 0;
  int fourcc = gavl_stream_get_compression_tag(stream_info);
  
  /* Find parsers */
  while(parsers[idx].fourcc)
    {
    if(parsers[idx].fourcc == fourcc)
      {
      ret = calloc(1, sizeof(*ret));

      //     fprintf(stderr, "bgav_packet_parser_create\n");
      //     gavl_dictionary_dump(stream_info, 2);
      
      ret->info = stream_info;
      ret->m = gavl_stream_get_metadata_nc(stream_info);
      ret->afmt = gavl_stream_get_audio_format_nc(ret->info);
      ret->vfmt = gavl_stream_get_video_format_nc(ret->info);
      gavl_stream_get_compression_info(ret->info, &ret->ci);
      
      gavl_dictionary_get_int(ret->m, GAVL_META_STREAM_PACKET_TIMESCALE, &ret->packet_timescale);
      
      ret->stream_flags = stream_flags;
      ret->fourcc = fourcc;
      gavl_stream_get_compression_info(ret->info, &ret->ci);
      
      parsers[idx].func(ret);
      break;
      }
    idx++;
    }
  
  if(!ret)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Found no packet parser for compression");
    return NULL;
    }
 
  if(stream_flags & STREAM_PARSE_FULL)
    {
    ret->sink = gavl_packet_sink_create(sink_get_func_full,
                                        sink_put_func_full,
                                        ret);
    }
  else if(stream_flags & STREAM_PARSE_FRAME)
    {
    ret->sink = gavl_packet_sink_create(sink_get_func_frame,
                                        sink_put_func_frame,
                                        ret);
    }
  
  bgav_packet_parser_reset(ret);
  return ret;
  }

void bgav_packet_parser_destroy(bgav_packet_parser_t * p)
  {
  if(p->packets)
    free(p->packets);
  
  if(p->sink)
    gavl_packet_sink_destroy(p->sink);

  gavl_buffer_free(&p->buf);
  gavl_packet_free(&p->in_packet);

  gavl_compression_info_free(&p->ci);

  if(p->cleanup)
    p->cleanup(p);
  
  free(p);
  }

void bgav_packet_parser_flush(bgav_packet_parser_t * p)
  {
  if(!(p->stream_flags & STREAM_PARSE_FULL) ||
     (!p->buf.len))
    return;
  
  }

/* Call after seeking */
void bgav_packet_parser_reset(bgav_packet_parser_t * p)
  {
  p->parser_flags &= ~PARSER_HAS_SYNC;
  //  p->timestamp = GAVL_TIME_UNDEFINED;
  p->num_packets = 0;
  gavl_buffer_reset(&p->buf);
  
  if(p->reset)
    p->reset(p);
  }
