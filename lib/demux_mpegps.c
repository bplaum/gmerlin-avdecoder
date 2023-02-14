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


#include <avdec_private.h>
#include <stdlib.h>
#include <stdio.h>

#include <pes_header.h>


// #define CDXA_SECTOR_SIZE_RAW 2352
// #define CDXA_SECTOR_SIZE     2324
// #define CDXA_HEADER_SIZE       24

// #define DUMP_PACK_HEADER
// #define DUMP_PES_HEADER

/* Number of sectors to read at once when searching backwards */
#define SCAN_SECTORS 16


/* We scan at most one megabyte */

#define SYNC_SIZE (1024*1024)

#define LOG_DOMAIN "mpegps"

/* Synchronization routines */

#define IS_START_CODE(h)  ((h&0xffffff00)==0x00000100)

static uint32_t next_start_code(bgav_input_context_t * ctx)
  {
  int bytes_skipped = 0;
  uint32_t c;
  while(1)
    {
    if(!bgav_input_get_32_be(ctx, &c))
      return 0;
    if(IS_START_CODE(c))
      {
      return c;
      }
    bgav_input_skip(ctx, 1);
    bytes_skipped++;
    if(bytes_skipped > SYNC_SIZE)
      {
      return 0;
      }
    }
  return 0;
  }

static uint32_t previous_start_code(bgav_input_context_t * ctx)
  {
  uint32_t c;
  //  int skipped = 0;
  bgav_input_seek(ctx, -1, SEEK_CUR);

  while(ctx->position >= 0)
    {
    if(!bgav_input_get_32_be(ctx, &c))
      {
      return 0;
      }
    if(IS_START_CODE(c))
      {
      return c;
      }
    bgav_input_seek(ctx, -1, SEEK_CUR);
    //    skipped++;
    }
  return 0;
  }

/* Probe */

static int probe_mpegps(bgav_input_context_t * input)
  {
  uint8_t probe_data[12];
  if(bgav_input_get_data(input, probe_data, 12) < 12)
    return 0;
  
  /* Check for pack header */

  if((probe_data[0] == 0x00) &&
     (probe_data[1] == 0x00) &&
     (probe_data[2] == 0x01) &&
     (probe_data[3] == 0xba))
    return 1;

  /* Check for CDXA header */
    
  if((probe_data[0] == 'R') &&
     (probe_data[1] == 'I') &&
     (probe_data[2] == 'F') &&
     (probe_data[3] == 'F') &&
     (probe_data[8] == 'C') &&
     (probe_data[9] == 'D') &&
     (probe_data[10] == 'X') &&
     (probe_data[11] == 'A'))
    return 1;
  return 0;
  }

/* Pack header */


/* System header */

typedef struct
  {
  uint16_t size;
  } system_header_t;

static int system_header_read(bgav_input_context_t * input,
                              system_header_t * ret)
  {
  bgav_input_skip(input, 4); /* Skip start code */  
  if(!bgav_input_read_16_be(input, &ret->size))
    return 0;
  bgav_input_skip(input, ret->size);
  return 1;
  }
#if 0
static void system_header_dump(system_header_t * h)
  {
  bgav_dprintf( "System header (skipped %d bytes)\n", h->size);
  }
#endif
/* Demuxer structure */

typedef struct
  {
  /* For sector based access */
  bgav_input_context_t * input_mem;
  
  int64_t position;
  
  //  int is_cdxa; /* Nanosoft VCD rip */
    
  //  int64_t data_start;
  int64_t data_size;
  
  /* Actions for next_packet */
  
  int find_streams;

  int have_pts; /* Whether PTS are present at all */
  
  int is_running;
  
  /* Headers */

  bgav_pack_header_t     pack_header;
  bgav_pes_header_t pes_header;
    
  } mpegps_priv_t;

static const int lpcm_freq_tab[4] = { 48000, 96000, 44100, 32000 };

/* Sector based utilities */

#if 0

static void goto_sector_cdxa(bgav_demuxer_context_t * ctx, int64_t sector)
  {
  mpegps_priv_t * priv;
  priv = ctx->priv;

  bgav_input_seek(ctx->input, ctx->tt->cur->data_start + priv->sector_size_raw * (sector + priv->start_sector),
                  SEEK_SET);
  bgav_input_reopen_memory(priv->input_mem,
                           NULL, 0);
  
  }

static int read_sector_cdxa(bgav_demuxer_context_t * ctx)
  {
  mpegps_priv_t * priv;
  priv = ctx->priv;
  if(bgav_input_read_data(ctx->input, priv->sector_buffer, priv->sector_size_raw) <
     priv->sector_size_raw)
    return 0;
  
  bgav_input_reopen_memory(priv->input_mem,
                           priv->sector_buffer + priv->sector_header_size,
                           priv->sector_size);
  return 1;
  }

static void goto_sector_input(bgav_demuxer_context_t * ctx, int64_t sector)
  {
  mpegps_priv_t * priv;
  priv = ctx->priv;
  bgav_input_seek_sector(ctx->input, sector + priv->start_sector);
  bgav_input_reopen_memory(priv->input_mem,
                           NULL, 0);

  }

static int read_sector_input(bgav_demuxer_context_t * ctx)
  {
  mpegps_priv_t * priv;
  priv = ctx->priv;
  if(!bgav_input_read_sector(ctx->input, priv->sector_buffer))
    {
    return 0;
    }
  bgav_input_reopen_memory(priv->input_mem,
                           priv->sector_buffer + priv->sector_header_size,
                           priv->sector_size);
  return 1;
  }
#endif

static int select_track_mpegps(bgav_demuxer_context_t * ctx, int track)
  {
  mpegps_priv_t * priv;
  priv = ctx->priv;

  /* If we didn't run yet, we do nothing.
     This prevents reopening of the input. */

  if(!priv->is_running)
    return 1;

  priv->is_running = 0;
  
  if(ctx->tt->cur->data_start != ctx->input->position)
    {
    if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
      bgav_input_seek(ctx->input, ctx->tt->cur->data_start, SEEK_SET);
    else
      return 0;
    }
  
  return 1;
  }

/* Generic initialization function for sector based access: Get the 
   empty sectors at the beginning and the end, and the duration of the track */
#if 0
static void init_sector_mode(bgav_demuxer_context_t * ctx)
  {
  int64_t scr_start, scr_end;
  uint32_t start_code = 0;
  
  mpegps_priv_t * priv;
  priv = ctx->priv;

  priv->input_mem          = bgav_input_open_memory(NULL, 0);

  if(priv->goto_sector)
    {
    priv->goto_sector(ctx, 0);
    while(1)
      {
      if(!priv->read_sector(ctx))
        {
        return;
        }
      if(!bgav_input_get_32_be(priv->input_mem, &start_code))
        return;
      if(start_code == START_CODE_PACK_HEADER)
        break;
      priv->start_sector++;
      priv->total_sectors--;
      }
    }
  else if(!priv->read_sector(ctx))
    {
    return;
    }
  
  /* Read first scr */
  
  if(!bgav_pack_header_read(priv->input_mem, &priv->pack_header))
    return;

#ifdef DUMP_PACK_HEADER
  bgav_pack_header_dump(&priv->pack_header);
#endif

  /* If we already have the duration, stop here. */
  if(gavl_track_get_duration(ctx->tt->cur->info) != GAVL_TIME_UNDEFINED)
    {
    if(priv->goto_sector)
      priv->goto_sector(ctx, 0);
    bgav_input_reopen_memory(priv->input_mem, NULL, 0);
    return;
    }
  
  if(priv->goto_sector)
    {
    int i;
    
    scr_end = -1;
    scr_start = priv->pack_header.scr;

    /* Read final sectors */    
    while(priv->total_sectors > 0)
      {
      //  SCAN_SECTORS

      priv->goto_sector(ctx, priv->total_sectors - SCAN_SECTORS - 1);

      for(i = 0; i < SCAN_SECTORS; i++)
        {
        if(!priv->read_sector(ctx))
          {
          priv->total_sectors -= SCAN_SECTORS;
          continue;
          }
        
        if(!bgav_input_get_32_be(priv->input_mem, &start_code))
          return;
        if(start_code == START_CODE_PACK_HEADER)
          {
          if(!bgav_pack_header_read(priv->input_mem, &priv->pack_header))
            return;
          scr_end = priv->pack_header.scr;
          }
        }
      
      if(scr_end > 0)
        break;
      else
        priv->total_sectors -= SCAN_SECTORS;
      }

    gavl_track_set_duration(ctx->tt->cur->info, ((int64_t)(scr_end - scr_start) * GAVL_TIME_SCALE) / 90000);
    
    priv->goto_sector(ctx, 0);
    
    bgav_input_reopen_memory(priv->input_mem, NULL, 0);
    }
  else
    return;
  }
#endif

static void init_stream(bgav_stream_t * s, uint32_t fourcc,
                        int stream_id)
  {
  s->timescale = 90000;
  s->index_mode = INDEX_MODE_SIMPLE;
  s->fourcc = fourcc;
  s->stream_id = stream_id;
  }

/* Get one packet */

static int next_packet(bgav_demuxer_context_t * ctx,
                       bgav_input_context_t * input)
  {
  uint8_t c;
  system_header_t system_header;
  int got_packet = 0;
  uint32_t start_code;
  
  mpegps_priv_t * priv;
  bgav_stream_t * stream = NULL;

  priv = ctx->priv;
  while(!got_packet)
    {
    if(!(start_code = next_start_code(input)))
      return 0;
    if(start_code == START_CODE_SYSTEM_HEADER)
      {
      if(!system_header_read(input, &system_header))
        return 0;
      //      system_header_dump(&system_header);
      }
    else if(start_code == START_CODE_PACK_HEADER)
      {
      if(!bgav_pack_header_read(input, &priv->pack_header))
        {
        return 0;
        }
      //      pack_header_dump(&priv->pack_header);
      }
    else if(start_code == START_CODE_PROGRAM_END)
      {
      bgav_input_skip(input, 4); /* Skip start code */  
      }
    else /* PES Packet */
      {
      priv->position = ctx->input->position;
      
      if(!bgav_pes_header_read(input, &priv->pes_header))
        {
        return 0;
        }
#ifdef DUMP_PES_HEADER
      bgav_pes_header_dump(&priv->pes_header);
#endif

      if(priv->pes_header.pts != GAVL_TIME_UNDEFINED)
        priv->have_pts = 1;
      
      /* Private stream 1 (non MPEG audio, subpictures) */
      if(priv->pes_header.stream_id == 0xbd)
        {
        if(!bgav_input_read_8(input, &c))
          return 0;
        priv->pes_header.payload_size--;

        priv->pes_header.stream_id <<= 8;
        priv->pes_header.stream_id |= c;

        if((c >= 0x20) && (c <= 0x3f))  /* Subpicture */
          {
          stream = bgav_track_find_stream(ctx,
                                          priv->pes_header.stream_id);
          }
        else if((c >= 0x80) && (c <= 0x87)) /* AC3 Audio */

          {
          bgav_input_skip(input, 3);
          priv->pes_header.payload_size -= 3;
          
          if(priv->find_streams)
            stream = bgav_track_find_stream_all(ctx->tt->cur,
                                                priv->pes_header.stream_id);
          else
            stream = bgav_track_find_stream(ctx,
                                            priv->pes_header.stream_id);
          if(!stream && priv->find_streams)
            {
            stream = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);

            init_stream(stream, BGAV_MK_FOURCC('.', 'a', 'c', '3'),
                        priv->pes_header.stream_id);
            /* Hack: This is set by the core later. We must set it here,
               because we buffer packets during initialization */
            stream->demuxer = ctx;
            }
          }
        else if(((c >= 0x88) && (c <= 0x8F)) ||
                ((c >= 0x98) && (c <= 0x9F))) /* DTS Audio */

          {
          bgav_input_skip(input, 3);
          priv->pes_header.payload_size -= 3;
          
          if(priv->find_streams)
            stream = bgav_track_find_stream_all(ctx->tt->cur,
                                                priv->pes_header.stream_id);
          else
            stream = bgav_track_find_stream(ctx,
                                            priv->pes_header.stream_id);
          if(!stream && priv->find_streams)
            {
            stream = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
            
            init_stream(stream, BGAV_MK_FOURCC('d', 't', 's', ' '),
                        priv->pes_header.stream_id);
            /* Hack: This is set by the core later. We must set it here,
               because we buffer packets during initialization */
            stream->demuxer = ctx;

            }
          }
        else if((c >= 0xA0) && (c <= 0xA7)) /* LPCM Audio */
          {
          bgav_input_skip(input, 3);
          priv->pes_header.payload_size -= 3;

          if(priv->find_streams)
            stream = bgav_track_find_stream_all(ctx->tt->cur,
                                                priv->pes_header.stream_id);
          else
            stream = bgav_track_find_stream(ctx,
                                            priv->pes_header.stream_id);
          if(!stream && priv->find_streams)
            {
            stream = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
            stream->index_mode = INDEX_MODE_SIMPLE;
            stream->timescale = 90000;
            stream->fourcc = BGAV_MK_FOURCC('L', 'P', 'C', 'M');
            stream->stream_id = priv->pes_header.stream_id;
            /* Hack: This is set by the core later. We must set it here,
               because we buffer packets during initialization */
            stream->demuxer = ctx;
            }
          
          /* Set stream format */

          if(stream && !stream->data.audio.format->samplerate)
            {
            
            /* emphasis (1), muse(1), reserved(1), frame number(5) */
            bgav_input_skip(input, 1);
            /* quant (2), freq(2), reserved(1), channels(3) */
            if(!bgav_input_read_data(input, &c, 1))
              return 0;
            
            stream->data.audio.format->samplerate =
              lpcm_freq_tab[(c >> 4) & 0x03];
            stream->data.audio.format->num_channels = 1 + (c & 7);
            stream->data.audio.bits_per_sample = 16;
            stream->timescale = stream->data.audio.format->samplerate;
            
            switch ((c>>6) & 3)
              {
              case 0: stream->data.audio.bits_per_sample = 16; break;
              case 1: stream->data.audio.bits_per_sample = 20; break;
              case 2: stream->data.audio.bits_per_sample = 24; break;
              }

            switch(stream->data.audio.format->num_channels)
              {
              case 1:
                stream->data.audio.format->channel_locations[0] =
                  GAVL_CHID_FRONT_CENTER;
                break;
              case 2:
                stream->data.audio.format->channel_locations[0] =
                  GAVL_CHID_FRONT_LEFT;
                stream->data.audio.format->channel_locations[1] =
                  GAVL_CHID_FRONT_RIGHT;
                break;
              case 6:
                stream->data.audio.format->channel_locations[0] =
                  GAVL_CHID_FRONT_LEFT;
                stream->data.audio.format->channel_locations[1] =
                  GAVL_CHID_FRONT_CENTER;
                stream->data.audio.format->channel_locations[2] =
                  GAVL_CHID_FRONT_RIGHT;
                stream->data.audio.format->channel_locations[3] =
                  GAVL_CHID_REAR_LEFT;
                stream->data.audio.format->channel_locations[4] =
                  GAVL_CHID_REAR_RIGHT;
                stream->data.audio.format->channel_locations[5] =
                  GAVL_CHID_LFE;
                break;
              }
            
            /* Dynamic range control */
            bgav_input_skip(input, 1);
            
            priv->pes_header.payload_size -= 3;
            }
          else /* lpcm header (3 bytes) */
            {
            bgav_input_skip(input, 3);
            priv->pes_header.payload_size -= 3;
            }
          }
#if 0
        else
          {
          gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                   "Unknown ID %02x in private stream 1", c);
          }
#endif
        }
      /* Audio stream */
      else if((priv->pes_header.stream_id & 0xE0) == 0xC0)
        {
        if(priv->find_streams)
          stream = bgav_track_find_stream_all(ctx->tt->cur,
                                              priv->pes_header.stream_id);
        else
          stream = bgav_track_find_stream(ctx,
                                          priv->pes_header.stream_id);
        if(!stream && priv->find_streams)
          {
          stream = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);

          init_stream(stream, BGAV_MK_FOURCC('m', 'p', 'g', 'a'),
                      priv->pes_header.stream_id);
          /* Hack: This is set by the core later. We must set it here,
             because we buffer packets during initialization */
          stream->demuxer = ctx;
          }
        }
      else if((priv->pes_header.stream_id & 0xF0) == 0xE0)
        {
        if(priv->find_streams)
          stream = bgav_track_find_stream_all(ctx->tt->cur,
                                              priv->pes_header.stream_id);
        else
          stream = bgav_track_find_stream(ctx,
                                          priv->pes_header.stream_id);
        if(!stream && priv->find_streams)
          {
          uint32_t fourcc;

          if(!bgav_input_get_fourcc(ctx->input, &fourcc))
            return 0;

          if(fourcc == BGAV_MK_FOURCC(0x00, 0x00, 0x01, 0xb0))
            fourcc = BGAV_MK_FOURCC('C', 'A', 'V', 'S');
#if 1
          // HACK for H.264 in EVO (HDDVD-) files
          else if((priv->pes_header.stream_id == 0xe2) ||
                  (priv->pes_header.stream_id == 0xe3))
            fourcc = BGAV_MK_FOURCC('H', '2', '6', '4');
#endif
          else
            fourcc = BGAV_MK_FOURCC('m', 'p', 'g', 'v');
          
          stream = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
          
          init_stream(stream, fourcc,
                      priv->pes_header.stream_id);

          /* Hack: This is set by the core later. We must set it here,
             because we buffer packets during initialization */
          stream->demuxer = ctx;
          }
        }
      else if((priv->pes_header.stream_id >= 0xfd55) &&
              (priv->pes_header.stream_id <= 0xfd5f))
        {
        if(priv->find_streams)
          stream = bgav_track_find_stream_all(ctx->tt->cur,
                                              priv->pes_header.stream_id);
        else
          stream = bgav_track_find_stream(ctx,
                                          priv->pes_header.stream_id);
        if(!stream && priv->find_streams)
          {
          stream = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);

          init_stream(stream, BGAV_MK_FOURCC('V', 'C', '-', '1'),
                      priv->pes_header.stream_id);
          /* Hack: This is set by the core later. We must set it here,
             because we buffer packets during initialization */
          stream->demuxer = ctx;
          }
        }
      else if((priv->pes_header.stream_id != 0xbe) &&
              (priv->pes_header.stream_id != 0xbf) &&
              (priv->pes_header.stream_id != 0xb9))
        {
        // Print a warning except for padding stream (0xbe) and private_stream_2 (0xbf)
        gavl_log(GAVL_LOG_WARNING,
                 LOG_DOMAIN, "Unknown PES ID %02x",
                 priv->pes_header.stream_id);
//        fprintf(stderr, "Unknown PES ID %02x\n",
//                priv->pes_header.stream_id);
        }

      if(stream)
        {
        if((priv->pes_header.pts < 0) && !stream->packet)
          {
          bgav_input_skip(input, priv->pes_header.payload_size);
          return 1;
          }

        /* Create packet */
        /* Fill packets even if we initialize. The packets won't have to be
           re-read when we start decoding and we don't need to reopen the input
         */
        if(!priv->find_streams)
          {
          //  fprintf(stderr, "get packet %d\n", stream->stream_id);

          if(stream->packet && (priv->pes_header.pts > 0))
            {
            bgav_stream_done_packet_write(stream, stream->packet);
            stream->packet = NULL;
            }
          
          if(!stream->packet)
            {
            stream->packet = bgav_stream_get_packet_write(stream);
            stream->packet->position = priv->position;
            stream->packet->pes_pts = priv->pes_header.pts;
            }
          bgav_packet_alloc(stream->packet, stream->packet->buf.len + priv->pes_header.payload_size);
          
          if(bgav_input_read_data(input, stream->packet->buf.buf + stream->packet->buf.len,
                                  priv->pes_header.payload_size) <
             priv->pes_header.payload_size)
            {
            bgav_stream_done_packet_write(stream, stream->packet);
            return 0;
            }
          stream->packet->buf.len += priv->pes_header.payload_size;

          if(stream->packet->pes_pts == GAVL_TIME_UNDEFINED)
            stream->packet->pes_pts = priv->pes_header.pts;
          
          /* Get LPCM duration */
          if(stream->fourcc == BGAV_MK_FOURCC('L', 'P', 'C', 'M'))
            {
            if(stream->packet->duration < 0)
              stream->packet->duration = 0;
            switch(stream->data.audio.bits_per_sample)
              {
              case 16:
                /* 4 bytes -> 2 samples */
                 stream->packet->duration = stream->packet->buf.len / (stream->data.audio.format->num_channels*2);
                 break;
              case 20:
                /* 5 bytes -> 2 samples */
                /* http://lists.mplayerhq.hu/pipermail/ffmpeg-devel/2006-September/016319.html */
                stream->packet->duration = (2 * stream->packet->buf.len) /
                  (stream->data.audio.format->num_channels*5);
                break;
              case 24:
                /* 6 bytes -> 2 samples */
                stream->packet->duration = stream->packet->buf.len / (stream->data.audio.format->num_channels*3);
                break;
              }
            }
          }
        else
          {
          bgav_input_skip(ctx->input, priv->pes_header.payload_size);
          }
        
        got_packet = 1;
        }
      else
        {
        //        bgav_pes_header_dump(&priv->pes_header);
        bgav_input_skip(input, priv->pes_header.payload_size);
        }
      
      }
    }

  if(!priv->find_streams)
    priv->is_running = 1;
  
  return 1;
  }

static gavl_source_status_t next_packet_mpegps(bgav_demuxer_context_t * ctx)
  {
  return next_packet(ctx, ctx->input) ? GAVL_SOURCE_OK : GAVL_SOURCE_EOF;
  }

#define NUM_PACKETS 200

static void find_streams(bgav_demuxer_context_t * ctx)
  {
  int i;
  mpegps_priv_t * priv;
  
  bgav_input_context_t * input_save = NULL;
  
  priv = ctx->priv;
  priv->find_streams = 1;
  priv->have_pts = 0;
  
  if(!(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    {
    input_save = ctx->input;
    ctx->input = bgav_input_open_as_buffer(ctx->input);
    }
  
  for(i = 0; i < NUM_PACKETS; i++)
    {
    if(!next_packet_mpegps(ctx))
      {
      break;
      }
    }
  priv->find_streams = 0;
  
  if(input_save)
    {
    bgav_input_close(ctx->input);
    bgav_input_destroy(ctx->input);
    ctx->input = input_save;
    }
  else
    bgav_input_seek(ctx->input, ctx->tt->cur->data_start, SEEK_SET);
  
  return;
  }

static void get_duration(bgav_demuxer_context_t * ctx)
  {
  int64_t scr_start, scr_end;
  uint32_t start_code = 0;
  
  mpegps_priv_t * priv;
  priv = ctx->priv;
  
  if(!ctx->input->total_bytes)
    return; 
 
  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    {
    /* We already have the first pack header */
    scr_start = priv->pack_header.scr;

    /* Get the last scr */
    bgav_input_seek(ctx->input, -3, SEEK_END);

    while(start_code != START_CODE_PACK_HEADER)
      {
      /* Some files only have one pack header at the beginning */
      if(ctx->input->position < ctx->input->total_bytes - 1024*1024)
        break;
      start_code = previous_start_code(ctx->input);
      }
        
    if(!bgav_pack_header_read(ctx->input, &priv->pack_header))
      {
      return;
      }
    scr_end = priv->pack_header.scr;

    gavl_track_set_duration(ctx->tt->cur->info, 
                            ((int64_t)(scr_end - scr_start) * GAVL_TIME_SCALE) / 90000);
    
    bgav_input_seek(ctx->input, ctx->tt->cur->data_start, SEEK_SET);
    }
#if 0
  else if(ctx->input->total_bytes && priv->pack_header.mux_rate)
    {
    gavl_track_set_duration(ctx->tt->cur->info, 
                            (ctx->input->total_bytes * GAVL_TIME_SCALE)/
                            (priv->pack_header.mux_rate*50));
    }
#endif
  }



/* Check for cdxa file, return 0 if there isn't one */
#if 0
static int init_cdxa(bgav_demuxer_context_t * ctx)
  {
  bgav_track_t * track;
  bgav_stream_t * stream;

  uint32_t fourcc;
  uint32_t size;
  mpegps_priv_t * priv;
  priv = ctx->priv;

  if(ctx->input->input->read_sector)
    return 0;
  
  if(!bgav_input_get_fourcc(ctx->input, &fourcc) ||
     (fourcc != BGAV_MK_FOURCC('R', 'I', 'F', 'F')))
    return 0;

  /* The CDXA is already tested by probe_mpegps, to we can
     directly proceed to the interesting stuff */
  bgav_input_skip(ctx->input, 12);

  while(1)
    {
    /* Go throuth the RIFF chunks until we have a data chunk */

    if(!bgav_input_read_fourcc(ctx->input, &fourcc) ||
       !bgav_input_read_32_le(ctx->input, &size))
      return 0;
    if(fourcc == BGAV_MK_FOURCC('d', 'a', 't', 'a'))
      break;
    bgav_input_skip(ctx->input, size);
    }
  ctx->tt->cur->data_start = ctx->input->position;
  priv->data_size = size;

  priv->total_sectors      = priv->data_size / CDXA_SECTOR_SIZE_RAW;
  priv->sector_size        = CDXA_SECTOR_SIZE;
  priv->sector_size_raw    = CDXA_SECTOR_SIZE_RAW;
  priv->sector_header_size = CDXA_HEADER_SIZE;
  priv->sector_buffer      = malloc(priv->sector_size_raw);

  priv->goto_sector = goto_sector_cdxa;
  priv->read_sector = read_sector_cdxa;
  
  priv->is_cdxa = 1;

  /* Initialize track table */

  ctx->tt = bgav_track_table_create(1);

  track = ctx->tt->cur;
  
  stream =  bgav_track_add_audio_stream(track, ctx->opt);
  stream->fourcc = BGAV_MK_FOURCC('.', 'm', 'p', '2');
  stream->index_mode = INDEX_MODE_SIMPLE;
  stream->stream_id = 0xc0;
  stream->timescale = 90000;
  
  stream =  bgav_track_add_video_stream(track, ctx->opt);
  stream->fourcc = BGAV_MK_FOURCC('m', 'p', 'v', '1');
  stream->index_mode = INDEX_MODE_SIMPLE;
  stream->stream_id = 0xe0;
  stream->timescale = 90000;
  
  return 1;
  }
#endif

static int open_mpegps(bgav_demuxer_context_t * ctx)
  {
  mpegps_priv_t * priv;
  int need_streams = 0;
  int j;
  bgav_stream_t * s;
  char * format;
  const char * mimetype;
  uint32_t start_code;
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
  
  /* Check for sector based access */

#if 0  
  if(!init_cdxa(ctx))
    {
    /* Check for VCD input module */
    if(ctx->input->sector_size)
      {
      priv->sector_size        = ctx->input->sector_size;
      priv->sector_size_raw    = ctx->input->sector_size_raw;
      priv->sector_header_size = ctx->input->sector_header_size;
      priv->total_sectors      = ctx->input->total_sectors;
      priv->sector_buffer      = malloc(ctx->input->sector_size_raw);

      if(ctx->input->input->seek_sector)
        priv->goto_sector = goto_sector_input;
      priv->read_sector = read_sector_input;
      }
    }

  if(priv->sector_size)
    init_sector_mode(ctx);
  else
#endif
  
  //  if(!pack_header_read(ctx->input, &priv->pack_header))
  //    return 0;
  
  if(!ctx->tt)
    {
    ctx->tt = bgav_track_table_create(1);
    need_streams = 1;
    }

  while(1)
    {
    if(!(start_code = next_start_code(ctx->input)))
      return 0;
    if(start_code == START_CODE_PACK_HEADER)
      break;
    bgav_input_skip(ctx->input, 4);
    }

  ctx->tt->cur->data_start = ctx->input->position;
  if(ctx->input->total_bytes)
    priv->data_size = ctx->input->total_bytes - ctx->tt->cur->data_start;
  
  if(!bgav_pack_header_read(ctx->input, &priv->pack_header))
    return 0;
  
  if(gavl_track_get_duration(ctx->tt->cur->info) == GAVL_TIME_UNDEFINED)
    get_duration(ctx);
  
  if(need_streams)
    find_streams(ctx);
  else
    priv->have_pts = 1;
  
  format = bgav_sprintf("MPEG-%d", priv->pack_header.version);

  if(priv->pack_header.version == 1)
    mimetype = "video/MP1S";
  else
    mimetype = "video/MP2P";

  bgav_track_set_format(ctx->tt->cur, format, mimetype);
  free(format);
  
  if(((ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE) && priv->have_pts) ||
     (ctx->input->flags & BGAV_INPUT_CAN_SEEK_TIME))
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
  
  /* Set the parser flags for all streams */

  for(j = 0; j < ctx->tt->cur->num_audio_streams; j++)
    {
    s = bgav_track_get_audio_stream(ctx->tt->cur, j);
      
    if(s->fourcc != BGAV_MK_FOURCC('L', 'P', 'C', 'M'))
      bgav_stream_set_parse_full(s);
      
    }
  for(j = 0; j < ctx->tt->cur->num_video_streams; j++)
    {
    s = bgav_track_get_video_stream(ctx->tt->cur, j);

    bgav_stream_set_parse_full(s);
      
    }
  for(j = 0; j < ctx->tt->cur->num_overlay_streams; j++)
    {
    s = bgav_track_get_overlay_stream(ctx->tt->cur, j);
    bgav_stream_set_parse_full(s);
    }
    
  ctx->index_mode = INDEX_MODE_MIXED;

  return 1;
  }

#if 0
static void seek_normal(bgav_demuxer_context_t * ctx, int64_t time,
                        int scale)
  {
  mpegps_priv_t * priv;
  int64_t file_position;
  uint32_t header = 0;

  gavl_time_t dur = gavl_track_get_duration(ctx->tt->cur->info);
  
  priv = ctx->priv;
  
  //  file_position = (priv->pack_header.mux_rate*50*time)/GAVL_TIME_SCALE;
  /* Using double is ugly, but in integer, this can overflow for large file (even in 64 bit).
     We do iterative seeking at this point anyway. */
  file_position = ctx->tt->cur->data_start +
    (int64_t)(priv->data_size * (double)gavl_time_unscale(scale, time)/(double)dur + 0.5);

  if(file_position <= ctx->tt->cur->data_start)
    file_position = ctx->tt->cur->data_start+1;
  if(file_position >= ctx->input->total_bytes)
    file_position = ctx->input->total_bytes - 4;

  while(1)
    {
    bgav_input_seek(ctx->input, file_position, SEEK_SET);
    
    while(header != START_CODE_PACK_HEADER)
      {
      header = previous_start_code(ctx->input);
      }
    if(do_sync(ctx))
      break;
    else
      {
      file_position -= 2000; /* Go a bit back */
      if(file_position <= ctx->tt->cur->data_start)
        {
        break; /* Escape from inifinite loop */
        }
      }
    }
  }
#endif

static int post_seek_resync_mpegps(bgav_demuxer_context_t * ctx)
  {
  uint32_t start_code;

  if(ctx->input->block_size > 1)
    {
    int rest = ctx->input->position % ctx->input->block_size;
    if(rest)
      bgav_input_seek(ctx->input, ctx->input->position - rest, SEEK_SET);
    }

  while(1)
    {
    if(!(start_code = next_start_code(ctx->input)))
      return 0;
    if(start_code == START_CODE_PACK_HEADER)
      break;
    bgav_input_skip(ctx->input, 4);
    }
  return 1;
  }

static void close_mpegps(bgav_demuxer_context_t * ctx)
  {
  mpegps_priv_t * priv;
  priv = ctx->priv;
  if(!priv)
    return;
  if(priv->input_mem)
    {
    bgav_input_close(priv->input_mem);
    bgav_input_destroy(priv->input_mem);
    }
  free(priv);
  }


const bgav_demuxer_t bgav_demuxer_mpegps =
  {
    .probe =          probe_mpegps,
    .open =           open_mpegps,
    .select_track =   select_track_mpegps,
    .next_packet  =   next_packet_mpegps,
    .post_seek_resync =   post_seek_resync_mpegps,
    //    .seek =           seek_mpegps,
    .close =          close_mpegps
  };
