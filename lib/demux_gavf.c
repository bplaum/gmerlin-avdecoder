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
#include <gavl/gavf.h>
#include <gavl/compression.h>

typedef struct
  {
  
  gavf_io_t * io;
  gavf_t * dec;
  } gavf_demuxer_t;

static int probe_gavf(bgav_input_context_t * input)
  {
  uint8_t probe_data[8];

  if(bgav_input_get_data(input, probe_data, 8) < 8)
    return 0;

  if(!strncmp((char*)probe_data, "GAVFFIDX", 8) ||
     !strncmp((char*)probe_data, "GAVFPHDR", 8))
    return 1;
  return 0;
  }

static int read_func(void * priv, uint8_t * data, int len)
  {
  return bgav_input_read_data(priv, data, len);
  }

static int64_t seek_func(void * priv, int64_t pos, int whence)
  {
  bgav_input_context_t * ctx = priv;
  bgav_input_seek(ctx, pos, whence);

  //  fprintf(stderr, "Seek func: %ld\n", pos);

  return ctx->position;
  }


static int init_track(bgav_track_t * track,
                      gavf_t * dec,
                      const bgav_options_t * opt)
  {
  int i;
  bgav_stream_t * s;
  const gavl_audio_format_t * afmt;
  const gavl_video_format_t * vfmt;
  const int64_t * pts;
  const gavf_program_header_t * ph;
  
  ph = gavf_get_program_header(dec);
  
  gavl_dictionary_copy(track->metadata, &ph->m);
  
  for(i = 0; i < ph->num_streams; i++)
    {
    s = NULL;
    afmt = NULL;
    vfmt = NULL;
    
    switch(ph->streams[i].type)
      {
      case GAVL_STREAM_AUDIO:
        s = bgav_track_add_audio_stream(track, opt);
        afmt = &ph->streams[i].format.audio;
        break;
      case GAVL_STREAM_VIDEO:
        s = bgav_track_add_video_stream(track, opt);
        vfmt = &ph->streams[i].format.video;
        break;
      case GAVL_STREAM_TEXT:
        s = bgav_track_add_text_stream(track, opt, BGAV_UTF8);
        gavl_dictionary_get_int(&ph->streams[i].m, GAVL_META_STREAM_SAMPLE_TIMESCALE, &s->timescale);
        break;
      case GAVL_STREAM_OVERLAY:
        s = bgav_track_add_overlay_stream(track, opt);
        vfmt = &ph->streams[i].format.video;
        break;
      case GAVL_STREAM_MSG:
      case GAVL_STREAM_NONE:
        break;
      }

    bgav_stream_set_from_gavl(s, &ph->streams[i].ci,
                              afmt, vfmt,
                              &ph->streams[i].m);
    
    pts = gavf_first_pts(dec);
    if(pts)
      {
      s->stats.pts_start = pts[i];
      pts = gavf_end_pts(dec);
      if(pts)
        s->stats.pts_end = pts[i];
      }
    s->stream_id = ph->streams[i].id;
    }
  
  return 1;
  }

static int open_gavf(bgav_demuxer_context_t * ctx)
  {
  gavf_options_t * opt;
  uint32_t flags = 0;
  gavf_demuxer_t * priv = calloc(1, sizeof(*priv));
  const gavf_program_header_t * ph;
  gavl_time_t duration;
  
  ctx->priv = priv;

  priv->dec = gavf_create();

  opt = gavf_get_options(priv->dec);

  if(ctx->opt->dump_headers)
    flags |= GAVF_OPT_FLAG_DUMP_HEADERS;
  if(ctx->opt->dump_indices)
    flags |= GAVF_OPT_FLAG_DUMP_INDICES;
  gavf_options_set_flags(opt, flags);

  priv->io =
    gavf_io_create(read_func,
                   NULL,
                   (ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE) ? seek_func : 0,
                   NULL,
                   NULL,
                   ctx->input);
  
  if(!gavf_open_read(priv->dec, priv->io))
    return 0;
  
  ctx->tt = bgav_track_table_create(1);

  if(!init_track(ctx->tt->cur, priv->dec,
                 ctx->opt))
    return 0;

  ph = gavf_get_program_header(priv->dec);
  
  if(gavl_dictionary_get_long(&ph->m, GAVL_META_APPROX_DURATION, &duration))
    {
    if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
      ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
    }
  
  gavl_dictionary_set_string(ctx->tt->cur->metadata, 
                             GAVL_META_FORMAT, "GAVF");

  
  return 1;
  }

static int select_track_gavf(bgav_demuxer_context_t * ctx, int track)
  {
  gavf_demuxer_t * priv;
  priv = ctx->priv;
  gavf_reset(priv->dec);
  return 1;
  }

static void seek_gavf(bgav_demuxer_context_t * ctx, int64_t time, int scale)
  {
  int i;
  const int64_t * sync_pts;
  gavf_demuxer_t * priv;
  const gavf_program_header_t * ph;
  bgav_stream_t * s;
  
  priv = ctx->priv;

  sync_pts = gavf_seek(priv->dec, time, scale);

  ph = gavf_get_program_header(priv->dec);

  for(i = 0; i < ph->num_streams; i++)
    {
    s = bgav_track_find_stream(ctx, ph->streams[i].id);
    if(s)
      STREAM_SET_SYNC(s, sync_pts[i]);
    }
  }


static int next_packet_gavf(bgav_demuxer_context_t * ctx)
  {
  bgav_packet_t * bp;
  gavl_packet_t gp;
  
  const gavf_packet_header_t * ph;
  gavf_demuxer_t * priv;
  bgav_stream_t * s;

  priv = ctx->priv;

  ph = gavf_packet_read_header(priv->dec);
  if(!ph)
    {
    // fprintf(stderr, "Reading packet header failed\n");
    return 0;
    }
  s = bgav_track_find_stream(ctx, ph->stream_id);
  if(!s)
    {
    gavf_packet_skip(priv->dec);
    return 1;
    }

  gavl_packet_init(&gp);
    
  bp = bgav_stream_get_packet_write(s);
    
  gp.data       = bp->data;
  gp.data_alloc = bp->data_alloc;
    
  if(!gavf_packet_read_packet(priv->dec, &gp))
    {
    // fprintf(stderr, "Reading packet failed\n");
    return 0;
    }
  bp->data       = gp.data;
  bp->data_alloc = gp.data_alloc;
  bp->data_size = gp.data_len;

  bgav_packet_from_gavl(&gp, bp);
  
  bgav_stream_done_packet_write(s, bp);
  return 1;
  }

static void resync_gavf(bgav_demuxer_context_t * ctx, bgav_stream_t * s)
  {
  }

static void close_gavf(bgav_demuxer_context_t * ctx)
  {
  gavf_demuxer_t * priv;
  priv = ctx->priv;
  gavf_close(priv->dec);
  gavf_io_destroy(priv->io);
  
  free(priv);
  }


const bgav_demuxer_t bgav_demuxer_gavf =
  {
    .probe        = probe_gavf,
    .open         = open_gavf,
    .select_track = select_track_gavf,
    .next_packet = next_packet_gavf,
    .seek =        seek_gavf,
    .resync      = resync_gavf,
    .close =       close_gavf
  };
