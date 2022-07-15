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


/*
 *  Flac support is implemented the following way:
 *  We parse the toplevel format here, without the
 *  need for FLAC specific libraries. Then we put the
 *  whole header into the extradata of the stream
 *
 *  The flac audio decoder will then be done using FLAC
 *  software
 */

#include <avdec_private.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vorbis_comment.h>
#include <flac_header.h>

#include <cue.h>


/* Probe */

static int probe_flac(bgav_input_context_t * input)
  {
  uint8_t probe_data[4];

  if(bgav_input_get_data(input, probe_data, 4) < 4)
    return 0;

  if((probe_data[0] == 'f') &&
     (probe_data[1] == 'L') &&
     (probe_data[2] == 'a') &&
     (probe_data[3] == 'C'))
    return 1;
     
  return 0;
  }

typedef struct
  {
  bgav_flac_streaminfo_t streaminfo;
  bgav_flac_seektable_t seektable;
  
  bgav_bytebuffer_t buf;
  int64_t next_header;
  int has_sync;
  bgav_flac_frame_header_t this_fh;
  
  bgav_flac_frame_header_t first_fh;
  int have_first_fh;
  int64_t pts;
  } flac_priv_t;


static int open_flac(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s = NULL;
  uint8_t header[4];
  uint32_t size;
  flac_priv_t * priv;
  bgav_input_context_t * input_mem;
  uint8_t * comment_buffer;

  bgav_vorbis_comment_t vc;
    
  /* Skip header */

  bgav_input_skip(ctx->input, 4);

  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
  ctx->tt = bgav_track_table_create(1);
  
  header[0] = 0;
  
  while(!(header[0] & 0x80))
    {
    if(bgav_input_read_data(ctx->input, header, 4) < 4)
      return 0;
    
    size = header[1];
    size <<= 8;
    size |= header[2];
    size <<= 8;
    size |= header[3];
    //    if(!bgav_input_read_24_be(ctx->input, &size))
    //      return 0;
    
    switch(header[0] & 0x7F)
      {
      case 0: // STREAMINFO
        /* Add audio stream */
        s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
        s->ci->global_header_len = BGAV_FLAC_STREAMINFO_SIZE + 8; // Make a complete file header
        s->ci->global_header = malloc(s->ci->global_header_len);

        s->ci->global_header[0] = 'f';
        s->ci->global_header[1] = 'L';
        s->ci->global_header[2] = 'a';
        s->ci->global_header[3] = 'C';
        
        memcpy(s->ci->global_header+4, header, 4);
        
        /* We tell the decoder, that this is the last metadata packet */
        s->ci->global_header[4] |= 0x80;
        
        if(bgav_input_read_data(ctx->input, s->ci->global_header + 8,
                                BGAV_FLAC_STREAMINFO_SIZE) < 
           BGAV_FLAC_STREAMINFO_SIZE)
          goto fail;
        
        if(!bgav_flac_streaminfo_read(s->ci->global_header + 8, &priv->streaminfo))
          goto fail;
        if(ctx->opt->dump_headers)
          bgav_flac_streaminfo_dump(&priv->streaminfo);
        bgav_flac_streaminfo_init_stream(&priv->streaminfo, s);
        
        if(priv->streaminfo.total_samples)
          s->stats.pts_end = priv->streaminfo.total_samples;

        if(priv->streaminfo.min_framesize > 0)
          s->stats.size_min = priv->streaminfo.min_framesize;

        if(priv->streaminfo.max_framesize > 0)
          s->stats.size_max = priv->streaminfo.max_framesize;

        if(priv->streaminfo.min_blocksize > 0)
          {
          if((priv->streaminfo.min_blocksize == priv->streaminfo.max_blocksize) &&
             priv->streaminfo.total_samples)
            s->stats.duration_min = priv->streaminfo.total_samples % priv->streaminfo.min_blocksize;
          }
        
        if(priv->streaminfo.max_blocksize > 0)
          s->stats.duration_max = priv->streaminfo.max_blocksize;
        
        
        //        bgav_input_skip(ctx->input, size);
        break;
      case 1: // PADDING
        bgav_input_skip(ctx->input, size);
        break;
      case 2: // APPLICATION
        bgav_input_skip(ctx->input, size);
        break;
      case 3: // SEEKTABLE
        if(!bgav_flac_seektable_read(ctx->input, &priv->seektable, size))
          goto fail;
        if(ctx->opt->dump_indices)
          bgav_flac_seektable_dump(&priv->seektable);
        break;
      case 4: // VORBIS_COMMENT
        comment_buffer = malloc(size);
        if(bgav_input_read_data(ctx->input, comment_buffer, size) < size)
          return 0;

        input_mem =
          bgav_input_open_memory(comment_buffer, size, ctx->opt);

        memset(&vc, 0, sizeof(vc));

        if(bgav_vorbis_comment_read(&vc, input_mem))
          {
          bgav_vorbis_comment_2_metadata(&vc,
                                         ctx->tt->cur->metadata);
          }

        if(s)
          gavl_dictionary_set_string(s->m, GAVL_META_SOFTWARE, vc.vendor);
        
        if(ctx->opt->dump_headers)
          bgav_vorbis_comment_dump(&vc);
        
        bgav_vorbis_comment_free(&vc);
        bgav_input_close(input_mem);
        bgav_input_destroy(input_mem);
        free(comment_buffer);
        break;
      case 5: // CUESHEET
        bgav_input_skip(ctx->input, size);
        break;
      case 6: // METADATA_BLOCK_PICTURE
        {
        uint32_t len;
        uint32_t width;
        uint32_t height;
        uint32_t type;
        char * mimetype = NULL;
        
        if(!bgav_input_read_32_be(ctx->input, &type) ||
           !bgav_input_read_32_be(ctx->input, &len))
          return 0;
 
        mimetype = malloc(len + 1);
        
        if(bgav_input_read_data(ctx->input, (uint8_t*)mimetype, len) < len)
          goto image_fail;

        mimetype[len] = '\0';
 
        if(!bgav_input_read_32_be(ctx->input, &len)) // Desc len
          goto image_fail;

        bgav_input_skip(ctx->input, len); // Description

        if(!bgav_input_read_32_be(ctx->input, &width) ||
           !bgav_input_read_32_be(ctx->input, &height))
          goto image_fail;
 
        bgav_input_skip(ctx->input, 8);      
        
        if(!bgav_input_read_32_be(ctx->input, &len)) // Data len
          goto image_fail;

        gavl_metadata_add_image_embedded(ctx->tt->cur->metadata,
                                         GAVL_META_COVER_EMBEDDED,
                                         width, height,
                                         mimetype,
                                         ctx->input->position,
                                         len);

        bgav_input_skip(ctx->input, len); // Skip actual image data

        free(mimetype);
        break;

        image_fail:
        if(mimetype)
          free(mimetype);        
        return 0;
        }
        break;
      default:
        bgav_input_skip(ctx->input, size);
      }

    }
  ctx->data_start = ctx->input->position;

  if(ctx->input->total_bytes > 0)
    s->stats.total_bytes = ctx->input->total_bytes - ctx->input->position;
  
  ctx->flags |= BGAV_DEMUXER_HAS_DATA_START;
  
  bgav_track_set_format(ctx->tt->cur, GAVL_META_FORMAT_FLAC, "audio/flac");
  
  ctx->index_mode = INDEX_MODE_SIMPLE;
  
  if(priv->seektable.num_entries && (ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    {
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;

    gavl_dictionary_set_int(ctx->tt->cur->metadata, GAVL_META_SAMPLE_ACCURATE, 1);
    
    }
  bgav_demuxer_init_cue(ctx);
  
  return 1;
    fail:
  return 0;
  }

#define BYTES_TO_READ 1024
#define SYNC_SIZE (1024*1024)

static int find_next_header(bgav_demuxer_context_t * ctx,
                            int start,
                            bgav_flac_frame_header_t * h)
  {
  int i;
  flac_priv_t * priv = ctx->priv;

  if(priv->buf.size < BGAV_FLAC_FRAMEHEADER_MIN)
    {
    if(!bgav_bytebuffer_append_read(&priv->buf, ctx->input, BYTES_TO_READ, 0))
      return -1;
    }
  
  while(1)
    {
    for(i = start; i < priv->buf.size - BGAV_FLAC_FRAMEHEADER_MAX; i++)
      {
      if(bgav_flac_frame_header_read(priv->buf.buffer + i, priv->buf.size - i,
                                     &priv->streaminfo, h))
        {
        if(!priv->have_first_fh)
          {
          memcpy(&priv->first_fh, h, sizeof(*h));
          priv->have_first_fh = 1;
          return i;
          }
        else if(!priv->has_sync) // Assume we seeked correctly
          return i;
        else if(bgav_flac_check_crc(priv->buf.buffer, i))
          return i;
        //        else
        //          fprintf(stderr, "crc mismatch\n");
        }
      }
    
    if(priv->buf.size > SYNC_SIZE)
      return -1;

    start = priv->buf.size - BGAV_FLAC_FRAMEHEADER_MAX;
    
    if(!bgav_bytebuffer_append_read(&priv->buf, ctx->input, BYTES_TO_READ, 0))
      return -1;
    
    }
  return -1;
  }

static int next_packet_flac(bgav_demuxer_context_t * ctx)
  {
  int pos, size;
  bgav_stream_t * s;
  bgav_packet_t * p;
  bgav_flac_frame_header_t next_fh;
  
  flac_priv_t * priv = ctx->priv;
  
  s = bgav_track_find_stream(ctx, 0);

  if(!s)
    return 0;

  if(ctx->next_packet_pos)
    {
    size = ctx->next_packet_pos - ctx->input->position;
    if(ctx->input->position + size > ctx->input->total_bytes)
      size = ctx->input->total_bytes - ctx->input->position;
    }
  else
    {
    if(!priv->has_sync)
      {
      pos = find_next_header(ctx, 0, &next_fh);
      if(pos < 0)
        return 0;

      if(pos > 0)
        bgav_bytebuffer_remove(&priv->buf, pos);

      memcpy(&priv->this_fh, &next_fh, sizeof(next_fh));
    
      priv->has_sync = 1;
      }
  
    /* Get next header */
    pos = find_next_header(ctx, BGAV_FLAC_FRAMEHEADER_MIN, &next_fh);

    //  if(pos < 0)
    //  fprintf(stderr, "pos < 0!!\n");
  
    if(pos < 0) // EOF
      size = priv->buf.size;
    else
      size = pos;

    if((pos < 0) && !size)
      return 0;
    }
  
  p = bgav_stream_get_packet_write(s);
  bgav_packet_alloc(p, size);

  if(ctx->next_packet_pos)
    {
    if(bgav_input_read_data(ctx->input, p->data, size) < size)
      return 0;
    p->position = ctx->input->position - size;

    if(!bgav_flac_frame_header_read(p->data, size,
                                    &priv->streaminfo, &priv->this_fh))
      return 0;
    }
  else
    {
    memcpy(p->data, priv->buf.buffer, size);
    p->position = ctx->input->position - priv->buf.size;
    bgav_bytebuffer_remove(&priv->buf, size);
    }
  
  //  p->pts = fh.sample_number;
  
  p->duration = priv->this_fh.blocksize;
  p->pts = priv->pts;

  //  fprintf(stderr, "Duration: %ld ", p->duration);
  
  if(priv->streaminfo.total_samples &&
     (p->pts < priv->streaminfo.total_samples) &&
     (p->pts + p->duration > priv->streaminfo.total_samples))
    {
    p->duration = priv->streaminfo.total_samples - p->pts;
    }
  // fprintf(stderr, "Packet pts %ld sn: %ld dur: %ld pos: %ld\n",
  // p->pts, priv->this_fh.sample_number, p->duration, p->position);

  //  fprintf(stderr, "%ld\n", p->duration);
  
  priv->pts += p->duration;
  p->data_size = size;
  
  //  memcpy(&priv->fh, &fh, sizeof(fh));

//  bgav_packet_dump(p);
  
  bgav_stream_done_packet_write(s, p);

  if(!ctx->next_packet_pos)
    {
    /* Save frame header for later use */
    memcpy(&priv->this_fh, &next_fh, sizeof(next_fh));
    }
  
  return 1;
  }

static void seek_flac(bgav_demuxer_context_t * ctx, int64_t time, int scale)
  {
  int i;
  flac_priv_t * priv;
  int64_t sample_pos;
  bgav_stream_t * s = bgav_track_get_audio_stream(ctx->tt->cur, 0);
  
  priv = ctx->priv;
  
  sample_pos = gavl_time_rescale(scale,
                                 priv->streaminfo.samplerate,
                                 time);
  
  for(i = 0; i < priv->seektable.num_entries - 1; i++)
    {
    if((priv->seektable.entries[i].sample_number <= sample_pos) &&
       (priv->seektable.entries[i+1].sample_number >= sample_pos))
      break;
    }
  
  if(priv->seektable.entries[i+1].sample_number == sample_pos)
    i++;
  
  /* Seek to the point */
  
  bgav_input_seek(ctx->input,
                  priv->seektable.entries[i].offset + ctx->data_start,
                  SEEK_SET);
  
  STREAM_SET_SYNC(s, priv->seektable.entries[i].sample_number);
  priv->pts = priv->seektable.entries[i].sample_number;

  priv->has_sync = 0;
  bgav_bytebuffer_flush(&priv->buf);
  }

static int select_track_flac(bgav_demuxer_context_t * ctx, int track)
  {
  flac_priv_t * priv;
  priv = ctx->priv;
  priv->has_sync = 0;
  bgav_bytebuffer_flush(&priv->buf);
  return 1;
  }

static void close_flac(bgav_demuxer_context_t * ctx)
  {
  flac_priv_t * priv;
  priv = ctx->priv;

  bgav_flac_seektable_free(&priv->seektable);

  bgav_bytebuffer_free(&priv->buf);

  free(priv);
  }

static void resync_flac(bgav_demuxer_context_t * ctx, bgav_stream_t * s)
  {
  flac_priv_t * priv;
  
  priv = ctx->priv;
  priv->has_sync = 0;
  bgav_bytebuffer_flush(&priv->buf);
  priv->pts = STREAM_GET_SYNC(s);
  }

const bgav_demuxer_t bgav_demuxer_flac =
  {
    .probe =       probe_flac,
    .open =        open_flac,
    .next_packet = next_packet_flac,
    .select_track = select_track_flac,
    .seek =        seek_flac,
    .resync        = resync_flac,
    .close =       close_flac
  };


