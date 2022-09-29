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
#include <videoparser_priv.h>
#include <utils.h>

// #define DUMP_INPUT
// #define DUMP_OUTPUT

#define MAX_SCAN_SIZE 5000000

#define LOG_DOMAIN "videoparser"

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

static const struct
  {
  uint32_t fourcc;
  init_func func;
  }
parsers[] =
  {
    { BGAV_MK_FOURCC('H', '2', '6', '4'), bgav_video_parser_init_h264 },
    { BGAV_MK_FOURCC('a', 'v', 'c', '1'), bgav_video_parser_init_h264 },
    { BGAV_MK_FOURCC('m', 'p', 'g', 'v'), bgav_video_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'p', 'v', '1'), bgav_video_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'p', 'v', '2'), bgav_video_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'x', '3', 'p'), bgav_video_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'x', '4', 'p'), bgav_video_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'x', '5', 'p'), bgav_video_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'x', '3', 'n'), bgav_video_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'x', '4', 'n'), bgav_video_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'x', '5', 'n'), bgav_video_parser_init_mpeg12 },
    { BGAV_MK_FOURCC('m', 'p', '4', 'v'), bgav_video_parser_init_mpeg4 },
    /* DivX Variants */
    { BGAV_MK_FOURCC('D', 'I', 'V', 'X'), bgav_video_parser_init_mpeg4 },
    { BGAV_MK_FOURCC('d', 'i', 'v', 'x'), bgav_video_parser_init_mpeg4 },
    { BGAV_MK_FOURCC('D', 'X', '5', '0'), bgav_video_parser_init_mpeg4 },
    { BGAV_MK_FOURCC('X', 'V', 'I', 'D'), bgav_video_parser_init_mpeg4 },
    { BGAV_MK_FOURCC('x', 'v', 'i', 'd'), bgav_video_parser_init_mpeg4 },
    { BGAV_MK_FOURCC('F', 'M', 'P', '4'), bgav_video_parser_init_mpeg4 },
    { BGAV_MK_FOURCC('f', 'm', 'p', '4'), bgav_video_parser_init_mpeg4 },
    /* */
    { BGAV_MK_FOURCC('C', 'A', 'V', 'S'), bgav_video_parser_init_cavs },
    { BGAV_MK_FOURCC('V', 'C', '-', '1'), bgav_video_parser_init_vc1 },
    { BGAV_MK_FOURCC('d', 'r', 'a', 'c'), bgav_video_parser_init_dirac },
    { BGAV_MK_FOURCC('m', 'j', 'p', 'a'), bgav_video_parser_init_mjpa },
    { BGAV_MK_FOURCC('j', 'p', 'e', 'g'), bgav_video_parser_init_jpeg },
    { BGAV_MK_FOURCC('B', 'B', 'C', 'D'), bgav_video_parser_init_dirac },
    { BGAV_MK_FOURCC('V', 'P', '8', '0'), bgav_video_parser_init_vp8 },
    { BGAV_MK_FOURCC('V', 'P', '9', '0'), bgav_video_parser_init_vp9 },
    
    { BGAV_MK_FOURCC('D', 'V', 'D', 'S'), bgav_video_parser_init_dvdsub },
    { BGAV_MK_FOURCC('m', 'p', '4', 's'), bgav_video_parser_init_dvdsub },

  };

int bgav_video_parser_supported(uint32_t fourcc)
  {
  int i;
  for(i = 0; i < sizeof(parsers)/sizeof(parsers[0]); i++)
    {
    if(parsers[i].fourcc == fourcc)
      return 1;
    }
  return 0;
  }

void bgav_video_parser_destroy(bgav_video_parser_t * parser)
  {
  if(parser->cleanup)
    parser->cleanup(parser);
  if(parser->packets)
    free(parser->packets);
  if(parser->out_packet)
    bgav_packet_pool_put(parser->s->pp, parser->out_packet);
  gavl_buffer_free(&parser->buf);
  free(parser);
  }

void bgav_video_parser_reset(bgav_video_parser_t * parser,
                             int64_t in_pts, int64_t out_pts)
  {
  gavl_buffer_reset(&parser->buf);
  
  parser->raw_position = -1;
  //  parser->cache_size = 0;
  parser->num_packets = 0;
  parser->eof = 0;

  if(parser->out_packet)
    {
    bgav_packet_pool_put(parser->s->pp, parser->out_packet);
    parser->out_packet = NULL;
    }
  
  if(in_pts != GAVL_TIME_UNDEFINED)
    {
    //    int64_t start_pts;
    parser->timestamp = gavl_time_rescale(parser->s->timescale,
                                          parser->format->timescale, in_pts);
    }
  else if(out_pts != GAVL_TIME_UNDEFINED)
    parser->timestamp = out_pts;
  else
    parser->timestamp = GAVL_TIME_UNDEFINED;
  
  parser->pos = 0;
  parser->non_b_count = 0;
  parser->keyframe_count = 0;
  
  if(parser->reset)
    parser->reset(parser);

  parser->have_sync = 0;
  }

void bgav_video_parser_add_packet(bgav_video_parser_t * parser,
                                  bgav_packet_t * p)
  {
  // cache_t * c;
#ifdef DUMP_INPUT
  bgav_dprintf("Add packet ");
  bgav_packet_dump(p);
  gavl_hexdump(p->data, 16, 16);
#endif
  /* Update cache */

  if(parser->s->flags & STREAM_RAW_PACKETS)
    {
    parser->raw = 1;
    gavl_buffer_append_data(&parser->buf, p->data, p->data_size);
    if(parser->raw_position < 0)
      parser->raw_position = p->position;
    return;
    }
  
  if(parser->s->flags & STREAM_PARSE_FULL)
    {
    if(parser->num_packets >= parser->packets_alloc)
      {
      parser->packets_alloc = parser->num_packets + 10;
      parser->packets       = realloc(parser->packets,
                                      parser->packets_alloc *
                                      sizeof(*parser->packets));
      }
    parser->packets[parser->num_packets].packet_position = p->position;
    parser->packets[parser->num_packets].parser_position = parser->buf.len;
    parser->packets[parser->num_packets].size = p->data_size;
    parser->packets[parser->num_packets].pts  = p->pts;
    parser->num_packets++;
    }
  gavl_buffer_append_data(&parser->buf, p->data, p->data_size);
  }

void bgav_video_parser_flush(bgav_video_parser_t * parser, int bytes)
  {
  if(!bytes)
    return;
  
  gavl_buffer_flush(&parser->buf, bytes);
  parser->pos -= bytes;
  if(parser->pos < 0)
    parser->pos = 0;
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

/* New (generic) API */

static gavl_source_status_t
get_input_packet(bgav_video_parser_t * parser, int force)
  {
  gavl_source_status_t st;
  bgav_packet_t * p = NULL;

  if(!force && ((st = parser->src.peek_func(parser->src.data, NULL, 0)) !=
                GAVL_SOURCE_OK))
    return st;
  
  if((st = parser->src.get_func(parser->src.data, &p)) != GAVL_SOURCE_OK)
    {
    if(st == GAVL_SOURCE_EOF)
      parser->eof = 1;
    return st;
    }
  else
    {
    bgav_video_parser_add_packet(parser, p);
    bgav_packet_pool_put(parser->s->pp, p);
    return GAVL_SOURCE_OK;
    }
  }

static gavl_source_status_t
parse_next_packet(bgav_video_parser_t * parser, int force, int64_t *pts_ret,
                  bgav_packet_t ** ret)
  {
  int skip = -1;
  int64_t pts = 0;
  gavl_source_status_t st;
  
  if(parser->eof)
    return GAVL_SOURCE_EOF;
  
  /* Synchronize */
  while(!parser->have_sync)
    {
    if(parser->find_frame_boundary(parser, &skip))
      {
      bgav_video_parser_flush(parser, parser->pos);
      parser->have_sync = 1;
      parser->pos += skip;
      break;
      }
    if((st = get_input_packet(parser, force)) != GAVL_SOURCE_OK)
      {
      // EOF while initializing the video parser
      return st;
      }

    if(parser->buf.len > MAX_SCAN_SIZE)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Didn't find a frame in the first %d bytes (misdetected codec?)",
               parser->buf.len);
      return GAVL_SOURCE_EOF;
      }
    
    force = 1;
    }
  
  /* Find frame end */
  while(1)
    {
    if(parser->find_frame_boundary(parser, &skip))
      break;
    if((st = get_input_packet(parser, force)) != GAVL_SOURCE_OK)
      {
      // EOF: Take this packet as the last one if there is data left
      if(st == GAVL_SOURCE_EOF)
        {
        if(!parser->buf.len)
          return GAVL_SOURCE_EOF;
        parser->pos = parser->buf.len;
        }
      else if(st == GAVL_SOURCE_AGAIN)
        return st;
      break;
      }

    if(parser->buf.len > MAX_SCAN_SIZE)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Didn't find a frame in the first %d bytes (misdetected codec?)",
               parser->buf.len);
      return GAVL_SOURCE_EOF;
      }
    }
  
  /* Set up packet */

  *ret = bgav_packet_pool_get(parser->s->pp);
  bgav_packet_alloc(*ret, parser->pos);
  memcpy((*ret)->data, parser->buf.buf, parser->pos);
  (*ret)->data_size = parser->pos;

  if(parser->raw)
    {
    (*ret)->position = parser->raw_position;
    }
  else
    {
    (*ret)->position = parser->packets[0].packet_position;
    pts = parser->packets[0].pts;
    }
  
  // fprintf(stderr, "FLUSH %d\n", parser->pos);
  
  bgav_video_parser_flush(parser, parser->pos);
  
  /* set up position for next packet */
  parser->pos = skip;
  
  /* Parse frame */

  if(!(*ret)->data_size)
    return GAVL_SOURCE_EOF;

  if(!parser->parse_frame(parser, *ret, pts))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Parsing frame failed");
    /* Error */
    bgav_packet_pool_put(parser->s->pp, *ret);
    return GAVL_SOURCE_EOF;
    }

  if(PACKET_GET_SKIP(*ret))
    return GAVL_SOURCE_OK;
  
  /* If it's the first packet without skip flag set, set start position */
  if(parser->start_pos < 0)
    parser->start_pos = (*ret)->position;

  /* If we are before the start position, set the skip flag */
  if((parser->start_pos >= 0) && ((*ret)->position < parser->start_pos))
    {
    PACKET_SET_SKIP(*ret);
    return GAVL_SOURCE_OK;
    }

  /* Set the timescale */
  if(!parser->s->timescale)
    parser->s->timescale = parser->format->timescale;

  if(pts_ret)
    *pts_ret = pts;
  
  //  fprintf(stderr, "Got packet:\n");
  //  bgav_packet_dump(ret);
  
  return GAVL_SOURCE_OK;
  }

static void process_packet(bgav_video_parser_t * parser, bgav_packet_t * p, int64_t pts_orig)
  {
  /* Set keyframe flag */
  if(PACKET_GET_CODING_TYPE(p) == BGAV_CODING_TYPE_I)
    PACKET_SET_KEYFRAME(p);
  else if((parser->flags & PARSER_NO_I_FRAMES) &&
          (PACKET_GET_CODING_TYPE(p) == BGAV_CODING_TYPE_P))
    PACKET_SET_KEYFRAME(p);

  /* If no keyframe was seen yet, skip this picture */

  if(PACKET_GET_KEYFRAME(p))
    parser->keyframe_count++;
  else
    {
    if(!parser->keyframe_count)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Skipping initial non-keyframe");
      PACKET_SET_SKIP(p);
      return;
      }
    }
  
  /* If a B-frame was found but less than 2 non b-frames,
     skip this */
  
  if(PACKET_GET_CODING_TYPE(p) != BGAV_CODING_TYPE_B)
    parser->non_b_count++;
  else if(parser->non_b_count < 2)
    {
    PACKET_SET_SKIP(p);
    return;
    }

  /* Initialize stuff */
  if(!(parser->flags & PARSER_INITIALIZED))
    {
    /* MPEG-type video parsers only get the *duration* of each frame.
       Timestamps, which implies that the timestamps are DTS. They will be
       converted to PTS by the packettimer
    */
    
    if((parser->s->ci->flags & GAVL_COMPRESSION_HAS_B_FRAMES) && (parser->flags & PARSER_GEN_PTS))
      parser->s->flags |= STREAM_DTS_ONLY;
    parser->flags |= PARSER_INITIALIZED;
    }
  
  /* Handle PTS */
  
  if(!(parser->flags & PARSER_GEN_PTS))
    return;

  if(STREAM_IS_STILL(parser->s))
    {
    // fprintf(stderr, "Still pts: %ld (timescale: %d)\n", pts_orig, parser->format->timescale);
    p->pts = gavl_time_rescale(parser->s->timescale,
                               parser->format->timescale,
                               pts_orig);
    p->duration = -1;
    }
  else
    {
    if(parser->timestamp == GAVL_TIME_UNDEFINED)
      {
      if(pts_orig != GAVL_TIME_UNDEFINED)
        {
        parser->timestamp = gavl_time_rescale(parser->s->timescale,
                                              parser->format->timescale,
                                              pts_orig);
        
        gavl_stream_set_start_pts(parser->s->info, pts_orig, parser->s->timescale);
        }
      else
        parser->timestamp = 0;
      }
    p->pts = parser->timestamp;
    parser->timestamp += p->duration;
    }
  }

static gavl_source_status_t
parse_full(void * parser1, int force, bgav_packet_t ** ret)
  {
  gavl_source_status_t st;
  int64_t pts_orig = GAVL_TIME_UNDEFINED;
  bgav_video_parser_t * parser = parser1;

  if((st = parse_next_packet(parser, force, &pts_orig, ret)) != GAVL_SOURCE_OK)
    return st;
  
  /* Merge field pictures */
  
  if((*ret)->flags & PACKET_FLAG_FIELD_PIC)
    {
    bgav_packet_t * field2;

    /* Reading the second field should never return GAVL_SOURCE_AGAIN! */
    if((st = parse_next_packet(parser, 1, NULL, &field2)) != GAVL_SOURCE_OK)
      return GAVL_SOURCE_EOF;
    bgav_packet_merge_field2(*ret, field2);
    bgav_packet_pool_put(parser->s->pp, field2);
    }

  if(PACKET_GET_SKIP(*ret))
    return GAVL_SOURCE_OK;

  process_packet(parser, *ret, pts_orig);
  
#ifdef DUMP_OUTPUT
  bgav_dprintf("Get packet ");
  bgav_packet_dump(*ret);
  gavl_hexdump((*ret)->data, 16, 16);
  //  bgav_dprintf("recovery_point %d\n", c->recovery_point);
#endif
 
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t
get_packet_parse_full(void * parser1, bgav_packet_t ** ret_p)
  {
  gavl_source_status_t st;
  bgav_packet_t * ret;
  bgav_video_parser_t * parser = parser1;

  if(parser->out_packet)
    {
    *ret_p = parser->out_packet;
    parser->out_packet = NULL;
    return GAVL_SOURCE_OK;
    }

  while(1)
    {
    if((st = parse_full(parser1, 1, &ret)) != GAVL_SOURCE_OK)
      return st;
    if(!PACKET_GET_SKIP(ret))
      break;
    bgav_packet_pool_put(parser->s->pp, ret);
    }

    
  *ret_p = ret;

  //  fprintf(stderr, "get_packet_parse_full %"PRId64"\n", (*ret_p)->pts);
  
  return ret ? GAVL_SOURCE_OK : GAVL_SOURCE_EOF;
  }

static gavl_source_status_t
peek_packet_parse_full(void * parser1, bgav_packet_t ** ret_p,
                                         int force)
  {
  gavl_source_status_t st;
  bgav_video_parser_t * parser = parser1;

  //  fprintf(stderr, "peek_packet_parse_full\n");
  
  if(!parser->out_packet)
    {
    while(1)
      {
      if((st = parse_full(parser1, force, &parser->out_packet)) != GAVL_SOURCE_OK)
        return st;
      if(!PACKET_GET_SKIP(parser->out_packet))
        break;
      bgav_packet_pool_put(parser->s->pp, parser->out_packet);
      parser->out_packet = NULL;
      }
    
    }
  if(ret_p)
    *ret_p = parser->out_packet;
  
  return GAVL_SOURCE_OK;
  }

static int parse_frame(bgav_video_parser_t * parser,
                       bgav_packet_t * p)
  {
  int ret;
  if(!parser->parse_frame)
    return 0;

#ifdef DUMP_INPUT
  bgav_dprintf("Parse frame input  [%p] ", p);
  bgav_packet_dump(p);
  gavl_hexdump(p->data, 16, 16);
#endif
  
  ret = parser->parse_frame(parser, p, p->pts);

#ifdef DUMP_OUTPUT
  bgav_dprintf("Parse frame output [%p] ", p);
  bgav_packet_dump(p);
  gavl_hexdump(p->data, 16, 16);
#endif

  if(ret)
    process_packet(parser, p, p->pts);

  // Unneccesary?
  //  if(PACKET_GET_SKIP(p))
  //    return ret;
  
  return ret;
  }


static
gavl_source_status_t
get_packet_parse_frame(void * parser1, bgav_packet_t ** ret_p)
  {
  gavl_source_status_t st;
  bgav_packet_t * ret;
  bgav_video_parser_t * parser = parser1;

  if(parser->out_packet)
    {
    *ret_p = parser->out_packet;
    parser->out_packet = NULL;
    return GAVL_SOURCE_OK;
    }

  while(1)
    {
    if((st = parser->src.get_func(parser->src.data, &ret)) != GAVL_SOURCE_OK)
      return st;
    
    if(!parse_frame(parser, ret))
      {
      bgav_packet_pool_put(parser->s->pp, ret);
      return GAVL_SOURCE_EOF;
      }
    if(!PACKET_GET_SKIP(ret))
      break;
    bgav_packet_pool_put(parser->s->pp, ret);
    }

  *ret_p = ret;
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t
peek_packet_parse_frame(void * parser1, bgav_packet_t ** ret_p,
                        int force)
  {
  gavl_source_status_t st;
  bgav_video_parser_t * parser = parser1;
  
  if(parser->out_packet)
    {
    if(ret_p)
      *ret_p = parser->out_packet;
    return GAVL_SOURCE_OK;
    }
  
  while(1)
    {
    if((st = parser->src.peek_func(parser->src.data, NULL, force)) != GAVL_SOURCE_OK)
      return st;

    if((st = parser->src.get_func(parser->src.data, &parser->out_packet)) != GAVL_SOURCE_OK)
      return st;
    
    if(!parse_frame(parser, parser->out_packet))
      {
      bgav_packet_pool_put(parser->s->pp, parser->out_packet);
      parser->out_packet = NULL;
      return GAVL_SOURCE_EOF;
      }
    if(!PACKET_GET_SKIP(parser->out_packet))
      break;
    
    bgav_packet_pool_put(parser->s->pp, parser->out_packet);
    parser->out_packet = NULL;
    }
  if(ret_p)
    *ret_p = parser->out_packet;
  
  return GAVL_SOURCE_OK;
  }

bgav_video_parser_t *
bgav_video_parser_create(bgav_stream_t * s)
  {
  int i;
  bgav_video_parser_t * ret;
  /* First find a parse function */
  
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
    {
    if(bgav_check_fourcc(s->fourcc, bgav_dv_fourccs))
      func = bgav_video_parser_init_dv;
    else if(bgav_check_fourcc(s->fourcc, bgav_png_fourccs))
      func = bgav_video_parser_init_png;
    }
  
  if(!func)
    return NULL;
  
  ret = calloc(1, sizeof(*ret));
  ret->s = s;
  
  ret->timestamp = GAVL_TIME_UNDEFINED;
  ret->raw_position = -1;

  if(s->type == GAVL_STREAM_VIDEO)
    {
    //    ret->s->ci->max_ref_frames = 2;
    ret->format = s->data.video.format;
    }
  ret->start_pos = -1;
  
  bgav_packet_source_copy(&ret->src, &s->src);

  if(s->flags & STREAM_PARSE_FULL)
    {
    s->src.get_func = get_packet_parse_full;
    s->src.peek_func = peek_packet_parse_full;
    ret->flags |= PARSER_GEN_PTS;
    }
  else if(s->flags & STREAM_PARSE_FRAME)
    {
    s->src.get_func = get_packet_parse_frame;
    s->src.peek_func = peek_packet_parse_frame;
    
    if(s->timescale && (s->timescale != ret->format->timescale))
      ret->flags |= PARSER_GEN_PTS;
    }
  s->src.data = ret;
  
  func(ret);
  return ret;
  }
