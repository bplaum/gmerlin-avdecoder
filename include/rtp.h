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

#include <pthread.h> // For the mutex

#include <sdp.h>

extern bgav_demuxer_t bgav_demuxer_rtp;

#define RTP_MAX_PACKET_LENGTH 1500

int bgav_demuxer_rtp_open(bgav_demuxer_context_t * ctx,
                          bgav_sdp_t * sdp);

void bgav_demuxer_rtp_start(bgav_demuxer_context_t * ctx);
void bgav_demuxer_rtp_stop(bgav_demuxer_context_t * ctx);

void bgav_rtp_set_tcp(bgav_demuxer_context_t * ctx);

/* rtp statistics for generating receiver reports */

typedef struct
  {
  uint16_t max_seq;        /* highest seq. number seen */
  int64_t cycles;         /* shifted count of seq. number cycles */
  uint32_t base_seq;       /* base seq number */
  uint32_t bad_seq;        /* last 'bad' seq number + 1 */
  uint32_t probation;      /* sequ. packets till source is valid */
  uint32_t received;       /* packets received */
  uint32_t expected_prior; /* packet expected at last interval */
  uint32_t received_prior; /* packet received at last interval */
  uint32_t transit;        /* relative trans time for prev pkt */
  uint32_t jitter;         /* estimated jitter */
  /* ... */
  int initialized;
  gavl_timer_t * timer;
  gavl_time_t time_offset;
  } rtp_stats_t;

typedef struct
  {
  uint8_t version;
  uint8_t padding;
  uint8_t extension;
  uint8_t csrc_count;
  uint8_t marker;
  uint8_t payload_type;
  uint64_t sequence_number; /* These 2 are smaller in the rtp header,
                               we make them 64 bit because we add offsets */
  uint64_t timestamp;
  uint32_t ssrc;
  uint32_t csrc_list[15];
  } rtp_header_t;

typedef struct rtp_packet_s
  {
  rtp_header_t h;
  uint8_t buffer[RTP_MAX_PACKET_LENGTH];
  uint8_t * buf;
  int len;
  int broken; /* 1 if sequence number gap */
  struct rtp_packet_s * next;
  } rtp_packet_t;

typedef struct bgav_rtp_packet_buffer_s bgav_rtp_packet_buffer_t;

bgav_rtp_packet_buffer_t *
bgav_rtp_packet_buffer_create(const bgav_options_t * opt,
                              int timescale);
void bgav_rtp_packet_buffer_destroy(bgav_rtp_packet_buffer_t *);

rtp_packet_t *
bgav_rtp_packet_buffer_lock_write(bgav_rtp_packet_buffer_t *);

void bgav_rtp_packet_buffer_unlock_write(bgav_rtp_packet_buffer_t *);

void bgav_rtp_packet_buffer_set_eof(bgav_rtp_packet_buffer_t *);
int bgav_rtp_packet_buffer_get_eof(bgav_rtp_packet_buffer_t *);

rtp_stats_t * bgav_rtp_packet_buffer_get_stats(bgav_rtp_packet_buffer_t *);

#if 0
rtp_packet_t *
bgav_rtp_packet_buffer_lock_read(bgav_rtp_packet_buffer_t *);
#endif

rtp_packet_t *
bgav_rtp_packet_buffer_try_lock_read(bgav_rtp_packet_buffer_t *);

void bgav_rtp_packet_buffer_unlock_read(bgav_rtp_packet_buffer_t *);


typedef struct
  {
  int size;
  int delta;
  int dts_delta;
  int pts_delta;
  int keyftrame;
  } mpeg4_au_t;

typedef struct rtp_priv_s rtp_priv_t;

typedef struct rtp_stream_priv_s
  {
  pthread_mutex_t mutex;
  
  char * control_url;
  int rtp_fd;
  int rtcp_fd;
  int64_t first_rtptime;
  int first_seq;
  char ** fmtp;

  int interleave_base;
  
  uint32_t server_ssrc;
  uint32_t client_ssrc;
  
  int (*process)(bgav_stream_t * s, rtp_header_t * h, uint8_t * data, int len);
  
  void (*free_priv)(struct rtp_stream_priv_s*);
  
  rtp_priv_t * rtp_priv;
  
  union
    {
    struct
      {
      int sizelength;
      int indexlength;
      int indexdeltalength;
      int num_aus;
      mpeg4_au_t * aus;
      int aus_alloc;
      } mpeg4_generic;
    struct
      {
      int ident;
      } xiph;
    } priv;
  
  } rtp_stream_priv_t;

/* RTCP Stuff */

#define MAX_RTCP_REPORTS 31
#define RR_MAX_SIZE (8+(MAX_RTCP_REPORTS*24))

typedef struct
  {
  uint8_t version;
  uint8_t padding;
  uint8_t rc;
  uint8_t type;
  uint16_t length;
  uint32_t ssrc;

  /* These miss in Receiver report */
  uint64_t ntp_time;
  uint32_t rtp_time;
  uint32_t packet_count;
  uint32_t octet_count;

  /* Report blocks */
  
  struct
    {
    uint32_t ssrc;
    uint8_t fraction_lost;
    uint32_t cumulative_lost;
    uint32_t highest_ext_seq;
    uint32_t jitter;
    uint32_t lsr;
    uint32_t dlsr;
    } reports[MAX_RTCP_REPORTS];
  } rtcp_sr_t;



int bgav_rtcp_sr_read(bgav_input_context_t * ctx, rtcp_sr_t * ret);
int bgav_rtcp_rr_write(rtcp_sr_t * r, uint8_t * data);
void bgav_rtcp_sr_dump(rtcp_sr_t * r);
void bgav_rtcp_rr_setup(rtcp_sr_t * r, rtp_stats_t * s,
                        uint32_t lsr, uint32_t client_ssrc, uint32_t server_ssrc);
