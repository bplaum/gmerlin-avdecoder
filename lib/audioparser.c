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
#include <audioparser_priv.h>

// #define DUMP_INPUT
// #define DUMP_OUTPUT

static const struct
  {
  uint32_t fourcc;
  init_func func;
  }
parsers[] =
  {
    { BGAV_WAVID_2_FOURCC(0x0050), bgav_audio_parser_init_mpeg },
    { BGAV_WAVID_2_FOURCC(0x0055), bgav_audio_parser_init_mpeg },
    { BGAV_MK_FOURCC('.','m','p','1'), bgav_audio_parser_init_mpeg },
    { BGAV_MK_FOURCC('.','m','p','2'), bgav_audio_parser_init_mpeg },
    { BGAV_MK_FOURCC('.','m','p','3'), bgav_audio_parser_init_mpeg },
    { BGAV_MK_FOURCC('m','p','g','a'), bgav_audio_parser_init_mpeg },
    { BGAV_MK_FOURCC('L','A','M','E'), bgav_audio_parser_init_mpeg },
    { BGAV_WAVID_2_FOURCC(0x2000), bgav_audio_parser_init_a52 },
    { BGAV_MK_FOURCC('.','a','c','3'), bgav_audio_parser_init_a52 },
    { BGAV_MK_FOURCC('a','c','-','3'), bgav_audio_parser_init_a52 },
#ifdef HAVE_DCA
    { BGAV_MK_FOURCC('d','t','s',' '), bgav_audio_parser_init_dca },
#endif
#ifdef HAVE_VORBIS
    { BGAV_MK_FOURCC('V','B','I','S'), bgav_audio_parser_init_vorbis },
#endif
    { BGAV_MK_FOURCC('F','L','A','C'), bgav_audio_parser_init_flac },
#ifdef HAVE_OPUS
    { BGAV_MK_FOURCC('O','P','U','S'), bgav_audio_parser_init_opus },
#endif
#ifdef HAVE_SPEEX
    { BGAV_MK_FOURCC('S','P','E','X'), bgav_audio_parser_init_speex },
#endif
    { BGAV_MK_FOURCC('A','D','T','S'), bgav_audio_parser_init_adts },
    { BGAV_MK_FOURCC('A','A','C','P'), bgav_audio_parser_init_adts },
  };

int bgav_audio_parser_supported(uint32_t fourcc)
  {
  int i;
  for(i = 0; i < sizeof(parsers)/sizeof(parsers[0]); i++)
    {
    if(parsers[i].fourcc == fourcc)
      return 1;
    }
  return 0;
  }

static void set_eof(bgav_audio_parser_t * parser)
  {
  //  fprintf(stderr, "Audio parser EOF\n");
  parser->eof = 1;

  /* Output the last packet */
  if(parser->frame_bytes > parser->buf.size)
    parser->frame_bytes = parser->buf.size;
  }

static int check_output(bgav_audio_parser_t * parser)
  {
  if(parser->frame_bytes &&
     (parser->buf.size >= parser->frame_bytes))
    return 1;
  return 0;
  }

static int do_parse(bgav_audio_parser_t * parser)
  {
  int result;
  
  if(check_output(parser))
    return PARSER_HAVE_PACKET;
  else if(parser->eof)
    return PARSER_EOF;
    
  result = parser->parse(parser);
  switch(result)
    {
    case PARSER_HAVE_FORMAT:
    case PARSER_NEED_DATA:
      return result;
      break;
    case PARSER_HAVE_FRAME:
      if(check_output(parser))
        return PARSER_HAVE_PACKET;
      /* Check if we have enough data */
      break;
    }
  return PARSER_NEED_DATA;
  }


static void add_packet(bgav_audio_parser_t * parser, bgav_packet_t * p)
  {
#ifdef DUMP_INPUT  
  bgav_dprintf("Add packet ");
  bgav_packet_dump(p);
#endif
  /* Update cache */
  
  if(parser->num_packets >= parser->packets_alloc)
    {
    parser->packets_alloc = parser->num_packets + 10;
    parser->packets       = realloc(parser->packets,
                                    parser->packets_alloc *
                                    sizeof(*parser->packets));
    }
  parser->packets[parser->num_packets].packet_position = p->position;
  parser->packets[parser->num_packets].parser_position = parser->buf.size;
  parser->packets[parser->num_packets].size = p->data_size;
  parser->packets[parser->num_packets].pts  = p->pts;
  parser->num_packets++;
  bgav_bytebuffer_append_data(&parser->buf, p->data, p->data_size, 0);

  }

static int parse_frame(bgav_audio_parser_t * parser,
                       bgav_packet_t * p)
  {
  if(!parser->parse_frame)
    return PARSER_ERROR;

#ifdef DUMP_INPUT  
  bgav_dprintf("Add packet ");
  bgav_packet_dump(p);
#endif
 
  if(parser->timestamp == GAVL_TIME_UNDEFINED)
    {
    if(p->pts != GAVL_TIME_UNDEFINED)
      parser->timestamp = gavl_time_rescale(parser->s->timescale,
                                            parser->s->data.audio.format->samplerate,
                                            p->pts);
    else
      parser->timestamp = 0;
    }
  
  if((parser->s->action != BGAV_STREAM_PARSE) &&
     parser->s->file_index)
    {
    if(p->position == parser->s->file_index->num_entries-1)
      {
      p->duration = parser->s->stats.pts_end -
        parser->s->file_index->entries[parser->s->file_index->num_entries-1].pts;
      }
    else
      p->duration =
        parser->s->file_index->entries[p->position+1].pts -
        parser->s->file_index->entries[p->position].pts;
    
    parser->timestamp = parser->s->file_index->entries[p->position].pts + parser->s->stats.pts_start;
    }
  else
    {
    if(!parser->parse_frame(parser, p))
      return PARSER_ERROR;
    }
  
  p->pts = parser->timestamp;

  if((p->end_pts != GAVL_TIME_UNDEFINED) &&
     (p->pts + p->duration > p->end_pts))
    {
    p->duration = p->end_pts - p->pts;
    if(p->duration < 0)
      p->duration = 0;
    }
  
  parser->timestamp += p->duration;
#ifdef DUMP_OUTPUT  
  bgav_dprintf("Get packet ");
  bgav_packet_dump(p);
#endif

  return PARSER_HAVE_PACKET;
  }

static void get_out_packet(bgav_audio_parser_t * parser,
                           bgav_packet_t * p)
  {
  bgav_packet_alloc(p, parser->frame_bytes);
  memcpy(p->data, parser->buf.buffer, parser->frame_bytes);
  p->data_size = parser->frame_bytes;
  bgav_packet_pad(p);
  bgav_audio_parser_flush(parser, parser->frame_bytes);

  if(parser->timestamp == GAVL_TIME_UNDEFINED)
    {
    if(parser->frame_pts != GAVL_TIME_UNDEFINED)
      parser->timestamp = parser->frame_pts;
    else
      parser->timestamp = 0;
    }
  p->pts = parser->timestamp;
  parser->timestamp += parser->frame_samples;
  
  p->duration = parser->frame_samples;
  
  p->dts = GAVL_TIME_UNDEFINED;

  p->flags = 0;
  
  PACKET_SET_KEYFRAME(p);

  p->position = parser->frame_position;
  
  //  fprintf(stderr, "Get packet %c %ld\n", c->coding_type, p->pts);

  /* Clear frame bytes */
  parser->frame_bytes = 0;
  
#ifdef DUMP_OUTPUT
  bgav_dprintf("Get packet ");
  bgav_packet_dump(p);
#endif

  }


/* */

static gavl_source_status_t
get_packet_parse_full(void * parser1, bgav_packet_t ** ret)
  {
  gavl_source_status_t st;
  int result;
  bgav_audio_parser_t * parser = parser1;
  bgav_packet_t * p;
  
  if(parser->out_packet)
    {
    *ret = parser->out_packet;
    parser->out_packet = NULL;
    return GAVL_SOURCE_OK;
    }
  
  while(1)
    {
    result = do_parse(parser);

    switch(result)
      {
      case PARSER_EOF:
        return GAVL_SOURCE_EOF;
      case PARSER_NEED_DATA:
        p = NULL;
        st = parser->src.get_func(parser->src.data, &p);

        switch(st)
          {
          case GAVL_SOURCE_OK:
            add_packet(parser, p);
            bgav_packet_pool_put(parser->s->pp, p);
            break;
          case GAVL_SOURCE_EOF:
            set_eof(parser);
            continue;
            break;
          case GAVL_SOURCE_AGAIN:
            return st;
            break;
          }
        break;
      case PARSER_HAVE_PACKET:
        *ret = bgav_packet_pool_get(parser->s->pp);
        get_out_packet(parser, *ret);
        return GAVL_SOURCE_OK;
        break;
      case PARSER_ERROR:
        return GAVL_SOURCE_EOF;
      }
    }
  return GAVL_SOURCE_EOF;
  }

static gavl_source_status_t
peek_packet_parse_full(void * parser1, bgav_packet_t ** ret, int force)
  {
  int result;
  bgav_packet_t * p;
  gavl_source_status_t st;
  
  bgav_audio_parser_t * parser = parser1;

  if(parser->out_packet)
    {
    if(ret)
      *ret = parser->out_packet;
    return GAVL_SOURCE_OK;
    }
  while(1)
    {
    result = do_parse(parser);
    
    switch(result)
      {
      case PARSER_EOF:
        return GAVL_SOURCE_EOF;
        break;
      case PARSER_NEED_DATA:
        st = parser->src.peek_func(parser->src.data, NULL, force);
        switch(st)
          {
          case GAVL_SOURCE_EOF:
            set_eof(parser);
            continue;
            break;
          case GAVL_SOURCE_OK:
            {
            p = NULL;
            parser->src.get_func(parser->src.data, &p);
            add_packet(parser, p);
            bgav_packet_pool_put(parser->s->pp, p);
            }
            break;
          case GAVL_SOURCE_AGAIN:
            return st;
            break;
          }
        break;
      case PARSER_HAVE_PACKET:
        parser->out_packet = bgav_packet_pool_get(parser->s->pp);
        get_out_packet(parser, parser->out_packet);
        if(ret)
          *ret = parser->out_packet;
        return GAVL_SOURCE_OK;
        break;
      case PARSER_ERROR:
        return GAVL_SOURCE_EOF;
      }
    }

  /* Never get here */
  return GAVL_SOURCE_EOF;
  }

static gavl_source_status_t
get_packet_parse_frame(void * parser1, bgav_packet_t ** ret)
  {
  gavl_source_status_t st;
  bgav_audio_parser_t * parser = parser1;
  if(parser->out_packet)
    {
    *ret = parser->out_packet;
    parser->out_packet = NULL;
    return GAVL_SOURCE_OK;
    }

  if((st = parser->src.get_func(parser->src.data, ret)) != GAVL_SOURCE_OK)
    return st;
  
  if((*ret)->duration < 0)
    parse_frame(parser, *ret);
  
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t
peek_packet_parse_frame(void * parser1, bgav_packet_t ** ret,
                        int force)
  {
  gavl_source_status_t st;
  bgav_audio_parser_t * parser = parser1;
  
  if(parser->out_packet)
    {
    if(ret)
      *ret = parser->out_packet;
    return GAVL_SOURCE_OK;
    }
  
  if((st =
      parser->src.peek_func(parser->src.data,
                            NULL, force)) != GAVL_SOURCE_OK)
    return st;
  
  parser->src.get_func(parser->src.data, &parser->out_packet);
  
  if(parser->out_packet->duration < 0)
    {
    if(parse_frame(parser, parser->out_packet) == PARSER_ERROR)
      return GAVL_SOURCE_EOF;
    }
  if(ret)
    *ret = parser->out_packet;
  return GAVL_SOURCE_OK;
  }

/* */

bgav_audio_parser_t * bgav_audio_parser_create(bgav_stream_t * s)
  {
  bgav_audio_parser_t * ret;
  int i;
  
  init_func func = NULL;
  for(i = 0; i < sizeof(parsers)/sizeof(parsers[0]); i++)
    {
    if(parsers[i].fourcc == s->fourcc)
      {
      func = parsers[i].func;
      break;
      }
    }

  if(!func)
    return NULL;
  
  ret = calloc(1, sizeof(*ret));
  ret->s = s;
  
  ret->timestamp = GAVL_TIME_UNDEFINED;
  ret->raw_position = -1;

  bgav_packet_source_copy(&ret->src, &s->src);

  if(s->flags & STREAM_PARSE_FULL)
    {
    s->src.get_func = get_packet_parse_full;
    s->src.peek_func = peek_packet_parse_full;
    }
  else if(s->flags & STREAM_PARSE_FRAME)
    {
    s->src.get_func = get_packet_parse_frame;
    s->src.peek_func = peek_packet_parse_frame;
    }
  s->src.data = ret;
  
  func(ret);

  return ret;
  }

void bgav_audio_parser_destroy(bgav_audio_parser_t * parser)
  {
  if(parser->packets)
    free(parser->packets);
  bgav_bytebuffer_free(&parser->buf);

  if(parser->out_packet)
    bgav_packet_pool_put(parser->s->pp, parser->out_packet);
  
  if(parser->cleanup)
    parser->cleanup(parser);
  free(parser);
  }

void bgav_audio_parser_reset(bgav_audio_parser_t * parser,
                             int64_t in_pts, int64_t out_pts)
  {
  bgav_bytebuffer_flush(&parser->buf);
  
  parser->num_packets = 0;
  parser->frame_samples = 0;
  parser->frame_bytes = 0;
  parser->raw_position = -1;
  parser->eof = 0;

  if(parser->out_packet)
    {
    bgav_packet_pool_put(parser->s->pp, parser->out_packet);
    parser->out_packet = NULL;
    }
  
  if(out_pts != GAVL_TIME_UNDEFINED)
    parser->timestamp = out_pts;
  else if(in_pts != GAVL_TIME_UNDEFINED)
    parser->timestamp = gavl_time_rescale(parser->s->timescale,
                                          parser->s->data.audio.format->samplerate,
                                          in_pts);
  else
    parser->timestamp = GAVL_TIME_UNDEFINED;

  if(parser->reset)
    parser->reset(parser);
  }



void bgav_audio_parser_add_data(bgav_audio_parser_t * parser,
                                uint8_t * data, int len, int64_t position)
  {
  parser->raw = 1;
  bgav_bytebuffer_append_data(&parser->buf, data, len, 0);
  if(parser->raw_position < 0)
    parser->raw_position = position;
  }



void bgav_audio_parser_flush(bgav_audio_parser_t * parser, int bytes)
  {
  //  fprintf(stderr, "bgav_audio_parser_flush %d\n", bytes);
  bgav_bytebuffer_remove(&parser->buf, bytes);
  if(parser->raw)
    parser->raw_position += bytes;
  else
    {
    int num_remove, i;
    for(i = 0; i < parser->num_packets; i++)
      parser->packets[i].parser_position -= bytes;

    num_remove = 0;
    for(i = 0; i < parser->num_packets; i++)
      {
      if(parser->packets[i].parser_position + parser->packets[i].size <= 0)
        num_remove++;
      else
        break;
      }

    if(num_remove)
      {
      if(parser->num_packets - num_remove)
        memmove(parser->packets, parser->packets + num_remove,
                sizeof(*parser->packets) * (parser->num_packets - num_remove));
      parser->num_packets -= num_remove;
      }
    }
  }

void bgav_audio_parser_set_frame(bgav_audio_parser_t * parser,
                                 int pos, int len, int samples)
  {
  int i;

  //  fprintf(stderr, "bgav_audio_parser_set_frame\n");
  
  if(pos)
    bgav_audio_parser_flush(parser, pos);
  
  /* Get frame position */
  if(parser->raw)
    {
    parser->frame_position = parser->raw_position;
    parser->frame_pts      = 0;
    }
  else
    {
    for(i = 0; i < parser->num_packets; i++)
      {
      if(parser->packets[i].parser_position + parser->packets[i].size > 0)
        {
        parser->frame_position = parser->packets[i].packet_position;

        if(parser->packets[i].pts != GAVL_TIME_UNDEFINED) 
          parser->frame_pts      = gavl_time_rescale(parser->s->timescale,
                                                     parser->s->data.audio.format->samplerate,
                                                     parser->packets[i].pts);
        else
          parser->frame_pts      = GAVL_TIME_UNDEFINED;
        break;
        }
      }
    }
  parser->frame_samples = samples;
  parser->frame_bytes   = len;
  }

