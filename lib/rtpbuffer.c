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



#include <avdec_private.h>
#include <pthread.h>
#include <rtp.h>
#include <stdlib.h>
#define LOG_DOMAIN "rtpstack"

// #define MAX_PACKETS 10

#define MAX_DROPOUT 3000
#define MAX_MISORDER 100
#define MIN_SEQUENTIAL 2

#if 0
typedef struct packet_s
  {
  rtp_packet_t p;
  struct packet_s * next;
  } packet_t;
#endif

struct bgav_rtp_packet_buffer_s
  {
  rtp_packet_t * read_packets;
  rtp_packet_t * read_packet;
  
  rtp_packet_t * write_packets;
  rtp_packet_t * write_packet;
  pthread_mutex_t read_mutex;
  pthread_mutex_t write_mutex;
  
  int64_t last_seq;
  const bgav_options_t * opt;
  int num;
  rtp_stats_t stats;
  int timescale;
  
  int timestamp_wrap;
  int64_t timestamp_offset;
  int64_t last_timestamp;

  pthread_mutex_t eof_mutex;
  int eof;
  };

/* Statistics handling (from RFC 1889) */

#define RTP_SEQ_MOD (1<<16)

static void init_stats(rtp_stats_t *s, uint16_t seq, int64_t timestamp,
                       int timescale)
  {
  s->base_seq = seq - 1;
  s->max_seq = seq;
  s->bad_seq = RTP_SEQ_MOD + 1;
  s->cycles = 0;
  s->received = 0;
  s->received_prior = 0;
  s->expected_prior = 0;
  /* other initialization */
  s->initialized = 1;
  gavl_timer_stop(s->timer);
  gavl_timer_set(s->timer, 0);
  gavl_timer_start(s->timer);
  
  s->time_offset = gavl_time_unscale(timescale, timestamp);
  }

static int update_stats(rtp_stats_t * s, uint16_t seq,
                        uint64_t timestamp, int timescale)
  {
  uint16_t udelta = seq - s->max_seq;
  int64_t arrival;
  
  /* Jitter estimation */
  int transit;
  int d;
  
  /*
   * Source is not valid until MIN_SEQUENTIAL packets with
   * sequential sequence numbers have been received.
   */
  if (s->probation)
    {
    /* packet is in sequence */
    if (seq == s->max_seq + 1)
      {
      s->probation--;
      s->max_seq = seq;
      if (s->probation == 0)
        {
        init_stats(s, seq, timestamp, timescale);
        s->received++;
        return 1;
        }
      }
    else
      {
      s->probation = MIN_SEQUENTIAL - 1;
      s->max_seq = seq;
      }
    return 0;
    }
  else if (udelta < MAX_DROPOUT)
    {
    /* in order, with permissible gap */
    if (seq < s->max_seq)
      {
      /*
       * Sequence number wrapped - count another 64K cycle.
       */
      s->cycles += RTP_SEQ_MOD;
      }
    s->max_seq = seq;
    }
  else if (udelta <= RTP_SEQ_MOD - MAX_MISORDER)
    {
    /* the sequence number made a very large jump */
    if (seq == s->bad_seq)
      {
      /*
       * Two sequential packets -- assume that the other side
       * restarted without telling us so just re-sync
       * (i.e., pretend this was the first packet).
       */
      init_stats(s, seq, timestamp, timescale);
      }
    else
      {
      s->bad_seq = (seq + 1) & (RTP_SEQ_MOD-1);
      return 0;
      }
    }
  else
    {
    /* duplicate or reordered packet */
    }
  s->received++;

  /* Jitter estimation */
  arrival = gavl_time_scale(timescale,
                            gavl_timer_get(s->timer) + s->time_offset);
  
  transit = arrival - timestamp;
  d = transit - s->transit;
  s->transit = transit;
  if (d < 0)
    d = -d;
  s->jitter += d - ((s->jitter + 8) >> 4);
  
  
  return 1;
  }

/* */ 

bgav_rtp_packet_buffer_t *
bgav_rtp_packet_buffer_create(const bgav_options_t * opt, int timescale)
  {
  bgav_rtp_packet_buffer_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->last_seq = -1;
  ret->opt = opt;
  ret->timescale = timescale;
  ret->last_timestamp = GAVL_TIME_UNDEFINED;
  pthread_mutex_init(&ret->read_mutex, NULL);
  pthread_mutex_init(&ret->write_mutex, NULL);
  pthread_mutex_init(&ret->eof_mutex, NULL);
  ret->stats.timer = gavl_timer_create();
  
  return ret;
  }

static void free_packets(rtp_packet_t * p)
  {
  rtp_packet_t * tmp;
  while(p)
    {
    tmp = p->next;
    free(p);
    //    fprintf(stderr, "Destroy packet\n");
    p = tmp;
    }
  }

void bgav_rtp_packet_buffer_destroy(bgav_rtp_packet_buffer_t * b)
  {
  pthread_mutex_destroy(&b->read_mutex);
  pthread_mutex_destroy(&b->write_mutex);
  pthread_mutex_destroy(&b->eof_mutex);
  if(b->stats.timer) gavl_timer_destroy(b->stats.timer);
  free_packets(b->read_packets);
  free_packets(b->write_packets);
  free_packets(b->read_packet);
  free_packets(b->write_packet);
  free(b);
  }

rtp_packet_t *
bgav_rtp_packet_buffer_lock_write(bgav_rtp_packet_buffer_t * b)
  {
  //  fprintf(stderr, "Lock Write\n");
  pthread_mutex_lock(&b->write_mutex);
  if(!b->write_packets)
    {
    b->write_packet = calloc(1, sizeof(*b->write_packet));
    //    fprintf(stderr, "Create packet\n");
    }
  else
    {
    b->write_packet = b->write_packets;
    b->write_packets = b->write_packets->next;
    b->write_packet->next = NULL;
    }
  pthread_mutex_unlock(&b->write_mutex);
  return b->write_packet;
  }

void bgav_rtp_packet_buffer_unlock_write(bgav_rtp_packet_buffer_t * b)
  {
  int drop = 0;
  rtp_packet_t * p = b->write_packet;

  b->write_packet = NULL;

  //  fprintf(stderr, "Unlock Write\n");

  /* Push back and return */
  if(!b->timescale)
    {
    p->next = b->write_packets;
    b->write_packets = p;
    return;
    }
  //  if(b->next_seq == -1)
  //    b->next_seq = p->h.sequence_number;
  
  /* Correct timestamp */
  if((b->last_timestamp != GAVL_TIME_UNDEFINED) &&
     (int64_t)b->last_timestamp - (int64_t)p->h.timestamp > 0x80000000LL)
    b->timestamp_wrap = 1;
  else if(b->timestamp_wrap &&
          (p->h.timestamp > 0x80000000LL))
    {
    b->timestamp_wrap = 0;
    b->timestamp_offset += 0x100000000LL;
    }

  b->last_timestamp = p->h.timestamp;
  
  if(b->timestamp_wrap && 
     (p->h.timestamp < 0x80000000LL))
    p->h.timestamp += 0x100000000LL + b->timestamp_offset;
  else
    p->h.timestamp += b->timestamp_offset;
  
  /* Update statistics */
  if(!b->stats.initialized)
    init_stats(&b->stats, p->h.sequence_number, p->h.timestamp,
               b->timescale);
  else
    update_stats(&b->stats, p->h.sequence_number, p->h.timestamp,
                 b->timescale);

  /* Update sequence number */
  p->h.sequence_number += b->stats.cycles;
  
  /* Insert into read buffer */
  pthread_mutex_lock(&b->read_mutex);
  if(!b->read_packets)
    {
    b->read_packets = p;
    b->read_packets->next = NULL;
    }
  else if((b->last_seq >= 0) &&
          (b->last_seq > p->h.sequence_number))
    {
    drop = 1;
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Dropping obsolete packet");
    }
  else
    {
    rtp_packet_t * tmp;
    rtp_packet_t * p_iter;
    p_iter = b->read_packets;
    
    if(b->read_packets->h.sequence_number ==
       p->h.sequence_number)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Dropping duplicate packet");
      drop = 1;
      }
    else if(b->read_packets->h.sequence_number >
            p->h.sequence_number)
      {
      p->next = b->read_packets;
      b->read_packets = p;
      }
    else
      {
      p_iter = b->read_packets;
      while(p_iter->next)
        {
        if(p_iter->next->h.sequence_number > p->h.sequence_number)
          break;
        p_iter = p_iter->next;
        }
      tmp = p_iter->next;
      p_iter->next = p;
      p_iter->next->next = tmp;
      }
    }
  if(!drop)
    b->num++;
  
  pthread_mutex_unlock(&b->read_mutex);

  if(drop)
    {
    /* Push back */
    pthread_mutex_lock(&b->write_mutex);
    p->next = b->write_packets;
    b->write_packets = p;
    pthread_mutex_unlock(&b->write_mutex);
    }
  }

rtp_packet_t *
bgav_rtp_packet_buffer_try_lock_read(bgav_rtp_packet_buffer_t * b)
  {
  pthread_mutex_lock(&b->read_mutex);
  if(!b->read_packets)
    {
    pthread_mutex_unlock(&b->read_mutex);
    return NULL;
    }
  /* Waiting for packet */
  if((b->last_seq != -1) &&
     (b->read_packets->h.sequence_number != b->last_seq+1) &&
     (b->num < MAX_MISORDER))
    {
    pthread_mutex_unlock(&b->read_mutex);
    return NULL;
    }
  
  b->read_packet = b->read_packets;
  b->read_packets = b->read_packets->next;
  b->read_packet->next = NULL;

  if((b->last_seq >= 0) && 
     (b->read_packet->h.sequence_number != b->last_seq+1))
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "%"PRId64" packet(s) missing",
             b->read_packet->h.sequence_number - b->last_seq+1);
    b->read_packet->broken = 1;
    }
  else
    b->read_packet->broken = 0;

  b->last_seq = b->read_packet->h.sequence_number;
  
  b->num--;
  pthread_mutex_unlock(&b->read_mutex);
  //  fprintf(stderr, "Lock read\n");
  return b->read_packet;
  }

void bgav_rtp_packet_buffer_unlock_read(bgav_rtp_packet_buffer_t * b)
  {
  /* Push back */
  pthread_mutex_lock(&b->write_mutex);
  b->read_packet->next = b->write_packets;
  b->write_packets = b->read_packet;
  pthread_mutex_unlock(&b->write_mutex);
  //  fprintf(stderr, "Unlock read\n");
  
  b->read_packet = NULL;
  }

void bgav_rtp_packet_buffer_set_eof(bgav_rtp_packet_buffer_t * b)
  {
  pthread_mutex_lock(&b->eof_mutex);
  b->eof = 1;
  pthread_mutex_unlock(&b->eof_mutex);
  }

int bgav_rtp_packet_buffer_get_eof(bgav_rtp_packet_buffer_t * b)
  {
  int ret;
  pthread_mutex_lock(&b->eof_mutex);
  ret = b->eof;
  pthread_mutex_unlock(&b->eof_mutex);
  return ret;
  }

rtp_stats_t * bgav_rtp_packet_buffer_get_stats(bgav_rtp_packet_buffer_t * b)
  {
  return &b->stats;
  }
