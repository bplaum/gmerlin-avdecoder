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


/* System includes */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Package includes */

#include <avdec_private.h>
#include <pes_header.h>
#include <a52_header.h>

#include <mpegts_common.h>

#define LOG_DOMAIN "demux_ts"

/* Packet to read at once during scanning */
#define SCAN_PACKETS      1000

/* Packet to read at once during scanning if input is seekable */
#define SCAN_PACKETS_SEEK 32000

#define TS_PACKET_SIZE      188
#define TS_DVHS_PACKET_SIZE 192
#define TS_FEC_PACKET_SIZE  204

#define TS_MAX_PACKET_SIZE  204

/* Maximum number of consecutive error packets */
#define MAX_ERROR_PACKETS   10

#if 0

// #define DUMP_HDV_AUX
typedef struct
  {
  gavl_timecode_t tc; /* Timecode */
  gavl_timecode_t rd; /* Recording date/time */
  
  int timescale;
  int frame_duration;
  int int_framerate;
  } hdv_vaux_t;

#ifdef DUMP_HDV_AUX
static void dump_vaux(const hdv_vaux_t * vaux)
  {
  bgav_dprintf("HDV VAUX packet\n");

  bgav_dprintf("  Timecode: ");
  if(vaux->tc != GAVL_TIMECODE_UNDEFINED)
    {
    gavl_timecode_dump(NULL, vaux->tc);
    bgav_dprintf("\n");
    }
  else
    bgav_dprintf("None\n");

  bgav_dprintf("  Date/Time: ");
  if(vaux->rd != GAVL_TIMECODE_UNDEFINED)
    {
    gavl_timecode_dump(NULL, vaux->rd);
    bgav_dprintf("\n");
    }
  else
    bgav_dprintf("None\n");
  
  bgav_dprintf("  Framerate: %d/%d\n",
               vaux->timescale, vaux->frame_duration); 
  bgav_dprintf("  Timecode rate: %d\n", vaux->int_framerate);
  }
#endif

#endif

typedef struct
  {
  int64_t last_pts;
  int64_t pts_offset;
  int64_t pts_offset_2nd;
  } stream_priv_t;

typedef struct
  {
  int initialized;
  
  uint16_t program_map_pid;
  
  int64_t start_pcr;
  int64_t end_pcr;
  int64_t end_pcr_test; /* For scanning the end timestamps */
  
  uint16_t pcr_pid;

  /* AAUX and VAUX for HDV */
  //  uint16_t aaux_pid;
  //  uint16_t vaux_pid;
  
  pmt_section_t pmts;
  
  stream_priv_t * streams;
  } program_priv_t;

static void init_streams_priv(program_priv_t * program,
                              bgav_track_t * track)
  {
  bgav_stream_t * s;
  
  int num_streams, index;
  int i;
  num_streams =
    track->num_audio_streams +
    track->num_video_streams;
  
  program->streams = calloc(num_streams, sizeof(*program->streams));
  index = 0;
  for(i = 0; i < track->num_audio_streams; i++)
    {
    s = bgav_track_get_audio_stream(track, i);
    program->streams[index].last_pts = BGAV_TIMESTAMP_UNDEFINED;
    s->priv = &program->streams[index];
    index++;
    }
  for(i = 0; i < track->num_video_streams; i++)
    {
    s = bgav_track_get_video_stream(track, i);
    program->streams[index].last_pts = BGAV_TIMESTAMP_UNDEFINED;
    s->priv = &program->streams[index];
    index++;
    }
  }

static void reset_streams_priv(bgav_track_t * track)
  {
  int i;
  stream_priv_t * priv;
  for(i = 0; i < track->num_streams; i++)
    {
    if(track->streams[i].flags & STREAM_EXTERN)
      continue;
    
    priv = track->streams[i].priv;
    priv->last_pts = BGAV_TIMESTAMP_UNDEFINED;
    priv->pts_offset = 0;
    }

  }

#define WRAP_THRESHOLD 9000000 /* 100 seconds */

static void check_pts_wrap(bgav_stream_t * s, int64_t * pts)
  {
  char tmp_string1[128];
  char tmp_string2[128];
  stream_priv_t * priv;
  priv = s->priv;

  if(priv->last_pts == BGAV_TIMESTAMP_UNDEFINED)
    {
    priv->last_pts = *pts;
    return;
    }

  /* Detected PTS wrap */
  if(*pts + WRAP_THRESHOLD < priv->last_pts)
    {
    priv->pts_offset_2nd = priv->pts_offset;
    priv->pts_offset += ((int64_t)1) << 33;
    sprintf(tmp_string1, "%" PRId64, *pts);
    sprintf(tmp_string2, "%" PRId64, priv->last_pts);
    
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
             "Detected pts wrap (%s < %s)",
             tmp_string1, tmp_string2);
    priv->last_pts = *pts;
    *pts += priv->pts_offset;
    }
  /* Old timestamp (from before PTS wrap, might come due to MPEG frame reordering) */
  else if(*pts - WRAP_THRESHOLD > priv->last_pts)
    *pts += priv->pts_offset_2nd;
  else
    {
    priv->last_pts = *pts;
    *pts += priv->pts_offset;
    }
  }

typedef struct
  {
  int packet_size;
  
  int num_programs;
  program_priv_t * programs;
  
  /* Input needed for pes header parsing */
  bgav_input_context_t * input_mem;
  
  int64_t first_packet_pos;

  int current_program;
  
  transport_packet_t packet;

  uint8_t * buffer;
  uint8_t * ptr;
  uint8_t * packet_start; 
  
  int buffer_size;
  int do_sync;

  int error_counter;

  int discontinuous;

  int is_running;
  
  } mpegts_t;

static int resync_file(bgav_demuxer_context_t * ctx,
                       uint8_t * ptr, int offset, int * len)
  {
  int i;
  mpegts_t * priv = ctx->priv;
  uint8_t * ptr1 = ptr + offset + 1;
  
  int bytes_skipped = 1;
  
  for(i = 1; i < priv->packet_size; i++)
    {
    if(*ptr1 == 0x47)
      {
      if(*len - offset - bytes_skipped > 0)
        memmove(ptr + offset, ptr1, *len - offset - bytes_skipped);
      *len -= bytes_skipped;
      *len += bgav_input_read_data(ctx->input, ptr + *len, bytes_skipped);
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
               "Skipped discontinuity %d bytes", bytes_skipped);
      priv->discontinuous = 1;
      return bytes_skipped;
      }
    ptr1++;
    bytes_skipped++;
    }
  return 0;
  }

static int read_data(bgav_demuxer_context_t * ctx, int num_packets)
  {
  int i;
  int bytes_read;
  mpegts_t * priv = ctx->priv;
  uint8_t * ptr;
  
  /* Read data */
  bytes_read =
    bgav_input_read_data(ctx->input,
                         priv->buffer, num_packets * priv->packet_size);
  
  /* Check if we are in sync */
  
  ptr = priv->buffer;

  num_packets = bytes_read / priv->packet_size;
  
  for(i = 0; i < num_packets; i++)
    {
    if(*ptr != 0x47)
      {
      if(!resync_file(ctx, priv->buffer, ptr - priv->buffer, &bytes_read))
        break;
      }
    ptr += priv->packet_size;
    }
  num_packets = bytes_read / priv->packet_size;
  return num_packets * priv->packet_size;
  }

#if 0
static int get_data(bgav_demuxer_context_t * ctx, int num_packets)
  {
  return 0;
  }
#endif

static inline int
parse_transport_packet(bgav_demuxer_context_t * ctx)
  {
  mpegts_t * priv = ctx->priv;
  
  if(!bgav_transport_packet_parse(&priv->ptr, &priv->packet))
    {
    if(ctx->input->total_bytes > 0)
      {
      if(ctx->input->total_bytes - ctx->input->position > 1024)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Lost sync %"PRId64" bytes before file end\n",
                 ctx->input->total_bytes - ctx->input->position);
        }
      }
    else
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Lost sync\n");
      }
    return 0;
    }
  if(priv->packet.transport_error)
    {
    priv->error_counter++;
    if(priv->error_counter > MAX_ERROR_PACKETS)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Too many transport errors");
      return 0;
      }
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Transport error");
    return 1;
    }

  priv->error_counter = 0;
  return 1;
  }




static inline int next_packet(mpegts_t * priv)
  {
  priv->packet_start += priv->packet_size;
  priv->ptr = priv->packet_start;
  return (priv->ptr - priv->buffer < priv->buffer_size);
  }

static inline int next_packet_scan(bgav_demuxer_context_t * ctx)
  {
  int packets_scanned;

  mpegts_t * priv = ctx->priv;
  int can_seek = (ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE) ? 1 : 0;
  
  if(!next_packet(priv))
    {
    if(can_seek)
      {
      packets_scanned =
        (ctx->input->position - priv->first_packet_pos) / priv->packet_size;

      if(packets_scanned < SCAN_PACKETS_SEEK)
        {
        priv->buffer_size =read_data(ctx, SCAN_PACKETS);
        if(!priv->buffer_size)
          return 0;
        
        priv->ptr = priv->buffer;
        priv->packet_start = priv->buffer;
        }
      else
        return 0;
      }
    else
      return 0;
    }
  return 1;
  }

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

/* Get program durations */

static int64_t
get_program_timestamp(bgav_demuxer_context_t * ctx, int * program)
  {
  int i, j, program_index;
  mpegts_t * priv;
  bgav_pes_header_t pes_header;

  priv = ctx->priv;

  /* Get the program to which the packet belongs */

  program_index = -1;
  for(i = 0; i < priv->num_programs; i++)
    {
    if(priv->programs[i].pcr_pid == priv->packet.pid)
      program_index = i;
    }
  
  if(program_index < 0)
    {
    for(i = 0; i < priv->num_programs; i++)
      {
      if(priv->programs[i].pcr_pid > 0)
        continue;
      
      for(j = 0; j < ctx->tt->tracks[i]->num_streams; j++)
        {
        if(ctx->tt->tracks[i]->streams[j].stream_id == priv->packet.pid)
          {
          program_index = i;
          break;
          }
        }
      if(program_index >= 0)
        break;
      }
    }

  if(program_index < 0)
    return -1;
  
  /* PCR timestamp */
  if(priv->programs[program_index].pcr_pid == priv->packet.pid)
    {
    if(priv->packet.adaption_field.pcr > 0)
      {
      *program = program_index;
      return priv->packet.adaption_field.pcr;
      }
    else
      return -1;
    }
  /* PES timestamp */
  if(!priv->packet.payload_start)
    return -1;
  
  bgav_input_reopen_memory(priv->input_mem, priv->ptr,
                           priv->packet.payload_size);
  
  bgav_pes_header_read(priv->input_mem, &pes_header);
  priv->ptr += priv->input_mem->position;

  if(pes_header.pts > 0)
    {
    *program = program_index;
    return pes_header.pts;
    }
  else
    return -1;
  }

static int get_program_durations(bgav_demuxer_context_t * ctx)
  {
  mpegts_t * priv;
  int keep_going;
  int i;
  int64_t pts;
  int program_index = -1;
  int64_t total_packets;
  int64_t position;
  
  priv = ctx->priv;
  
  bgav_input_seek(ctx->input, priv->first_packet_pos, SEEK_SET);
  
  for(i = 0; i < priv->num_programs; i++)
    {
    priv->programs[i].start_pcr    = -1;
    priv->programs[i].end_pcr      = -1;
    priv->programs[i].end_pcr_test = -1;
    }
  
  /* Get the start timestamps of all programs */

  keep_going = 1;
  
  priv->buffer_size = read_data(ctx, SCAN_PACKETS);
  if(!priv->buffer_size)
    return 0;
  
  priv->ptr = priv->buffer;
  priv->packet_start = priv->buffer;
  
  while(keep_going)
    {
    if(!parse_transport_packet(ctx))
      return 0;

    pts = get_program_timestamp(ctx, &program_index);
    if((pts > 0) && (priv->programs[program_index].start_pcr < 0))
      {
      priv->programs[program_index].start_pcr = pts;
      }
    if(!next_packet_scan(ctx))
      return 0;
    
    /* Check if we are done */
    keep_going = 0;
    for(i = 0; i < priv->num_programs; i++)
      {
      if(priv->programs[i].initialized && (priv->programs[i].start_pcr == -1))
        {
        keep_going = 1;
        break;
        }
      }
    }

  /* Now, get the end timestamps */

  total_packets =
    (ctx->input->total_bytes - priv->first_packet_pos) / priv->packet_size;
  position =
    priv->first_packet_pos + (total_packets - SCAN_PACKETS) * priv->packet_size;
  
  keep_going = 1;

  while(keep_going)
    {
    bgav_input_seek(ctx->input, position, SEEK_SET);

    priv->buffer_size = read_data(ctx, SCAN_PACKETS);
    if(!priv->buffer_size)
      return 0;

    if(priv->discontinuous)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "Got discontinuities, won't get wrong durations");
      return 0;
      }
    
    priv->ptr = priv->buffer;
    priv->packet_start = priv->buffer;

    for(i = 0; i < SCAN_PACKETS; i++)
      {
      if(!parse_transport_packet(ctx))
        return 0;
      
      pts = get_program_timestamp(ctx, &program_index);
      if(pts > 0)
        priv->programs[program_index].end_pcr_test = pts;
      
      next_packet(priv);
      }
    
    /* Check if we are done */
    for(i = 0; i < priv->num_programs; i++)
      {
      if(priv->programs[i].end_pcr < priv->programs[i].end_pcr_test)
        priv->programs[i].end_pcr = priv->programs[i].end_pcr_test;
      }
    
    keep_going = 0;
    for(i = 0; i < priv->num_programs; i++)
      {
      if(priv->programs[i].initialized &&
         (priv->programs[i].end_pcr == -1))
        {
        keep_going = 1;
        break;
        }
      }
    position -= SCAN_PACKETS * priv->packet_size;
    if(position < priv->first_packet_pos)
      return 0; // Should never happen
    }
  
  /* Set the durations */
  for(i = 0; i < priv->num_programs; i++)
    {
    if(priv->programs[i].initialized)
      {
      if(priv->programs[i].end_pcr > priv->programs[i].start_pcr)
        gavl_track_set_duration(ctx->tt->tracks[i]->info,
                                gavl_time_unscale(90000,
                                                  priv->programs[i].end_pcr -
                                                  priv->programs[i].start_pcr));
      else
        return 0;
      }
    }
  return 1;
  }

/*
 *  Initialize using a PAT and PMTs
 *  This function expects a PAT table at the beginning
 *  of the parsed buffer.
 */

static int init_psi(bgav_demuxer_context_t * ctx,
                    int input_can_seek)
  {
  int program;
  int keep_going;
  pat_section_t pats;
  int skip;
  mpegts_t * priv;
  int i, j;
  
  priv = ctx->priv;
  //  gavl_hexdump(data, packet.payload_size, 16);

  /* We are at the beginning of the payload of a PAT packet */
  skip = 1 + priv->ptr[0];
  
  priv->ptr += skip;
  
  if(!bgav_pat_section_read(priv->ptr, priv->packet.payload_size - skip,
                       &pats))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "PAT section spans multiple packets, please report");
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
    
  /* Allocate programs and track table */
  
  priv->programs = calloc(priv->num_programs, sizeof(*(priv->programs)));
  ctx->tt = bgav_track_table_create(priv->num_programs);
  
  /* Assign program map pids */

  j = 0;

  for(i = 0; i < pats.num_programs; i++)
    {
    if(pats.programs[i].program_number != 0x0000)
      {
      priv->programs[j].program_map_pid =
        pats.programs[i].program_map_pid;
      j++;
      }
    }

  if(!next_packet_scan(ctx))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Premature EOF");
    return 0;
    }
  
  /* Next, we want to get all programs */

  keep_going = 1;

  while(keep_going)
    {
    if(!parse_transport_packet(ctx))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Lost sync during initializing");
      return 0;
      }
    
    //fprintf(stderr, "Got PID: %d\n", priv->packet.pid);
    
    for(program = 0; program < priv->num_programs; program++)
      {
      /* Check if we got the PMT of a program */
      if(priv->packet.pid == priv->programs[program].program_map_pid)
        break;
#if 0
      /* Check if the PMT is already parsed and we got a stream ID */
      else if(priv->programs[program].pmts.table_id == 0x02)
        {
        for(i = 0; i < priv->programs[program].pmts.num_streams; i++)
          {
          if(priv->packet.pid == priv->programs[program].pmts.streams[i].pid)
            {
            priv->programs[program].pmts.streams[i].present = 1;
            }
          }
        }
#endif
      }
    
    if(program == priv->num_programs)
      {
      if(!next_packet_scan(ctx))
        break;
      continue;
      }

    skip = 1 + priv->ptr[0];
    priv->ptr += skip;

    if(!priv->programs[program].pmts.table_id)
      {
      if(!bgav_pmt_section_read(priv->ptr, priv->packet.payload_size-skip,
                                &priv->programs[program].pmts))
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "PMT section spans multiple packets, please report");
        return 0;
        }
      if(ctx->opt->dump_headers)
        bgav_pmt_section_dump(&priv->programs[program].pmts);
      if(priv->programs[program].pmts.section_number ||
         priv->programs[program].pmts.last_section_number)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "PMT has multiple sections, please report");
        return 0;
        }
      }

    keep_going = 0;
    for(program = 0; program < priv->num_programs; program++)
      {
      /* Check if we got the PMT of a program */
      if(!priv->programs[program].pmts.table_id)
        keep_going = 1;
      }
    if(!keep_going)
      break;
    
    if(!next_packet_scan(ctx))
      break;
    }

  for(program = 0; program < priv->num_programs; program++)
    {
    if(bgav_pmt_section_setup_track(&priv->programs[program].pmts,
                                    ctx->tt->tracks[program],
                                    ctx->opt, -1, -1, -1, NULL, NULL))
      {
      bgav_track_set_format(ctx->tt->tracks[program], "MPEGTS", "video/MP2T");
      priv->programs[program].pcr_pid = priv->programs[program].pmts.pcr_pid;
      priv->programs[program].initialized = 1;
      init_streams_priv(&priv->programs[program],
                        ctx->tt->tracks[program]);
      //      fprintf(stderr, "Got streams from PMT:\n");
      //      gavl_dictionary_dump(ctx->tt->tracks[program]->info, 2);

#if 0
      /* Get the AAUX and VAUX PIDs */
      for(i = 0; i < priv->programs[program].pmts.num_streams; i++)
        {
        if(priv->programs[program].pmts.streams[i].type == 0xa0)
          {
          priv->programs[program].aaux_pid =
            priv->programs[program].pmts.streams[i].pid;
          fprintf(stderr, "Got AAUX PID: %04x\n",
                  priv->programs[program].aaux_pid);
          }
        if(priv->programs[program].pmts.streams[i].type == 0xa1)
          {
          priv->programs[program].vaux_pid =
            priv->programs[program].pmts.streams[i].pid;
          fprintf(stderr, "Got VAUX PID: %04x\n",
                  priv->programs[program].vaux_pid);
          }
        
        }
#endif
      }
    }
  return 1;
  }

typedef struct
  {
  int pid;
  int pes_id;
  uint8_t * buffer;
  int buffer_size;
  int buffer_alloc;
  int done;
  } test_stream_t;

typedef struct
  {
  int num_streams;
  int last_added;
  test_stream_t * streams;
  } test_streams_t;

static void
test_streams_append_packet(test_streams_t * s, uint8_t * data,
                           int data_size,
                           int pid, bgav_pes_header_t * header)
  {
  int i, index = -1;
  for(i = 0; i < s->num_streams; i++)
    {
    if(s->streams[i].pid == pid)
      {
      index = i;
      break;
      }
    }

  if(index == -1)
    {
    if(!header)
      {
      s->last_added = -1;
      return;
      }
    else
      {
      s->streams =
        realloc(s->streams, (s->num_streams+1)*sizeof(*s->streams));
      memset(s->streams + s->num_streams, 0, sizeof(*s->streams));
      s->last_added = s->num_streams;
      s->num_streams++;
      s->streams[s->last_added].pes_id = header->stream_id;
      s->streams[s->last_added].pid = pid;
      }
    }
  else if(s->streams[index].done)
    {
    s->last_added = -1;
    return;
    }
  else
    s->last_added = index;
    
  if(s->streams[s->last_added].buffer_size + data_size >
     s->streams[s->last_added].buffer_alloc)
    {
    s->streams[s->last_added].buffer_alloc =
      s->streams[s->last_added].buffer_size + data_size + 1024;
    s->streams[s->last_added].buffer =
      realloc(s->streams[s->last_added].buffer,
              s->streams[s->last_added].buffer_alloc);
    }
  memcpy(s->streams[s->last_added].buffer +
         s->streams[s->last_added].buffer_size,
         data, data_size);
  s->streams[s->last_added].buffer_size += data_size;
  }

static int test_a52(test_stream_t * st)
  {
  /* Check for 2 consecutive a52 headers */
  bgav_a52_header_t header;
  uint8_t * ptr, *ptr_end;
  
  ptr     = st->buffer;
  ptr_end = st->buffer + st->buffer_size;

  while(ptr_end - ptr > BGAV_A52_HEADER_BYTES)
    {
    if(bgav_a52_header_read(&header, ptr))
      {
      ptr += header.total_bytes;
      
      if((ptr_end - ptr > BGAV_A52_HEADER_BYTES) &&
         bgav_a52_header_read(&header, ptr))
        {
        return 1;
        }
      }
    else
      ptr++;
    }
  
  return 0;
  }

static bgav_stream_t *
test_streams_detect(test_streams_t * s, bgav_track_t * track,
                    const bgav_options_t * opt)
  {
  bgav_stream_t * ret = NULL;
  test_stream_t * st;
  if(s->last_added < 0)
    return NULL;

  st = s->streams + s->last_added;
  
  if(test_a52(st))
    {
    ret = bgav_track_add_audio_stream(track, opt);
    ret->fourcc = BGAV_MK_FOURCC('.','a','c','3');
    ret->index_mode = INDEX_MODE_SIMPLE;
    }

  if(ret)
    st->done = 1;
  
  return ret;
  }

static void test_data_free(test_streams_t * s)
  {
  int i;
  for(i = 0; i < s->num_streams; i++)
    {
    if(s->streams[i].buffer)
      free(s->streams[i].buffer);
    }
  free(s->streams);
  }

static int init_raw(bgav_demuxer_context_t * ctx, int input_can_seek)
  {
  mpegts_t * priv;
  bgav_pes_header_t pes_header;
  bgav_stream_t * s;

  test_streams_t ts;

  memset(&ts, 0, sizeof(ts));
  
  priv = ctx->priv;
  
  /* Allocate programs and track table */
  priv->num_programs = 1;
  priv->programs = calloc(1, sizeof(*(priv->programs)));
  ctx->tt = bgav_track_table_create(priv->num_programs);
  
  while(1)
    {
    if(!parse_transport_packet(ctx))
      break;

    /* Find the PCR PID */
    if((priv->packet.adaption_field.pcr > 0) &&
       !priv->programs[0].pcr_pid)
      {
      priv->programs[0].pcr_pid = priv->packet.pid;
      }
    
    if(bgav_track_find_stream_all(ctx->tt->tracks[0],
                                  priv->packet.pid))
      {
      if(!next_packet_scan(ctx))
        break;
      else
        continue;
      }

    if(priv->packet.payload_start)
      {
      bgav_input_reopen_memory(priv->input_mem, priv->ptr,
                               priv->buffer_size -
                               (priv->ptr - priv->buffer));
      
      bgav_pes_header_read(priv->input_mem, &pes_header);
      priv->ptr += priv->input_mem->position;
      
      //      bgav_pes_header_dump(&pes_header);
      
      /* MPEG-2 Video */
      if((pes_header.stream_id >= 0xe0) && (pes_header.stream_id <= 0xef))
        {
        int size = priv->buffer_size - (priv->ptr - priv->buffer);
        
        //        gavl_hexdump(priv->ptr, 16, 16);
        
        s = bgav_track_add_video_stream(ctx->tt->tracks[0], ctx->opt);
        s->index_mode = INDEX_MODE_SIMPLE;

        /* Try to distinguish between MPEG-1/2 and H.264 */
        if((size >= 4) &&
           (priv->ptr[0] == 0x00) && 
           (priv->ptr[1] == 0x00) && 
           (priv->ptr[2] == 0x01) &&
           ((priv->ptr[3] == 0xb3) || // Sequence header
            (priv->ptr[3] == 0x00) || // Picture
            (priv->ptr[3] == 0xb8))) //  GOP 
          {
          s->fourcc = BGAV_MK_FOURCC('m', 'p', 'g', 'v');
          }
        else
          {
          s->fourcc = BGAV_MK_FOURCC('H', '2', '6', '4');
          //          fprintf(stderr, "Detected H.264 video\n");
          }
        
        }
      /* MPEG Audio */
      else if((pes_header.stream_id & 0xe0) == 0xc0)
        {
        s = bgav_track_add_audio_stream(ctx->tt->tracks[0], ctx->opt);
        s->fourcc = BGAV_MK_FOURCC('m', 'p', 'g', 'a');
        s->index_mode = INDEX_MODE_SIMPLE;
        }
      else
        {
        test_streams_append_packet(&ts, priv->ptr,
                                   priv->packet.payload_size -
                                   priv->input_mem->position,
                                   priv->packet.pid,
                                   &pes_header);
        s = test_streams_detect(&ts, ctx->tt->tracks[0], ctx->opt);
        }
      }
    else
      {
      test_streams_append_packet(&ts, priv->ptr,
                                 priv->packet.payload_size,
                                 priv->packet.pid,
                                 NULL);
      s = test_streams_detect(&ts, ctx->tt->tracks[0], ctx->opt);
      }
    
    if(s)
      {
      s->stream_id = priv->packet.pid;
      s->timescale = 90000;
      s->stats.pts_start = GAVL_TIME_UNDEFINED;
      s->flags |= STREAM_PARSE_FULL;
      s->flags |= STREAM_NEED_START_PTS;
      }
    if(!next_packet_scan(ctx))
        break;
    }
  test_data_free(&ts);
  priv->programs[0].initialized = 1;
  init_streams_priv(&priv->programs[0],
                    ctx->tt->tracks[0]);
  
  bgav_track_set_format(ctx->tt->tracks[0], "MPEGTS", "video/MP2T");
  
  return 1;
  }


static int open_mpegts(bgav_demuxer_context_t * ctx)
  {
  int have_pat;
  int input_can_seek;
  int packets_scanned;
  mpegts_t * priv;
  int i;
  
  /* Allocate private data */
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  priv->packet_size = guess_packet_size(ctx->input);

  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    input_can_seek = 1;
  else
    input_can_seek = 0;
  
  if(!priv->packet_size)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Cannot get packet size");
    return 0;
    }
  else
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Packet size: %d",
             priv->packet_size);
  
  priv->buffer = malloc(priv->packet_size * SCAN_PACKETS);

  
  priv->input_mem =
    bgav_input_open_memory(NULL, 0, ctx->opt);
  
  priv->ptr = priv->buffer;
  priv->packet_start = priv->buffer;
  
  priv->first_packet_pos = ctx->input->position;
  /* Scan the stream for a PAT */

  if(!ctx->tt)
    {
    packets_scanned = 0;
    have_pat = 0;
    
    if(input_can_seek)
      priv->buffer_size = read_data(ctx, SCAN_PACKETS);
    else
      priv->buffer_size =
        bgav_input_get_data(ctx->input, priv->buffer,
                            priv->packet_size * SCAN_PACKETS);
    
    while(1)
      {
      parse_transport_packet(ctx);
    
      if(priv->packet.pid == 0x0000)
        {
        have_pat = 1;
        break;
        }
      packets_scanned++;

      if(!next_packet(priv))
        {
        if(input_can_seek && (packets_scanned < SCAN_PACKETS_SEEK))
          {
          priv->buffer_size = read_data(ctx, SCAN_PACKETS);
          if(!priv->buffer_size)
            break;
          priv->ptr = priv->buffer;
          priv->packet_start = priv->buffer;
          }
        else
          break;
        }
      }
  
    if(have_pat)
      {
      /* Initialize using PAT and PMTs */
      if(!init_psi(ctx, input_can_seek))
        return 0;
      }
    else
      {
      /* Initialize raw TS */
      if(input_can_seek)
        {
        bgav_input_seek(ctx->input, priv->first_packet_pos,
                        SEEK_SET);

        priv->buffer_size = read_data(ctx, SCAN_PACKETS);
        priv->ptr = priv->buffer;
        priv->packet_start = priv->buffer;
        }
      else
        {
        priv->ptr = priv->buffer;
        priv->packet_start = priv->buffer;
        }
      if(!init_raw(ctx, input_can_seek))
        return 0;
      }
    if(input_can_seek)
      bgav_input_seek(ctx->input, priv->first_packet_pos, SEEK_SET);
    }
  else /* Track table already present */
    {
    priv->num_programs = ctx->tt->num_tracks;
    priv->programs = calloc(priv->num_programs, sizeof(*priv->programs));
    priv->programs[0].pcr_pid = ctx->input->sync_id;

    for(i = 0; i < priv->num_programs; i++)
      {
      init_streams_priv(&priv->programs[i],
                        ctx->tt->tracks[i]);
      bgav_track_set_format(ctx->tt->tracks[i], "MPEGTS", "video/MP2T");
      }
    }
  
  //  transport_packet_dump(&packet);
  if(input_can_seek)
    {
    if(get_program_durations(ctx))
      {
      ctx->flags |=
        (BGAV_DEMUXER_CAN_SEEK|BGAV_DEMUXER_SEEK_ITERATIVE);
      }
    else
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "Could not get program durations, seeking disabled");

      }

    }

  bgav_track_set_format(ctx->tt->cur, "MPEGTS", "video/MP2T");
  
  ctx->index_mode = INDEX_MODE_MIXED;
  return 1;
  }

/*
 * Parse HDV AAUX/VAUX
 *
 * Modeled after the gstreamer parser by Edward Hervey
 * http://cgit.freedesktop.org/gstreamer/gst-plugins-bad/tree/gst/hdvparse/gsthdvparse.c
 */

#if 0
typedef struct
  {
  int dummy;
  } hdv_aaux_t;


static const uint32_t hdv_aux_header = BGAV_MK_FOURCC(0x00, 0x00, 0x01, 0xbf);

static int parse_hdv_aux_header(uint8_t ** data, int * len)
  {
  uint32_t h;
  int size;
  uint8_t * ptr = *data;

  h = GAVL_PTR_2_32BE(ptr); ptr += 4;
  if(h != hdv_aux_header)
    return 0;
  
  size = GAVL_PTR_2_16BE(ptr); ptr += 2;
  if(size != *len - (ptr - *data))
    return 0;

  *data = ptr;
  *len = size;
  return 1;
  }
static int parse_hdv_aaux(uint8_t * data, int len, hdv_aaux_t * ret)
  {
  uint8_t tag;
  int size;
  uint8_t * end;
  
  if(!parse_hdv_aux_header(&data, &len))
    return 0;

  end = data + len;

  while(data < end)
    {
    tag = *data; data++;

    if(tag >= 0x40)
      {
      size = *data; data++;
      }
    else
      size = 4;
    
    //    fprintf(stderr, "Got AAUX data %02x, len: %d\n", tag, size);
    data += size;
    }
  return 1;
  }

#define BCD(c) ( ((((c) >> 4) & 0x0f) * 10) + ((c) & 0x0f) )

static const struct
  {
  int timescale;
  int frame_duration;
  int int_framerate;
  }
vaux_framerates[] =
  {
    {  },
    { 24000, 1,    24 },
    {  },
    { 25,    1,    25 },
    { 30000, 1001, 30 },
    {  },
    {  },
    {  },
    {  },
    {  },
    {  },
    {  },
    {  },
    {  },
    {  },
    {  },
  };

static int parse_hdv_vaux(uint8_t * data, int len, hdv_vaux_t * ret)
  {
  uint8_t tag;
  int size;
  uint8_t * end;
  int rate_index;
  int have_date = 0;
  int have_time = 0;

  memset(ret, 0, sizeof(*ret));
  
  if(!parse_hdv_aux_header(&data, &len))
    return 0;

  ret->tc = GAVL_TIMECODE_UNDEFINED;
  ret->rd = GAVL_TIMECODE_UNDEFINED;
  
  end = data + len;

  while(data < end)
    {
    tag = *data; data++;

    if(tag >= 0x40)
      {
      size = *data; data++;
      }
    else
      size = 4;

    fprintf(stderr, "Got VAUX data %02x, len: %d\n", tag, size);
    gavl_hexdump(data, size, 16);
    if((tag == 0x44) && (len >= 0x39))
      {
      rate_index = data[13] & 0x07;
      
      ret->timescale      = vaux_framerates[rate_index].timescale;
      ret->frame_duration = vaux_framerates[rate_index].frame_duration;
      ret->int_framerate  = vaux_framerates[rate_index].int_framerate;
      
      if(data[28] & 0x01) /* Timecode valid */
        {
        uint8_t fr, sec, min, hr;
        //int bf, df;
        
        /* HD2 TTC
         *      ---------------------------------
         * 29   |BF |DF |Tens Fr|Units of Frames|
         *      ---------------------------------
         * 30   | 1 |Tens second|Units of Second|
         *      ---------------------------------
         * 31   | 1 |Tens minute|Units of Minute|
         *      ---------------------------------
         * 32   | 1 | 1 |Tens Hr|Units of Hours |
         *      ---------------------------------
         */
        // ret->drop = (data[29] >> 6) & 0x1;

        fr = BCD (data[29] & 0x3f);
        sec = BCD (data[30] & 0x7f);
        min = BCD (data[31] & 0x7f);
        hr = BCD (data[32] & 0x3f);
        
        // fprintf(stderr, "Timecode: %02d:%02d:%02d:%02d\n",
        //         hr, min, sec, fr);
        
        gavl_timecode_from_hmsf(&ret->tc, hr, min, sec, fr);
        }
      else
        ret->tc = GAVL_TIMECODE_UNDEFINED;
      
      if(data[28] & 0x02) /* Date valid */
        {
        int ds, tm;
        uint8_t tz, day, dow, month;
        int year;
        
        /* REC DATE
         *      ---------------------------------
         * 33   |DS |TM |Tens TZ|Units of TimeZn|
         *      ---------------------------------
         * 34   | 1 | 1 |Tens dy| Units of Days |
         *      ---------------------------------
         * 35   |   Week    |TMN|Units of Months|
         *      ---------------------------------
         * 36   | Tens of Years |Units of Years |
         *      ---------------------------------
         */
        ds = data[33] >> 7;
        tm = (data[33] >> 6) & 0x1;
        tz = BCD (data[33] & 0x3f);
        day = BCD (data[34] & 0x3f);
        dow = data[35] >> 5;
        month = BCD (data[35] & 0x1f);
        year = BCD (data[36]);
        year += 2000;
        
        // fprintf(stderr, "Date: %d %02d/%02d/%04d\n", dow, day, month, year);
        
        gavl_timecode_from_ymd(&ret->rd, year, month, day);
        have_date = 1;
        }
      if(data[28] & 0x04) /* Time valid */
        {
        uint8_t fr, sec, min, hr;

        /* REC TIME
         *      ---------------------------------
         * 37   | 1 | 1 |Tens Fr|Units of Frames|
         *      ---------------------------------
         * 38   | 1 |Tens second|Units of Second|
         *      ---------------------------------
         * 39   | 1 |Tens minute|Units of Minute|
         *      ---------------------------------
         * 40   | 1 | 1 |Tens Hr|Units of Hours |
         *      ---------------------------------
         */
        
        // Always 0xff ?
        //        fprintf(stderr, "data[37]: %02x\n", data[37]);
        
        fr = BCD (data[37] & 0x3f);
        sec = BCD (data[38] & 0x7f);
        min = BCD (data[39] & 0x7f);
        hr = BCD (data[40] & 0x3f);

        //        fprintf(stderr, "Time: %02d:%02d:%02d:%02d\n",
        //        hr, min, sec, fr);

        gavl_timecode_from_hmsf(&ret->rd, hr, min, sec, 0);
        have_time = 1;
        }

      if(!have_date || !have_time)
        ret->rd = GAVL_TIMECODE_UNDEFINED;

      }
    
    data += size;
    
    }
  return 1;
  }
#endif

#if 0
static void predict_pcr_wrap(const bgav_options_t * opt, int64_t pcr)
  {
  char str[GAVL_TIME_STRING_LEN];
  int64_t time_scaled;
  gavl_time_t time;
  time_scaled = (1LL << 33) - pcr;
  time = gavl_time_unscale(90000, time_scaled);
  gavl_time_prettyprint(time, str);
  gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Next PCR wrap in %s", str);
  }
#endif

#define NUM_PACKETS 5 /* Packets to be processed at once */

static int process_packet(bgav_demuxer_context_t * ctx)
  {
  int i;
  bgav_stream_t * s;
  mpegts_t * priv;
  int num_packets;
  int bytes_to_copy;
  bgav_pes_header_t pes_header;
  int64_t position;
  priv = ctx->priv;

  if(!priv->packet_size)
    return 0;
  
  position = ctx->input->position;

  num_packets = NUM_PACKETS;
  
  if(ctx->next_packet_pos &&
     (position + priv->packet_size * num_packets > ctx->next_packet_pos))
    num_packets = (ctx->next_packet_pos - position) / priv->packet_size;
    
  priv->buffer_size = read_data(ctx, num_packets);
  
  if(priv->buffer_size < priv->packet_size)
    return 0;
  
  priv->ptr = priv->buffer;
  priv->packet_start = priv->buffer;
  
  num_packets = priv->buffer_size / priv->packet_size;
  
  for(i = 0; i < num_packets; i++)
    {
    if(!parse_transport_packet(ctx))
      {
      return 0;
      }
    if(priv->packet.transport_error)
      {
      next_packet(priv);
      position += priv->packet_size;
      continue;
      }
    
    
#if 1
    //    bgav_transport_packet_dump(&priv->packet);
        
    if(!(ctx->flags & BGAV_DEMUXER_HAS_TIMESTAMP_OFFSET) &&
       (priv->programs[priv->current_program].pcr_pid > 0))
      {
      if(priv->packet.adaption_field.pcr < 0)
        {
        next_packet(priv);
        position += priv->packet_size;
        continue;
        }
      else if(priv->packet.pid !=
              priv->programs[priv->current_program].pcr_pid)
        {
        next_packet(priv);
        position += priv->packet_size;
        continue;
        }
      else
        {
        ctx->flags |= BGAV_DEMUXER_HAS_TIMESTAMP_OFFSET;
        ctx->timestamp_offset = -priv->packet.adaption_field.pcr;
        //        predict_pcr_wrap(priv->packet.adaption_field.pcr);
        }
      }
#endif
    /* Skip PAT/PMT */
    if(!priv->packet.pid ||
       (priv->packet.pid == priv->programs[priv->current_program].program_map_pid))
      {
      next_packet(priv);
      position += priv->packet_size;
      continue;
      }
#if 0
    if(priv->packet.pid == priv->programs[priv->current_program].aaux_pid)
      {
      hdv_aaux_t aaux;
      /* Got AAUX packet */
      //      fprintf(stderr, "Got AAUX packet\n");
      // bgav_transport_packet_dump(&priv->packet);
      // gavl_hexdump(priv->ptr, priv->packet.payload_size, 16);

      parse_hdv_aaux(priv->ptr, priv->packet.payload_size, &aaux);

      next_packet(priv);
      position += priv->packet_size;
      continue;

      }
    else
#endif

#if 0
      if(priv->packet.pid == priv->programs[priv->current_program].vaux_pid)
        {
        hdv_vaux_t vaux;
        gavl_video_format_t * fmt;
      
        /* Got VAUX packet */
        fprintf(stderr, "Got VAUX packet\n");
        bgav_transport_packet_dump(&priv->packet);
        gavl_hexdump(priv->ptr, priv->packet.payload_size, 16);
        parse_hdv_vaux(priv->ptr, priv->packet.payload_size, &vaux);
#ifdef DUMP_HDV_AUX
        dump_vaux(&vaux);
#endif
      
        if(ctx->tt->cur->video_streams)
          {
          fmt = &ctx->tt->cur->video_streams[0].data.video.format;
        
          if(!fmt->timecode_format.int_framerate)
            {
            fmt->timecode_format.int_framerate = vaux.int_framerate;
            if(vaux.frame_duration == 1001)
              fmt->timecode_format.flags =
                GAVL_TIMECODE_DROP_FRAME;
            }
          }
        
        next_packet(priv);
        position += priv->packet_size;
      
        continue;
        }
#endif
    
    s = bgav_track_find_stream(ctx, priv->packet.pid);
    
    if(!s)
      {
      // fprintf(stderr, "No stream for PID %04x\n", priv->packet.pid);      
      //      gavl_hexdump(priv->packet_start, 188, 16);
      
      next_packet(priv);
      position += priv->packet_size;
      continue;
      }
    
    if(priv->packet.payload_start) /* New packet starts here */
      {
      bgav_input_reopen_memory(priv->input_mem, priv->ptr,
                               priv->buffer_size -
                               (priv->ptr - priv->buffer));
      /* Read PES header */
      
      if(!bgav_pes_header_read(priv->input_mem, &pes_header))
        {
        return !!i;
        }
      priv->ptr += priv->input_mem->position;
      
      if(s->packet)
        {
        bgav_stream_done_packet_write(s, s->packet);
        s->packet = NULL;
        }

      /* Get start pts */

#if 1
      if(!(ctx->flags & BGAV_DEMUXER_HAS_TIMESTAMP_OFFSET) &&
         (priv->programs[priv->current_program].pcr_pid <= 0))
        {
        if(pes_header.pts < 0)
          {
          next_packet(priv);
          position += priv->packet_size;
          continue;
          }
        ctx->timestamp_offset = -pes_header.pts;
        ctx->flags |= BGAV_DEMUXER_HAS_TIMESTAMP_OFFSET;
        }
#endif
      if(!s->packet)
        {
        if(priv->do_sync)
          {
          if(STREAM_HAS_SYNC(s))
            {
            s->packet = bgav_stream_get_packet_write(s);
            s->packet->position = position;
            }
          else if(pes_header.pts < 0)
            {
            next_packet(priv);
            position += priv->packet_size;
            continue;
            }
          else
            {
            STREAM_SET_SYNC(s, pes_header.pts + ctx->timestamp_offset);
            s->packet = bgav_stream_get_packet_write(s);
            s->packet->position = position;
            }
          }
        else
          {
          s->packet = bgav_stream_get_packet_write(s);
          s->packet->position = position;
          }
        }
     
      /*
       *  Now the bad news: Some transport streams contain PES packets
       *  with a packet length field of zero. This means, we must use
       *  the payload_start bit to find out when a packet ended.
       *
       *  Here, we allocate 1024 bytes (>> transport payload) to
       *  reduce realloc overhead afterwards
       */
      
      bytes_to_copy =
        //        priv->packet_size - (priv->ptr - priv->packet_start);
        188 - (priv->ptr - priv->packet_start);
      
      bgav_packet_alloc(s->packet, 1024);

      /* Read data */
      
      memcpy(s->packet->data, priv->ptr, bytes_to_copy);

      s->packet->data_size = bytes_to_copy;
      
      if(pes_header.pts > 0)
        {
        s->packet->pts = pes_header.pts;
        check_pts_wrap(s, &s->packet->pts);
        s->packet->pts += ctx->timestamp_offset;
        }
      }
    else if(s->packet)
      {
      /* Read data */
      
      bgav_packet_alloc(s->packet,
                        s->packet->data_size +
                        priv->packet.payload_size);
      
      memcpy(s->packet->data + s->packet->data_size, priv->ptr,
             priv->packet.payload_size);
      s->packet->data_size  += priv->packet.payload_size;
      }
    next_packet(priv);
    position += priv->packet_size;
    }
  
  return 1;
  }

static int next_packet_mpegts(bgav_demuxer_context_t * ctx)
  {
  mpegts_t * priv;
  priv = ctx->priv;
  priv->is_running = 1;
  
  if(ctx->next_packet_pos)
    {
    int ret = 0;
    while(1)
      {
      if(!process_packet(ctx))
        {
        if(ctx->request_stream && ctx->request_stream->packet)
          {
          bgav_stream_done_packet_write(ctx->request_stream,
                                        ctx->request_stream->packet);
          ctx->request_stream->packet = NULL;
          }
        return ret;
        }
      else
        ret = 1;
      if(ctx->input->position >= ctx->next_packet_pos)
        {
        /* We would send this packet only after the next
           packet starts, but since we know the packet is
           finished, we can do it now */
        if(ctx->request_stream && ctx->request_stream->packet)
          {
          bgav_stream_done_packet_write(ctx->request_stream,
                                        ctx->request_stream->packet);
          ctx->request_stream->packet = NULL;
          }
        return ret;
        }
      }
    }
  else
    {
    return process_packet(ctx);
    }
  return 0;
  }

static void resync_mpegts(bgav_demuxer_context_t * ctx, bgav_stream_t * s)
  {
  stream_priv_t * priv;
  priv = s->priv;
  priv->last_pts = BGAV_TIMESTAMP_UNDEFINED;
  priv->pts_offset = 0;
  }

static void seek_mpegts(bgav_demuxer_context_t * ctx, int64_t time, int scale)
  {
  int64_t total_packets;
  int64_t packet;
  int64_t position;
  gavl_time_t duration;
  
  mpegts_t * priv;
  priv = ctx->priv;

  duration = gavl_track_get_duration(ctx->tt->cur->info);
  
  reset_streams_priv(ctx->tt->cur);
  
  total_packets =
    (ctx->input->total_bytes - priv->first_packet_pos) / priv->packet_size;

  packet =
    (int64_t)((double)total_packets *
              (double)gavl_time_unscale(scale, time) /
              (double)(duration)+0.5);
  
  position = priv->first_packet_pos + packet * priv->packet_size;

  if(position < priv->first_packet_pos)
    position = priv->first_packet_pos;
  if(position >= ctx->input->total_bytes)
    position = ctx->input->total_bytes-1;
  
  bgav_input_seek(ctx->input, position, SEEK_SET);

  priv->do_sync = 1;
  while(!bgav_track_has_sync(ctx->tt->cur))
    {
    if(!next_packet_mpegts(ctx))
      break;
    }
  priv->do_sync = 0;
  }

static void close_mpegts(bgav_demuxer_context_t * ctx)
  {
  int i;
  mpegts_t * priv;
  priv = ctx->priv;
  
  if(!priv)
    return;

  if(priv->input_mem)
    {
    bgav_input_close(priv->input_mem);
    bgav_input_destroy(priv->input_mem);
    }
  if(priv->buffer)
    free(priv->buffer);
  if(priv->programs)
    {
    for(i = 0; i < priv->num_programs; i++)
      {
      if(priv->programs[i].streams)
        free(priv->programs[i].streams);
      }
    free(priv->programs);
    }
  free(priv);
  }

static int select_track_mpegts(bgav_demuxer_context_t * ctx,
                                int track)
  {
  mpegts_t * priv;
  priv = ctx->priv;

  if(!priv->is_running || (priv->num_programs == 1))
    return 1;

  priv->is_running = 0;

  priv->current_program = track;
  priv->error_counter = 0;
  
  if(ctx->flags & BGAV_DEMUXER_CAN_SEEK)
    {
    ctx->flags |= BGAV_DEMUXER_HAS_TIMESTAMP_OFFSET;
    ctx->timestamp_offset = -priv->programs[track].start_pcr;
    }
  else
    ctx->flags &= ~BGAV_DEMUXER_HAS_TIMESTAMP_OFFSET;

  reset_streams_priv(ctx->tt->cur);
  
  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    {
    bgav_input_seek(ctx->input, priv->first_packet_pos,
                    SEEK_SET);
    return 1;
    }
  else
    return 0;
    
  }

const bgav_demuxer_t bgav_demuxer_mpegts =
  {
    .probe =        probe_mpegts,
    .open =         open_mpegts,
    .next_packet =  next_packet_mpegts,
    .seek =         seek_mpegts,
    .resync =       resync_mpegts,
    .close =        close_mpegts,
    .select_track = select_track_mpegts
  };

