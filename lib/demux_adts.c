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
#include <stdio.h>
#include <string.h>

#include <avdec_private.h>

#define LOG_DOMAIN "adts"

#include <adts_header.h>

/* Supported header types */


#define BYTES_TO_READ (768*GAVL_MAX_CHANNELS)
     
#define IS_ADTS(h) ((h[0] == 0xff) && \
                    ((h[1] & 0xf0) == 0xf0) && \
                    ((h[1] & 0x06) == 0x00))

/* AAC demuxer */

typedef struct
  {
  int64_t data_size;
  int block_samples;
  } aac_priv_t;

static int probe_adts(bgav_input_context_t * input)
  {
  int ret;
  uint8_t * buffer;
  uint8_t header[7];
  bgav_adts_header_t h1, h2;
  
  /* Support aac live streams */

  
  if(bgav_input_get_data(input, header, 7) < 7)
    return 0;

  if(!bgav_adts_header_read(header, &h1))
    return 0;
  
  buffer = malloc(ADTS_HEADER_LEN + h1.frame_bytes);
  
  if(bgav_input_get_data(input, buffer, ADTS_HEADER_LEN + h1.frame_bytes) <
     ADTS_HEADER_LEN + h1.frame_bytes)
    return 0;

  ret = 0;

  if(bgav_adts_header_read(buffer + h1.frame_bytes, &h2) &&
     (h1.mpeg_version == h2.mpeg_version) &&
     (h1.samplerate == h2.samplerate) &&
     (h1.channel_configuration == h2.channel_configuration))
    ret = 1;
  free(buffer);
  return ret;
  }

static int open_adts(bgav_demuxer_context_t * ctx)
  {
  uint8_t header[4];
  aac_priv_t * priv;
  bgav_stream_t * s;
  bgav_id3v1_tag_t * id3v1 = NULL;
  gavl_dictionary_t id3v1_metadata, id3v2_metadata;
  uint8_t buf[ADTS_HEADER_LEN];
  bgav_adts_header_t adts;

  int64_t pts_start_num = 0;
  int     pts_start_den = 0;
  
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  /* Recheck header */

  while(1)
    {
    if(bgav_input_get_data(ctx->input, header, 4) < 4)
      return 0;
    
    if(IS_ADTS(header))
      break;
    bgav_input_skip(ctx->input, 1);
    }
  
  /* Create track */

  ctx->tt = bgav_track_table_create(1);
  ctx->tt->cur->data_start = ctx->input->position;
  
  /* Check for id3v1 tag at the end */

  if((ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE) &&
     ctx->input->location &&
     (!gavl_string_starts_with(ctx->input->location, "http://")))
    {
    bgav_input_seek(ctx->input, -128, SEEK_END);
    if(bgav_id3v1_probe(ctx->input))
      {
      id3v1 = bgav_id3v1_read(ctx->input);
      }
    bgav_input_seek(ctx->input, ctx->tt->cur->data_start, SEEK_SET);
    }

  //  if(ctx->input->id3v2)
  //    bgav_id3v2_dump(ctx->input->id3v2);
  
  if(ctx->input->id3v2 && id3v1)
    {
    memset(&id3v1_metadata, 0, sizeof(id3v1_metadata));
    memset(&id3v2_metadata, 0, sizeof(id3v2_metadata));
    bgav_id3v1_2_metadata(id3v1, &id3v1_metadata);
    bgav_id3v2_2_metadata(ctx->input->id3v2, &id3v2_metadata);
    //    gavl_dictionary_dump(&id3v2_metadata);

    gavl_dictionary_merge(ctx->tt->cur->metadata,
                        &id3v2_metadata, &id3v1_metadata);
    gavl_dictionary_free(&id3v1_metadata);
    gavl_dictionary_free(&id3v2_metadata);
    }
  else if(ctx->input->id3v2)
    bgav_id3v2_2_metadata(ctx->input->id3v2,
                          ctx->tt->cur->metadata);
  else if(id3v1)
    bgav_id3v1_2_metadata(id3v1,
                          ctx->tt->cur->metadata);

  if(ctx->input->total_bytes)
    priv->data_size = ctx->input->total_bytes - ctx->tt->cur->data_start;

  if(id3v1)
    {
    bgav_id3v1_destroy(id3v1);
    priv->data_size -= 128;
    }

  s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);

  /* This fourcc reminds the decoder to call a different init function */

  s->fourcc = BGAV_MK_FOURCC('A', 'D', 'T', 'S');
  
  /* Initialize rest */

  if(bgav_input_get_data(ctx->input, buf, ADTS_HEADER_LEN) < ADTS_HEADER_LEN)
    goto fail;

  if(!bgav_adts_header_read(buf, &adts))
    goto fail;

  //  bgav_adts_header_dump(&adts);

  // One block per frame: That means we can convert these
#if 0
  if(adts.num_blocks == 1) 
    {
    s->flags |= STREAM_FILTER_PACKETS;
    }
#endif
  
  if(adts.profile == 2) 
    priv->block_samples = 960;
  else
    priv->block_samples = 1024;
  
  s->data.audio.format->samplerate = adts.samplerate;
  s->timescale = adts.samplerate; // Timescale will be the same even if the samplerate changes

  if(gavl_dictionary_get_int(&ctx->input->m, META_START_PTS_DEN, &pts_start_den) &&
     (pts_start_den > 0) &&
     gavl_dictionary_get_long(&ctx->input->m, META_START_PTS_NUM, &pts_start_num))
    {
    gavl_stream_set_start_pts(s->info, pts_start_num, pts_start_den);
    }
  
  //  adts_header_dump(&adts);

  ctx->index_mode = INDEX_MODE_SIMPLE;
  
  bgav_track_set_format(ctx->tt->cur, "ADTS", "audio/aac");
  
  gavl_dictionary_get_int(&ctx->input->m, GAVL_META_BITRATE, &s->container_bitrate);

  //  fprintf(stderr, "adts_open\n");
  //  gavl_dictionary_dump(ctx->tt->cur->info, 2);

  
  return 1;
  
  fail:
  return 0;
  }

static gavl_source_status_t next_packet_adts(bgav_demuxer_context_t * ctx)
  {
  bgav_packet_t * p;
  bgav_stream_t * s;
  bgav_adts_header_t adts;
  aac_priv_t * priv;
  uint8_t buf[ADTS_HEADER_LEN];
  
  priv = ctx->priv;

  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);

  if(bgav_input_get_data(ctx->input, buf, ADTS_HEADER_LEN) < ADTS_HEADER_LEN)
    return GAVL_SOURCE_EOF;

  if(!bgav_adts_header_read(buf, &adts))
    return GAVL_SOURCE_EOF;

  
  p = bgav_stream_get_packet_write(s);
  
  p->duration = priv->block_samples * adts.num_blocks;
  p->position = ctx->input->position;

  PACKET_SET_KEYFRAME(p);
  
  bgav_packet_alloc(p, adts.frame_bytes);

  p->buf.len = bgav_input_read_data(ctx->input, p->buf.buf, adts.frame_bytes);

  if(p->buf.len < adts.frame_bytes)
    return GAVL_SOURCE_EOF;
  
  bgav_stream_done_packet_write(s, p);

  return GAVL_SOURCE_OK;
  }

static int select_track_adts(bgav_demuxer_context_t * ctx, int track)
  {
  int64_t pts_start_num = 0;
  int     pts_start_den = 0;
  bgav_stream_t * s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  
  if(gavl_dictionary_get_int(&ctx->input->m, META_START_PTS_DEN, &pts_start_den) &&
     (pts_start_den > 0) &&
     gavl_dictionary_get_long(&ctx->input->m, META_START_PTS_NUM, &pts_start_num))
    {
    STREAM_SET_SYNC(s, gavl_time_rescale(pts_start_den, s->timescale, pts_start_num));
    gavl_stream_set_start_pts(s->info, pts_start_num, pts_start_den);
    }
  return 1;
  }

static void close_adts(bgav_demuxer_context_t * ctx)
  {
  aac_priv_t * priv;
  priv = ctx->priv;

  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_adts =
  {
    .probe        = probe_adts,
    .open         = open_adts,
    .select_track = select_track_adts,
    .next_packet  = next_packet_adts,
    .close        = close_adts,
  };
