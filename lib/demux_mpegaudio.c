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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
// #include <id3.h>
#include <xing.h>
#include <utils.h>
#include <mpa_header.h>

#define LOG_DOMAIN "mpegaudio"

#define PROBE_FRAMES    5
#define PROBE_BYTES     ((PROBE_FRAMES-1)*BGAV_MPA_MAX_FRAME_BYTES+4)

/* This is the actual demuxer */

typedef struct
  {
  int64_t data_start;
  int64_t data_end;

  /* Global metadata */
  gavl_dictionary_t metadata;
  
  bgav_xing_header_t xing;
  int have_xing;
  bgav_mpa_header_t header;

  } mpegaudio_priv_t;


static int select_track_mpegaudio(bgav_demuxer_context_t * ctx,
                                  int track);

#define MAX_BYTES 2885 /* Maximum size of an mpeg audio frame + 4 bytes for next header */

static int get_header(bgav_input_context_t * input, bgav_mpa_header_t * h)
  {
  uint8_t probe_data[4];
  
  /* Check for audio header */
  if((bgav_input_get_data(input, probe_data, 4) < 4) ||
     !bgav_mpa_header_decode(h, probe_data))
    return 0;
  
  return 1;
  }


static int probe_mpegaudio(bgav_input_context_t * input)
  {
  bgav_mpa_header_t h1, h2;
  uint8_t probe_data[BGAV_MPA_MAX_FRAME_BYTES+4];
  

  /* Check for audio header */
  if(!get_header(input, &h1))
    return 0;
  
  /* Now, we look where the next header might come
     and decode from that point */

  if(h1.frame_bytes > BGAV_MPA_MAX_FRAME_BYTES) /* Prevent possible security hole */
    return 0;
  
  if(bgav_input_get_data(input, probe_data, h1.frame_bytes + 4) < h1.frame_bytes + 4)
    return 0;

  if(!bgav_mpa_header_decode(&h2, &probe_data[h1.frame_bytes]))
    return 0;

  if(!bgav_mpa_header_equal(&h1, &h2))
    return 0;

  return 1;
  }

static int resync(bgav_demuxer_context_t * ctx, int check_next)
  {
  uint8_t buffer[BGAV_MPA_MAX_FRAME_BYTES+4];
  mpegaudio_priv_t * priv;
  int skipped_bytes = 0;
  bgav_mpa_header_t next_header;
    
  priv = ctx->priv;

  while(1)
    {
    if(bgav_input_get_data(ctx->input, buffer, 4) < 4)
      return 0;
    if(bgav_mpa_header_decode(&priv->header, buffer))
      {
      if(priv->header.frame_bytes > BGAV_MPA_MAX_FRAME_BYTES) /* Prevent possible security hole */
        return 0;

      if(!check_next)
        break;
      
      /* No next header, stop here */
      if(bgav_input_get_data(ctx->input, buffer, priv->header.frame_bytes + 4) < priv->header.frame_bytes + 4)
        break;

      /* Read the next header and check if it's equal to this one */
      if(bgav_mpa_header_decode(&next_header, &buffer[priv->header.frame_bytes]) && 
         bgav_mpa_header_equal(&priv->header, &next_header))
        break;
      }
    bgav_input_skip(ctx->input, 1);
    skipped_bytes++;
    }
  if(skipped_bytes)
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
             "Skipped %d bytes in MPEG audio stream", skipped_bytes);
  return 1;
  }

static gavl_time_t get_duration(bgav_demuxer_context_t * ctx,
                                int64_t start_offset,
                                int64_t end_offset)
  {
  gavl_time_t ret = GAVL_TIME_UNDEFINED;
  uint8_t frame[BGAV_MPA_MAX_FRAME_BYTES]; /* Max possible mpeg audio frame size */
  mpegaudio_priv_t * priv;

  //  memset(&priv->xing, 0, sizeof(xing));

  if(!(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    return GAVL_TIME_UNDEFINED;
  
  priv = ctx->priv;
  
  bgav_input_seek(ctx->input, start_offset, SEEK_SET);
  if(!resync(ctx, 1))
    return 0;
  
  if(bgav_input_get_data(ctx->input, frame,
                         priv->header.frame_bytes) < priv->header.frame_bytes)
    return 0;
  
  if(bgav_xing_header_read(&priv->xing, frame))
    {
    //    bgav_xing_header_dump(&priv->xing);
    ret = gavl_samples_to_time(priv->header.samplerate,
                               (int64_t)(priv->xing.frames) *
                               priv->header.samples_per_frame);
    return ret;
    }
  else if(bgav_mp3_info_header_probe(frame))
    {
    start_offset += priv->header.frame_bytes;
    }
  ret = (GAVL_TIME_SCALE * (end_offset - start_offset) * 8) /
    (priv->header.bitrate);
  return ret;
  }

static int set_stream(bgav_demuxer_context_t * ctx)
     
  {
  bgav_stream_t * s;
  uint8_t frame[BGAV_MPA_MAX_FRAME_BYTES]; /* Max possible mpeg audio frame size */
  mpegaudio_priv_t * priv;
  
  priv = ctx->priv;
  if(!resync(ctx, 1))
    return 0;
  
  /* Check for a VBR header */
  
  if(bgav_input_get_data(ctx->input, frame,
                         priv->header.frame_bytes) < priv->header.frame_bytes)
    return 0;
  
  if(bgav_xing_header_read(&priv->xing, frame))
    {
    priv->have_xing = 1;
    bgav_input_skip(ctx->input, priv->header.frame_bytes);
    priv->data_start += priv->header.frame_bytes;
    }
  else if(bgav_mp3_info_header_probe(frame))
    {
    bgav_input_skip(ctx->input, priv->header.frame_bytes);
    priv->data_start += priv->header.frame_bytes;
    }
  else
    {
    priv->have_xing = 0;
    }

  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  
  /* Get audio format */
  bgav_mpa_header_get_format(&priv->header,
                             s->data.audio.format);
  
  if(!s->container_bitrate)
    {
    if(priv->have_xing)
      s->container_bitrate = GAVL_BITRATE_VBR;
    else
      s->container_bitrate = priv->header.bitrate;
    }
  return 1;
  }


static int open_mpegaudio(bgav_demuxer_context_t * ctx)
  {
  int i;
  gavl_dictionary_t metadata_v1;
  gavl_dictionary_t metadata_v2;

  bgav_id3v1_tag_t * id3v1 = NULL;
  bgav_stream_t * s;
  
  mpegaudio_priv_t * priv;
  int64_t oldpos;
  const char * format;
  
  memset(&metadata_v1, 0, sizeof(metadata_v1));
  memset(&metadata_v2, 0, sizeof(metadata_v2));
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;    
  priv->data_start = ctx->input->position;
  if(ctx->input->id3v2)
    {
    if(ctx->input->opt.dump_headers)
      bgav_id3v2_dump(ctx->input->id3v2);
    
    bgav_id3v2_2_metadata(ctx->input->id3v2, &metadata_v2);

    }
  
  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    {
    oldpos = ctx->input->position;
    bgav_input_seek(ctx->input, -128, SEEK_END);

    if(bgav_id3v1_probe(ctx->input))
      {
      id3v1 = bgav_id3v1_read(ctx->input);
      if(id3v1)
        bgav_id3v1_2_metadata(id3v1, &metadata_v1);
      }
    bgav_input_seek(ctx->input, oldpos, SEEK_SET);
    }
  gavl_dictionary_merge(&priv->metadata, &metadata_v2, &metadata_v1);
  
  ctx->tt = bgav_track_table_create(1);

  s = bgav_track_add_audio_stream(*ctx->tt->tracks, ctx->opt);
  s->fourcc = BGAV_MK_FOURCC('.', 'm', 'p', '3');
    
  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    {
    priv->data_start = (ctx->input->id3v2) ?
      bgav_id3v2_total_bytes(ctx->input->id3v2) : 0;
    priv->data_end   = (id3v1) ? ctx->input->total_bytes - 128 :
      ctx->input->total_bytes;
    }

  gavl_track_set_duration(ctx->tt->tracks[0]->info, get_duration(ctx,
                                                                 priv->data_start,
                                                                 priv->data_end));
  gavl_dictionary_merge(ctx->tt->tracks[0]->metadata,
                        &metadata_v2, &metadata_v1);
    

  if(id3v1)
    bgav_id3v1_destroy(id3v1);
  gavl_dictionary_free(&metadata_v1);
  gavl_dictionary_free(&metadata_v2);
  
  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
  
  /* Set the format for each track */

  select_track_mpegaudio(ctx, 0);
  get_header(ctx->input, &priv->header);

  if(priv->header.layer == 3)
    format = GAVL_META_FORMAT_MP3;
  else
    format = "MPEG Audio";
  
  for(i = 0; i < ctx->tt->num_tracks; i++)
    {
    bgav_track_set_format(ctx->tt->cur, format, "audio/mpeg");
    s = bgav_track_get_audio_stream(ctx->tt->tracks[i], 0);
    
    /* Get audio format */
    bgav_mpa_header_get_format(&priv->header, s->data.audio.format);
    }
  ctx->index_mode = INDEX_MODE_SIMPLE;
  return 1;
  }

static gavl_source_status_t next_packet_mpegaudio(bgav_demuxer_context_t * ctx)
  {
  bgav_packet_t * p;
  bgav_stream_t * s;
  mpegaudio_priv_t * priv;
  int64_t bytes_left = -1;
  priv = ctx->priv;
  
  if(priv->data_end && (priv->data_end - ctx->input->position < 4))
    return GAVL_SOURCE_EOF;
  
  if(!resync(ctx, 0))
    return GAVL_SOURCE_EOF;
  
  if(priv->data_end)
    {
    bytes_left = priv->data_end - ctx->input->position;
    if(priv->header.frame_bytes < bytes_left)
      bytes_left = priv->header.frame_bytes;
    }
  else
    bytes_left = priv->header.frame_bytes;
  
  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  p = bgav_stream_get_packet_write(s);
  gavl_packet_alloc(p, bytes_left);

  p->position = ctx->input->position;
  
  if(bgav_input_read_data(ctx->input, p->buf.buf, bytes_left) < bytes_left)
    {
    return GAVL_SOURCE_EOF;
    }
  p->buf.len = bytes_left;
  PACKET_SET_KEYFRAME(p);

  p->duration = priv->header.samples_per_frame;
  
  bgav_stream_done_packet_write(s, p);

  return GAVL_SOURCE_OK;
  }

static void seek_mpegaudio(bgav_demuxer_context_t * ctx, int64_t time,
                           int scale)
  {
  int64_t pos;
  mpegaudio_priv_t * priv;
  bgav_stream_t * s;
  
  priv = ctx->priv;
  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);

  time -= gavl_time_rescale(scale,
                            s->data.audio.format->samplerate,
                            s->data.audio.preroll);
  if(time < 0)
    time = 0;
  
  if(priv->have_xing) /* VBR */
    {
    pos =
      bgav_xing_get_seek_position(&priv->xing,
                                  100.0 *
                                  (float)gavl_time_unscale(scale, time) /
                                  (float)(gavl_track_get_duration(ctx->tt->cur->info)));
    }
  else /* CBR */
    {
    pos = ((priv->data_end - priv->data_start) *
           gavl_time_unscale(scale, time)) / gavl_track_get_duration(ctx->tt->cur->info);
    }
  
  STREAM_SET_SYNC(s,
                  gavl_time_rescale(scale,
                                    s->data.audio.format->samplerate, time));
  
  pos += priv->data_start;
  bgav_input_seek(ctx->input, pos, SEEK_SET);
  }

static void close_mpegaudio(bgav_demuxer_context_t * ctx)
  {
  mpegaudio_priv_t * priv;
  priv = ctx->priv;
  
  gavl_dictionary_free(&priv->metadata);
  
  free(priv);
  }

static int select_track_mpegaudio(bgav_demuxer_context_t * ctx,
                                   int track)
  {
  mpegaudio_priv_t * priv;

  priv = ctx->priv;

  if(ctx->input->position != priv->data_start)
    {
    if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
      bgav_input_seek(ctx->input, priv->data_start, SEEK_SET);
    else
      return 0;
    }
  set_stream(ctx);
  return 1;
  }

const bgav_demuxer_t bgav_demuxer_mpegaudio =
  {
    .probe =        probe_mpegaudio,
    .open =         open_mpegaudio,
    .next_packet =  next_packet_mpegaudio,
    .seek =         seek_mpegaudio,
    .close =        close_mpegaudio,
    .select_track = select_track_mpegaudio
  };
