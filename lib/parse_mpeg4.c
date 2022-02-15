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

#include <string.h>
#include <stdlib.h>


#include <avdec_private.h>
#include <parser.h>
#include <videoparser_priv.h>

#include <mpeg4_header.h>
#include <mpv_header.h>

#define LOG_DOMAIN "parse_mpeg4"

// #define DUMP_HEADERS

#define STATE_SYNC 100
#define STATE_VOS  1
#define STATE_VO   2
#define STATE_VOL  3
#define STATE_GOV  4
#define STATE_VOP  5

typedef struct
  {
  /* Sequence header */
  bgav_mpeg4_vol_header_t vol;
  int have_vol;
  int has_picture_start;
  int state;

  char * user_data;
  int user_data_size;

  /* Save frames for packed B-frames */

  bgav_packet_t * saved_packet;

  int packed_b_frames;

  int set_pts;
  } mpeg4_priv_t;

static void set_format(bgav_video_parser_t * parser)
  {
  mpeg4_priv_t * priv = parser->priv;
  
  if(parser->s->flags & STREAM_PARSE_FULL)
    {
    parser->format->timescale = priv->vol.vop_time_increment_resolution;
    parser->format->frame_duration = priv->vol.fixed_vop_time_increment;
    priv->set_pts = 1;
    }

  if(!parser->format->image_width)
    {
    parser->format->image_width  = priv->vol.video_object_layer_width;
    parser->format->image_height = priv->vol.video_object_layer_height;
    parser->format->frame_width  =
      (parser->format->image_width + 15) & ~15;
    parser->format->frame_height  =
      (parser->format->image_height + 15) & ~15;
  
    bgav_mpeg4_get_pixel_aspect(&priv->vol,
                                &parser->format->pixel_width,
                                &parser->format->pixel_height);
    }
  
  if(!priv->vol.low_delay)
    parser->s->ci->flags |= GAVL_COMPRESSION_HAS_B_FRAMES;
  }

static void reset_mpeg4(bgav_video_parser_t * parser)
  {
  mpeg4_priv_t * priv = parser->priv;
  priv->state = STATE_SYNC;
  priv->has_picture_start = 0;
  priv->saved_packet->data_size = 0;
  }

static int extract_user_data(bgav_video_parser_t * parser,
                             const uint8_t * data, const uint8_t * data_end)
  {
  int i;
  int is_vendor;
  mpeg4_priv_t * priv = parser->priv;
  const uint8_t * pos1;
  if(priv->user_data)
    return 4;

  pos1 = data+4;
  pos1 = bgav_mpv_find_startcode(pos1, data_end);
  
  if(pos1)
    priv->user_data_size = pos1 - (data + 4);
  else
    priv->user_data_size = (data_end - data) - 4;
  
  priv->user_data = calloc(1, priv->user_data_size+1);
  memcpy(priv->user_data, data+4, priv->user_data_size);

#ifdef DUMP_HEADERS
  bgav_dprintf("Got user data\n");
  gavl_hexdump((uint8_t*)priv->user_data, priv->user_data_size, 16);
#endif

  if(!strncasecmp(priv->user_data, "divx", 4) &&
     (priv->user_data[priv->user_data_size-1] == 'p'))
    {
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
             "Detected packed B-frames");
    priv->packed_b_frames = 1;
    }

  /* Set software field in metadata */
  is_vendor = 1;
  for(i = 0; i < priv->user_data_size - priv->packed_b_frames; i++)
    {
    if(((uint8_t)priv->user_data[i] < 32) ||
       ((uint8_t)priv->user_data[i] > 127))
      {
      is_vendor = 0;
      break;
      }
    }
  if(is_vendor)
    {
    int vendor_len = priv->user_data_size - priv->packed_b_frames;
    char * vendor = malloc(vendor_len + 1);
    memcpy(vendor, priv->user_data, vendor_len);
    vendor[vendor_len] = '\0';
    gavl_dictionary_set_string_nocopy(parser->s->m, GAVL_META_SOFTWARE,
                            vendor);
    }
  
  return priv->user_data_size+4;
  }

static int parse_header_mpeg4(bgav_video_parser_t * parser)
  {
  mpeg4_priv_t * priv = parser->priv;
  const uint8_t * pos = parser->s->ci->global_header;
  int len;
  while(1)
    {
    pos = bgav_mpv_find_startcode(pos, parser->s->ci->global_header +
                                  parser->s->ci->global_header_len);
    if(!pos)
      return priv->have_vol;
    
    switch(bgav_mpeg4_get_start_code(pos))
      {
      case MPEG4_CODE_VOL_START:
        len = bgav_mpeg4_vol_header_read(parser->s->opt,
                                         &priv->vol, pos,
                                         parser->s->ci->global_header_len -
                                         (pos - parser->s->ci->global_header));
        if(!len)
          return 0;
        priv->have_vol = 1;
#ifdef DUMP_HEADERS
        bgav_mpeg4_vol_header_dump(&priv->vol);
#endif
        set_format(parser);
        pos += len;
        break;
      case MPEG4_CODE_USER_DATA:
        pos += extract_user_data(parser, pos, parser->s->ci->global_header +
                                 parser->s->ci->global_header_len);
        break;
      default:
        pos += 4;
        break;
      }
    }
  return 0;
  }

static void set_header_end(bgav_video_parser_t * parser, bgav_packet_t * p,
                           int pos)
  {
  if(p->header_size)
    return;
  
  if(!parser->s->ci->global_header)
    {
    parser->s->ci->global_header_len = pos;
    parser->s->ci->global_header = malloc(parser->s->ci->global_header_len);
    memcpy(parser->s->ci->global_header, p->data, parser->s->ci->global_header_len);
    }
  p->header_size = pos;
  }

#define SWAP(n1, n2) \
  swp = n1; n1 = n2; n2 = swp;

static int parse_frame_mpeg4(bgav_video_parser_t * parser, bgav_packet_t * p,
                            int64_t pts_orig)
  {
  mpeg4_priv_t * priv = parser->priv;
  const uint8_t * data;
  uint8_t * data_end;
  int sc;
  int result;
  bgav_mpeg4_vop_header_t vh;
  int num_pictures = 0;

  /* Skip weird packets with just one 0x7f byte */
  if((p->data_size == 1) && (p->data[0] == 0x7f))
    {
    p->flags |= GAVL_PACKET_NOOUTPUT; 
    return 1;
    }
  
  data = p->data;
  data_end = p->data + p->data_size;
  
  while(1)
    {
    data = bgav_mpv_find_startcode(data, data_end);

    if(!data)
      return num_pictures;
    
    sc = bgav_mpeg4_get_start_code(data);

    switch(sc)
      {
      case MPEG4_CODE_VO_START:
        data += 4;
        break;
      case MPEG4_CODE_VOL_START:
        if(!priv->have_vol)
          {
          result = bgav_mpeg4_vol_header_read(parser->s->opt,
                                              &priv->vol, data,
                                              data_end - data);
          if(!result)
            return 0;
          set_format(parser);
          data += result;

#ifdef DUMP_HEADERS
          bgav_mpeg4_vol_header_dump(&priv->vol);
#endif
          priv->have_vol = 1;
          }
        else
          data += 4;
        break;
      case MPEG4_CODE_VOP_START:

        if(!priv->have_vol)
          {
          PACKET_SET_SKIP(p);
          return 1;
          }

        if(priv->set_pts)
          p->duration = parser->format->frame_duration;
                
        result = bgav_mpeg4_vop_header_read(parser->s->opt,
                                            &vh, data, data_end-data,
                                            &priv->vol);
        if(!result)
          return 0;
#ifdef DUMP_HEADERS
        bgav_mpeg4_vop_header_dump(&vh);
#endif

        /* Check whether to copy a saved frame back */
        if(priv->saved_packet->data_size)
          {
          if(!vh.vop_coded)
            {
            /* Copy stuff back, overwriting the packet */
            bgav_packet_alloc(p, priv->saved_packet->data_size);
            memcpy(p->data, priv->saved_packet->data,
                   priv->saved_packet->data_size);
            p->data_size = priv->saved_packet->data_size;
            priv->saved_packet->data_size = 0;
            p->flags = priv->saved_packet->flags;
            p->position = priv->saved_packet->position;
            }
          else
            {
            int64_t swp;
            /* Output saved frame but save this one */
            bgav_packet_swap_data(priv->saved_packet, p);
            p->flags = priv->saved_packet->flags;
            SWAP(priv->saved_packet->position, p->position);
            PACKET_SET_CODING_TYPE(priv->saved_packet, vh.coding_type);
            }
          return 1;
          }
        /* save this frame for later use */
        else if(priv->packed_b_frames && (num_pictures == 1))
          {
          bgav_packet_reset(priv->saved_packet);
          priv->saved_packet->data_size = data_end - data;
          bgav_packet_alloc(priv->saved_packet,
                            priv->saved_packet->data_size);
          memcpy(priv->saved_packet->data, data,
                 priv->saved_packet->data_size);
          PACKET_SET_CODING_TYPE(priv->saved_packet, vh.coding_type);
          priv->saved_packet->position = p->position;
          p->data_size -= priv->saved_packet->data_size;
          num_pictures++;
          }
        else
          {
          set_header_end(parser, p, data - p->data);
          
          PACKET_SET_CODING_TYPE(p, vh.coding_type);
          data += result;
          
          if(p->header_size && priv->packed_b_frames)
            bgav_mpeg4_remove_packed_flag(p->data,
                                          &p->data_size, &p->header_size);
          num_pictures++;
          }
        if(!priv->packed_b_frames || (num_pictures == 2))
          return 1;
        break;
      case MPEG4_CODE_GOV_START:
        set_header_end(parser, p, data - p->data);
        data += 4;
        break;
      case MPEG4_CODE_USER_DATA:
        result = extract_user_data(parser, data, data_end);
        data += result;
        break;
      default:
        data += 4;
        break;
      }
    }
  return 0;
  }

static int find_frame_boundary_mpeg4(bgav_video_parser_t * parser, int * skip)
  {
  mpeg4_priv_t * priv = parser->priv;
  int new_state;
  int code;
  const uint8_t * sc;

  while(1)
    {
    // fprintf(stderr, "find startcode %d %d\n",
    // parser->pos, parser->buf.size - parser->pos - 4);
    
    sc = bgav_mpv_find_startcode(parser->buf.buffer + parser->pos,
                                 parser->buf.buffer + parser->buf.size - 4);
    if(!sc)
      {
      parser->pos = parser->buf.size - 4;
      if(parser->pos < 0)
        parser->pos = 0;
      return 0;
      }

    code = bgav_mpeg4_get_start_code(sc);
    new_state = -1;
    
    // fprintf(stderr, "Got code: %d\n", code);
    
    switch(code)
      {
      case MPEG4_CODE_VOS_START:
        new_state = STATE_VOS;
        break;
      case MPEG4_CODE_VO_START:
        new_state = STATE_VO;
        break;
      case MPEG4_CODE_VOL_START:
        new_state = STATE_VOL;
        break;
      case MPEG4_CODE_VOP_START:
        new_state = STATE_VOP;
        break;
      case MPEG4_CODE_USER_DATA:
        break;
      case MPEG4_CODE_GOV_START:
        new_state = STATE_GOV;
        break;
      default:
        break;
      }
    parser->pos = sc - parser->buf.buffer;

    if(new_state < 0)
      parser->pos += 4;
    else if((new_state < priv->state) ||
            ((new_state == STATE_VOP) &&
             (priv->state == STATE_VOP)))
      {
      // fprintf(stderr, "GOT BOUNDARY %d\n", parser->pos);
      priv->state = new_state;
      *skip = 4;
      return 1;
      }
    else
      {
      priv->state = new_state;
      parser->pos += 4;
      }
    }
  return 0;
  }

static void cleanup_mpeg4(bgav_video_parser_t * parser)
  {
  mpeg4_priv_t * priv = parser->priv;
  if(priv->user_data)
    free(priv->user_data);
  if(priv->saved_packet)
    bgav_packet_destroy(priv->saved_packet);
  free(parser->priv);
  }


void bgav_video_parser_init_mpeg4(bgav_video_parser_t * parser)
  {
  mpeg4_priv_t * priv;
  priv = calloc(1, sizeof(*priv));
  
  parser->priv = priv;
  priv->saved_packet = bgav_packet_create();

  priv->state = STATE_SYNC;
  
  if(parser->s->ci->global_header)
    parse_header_mpeg4(parser);
  
  //  parser->parse = parse_mpeg4;
  parser->parse_frame = parse_frame_mpeg4;
  parser->cleanup = cleanup_mpeg4;
  parser->reset = reset_mpeg4;
  parser->find_frame_boundary = find_frame_boundary_mpeg4;
  }
