/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
 *
 * Copyright (c) 2001 - 2024 Members of the Gmerlin project
 * http://github.com/bplaum
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
  
  bgav_flac_frame_header_t this_fh;
  
  } flac_priv_t;

static void import_seek_table(bgav_flac_seektable_t * tab,
                              bgav_demuxer_context_t * ctx,
                              int64_t offset)
  {
  int i;

  ctx->si = gavl_packet_index_create(tab->num_entries);
  ctx->si->flags |= GAVL_PACKET_INDEX_SPARSE;
  for(i = 0; i < tab->num_entries; i++)
    {
    ctx->si->entries[i].position = tab->entries[i].offset + offset;
    ctx->si->entries[i].pts       = tab->entries[i].sample_number;
    ctx->si->entries[i].stream_id = BGAV_DEMUXER_STREAM_ID_RAW;
    ctx->si->entries[i].flags     = GAVL_PACKET_KEYFRAME;
    }
  ctx->si->num_entries = tab->num_entries;
  }

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

        gavl_buffer_alloc(&s->ci->codec_header, BGAV_FLAC_STREAMINFO_SIZE + 8);
        
        s->ci->codec_header.buf[0] = 'f';
        s->ci->codec_header.buf[1] = 'L';
        s->ci->codec_header.buf[2] = 'a';
        s->ci->codec_header.buf[3] = 'C';
        s->fourcc = BGAV_MK_FOURCC('F', 'L', 'A', 'C');
        s->stream_id = BGAV_DEMUXER_STREAM_ID_RAW;
        
        memcpy(s->ci->codec_header.buf + 4, header, 4);
        
        /* We tell the decoder, that this is the last metadata packet */
        s->ci->codec_header.buf[4] |= 0x80;
        s->flags |= STREAM_RAW_PACKETS;
        
        if(bgav_input_read_data(ctx->input, s->ci->codec_header.buf + 8,
                                BGAV_FLAC_STREAMINFO_SIZE) < 
           BGAV_FLAC_STREAMINFO_SIZE)
          goto fail;
        
        if(!bgav_flac_streaminfo_read(s->ci->codec_header.buf + 8, &priv->streaminfo))
          goto fail;

        s->ci->codec_header.len = BGAV_FLAC_STREAMINFO_SIZE + 8;
        
        
        if(ctx->opt->dump_headers)
          bgav_flac_streaminfo_dump(&priv->streaminfo);
        
        bgav_flac_streaminfo_init_stream(&priv->streaminfo, s->info);
        
        if(priv->streaminfo.total_samples)
          {
          s->stats.pts_start = 0;
          s->stats.pts_end = priv->streaminfo.total_samples;
          }
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
          bgav_input_open_memory(comment_buffer, size);

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
  ctx->tt->cur->data_start = ctx->input->position;

  if(ctx->input->total_bytes > 0)
    s->stats.total_bytes = ctx->input->total_bytes - ctx->input->position;
  
  bgav_track_set_format(ctx->tt->cur, GAVL_META_FORMAT_FLAC, "audio/flac");
  
  ctx->index_mode = INDEX_MODE_SIMPLE;
  
  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    {
    if(priv->seektable.num_entries)
      import_seek_table(&priv->seektable, ctx, ctx->input->position);
    
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
    gavl_dictionary_set_int(ctx->tt->cur->metadata, GAVL_META_SAMPLE_ACCURATE, 1);
    }

  s->flags |= STREAM_RAW_PACKETS;
  bgav_stream_set_parse_full(s);
  
  return 1;
    fail:
  return 0;
  }

static void close_flac(bgav_demuxer_context_t * ctx)
  {
  flac_priv_t * priv;
  priv = ctx->priv;

  bgav_flac_seektable_free(&priv->seektable);

  //  gavl_buffer_free(&priv->buf);

  free(priv);
  }


const bgav_demuxer_t bgav_demuxer_flac =
  {
    .probe =       probe_flac,
    .open =        open_flac,
    .next_packet = bgav_demuxer_next_packet_raw,
    .close =       close_flac
  };


