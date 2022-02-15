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
#include <ctype.h>
#include <string.h>
#ifdef HAVE_POLL
#include <poll.h>
#endif
#include <unistd.h>

#ifdef _WIN32
#include <ws2spi.h>
#include <winsock2.h>
#endif


#include <avdec_private.h>
// #include <inttypes.h>
#include <rtp.h>

int bgav_rtcp_sr_read(bgav_input_context_t * ctx, rtcp_sr_t * ret)
  {
  uint16_t h;
  int i;
  if(!bgav_input_read_16_be(ctx, &h))
    return 0;
  ret->version         = (h >> 14) & 0x03;
  ret->padding         = (h >> 13) & 0x01;
  ret->rc              = (h >>  8) & 0x1f;
  ret->type            = (h) & 0xff;

  if(!bgav_input_read_16_be(ctx, &ret->length) ||
     !bgav_input_read_32_be(ctx, &ret->ssrc) ||
     !bgav_input_read_64_be(ctx, &ret->ntp_time) ||
     !bgav_input_read_32_be(ctx, &ret->rtp_time) ||
     !bgav_input_read_32_be(ctx, &ret->packet_count) ||
     !bgav_input_read_32_be(ctx, &ret->octet_count))
    return 0;

  for(i = 0; i < ret->rc; i++)
    {
    if(!bgav_input_read_32_be(ctx, &ret->reports[i].ssrc) ||
       !bgav_input_read_8(ctx, &ret->reports[i].fraction_lost) ||
       !bgav_input_read_24_be(ctx, &ret->reports[i].cumulative_lost) ||
       !bgav_input_read_32_be(ctx, &ret->reports[i].highest_ext_seq) ||
       !bgav_input_read_32_be(ctx, &ret->reports[i].jitter) ||
       !bgav_input_read_32_be(ctx, &ret->reports[i].lsr) ||
       !bgav_input_read_32_be(ctx, &ret->reports[i].dlsr))
      return 0;
    }
  return 1;
  }


int bgav_rtcp_rr_write(rtcp_sr_t * r, uint8_t * data)
  {
  int i;
  uint8_t * pos = data;
  
  pos[0] = (r->version << 6) | (r->padding << 5) | r->rc;
  pos++;
  pos[0] = r->type;
  pos++;
  GAVL_16BE_2_PTR(r->length, pos);pos+=2;
  GAVL_32BE_2_PTR(r->ssrc, pos);pos+=4;

  for(i = 0; i < r->rc; i++)
    {
    GAVL_32BE_2_PTR(r->reports[i].ssrc, pos);pos+=4;
    *pos = r->reports[i].fraction_lost;pos++;
    GAVL_24BE_2_PTR(r->reports[i].cumulative_lost, pos);pos+=3;
    GAVL_32BE_2_PTR(r->reports[i].highest_ext_seq, pos);pos+=4;
    GAVL_32BE_2_PTR(r->reports[i].jitter, pos);pos+=4;
    GAVL_32BE_2_PTR(r->reports[i].lsr, pos);pos+=4;
    GAVL_32BE_2_PTR(r->reports[i].dlsr, pos);pos+=4;
    }
  return pos - data;
  }


void bgav_rtcp_sr_dump(rtcp_sr_t * r)
  {
  int i;
  bgav_dprintf("RTCP RR\n");
  bgav_dprintf("  version:      %d\n", r->version);
  bgav_dprintf("  padding:      %d\n", r->padding);
  bgav_dprintf("  rc:           %d\n", r->rc);
  bgav_dprintf("  type:         %d\n", r->type);
  bgav_dprintf("  length:       %d\n", r->length);
  bgav_dprintf("  ssrc:         %08x\n", r->ssrc);
  bgav_dprintf("  ntp_time:     %"PRIu64"\n", r->ntp_time);
  bgav_dprintf("  rtp_time:     %u\n", r->rtp_time);
  bgav_dprintf("  packet_count: %u\n", r->packet_count);
  bgav_dprintf("  octet_count:  %u\n", r->octet_count);

  for(i = 0; i < r->rc; i++)
    {
    bgav_dprintf("  Report %d\n", i+1);
    bgav_dprintf("    ssrc:            %08x\n", r->reports[i].ssrc);
    bgav_dprintf("    fraction_lost:   %d\n", r->reports[i].fraction_lost);
    bgav_dprintf("    cumulative_lost: %d\n", r->reports[i].cumulative_lost);
    bgav_dprintf("    highest_ext_seq: %d\n", r->reports[i].highest_ext_seq);
    bgav_dprintf("    jitter:          %d\n", r->reports[i].jitter);
    bgav_dprintf("    lsr:             %d\n", r->reports[i].lsr);
    bgav_dprintf("    dlsr:            %d\n", r->reports[i].dlsr);
    }
  }

void bgav_rtcp_rr_setup(rtcp_sr_t * r, rtp_stats_t * s,
                        uint32_t lsr, uint32_t client_ssrc, uint32_t server_ssrc)
  {
  int extended_max;
  int expected;
  int fraction_lost;
  int lost;
  int lost_interval;
  int received_interval;
  int expected_interval;
  
  extended_max = s->cycles + s->max_seq;
  expected = extended_max - s->base_seq + 1;
  lost = expected - s->received;

  expected_interval = expected - s->expected_prior;
  s->expected_prior = expected;
  received_interval = s->received - s->received_prior;
  s->received_prior = s->received;
  lost_interval = expected_interval - received_interval;
  if (expected_interval == 0 || lost_interval <= 0)
    fraction_lost = 0;
  else
    fraction_lost = (lost_interval << 8) / expected_interval;
  
  r->version = 2;
  r->padding = 0;
  r->rc      = 1;
  r->type    = 201;
  
  r->length  = ((8+(r->rc*24)) / 4) - 1;

  r->ssrc = client_ssrc;
  
  r->reports[0].ssrc = server_ssrc;
  
  r->reports[0].fraction_lost = fraction_lost;
  r->reports[0].cumulative_lost = lost;
  r->reports[0].highest_ext_seq = extended_max;
  
  r->reports[0].jitter = s->jitter;
  r->reports[0].lsr = lsr;
  // r->reports[0].dlsr = 
  }
