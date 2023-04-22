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

/* Package includes */

#include <avdec_private.h>
#include <mpegts_common.h>
#include <pes_header.h>

#define LOG_DOMAIN "mpegts2"

typedef struct
  {
  uint16_t pmt_pid;
  uint16_t pcr_pid;

  int has_pmt;
  } program_t;

typedef struct
  {
  int packet_size;

  program_t * programs;
  int num_programs;
  
  int have_pat;

  gavl_buffer_t buf;

  bgav_input_context_t * pes_parser;
  
  } mpegts_priv_t;

#define PROBE_SIZE 32000

static int test_packet_size(uint8_t * probe_data, int size)
  {
  int i;
  for(i = 0; i < PROBE_SIZE; i+= size)
    {
    if(probe_data[i] != 0x47)
      return 0;
    }
  return 1;
  }

static int guess_packet_size(bgav_input_context_t * input)
  {
  uint8_t probe_data[PROBE_SIZE];
  if(bgav_input_get_data(input, probe_data, PROBE_SIZE) < PROBE_SIZE)
    return 0;

  if(test_packet_size(probe_data, TS_FEC_PACKET_SIZE))
    return TS_FEC_PACKET_SIZE;
  if(test_packet_size(probe_data, TS_DVHS_PACKET_SIZE))
    return TS_DVHS_PACKET_SIZE;
  if(test_packet_size(probe_data, TS_PACKET_SIZE))
    return TS_PACKET_SIZE;
  
  return 0;
  }


static int probe_mpegts(bgav_input_context_t * input)
  {
  if(guess_packet_size(input))
    return 1;
  return 0;
  }


static int init_psi(bgav_demuxer_context_t * ctx)
  {
  int scan_packets;
  int scan_size;
  gavl_buffer_t buf;
  uint8_t * pos;
  uint8_t * packet_start;
  uint8_t * end;
  transport_packet_t pkt;
  pat_section_t pats;
  pmt_section_t pmts;
  int i, j;
  int skip;
  int ret = 0;
  int done = 0;
  mpegts_priv_t * priv = ctx->priv;

  if(!(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    scan_packets = 32000 / priv->packet_size; // 32 kB
  else
    scan_packets = (5 * 1024 * 1024) / priv->packet_size;

  scan_size = scan_packets * priv->packet_size;
  
  gavl_buffer_init(&buf);
  gavl_buffer_alloc(&buf, scan_size);

  buf.len = bgav_input_get_data(ctx->input, buf.buf, scan_size);

  packet_start = buf.buf;
  pos = packet_start;
  end = buf.buf + buf.len;

  /* Scan for PAT */
  
  while(pos < end)
    {
    memset(&pkt, 0, sizeof(pkt));
    
    if(!bgav_transport_packet_parse(&pos, &pkt))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Lost sync during initialization");
      return 0;
      }

    if(pkt.pid == 0x0000)
      {
      skip = 1 + pos[0];
      pos += skip;
      
      if(!bgav_pat_section_read(pos, end - pos, &pats))
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "PAT spans multiple packets, please report");
        return 0;
        }

      if(ctx->opt->dump_headers)
        bgav_pat_section_dump(&pats);
      if(pats.section_number || pats.last_section_number)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "PAT has multiple sections, please report");
        return 0;
        }
      
      /* Count the programs */
      
      for(i = 0; i < pats.num_programs; i++)
        {
        if(pats.programs[i].program_number != 0x0000)
          priv->num_programs++;
        }
      packet_start += priv->packet_size;
      pos = packet_start;
      break;
      }
    packet_start += priv->packet_size;
    pos = packet_start;
    }

  if(!priv->num_programs)
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Got no PAT or no programs");
    goto fail;
    }

  /* Initialize track table */
  priv->programs = calloc(priv->num_programs, sizeof(*priv->programs));

  j = 0;
  for(i = 0; i < pats.num_programs; i++)
    {
    if(!pats.programs[i].program_number)
      {
      i++;
      continue;
      }
    priv->programs[j].pmt_pid = pats.programs[i].program_map_pid;
    j++;
    }

  ctx->tt = bgav_track_table_create(priv->num_programs);
  
  for(i = 0; i < priv->num_programs; i++)
    {
    ctx->tt->tracks[i]->priv = &priv->programs[i];
    }
  
  /* Scan for PMTs */
  while(pos < end)
    {
    memset(&pkt, 0, sizeof(pkt));

    if(!bgav_transport_packet_parse(&pos, &pkt))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Lost sync during initialization");
      goto fail;
      }

    for(i = 0; i < priv->num_programs; i++)
      {
      if(priv->programs[i].has_pmt)
        continue;
      
      if(pkt.pid == priv->programs[i].pmt_pid)
        {
        skip = 1 + pos[0];
        pos += skip;
        
        if(!bgav_pmt_section_read(pos, pkt.payload_size-skip,
                                  &pmts))
          {
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                   "PMT section spans multiple packets, please report");
          goto fail;
          }
        if(ctx->opt->dump_headers)
          bgav_pmt_section_dump(&pmts);
        if(pmts.section_number ||
           pmts.last_section_number)
          {
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                   "PMT has multiple sections, please report");
          goto fail;
          }

        priv->programs[i].pcr_pid = pmts.pcr_pid;
        
        if(bgav_pmt_section_setup_track(&pmts,
                                        ctx->tt->tracks[i],
                                        ctx->opt, -1, -1, -1, NULL, NULL))
          {
          bgav_track_set_format(ctx->tt->tracks[i], "MPEGTS", "video/MP2T");
          priv->programs[i].has_pmt = 1;
          ctx->tt->tracks[i]->priv = &priv->programs[i];
          }
        else
          {
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Setting up track failed");
          return 0;
          }

        /* Check if we are done */

        done = 1;

        for(j = 0; j < ctx->tt->num_tracks; j++)
          {
          if(!priv->programs[i].has_pmt)
            {
            done = 0;
            break;
            }
          }
        break;
        }
      
      }

    if(done)
      break;
    
    packet_start += priv->packet_size;
    pos = packet_start;
    
    }

#if 0  
  fprintf(stderr, "Initialized streams\n");

  for(i = 0; i < ctx->tt->num_tracks; i++)
    bgav_track_dump(ctx->tt->tracks[i]);
#endif
  
  priv->have_pat = 1;

  ret = 1;

  fail:
  
  gavl_buffer_free(&buf);
  
  return ret;
  }


static int open_mpegts(bgav_demuxer_context_t * ctx)
  {
  int64_t start;
  mpegts_priv_t * priv;

  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
  
  /* Obtain packet size */
  priv->packet_size = guess_packet_size(ctx->input);

  start = ctx->input->position;
  
  /* Scan for PAT/PMT and Initialize streams */
  if(!init_psi(ctx))
    {
    if(priv->have_pat)
      return 0;
    
    /* TODO: initialize "raw" stream */
    
    }

  if(ctx->tt)
    ctx->tt->cur->data_start = start;
  
  gavl_buffer_alloc(&priv->buf, priv->packet_size);
  priv->pes_parser = bgav_input_open_memory(NULL, 0);
  
  if(ctx->input->flags & (BGAV_INPUT_CAN_SEEK_BYTE | BGAV_INPUT_CAN_SEEK_TIME))
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
  
  ctx->flags |= BGAV_DEMUXER_GET_DURATION;
  return 1;
  }

static gavl_source_status_t next_packet_mpegts(bgav_demuxer_context_t * ctx)
  {
  mpegts_priv_t * priv;
  int done = 0;
  uint8_t * ptr;
  transport_packet_t pkt;
  bgav_stream_t * s;
  int len;
  bgav_pes_header_t pes_header; 
  int64_t pos;
  program_t * p;
  /* To avoid overhead we scan transport packets until we finished at least one PES packet */
  
  priv = ctx->priv;
  p = ctx->tt->cur->priv;

  while(!done)
    {
    
    pos = ctx->input->position;

    if(bgav_input_read_data(ctx->input, priv->buf.buf, priv->packet_size) < priv->packet_size)
      {
      /* EOF */
      return GAVL_SOURCE_EOF;
      }
    ptr = priv->buf.buf;

    
    bgav_transport_packet_parse(&ptr, &pkt);

    if(!pkt.pid)
      continue;

    /* Handle PCR */
    if((pkt.pid == p->pcr_pid) && (pkt.adaption_field.pcr >= 0))
      {
      //      fprintf(stderr, "Got PCR: %d %"PRId64"\n", p->pcr_pid, pkt.adaption_field.pcr);
      }


    //    if(pkt.pid == 0x01e2)
    //      fprintf(stderr, "Got PID %04x\n", pkt.pid);      
    
    /* Check if this belongs to a stream */
    s = bgav_track_find_stream(ctx, pkt.pid);

    if(!s)
      {
      //      fprintf(stderr, "No stream for PID %04x\n", pkt.pid);
      //      gavl_hexdump(ptr, 188, 16);
      continue;
      }

    //    fprintf(stderr, "Got stream %d\n", s->stream_id);
    
    if(pkt.payload_start) // First transport packet of one PES packet
      {
      if(s->type == GAVL_STREAM_VIDEO)
        {
        //        fprintf(stderr, "Got packet for video stream %d\n", s->stream_id);
        //        gavl_hexdump(ptr, 4, 4);
        //        bgav_transport_packet_dump(&pkt);
        }

      if(s->packet)
        {
#if 0
        if(s->type == GAVL_STREAM_VIDEO)
          {
          fprintf(stderr, "Got packet for stream %d\n", s->stream_id);
          bgav_packet_dump(s->packet);
          }
#endif
        bgav_stream_done_packet_write(s, s->packet);
        s->packet = NULL;
        done = 1;
        }
      
      s->packet = bgav_stream_get_packet_write(s);
      s->packet->position = pos;
      
      bgav_input_reopen_memory(priv->pes_parser,
                               ptr, pkt.payload_size);
      
      if(!bgav_pes_header_read(priv->pes_parser,
                               &pes_header))
        return GAVL_SOURCE_EOF;
      
      //      fprintf(stderr, "Got PTS: %"PRId64"\n", pes_header.pts);
      s->packet->pes_pts = pes_header.pts;
      
      bgav_input_set_demuxer_pts(ctx->input, pes_header.pts, 90000);
      
      if(ctx->input->clock_time != GAVL_TIME_UNDEFINED)
        {
        bgav_demuxer_set_clock_time(ctx,
                                    pes_header.pts, 90000, ctx->input->clock_time);
        
        ctx->input->clock_time = GAVL_TIME_UNDEFINED;
        }
      
      ptr += priv->pes_parser->position;
      
      len = 188 - (ptr - priv->buf.buf);
      
      bgav_packet_alloc(s->packet, len);
      memcpy(s->packet->buf.buf, ptr, len);
      s->packet->buf.len = len;

      }
    else
      {
      /* Skip initial transport packet */
      if(!s->packet)
        {
#if 0
        fprintf(stderr, "Discarding packet (%d bytes)\n", 188 - (ptr - priv->buf.buf));
        gavl_hexdump(ptr, 16, 16);
#endif
        continue;
        }
      /* Append to packet */
      else
        {
        bgav_packet_alloc(s->packet,
                          s->packet->buf.len +
                          pkt.payload_size);
        
        memcpy(s->packet->buf.buf + s->packet->buf.len, ptr,
               pkt.payload_size);
        s->packet->buf.len += pkt.payload_size;
        }
      }
    }
  return GAVL_SOURCE_OK;
  }


static void seek_mpegts(bgav_demuxer_context_t * ctx, int64_t time, int scale)
  {

  }

static void close_mpegts(bgav_demuxer_context_t * ctx)
  {
  mpegts_priv_t * priv;

  priv = ctx->priv;
  
  gavl_buffer_free(&priv->buf);
  if(priv->pes_parser)
    bgav_input_destroy(priv->pes_parser);
  
  if(priv->programs)
    free(priv->programs);
  
  free(priv);
  }

static int post_seek_resync_mpegts(bgav_demuxer_context_t * ctx)
  {
  int rest;
  mpegts_priv_t * priv;

  priv = ctx->priv;

  /* Skip everything until the next packet */
  if((rest = (ctx->input->position - ctx->tt->cur->data_start) % priv->packet_size))
    bgav_input_skip(ctx->input, priv->packet_size - rest);

  /* Everything else is handled by next_packet_mpegts */
  
  return 1;
  }

#if 0
static int select_track_mpegts(bgav_demuxer_context_t * ctx,
                               int track)
  {
  
  return 1;
  }
#endif

const bgav_demuxer_t bgav_demuxer_mpegts2 =
  {
    .probe =            probe_mpegts,
    .open =             open_mpegts,
    .next_packet =      next_packet_mpegts,
    .post_seek_resync = post_seek_resync_mpegts,
    .seek =             seek_mpegts,
    .close =            close_mpegts,
    //    .select_track = select_track_mpegts
  };

