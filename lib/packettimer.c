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
#include <avdec_private.h>

#define LOG_DOMAIN "packettimer"

#define MAX_PACKETS 64

// #define DUMP_INPUT
// #define DUMP_OUTPUT

struct bgav_packet_timer_s
  {
  bgav_packet_t * packets[MAX_PACKETS];
  int num_packets;
  bgav_stream_t * s;
  
  bgav_packet_source_t src;

  int eof;
  int64_t last_duration;
  
  bgav_packet_t * out_packet;
  
  int num_ip_frames;
  
  gavl_source_status_t (*next_packet)(bgav_packet_timer_t *, int force);
  
  int64_t current_pts;
  int64_t last_b_pts; // For detecting B-Pyramid
  
  int b_pyramid;
  };

// #define IS_B(idx) (PACKET_GET_CODING_TYPE(pt->packets[idx]) == BGAV_CODING_TYPE_B)
#define IS_IP(idx) ((PACKET_GET_CODING_TYPE(pt->packets[idx]) != BGAV_CODING_TYPE_B) && \
                    !(pt->packets[idx]->flags & GAVL_PACKET_NOOUTPUT))

static gavl_source_status_t
insert_packet(bgav_packet_timer_t * pt, bgav_packet_t ** ret, int force)
  {
  gavl_source_status_t st;
  bgav_packet_t * p;
  if(pt->num_packets >= MAX_PACKETS)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Packet cache full");
    return GAVL_SOURCE_EOF;
    }

  while(1)
    {
    p = NULL;

    if(!force)
      {
      if(pt->src.peek_func(pt->src.data, &p, 0) == GAVL_SOURCE_AGAIN)
        return GAVL_SOURCE_AGAIN;
      }
    
    if((st = pt->src.get_func(pt->src.data, &p)) != GAVL_SOURCE_OK)
      {
      if(st == GAVL_SOURCE_EOF)
        pt->eof = 1;
      return st;
      }
#ifdef DUMP_INPUT
    bgav_dprintf("packet_timer in:  ");
    bgav_packet_dump(p);
#endif
    if(!PACKET_GET_SKIP(p))
      break;
    bgav_packet_pool_put(pt->s->pp, p);
    }
  
  if(PACKET_GET_CODING_TYPE(p) == BGAV_CODING_TYPE_B)
    {
    if(pt->num_ip_frames < 2)
      PACKET_SET_SKIP(p);
    }
  else
    pt->num_ip_frames++;

  if(p->flags & GAVL_PACKET_NOOUTPUT)
    p->duration = 0;
  else
    p->duration = -1;
  
  pt->packets[pt->num_packets] = p;
  pt->num_packets++;
  if(ret)
    *ret = p;
  return GAVL_SOURCE_OK;
  }

static bgav_packet_t * remove_packet(bgav_packet_timer_t * pt)
  {
  bgav_packet_t * ret;
  ret = pt->packets[0];

  pt->num_packets--;
  if(pt->num_packets)
    memmove(&pt->packets[0], &pt->packets[1], sizeof(pt->packets[0])*pt->num_packets);
#ifdef DUMP_OUTPUT
  bgav_dprintf("packet_timer out: ");
  bgav_packet_dump(ret);
#endif

  return ret;
  }

/* These are the actual worker functions */

/*
 *  Duration from DTS
 */

static int set_duration_from_dts(bgav_packet_timer_t * pt, uint32_t index)
  {
  if(index >= pt->num_packets)
    {
    // BUG!!!
    return 0;
    }
  if(index == pt->num_packets - 1)
    pt->packets[index]->duration = pt->last_duration;
  else
    {
    pt->packets[index]->duration = pt->packets[index+1]->dts - pt->packets[index]->dts;
    pt->last_duration = pt->packets[index]->duration;
    }
  return 1;
  }

static gavl_source_status_t
next_packet_duration_from_dts(bgav_packet_timer_t * pt, int force)
  {
  gavl_source_status_t st = GAVL_SOURCE_OK;
  if(pt->num_packets &&
     ((pt->packets[0]->duration > 0) || PACKET_GET_SKIP(pt->packets[0])))
    return 1;
  
  while(pt->num_packets < 2)
    {
    st = insert_packet(pt, NULL, force);
    if(st == GAVL_SOURCE_AGAIN)
      return GAVL_SOURCE_AGAIN;
    else if(st != GAVL_SOURCE_OK)
      break;
    }
  
  if(!pt->num_packets)
    return GAVL_SOURCE_EOF;
  set_duration_from_dts(pt, 0);
  return GAVL_SOURCE_OK;
  }

/*
 *  Duration from PTS (Matroska, ASF, NSV, NUV, rm, smjpeg....)
 */

static gavl_source_status_t
insert_packet_duration_from_pts(bgav_packet_timer_t * pt,
                                bgav_packet_t ** ret, int force)
  {
  bgav_packet_t * p;
  gavl_source_status_t st;
  p = NULL;

  if((st = insert_packet(pt, &p, force)) != GAVL_SOURCE_OK)
    return st;
  
  /* Detect B-Pyramid */

  if((PACKET_GET_CODING_TYPE(p) == BGAV_CODING_TYPE_B) && !PACKET_GET_SKIP(p))
    {
    if(!pt->b_pyramid &&
       (pt->last_b_pts != GAVL_TIME_UNDEFINED) &&
       (p->pts < pt->last_b_pts))
      {
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
               "Detected B-Pyramid");
      pt->b_pyramid = 1;
      }
    pt->last_b_pts = p->pts;
    }

  if(ret)
    *ret = p;
  
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t get_next_ip_duration_from_pts(bgav_packet_timer_t * pt,
                                                          int index, int * ret, int force)
  {
  int i;
  bgav_packet_t * p;
  gavl_source_status_t st;
  
  for(i = index + 1; i < pt->num_packets; i++)
    {
    if(IS_IP(i))
      {
      *ret = i;
      return GAVL_SOURCE_OK;
      }
    }

  // Read packets until this B-frame sequence is finished
  while(1)
    {
    p = NULL;
    if((st = insert_packet_duration_from_pts(pt, &p, force)) != GAVL_SOURCE_OK)
      {
      *ret = -1;
      return st;
      }
    if(PACKET_GET_CODING_TYPE(p) != BGAV_CODING_TYPE_B)
      {
      *ret = pt->num_packets-1;
      return GAVL_SOURCE_OK;
      }
    }
  
  //  return GAVL_SOURCE_EOF; // Never get here
  }



/*
 *  Set duration of this_index from the pts difference
 *  of this_index and next_index
 *  If next_index < 0, we have EOF
 */ 

static void
set_duration_from_pts(bgav_packet_timer_t * pt,
                      int this_index, int next_index)
  {
  if(next_index < 0)
    {
    pt->packets[this_index]->duration = pt->last_duration;
    return;
    }
  pt->packets[this_index]->duration =
    pt->packets[next_index]->pts - pt->packets[this_index]->pts;
  pt->last_duration = pt->packets[this_index]->duration;
  return;
  }

/*
 *  Get the index of the next smallest pts after pts in the
 *  range between start (inclusive) and end (exclusive)
 */

static int get_next_pts(bgav_packet_timer_t * pt,
                        int start, int end, int current)
  {
  int i;
  int ret = -1;
  int64_t next_pts = 0;
  
  for(i = start; i < end; i++)
    {
    if(pt->packets[i]->flags & GAVL_PACKET_NOOUTPUT)
      continue;
    if(pt->packets[i]->pts <= pt->packets[current]->pts)
      continue;
    if((ret < 0) || (pt->packets[i]->pts < next_pts))
      {
      ret = i;
      next_pts = pt->packets[i]->pts;
      }
    }
  return ret;
  }

static void
flush_duration_from_pts(bgav_packet_timer_t * pt,
                        int start, int end,
                        int start_seek, int end_seek)
  {
  int i;
  int next;

  for(i = start; i < end; i++)
    {
    if((pt->packets[i]->duration > 0) || 
       (pt->packets[i]->flags & GAVL_PACKET_NOOUTPUT))
      continue;
    next = get_next_pts(pt, start_seek, end_seek, i);
    set_duration_from_pts(pt, i, next);
    }
  }

static gavl_source_status_t
next_packet_duration_from_pts(bgav_packet_timer_t * pt, int force)
  {
  int ip1, ip2, ip3;
  int next;
  gavl_source_status_t st = GAVL_SOURCE_OK;
  
  if(pt->num_packets &&
     ((pt->packets[0]->duration > 0) || PACKET_GET_SKIP(pt->packets[0]) ||
      (pt->packets[0]->flags & GAVL_PACKET_NOOUTPUT)))
    return GAVL_SOURCE_OK;

  if((st = get_next_ip_duration_from_pts(pt, -1, &ip1, force)) != GAVL_SOURCE_OK)
    goto flush;
    
  if(ip1 > 0)
    {
    fprintf(stderr, "BUUUG");
    return GAVL_SOURCE_EOF;
    }

  if((st = get_next_ip_duration_from_pts(pt, ip1, &ip2, force)) != GAVL_SOURCE_OK)
    goto flush;

  if(ip2 < 1)
    goto flush;

  if((st = get_next_ip_duration_from_pts(pt, ip2, &ip3, force)) != GAVL_SOURCE_OK)
    goto flush;

  if(ip3 < 2)
    goto flush;
  
  /* PPP */
  if((ip2 - ip1 == 1) &&
     (ip3 - ip2 == 1))
    {
    set_duration_from_pts(pt, 0, 1);
    return 1;
    }

  /* PPBBBP */
  else if(ip2 - ip1 == 1)
    {
    next = get_next_pts(pt, ip2+1, ip3, 0);
    set_duration_from_pts(pt, 0, next);

    /* Flush B-frame sequence */
    flush_duration_from_pts(pt, ip2+1, ip3, ip2, ip3);
    }
  /* PBBBPP */
  else if(ip3 - ip2 == 1)
    {
    set_duration_from_pts(pt, 0, ip2);

    /* Flush B-frame sequence */
    flush_duration_from_pts(pt, 1, ip2, 0, ip2);
    }
  /* PBBBPBBBP */
  else
    {
    next = get_next_pts(pt, ip2+1, ip3, 0);
    set_duration_from_pts(pt, 0, next);

    /* Flush B-frame sequence */
    flush_duration_from_pts(pt, 1, ip2, 0, ip2);
    }
  return GAVL_SOURCE_OK;
  flush:

  if(st == GAVL_SOURCE_EOF)
    flush_duration_from_pts(pt, 0, pt->num_packets,
                            0, pt->num_packets);
  return st;
  }

/*
 *  PTS from DTS (AVI, OGM)
 */

static void set_pts_from_dts(bgav_packet_timer_t * pt,
                             bgav_packet_t * p)
  {
  if(pt->current_pts == GAVL_TIME_UNDEFINED)
    pt->current_pts = p->dts;
  
  p->pts = pt->current_pts;
  pt->current_pts += p->duration;
  }

static gavl_source_status_t
insert_packet_pts_from_dts(bgav_packet_timer_t * pt,
                           bgav_packet_t ** ret, int force)
  {
  gavl_source_status_t st;
  bgav_packet_t * p = NULL;

  st = insert_packet(pt, &p, force);
  
  if(st == GAVL_SOURCE_EOF)
    {
    if(pt->num_packets)
      set_duration_from_dts(pt, pt->num_packets-1);
    return st;
    }
  else if(st == GAVL_SOURCE_AGAIN)
    return st;
  
  /* Some demuxers output dts as pts */
  if(p->dts == GAVL_TIME_UNDEFINED)
    {
    p->dts = p->pts;
    p->pts = GAVL_TIME_UNDEFINED;
    }

  /* Set duration */
  if(pt->num_packets > 1)
    set_duration_from_dts(pt, pt->num_packets-2);

  if(ret)
    *ret = p;
  
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t
next_packet_pts_from_dts(bgav_packet_timer_t * pt, int force)
  {
  int i;
  gavl_source_status_t st = GAVL_SOURCE_OK;
  
  if(pt->num_packets &&
     ((pt->packets[0]->pts != GAVL_TIME_UNDEFINED) ||
      PACKET_GET_SKIP(pt->packets[0])))
    return GAVL_SOURCE_OK;
  
  while(pt->num_packets < 2)
    {
    if((st = insert_packet_pts_from_dts(pt, NULL, force)) != GAVL_SOURCE_OK)
      break;
    }

  if(!pt->num_packets)
    return st;
  else if((pt->num_packets < 2) && (st == GAVL_SOURCE_AGAIN))
    return st;
  
  if(PACKET_GET_CODING_TYPE(pt->packets[0]) != BGAV_CODING_TYPE_B)
    {
    /* Last packet (non B) */
    if(pt->num_packets == 1)
      {
      set_pts_from_dts(pt, pt->packets[0]);
      return GAVL_SOURCE_OK;
      }
    else
      {
      /* IP , PP -> output first packet */
      if(PACKET_GET_CODING_TYPE(pt->packets[1]) != BGAV_CODING_TYPE_B)
        {
        set_pts_from_dts(pt, pt->packets[0]);
        return GAVL_SOURCE_OK;
        }
      /* IB, PB -> go to the end of the B-frame
         sequence to get the pts of the non-B frame */
      else
        {
        bgav_packet_t * p;
        while(1)
          {
          p = NULL;
          /* Get packets until the next non B-frame */
          
          if((st = insert_packet_pts_from_dts(pt, &p, force)) != GAVL_SOURCE_OK)
            {
            if(st == GAVL_SOURCE_AGAIN)
              return st;
            
            if(pt->eof)
              break;
            else
              return GAVL_SOURCE_EOF; // Cache full
            }
          if(PACKET_GET_CODING_TYPE(p) != BGAV_CODING_TYPE_B)
            break;
          }

        for(i = 1; i < pt->num_packets; i++)
          {
          if(PACKET_GET_CODING_TYPE(pt->packets[i]) != BGAV_CODING_TYPE_B)
            break;
          
          if(!PACKET_GET_SKIP(pt->packets[i]))
            set_pts_from_dts(pt, pt->packets[i]);
          }
        set_pts_from_dts(pt, pt->packets[0]);
        return GAVL_SOURCE_OK;
        }
      }
    }
  return GAVL_SOURCE_EOF;
  }

/*
 *  Generic functions
 */ 

static gavl_source_status_t peek_func(void * pt1, bgav_packet_t ** ret,
                                      int force)
  {
  gavl_source_status_t st;
  bgav_packet_timer_t * pt = pt1;

  if(pt->out_packet)
    {
    if(ret)
      *ret = pt->out_packet;
    return GAVL_SOURCE_OK;
    }
  if(!pt->num_packets && pt->eof)
    return GAVL_SOURCE_EOF;
  
  if((st = pt->next_packet(pt, force)) != GAVL_SOURCE_OK)
    return st;
  
  pt->out_packet = remove_packet(pt);
  if(ret)
    *ret = pt->out_packet;
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t get_func(void * pt1, bgav_packet_t ** p)
  {
  bgav_packet_timer_t * pt = pt1;

  if(pt->out_packet)
    {
    *p = pt->out_packet;
    pt->out_packet = NULL;
    return GAVL_SOURCE_OK;
    }
  
  if(!pt->num_packets && pt->eof)
    return GAVL_SOURCE_EOF;
  
  if(!pt->next_packet(pt, 1))
    return GAVL_SOURCE_EOF;
  *p = remove_packet(pt);
  return GAVL_SOURCE_OK;
  }

bgav_packet_timer_t * bgav_packet_timer_create(bgav_stream_t * s)
  {
  bgav_packet_timer_t * ret = calloc(1, sizeof(*ret));
  ret->s = s;
  ret->current_pts = GAVL_TIME_UNDEFINED;
  ret->last_b_pts = GAVL_TIME_UNDEFINED;

  bgav_packet_source_copy(&ret->src, &s->src);

  s->src.get_func = get_func;
  s->src.peek_func = peek_func;
  s->src.data = ret;

  /* Set functions */
  if(ret->s->flags & STREAM_DTS_ONLY)
    ret->next_packet = next_packet_pts_from_dts;
  else if(ret->s->flags & STREAM_NO_DURATIONS)
    {
    if(ret->s->flags & STREAM_HAS_DTS)
      ret->next_packet = next_packet_duration_from_dts;
    else
      ret->next_packet = next_packet_duration_from_pts;
    }
  return ret;
  }

void bgav_packet_timer_destroy(bgav_packet_timer_t * pt)
  {
  int i;
  for(i = 0; i < pt->num_packets; i++)
    {
    bgav_packet_pool_put(pt->s->pp, pt->packets[i]);
    }
  if(pt->out_packet)
    {
    bgav_packet_pool_put(pt->s->pp, pt->out_packet);
    }
  free(pt);
  }

void bgav_packet_timer_reset(bgav_packet_timer_t * pt)
  {
  int i;

  //  fprintf(stderr, "bgav_packet_timer_reset %d\n",
  //          pt->num_packets);
  
  pt->num_ip_frames = 0;
  
  pt->eof = 0;
  pt->current_pts = GAVL_TIME_UNDEFINED;
  pt->last_b_pts = GAVL_TIME_UNDEFINED;
  
  for(i = 0; i < pt->num_packets; i++)
    bgav_packet_pool_put(pt->s->pp, pt->packets[i]);
  pt->num_packets = 0;

  if(pt->out_packet)
    {
    bgav_packet_pool_put(pt->s->pp, pt->out_packet);
    pt->out_packet = NULL;
    }
  }
