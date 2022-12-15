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

#include <mpc/mpcdec.h>

/*
 *  Musepack "demuxer": It's somewhat weird because libmusepack doesn't
 *  allow split the format parsing / decoding process.
 *
 *  For this reason, we we do all the decoding here and fake a PCM stream
 *  format such that the real "decoder" will just buffer/copy the samples
 *  to the output
 * 
 *  With this in approach, we'll have complete support for all libmusepack
 *  features (incl. seeking) without too much code duplication.
 */

#define LOG_DOMAIN "musepack"


typedef struct
  {
  bgav_input_context_t * ctx;

  /* We shorten the file at the start and end to hide eventual
     tags from libmusepack */
    
  int start_bytes;
  int end_bytes;
  
  } read_struct;

typedef struct
  {
  mpc_reader     reader;
  mpc_streaminfo si;
  mpc_demux * demux;
  read_struct    rs;
  } mpc_priv_t;

static int probe_mpc(bgav_input_context_t * input)
  {
  uint8_t test_data[3];
  if(bgav_input_get_data(input, test_data, 3) < 3)
    return 0;

  if((test_data[0] == 'M') &&
     (test_data[1] == 'P') &&
     (test_data[2] == '+'))
    return 1;
  return 0;
  }

static mpc_int32_t mpc_read(mpc_reader * reader, void *ptr, mpc_int32_t size)
  {
  read_struct * r = reader->data;
  return bgav_input_read_data(r->ctx, ptr, size);
  }

static mpc_bool_t mpc_seek(mpc_reader * reader, mpc_int32_t offset)
  {
  read_struct * r = reader->data;
  bgav_input_seek(r->ctx, offset + r->start_bytes, SEEK_SET);
  return MPC_TRUE;
  }

static mpc_int32_t mpc_tell(mpc_reader * reader)
  {
  read_struct * r = reader->data;
  return r->ctx->position - r->start_bytes;
  }

static mpc_int32_t mpc_get_size(mpc_reader * reader)
  {
  read_struct * r = reader->data;
  return r->ctx->total_bytes - r->start_bytes - r->end_bytes;
  }

static mpc_bool_t mpc_canseek(mpc_reader * reader)
  {
  read_struct * r = reader->data;
  return (r->ctx->flags & BGAV_INPUT_CAN_SEEK_BYTE) ? MPC_TRUE : MPC_FALSE;
  }

static int open_mpc(bgav_demuxer_context_t * ctx)
  {
  int ape_tag_size;
  gavl_dictionary_t start_metadata, end_metadata;

  bgav_id3v1_tag_t * id3v1  = NULL;
  bgav_ape_tag_t   * apetag = NULL;
    
  bgav_stream_t * s;
  mpc_priv_t * priv;

  if(!(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Cannot decode from nonseekable sources");
    return 0;
    }
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  /* Musepack is only readable from seekable sources */
  
  /* Setup reader */

  priv->rs.ctx = ctx->input;
  
  priv->reader.seek     = mpc_seek;
  priv->reader.read     = mpc_read;
  priv->reader.tell     = mpc_tell;
  priv->reader.get_size = mpc_get_size;
  priv->reader.canseek  = mpc_canseek;

  priv->reader.data     = &priv->rs;

  /* Set up track table */
  
  ctx->tt = bgav_track_table_create(1);
  
  /* Check for tags */

  priv->rs.start_bytes = ctx->input->position;

  if((ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE) && ctx->input->total_bytes)
    {
    /* Check for id3v1 */

    bgav_input_seek(ctx->input, -128, SEEK_END);

    if(bgav_id3v1_probe(ctx->input))
      {
      id3v1 = bgav_id3v1_read(ctx->input);
      priv->rs.end_bytes = 128;
      }
    else
      {
      bgav_input_seek(ctx->input, -32, SEEK_END);
      if(bgav_ape_tag_probe(ctx->input, &ape_tag_size))
        {
        bgav_input_seek(ctx->input, -ape_tag_size, SEEK_END);
        apetag = bgav_ape_tag_read(ctx->input, ape_tag_size);
        priv->rs.end_bytes = ape_tag_size;
        }
      }
    bgav_input_seek(ctx->input, priv->rs.start_bytes, SEEK_SET);
    }

  /* Setup metadata */

  if(ctx->input->id3v2 && ((id3v1) || (apetag)))
    {
    memset(&start_metadata, 0, sizeof(start_metadata));
    memset(&end_metadata, 0, sizeof(end_metadata));

    bgav_id3v2_2_metadata(ctx->input->id3v2, &start_metadata);

    if(id3v1)
      bgav_id3v1_2_metadata(id3v1, &end_metadata);
    else if(apetag)
      bgav_ape_tag_2_metadata(apetag, &end_metadata);
    
    gavl_dictionary_merge(ctx->tt->cur->metadata,
                        &start_metadata, &end_metadata);
    gavl_dictionary_free(&start_metadata);
    gavl_dictionary_free(&end_metadata);
    }
  
  else if(ctx->input->id3v2)
    bgav_id3v2_2_metadata(ctx->input->id3v2,
                          ctx->tt->cur->metadata);
  else if(id3v1)
    bgav_id3v1_2_metadata(id3v1,
                          ctx->tt->cur->metadata);
  else if(apetag)
    bgav_ape_tag_2_metadata(apetag,
                            ctx->tt->cur->metadata);
  
  if(id3v1)
    bgav_id3v1_destroy(id3v1);
  if(apetag)
    bgav_ape_tag_destroy(apetag);
  
  /* Initialize demuxer */
  priv->demux = mpc_demux_init(&priv->reader);
  mpc_demux_get_info(priv->demux, &priv->si);
  
  //  mpc_streaminfo_init(&priv->si);
  
  //  if(mpc_streaminfo_read(&priv->si, &priv->reader) != ERROR_CODE_OK)
  //    return 0;
  
  /* Fire up decoder and set up stream */
  
  //  mpc_decoder_setup(&priv->dec, &priv->reader);
  //  if(!mpc_decoder_initialize(&priv->dec, &priv->si))
  //    return 0;
  
  s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
  
  s->data.audio.format->samplerate    = priv->si.sample_freq;
  s->data.audio.format->num_channels  = priv->si.channels;
  s->data.audio.format->sample_format = GAVL_SAMPLE_FLOAT;
  s->data.audio.format->interleave_mode = GAVL_INTERLEAVE_ALL;
  s->data.audio.format->samples_per_frame = MPC_FRAME_LENGTH;
  gavl_set_channel_setup(s->data.audio.format);
  
  s->fourcc = BGAV_MK_FOURCC('g', 'a', 'v', 'l');

  s->timescale = priv->si.sample_freq;
  
  bgav_track_set_format(ctx->tt->cur, "Musepack", NULL);

  s->stats.pts_end = mpc_streaminfo_get_length_samples(&priv->si);

  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;

  return 1;
  }

static gavl_source_status_t next_packet_mpc(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;
  bgav_packet_t * p;
  mpc_priv_t * priv;

  mpc_frame_info fi;
  
  priv = ctx->priv;

  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  p = bgav_stream_get_packet_write(s);
  
  bgav_packet_alloc(p, MPC_DECODER_BUFFER_LENGTH * sizeof(float));
  
  //  if(!p->audio_frame)
  //    p->audio_frame = gavl_audio_frame_create(s->data.audio.format);

  fi.buffer = (float*)p->buf.buf;
  
  if(mpc_demux_decode(priv->demux, &fi) != MPC_STATUS_OK)
    return GAVL_SOURCE_EOF;
  
  p->duration = fi.samples;
  p->buf.len = fi.samples * s->data.audio.format->num_channels * sizeof(float);
  
  //  p->audio_frame->valid_samples = result;
  
  bgav_stream_done_packet_write(s, p);
  
  return GAVL_SOURCE_OK;
  }

static void seek_mpc(bgav_demuxer_context_t * ctx, int64_t time, int scale)
  {
  bgav_stream_t * s;
  
  mpc_priv_t * priv;
  priv = ctx->priv;

  s = bgav_track_get_audio_stream(ctx->tt->cur, 0);

  time = gavl_time_rescale(scale, s->timescale, time);
  time /= MPC_FRAME_LENGTH;
  time *= MPC_FRAME_LENGTH;
  
  STREAM_SET_SYNC(s, (time / MPC_FRAME_LENGTH) * MPC_FRAME_LENGTH );
  mpc_demux_seek_sample(priv->demux, time);
  }

static void close_mpc(bgav_demuxer_context_t * ctx)
  {
  mpc_priv_t * priv;
  priv = ctx->priv;

  if(priv->demux)
    mpc_demux_exit(priv->demux);
  
  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_mpc =
  {
    .probe =       probe_mpc,
    .open =        open_mpc,
    .next_packet = next_packet_mpc,
    .seek =        seek_mpc,
    .close =       close_mpc
  };

