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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>

#include <avdec_private.h>
#include <qt.h>
#include <utils.h>
#define LOG_DOMAIN "quicktime"

static gavl_source_status_t handle_emsg(bgav_demuxer_context_t * ctx,
                                        qt_atom_header_t * h, gavl_dictionary_t * metadata);

typedef struct
  {
  uint32_t fourcc;
  const char * format;
  const char * mimetype;
  } ftyp_t;
  
static const ftyp_t ftyps[];


static void set_mdat(bgav_input_context_t * input,
                     qt_atom_header_t * h,
                     qt_mdat_t * mdat)
  {
  mdat->start = input->position;
  mdat->size  = h->size - (input->position - h->start_position);
  }

static void time_to_metadata(gavl_dictionary_t * m,
                             const char * key,
                             uint64_t t)
  {
  time_t ti;
  struct tm tm;

  /*  2082844800 = seconds between 1/1/04 and 1/1/70 */
  ti = t - 2082844800;
  localtime_r(&ti, &tm);
  tm.tm_mon++;
  tm.tm_year+=1900;

  gavl_dictionary_set_date_time(m,
                              key,
                              tm.tm_year,
                              tm.tm_mon,
                              tm.tm_mday,
                              tm.tm_hour,
                              tm.tm_min,
                              tm.tm_sec);
  
  }


typedef struct
  {
  qt_trak_t * trak;
  qt_stbl_t * stbl; /* For convenience */
  int64_t ctts_pos;
  int64_t stts_pos;
  int64_t stss_pos;
  int64_t stps_pos;
  int64_t stsd_pos;
  int64_t stsc_pos;
  int64_t stco_pos;
  int64_t stsz_pos;

  int64_t stts_count;
  int64_t ctts_count;
  int64_t stss_count;
  int64_t stsd_count;
  //  int64_t stsc_count;

  //  int64_t tics; /* Time in tics (depends on time scale of this stream) */
  //  int64_t total_tics;

  int skip_first_frame; /* Enabled only if the first frame has a different codec */
  int skip_last_frame; /* Enabled only if the last frame has a different codec */
  int64_t dts;
  } stream_priv_t;

typedef struct
  {
  gavl_dictionary_t m_emsg;
  
  uint32_t ftyp_fourcc;
  qt_moov_t moov;

  stream_priv_t * streams;
  
  int seeking;

  int has_edl;
  /* Ohhhh, no, MPEG-4 can have multiple mdats... */
  
  int num_mdats;
  int mdats_alloc;
  int current_mdat;
  
  qt_mdat_t * mdats;
  
  qt_trak_t * timecode_track;
  int num_timecode_tracks;
 
  int fragmented;
  int64_t first_moof;

  qt_moof_t current_moof;
  qt_mdat_t fragment_mdat;
  } qt_priv_t;

static void bgav_qt_moof_to_superindex(bgav_demuxer_context_t * ctx,
                                       qt_moof_t * m, gavl_packet_index_t * si)
  {
  int i, j, k;

  int64_t offset;
  uint32_t size = 0; //
  int stream_id = -1; //
  int64_t timestamp;
  int keyframe;
  int duration = 0; //
  bgav_stream_t * s;
  qt_priv_t * priv = ctx->priv;
  bgav_track_t * t = ctx->tt->cur;
  stream_priv_t * sp;
  
  for(i = 0; i < m->num_trafs; i++)
    {
    qt_tfhd_t * tfhd = &m->traf[i].tfhd;

    for(j = 0; j < priv->moov.num_tracks; j++)
      {
      if(priv->streams[j].trak->tkhd.track_id == tfhd->track_ID)
        {
        stream_id = j;
        break;
        }
      }

    s = bgav_track_find_stream_all(t, stream_id);
    sp = s->priv;
    
    for(j = 0; j < m->traf[i].num_truns; j++)
      {
      qt_trun_t * trun = &m->traf[i].trun[j];

      if(tfhd->flags & TFHD_BASE_DATA_OFFSET_PRESENT)
        offset = tfhd->base_data_offset;
      else
        offset = m->h.start_position; // moof Start

      offset += trun->data_offset;
      
      for(k = 0; k < trun->sample_count; k++)
        {
        if(trun->flags & TRUN_SAMPLE_SIZE_PRESENT)
          size = trun->samples[k].sample_size;
        else if(tfhd->flags & TFHD_DEFAULT_SAMPLE_SIZE_PRESENT)
          size = tfhd->default_sample_size;
        
        if(trun->flags & TRUN_SAMPLE_DURATION_PRESENT)
          duration = trun->samples[k].sample_duration;
        else if(tfhd->flags & TFHD_DEFAULT_SAMPLE_DURATION_PRESENT)
          duration = tfhd->default_sample_duration;

        keyframe = 1;
        if(trun->flags & TRUN_SAMPLE_FLAGS_PRESENT)
          {
          if(trun->samples[k].sample_flags & 0x10000)
            keyframe = 0;
          }
        else if(tfhd->flags & TFHD_DEFAULT_SAMPLE_FLAGS_PRESENT)
          {
          if(tfhd->default_sample_flags & 0x10000)
            keyframe = 0;
          }
        
        timestamp = sp->dts +
          trun->samples[k].sample_composition_time_offset -
          trun->samples[0].sample_composition_time_offset;

        if(s)
          {
          if(!keyframe) /* Got cleared earlier since stss is missing */
            s->ci->flags |= GAVL_COMPRESSION_HAS_P_FRAMES; 
          if(trun->samples[k].sample_composition_time_offset)
            s->ci->flags |= GAVL_COMPRESSION_HAS_B_FRAMES; 
          }
        
        gavl_packet_index_add(si, 
                              offset,
                              size,
                              stream_id,
                              timestamp,
                              keyframe ? GAVL_PACKET_KEYFRAME : 0,
                              duration);
        
        sp->dts += duration;
        offset += size;
        }
      }
    }
  }


/*
 *  We support all 3 types of quicktime audio encapsulation:
 *
 *  1: stsd version 0: Uncompressed audio
 *  2: stsd version 1: CBR encoded audio: Additional fields added
 *  3: stsd version 1, _Compression_id == -2: VBR audio, one "sample"
 *                     equals one frame of compressed audio data
 */

/* Intitialize everything */

static void stream_init(bgav_stream_t * bgav_s, qt_trak_t * trak,
                        int moov_scale)
  {
  stream_priv_t * s = bgav_s->priv;
  s->trak = trak;
  s->stbl = &trak->mdia.minf.stbl;

  bgav_s->stats.pts_start = 0;
  
  s->stts_pos = (s->stbl->stts.num_entries > 1) ? 0 : -1;
  s->ctts_pos = (s->stbl->has_ctts) ? 0 : -1;
  /* stsz_pos is -1 if all samples have the same size */
  s->stsz_pos = (s->stbl->stsz.sample_size) ? -1 : 0;

  /* Detect negative first timestamp */
  if((trak->edts.elst.num_entries == 1) &&
     (trak->edts.elst.table[0].media_time != 0))
    bgav_s->stats.pts_start = -trak->edts.elst.table[0].media_time;

  /* Detect positive first timestamp */
  else if((trak->edts.elst.num_entries == 2) &&
          (trak->edts.elst.table[0].media_time == -1))
    bgav_s->stats.pts_start = gavl_time_rescale(moov_scale, trak->mdia.mdhd.time_scale, 
                                                trak->edts.elst.table[0].duration);
  
  //  fprintf(stderr, "stream_init: %"PRId64"\n", bgav_s->stats.pts_start);
  
  //  if(s->first_pts)
  
  /* Set encoding software */
  if(trak->mdia.hdlr.component_name)
    gavl_dictionary_set_string(bgav_s->m, GAVL_META_SOFTWARE,
                      trak->mdia.hdlr.component_name);

  time_to_metadata(bgav_s->m,
                   GAVL_META_DATE_CREATE,
                   trak->mdia.mdhd.creation_time);
  time_to_metadata(bgav_s->m,
                   GAVL_META_DATE_MODIFY,
                   trak->mdia.mdhd.modification_time);
  
  }

static int trak_has_edl(qt_trak_t * trak)
  {
  if((trak->edts.elst.num_entries > 2) ||
     ((trak->edts.elst.num_entries == 2) && (trak->edts.elst.table[0].media_time >= 0)))
    return 1;
  return 0;
  }

static int probe_quicktime(bgav_input_context_t * input)
  {
  uint32_t header;
  uint8_t test_data[16];
  uint8_t * pos;
  
  if(bgav_input_get_data(input, test_data, 16) < 16)
    return 0;

  pos = test_data + 4;

  header = BGAV_PTR_2_FOURCC(pos);

  if(header == BGAV_MK_FOURCC('w','i','d','e'))
    {
    pos = test_data + 12;
    header = BGAV_PTR_2_FOURCC(pos);
    }

  switch(header)
    {
    case BGAV_MK_FOURCC('m','o','o','v'):
    case BGAV_MK_FOURCC('f','t','y','p'):
    case BGAV_MK_FOURCC('f','r','e','e'):
    case BGAV_MK_FOURCC('m','d','a','t'):
      return 1;
    }
  return 0;
  }


static int check_keyframe(stream_priv_t * s)
  {
  int ret = 0;
  if(!s->stbl->stss.num_entries)
    return 1;
  if((s->stss_pos >= s->stbl->stss.num_entries) &&
     (s->stps_pos >= s->stbl->stps.num_entries))
    return 0;

  s->stss_count++;

  /* Try stts */
  if(s->stss_pos < s->stbl->stss.num_entries)
    {
    ret = (s->stbl->stss.entries[s->stss_pos] == s->stss_count) ? 1 : 0;
    if(ret)
      {
      s->stss_pos++;
      return ret;
      }
    }
  /* Try stps */
  if(s->stps_pos < s->stbl->stps.num_entries)
    {
    ret = (s->stbl->stps.entries[s->stps_pos] == s->stss_count) ? 1 : 0;
    if(ret)
      {
      s->stps_pos++;
      return ret;
      }
    }
  return ret;
  }

static void add_packet(bgav_demuxer_context_t * ctx,
                       qt_priv_t * priv,
                       bgav_stream_t * s,
                       int index,
                       int64_t offset,
                       int stream_id,
                       int64_t timestamp,
                       int keyframe,
                       int duration,
                       int chunk_size)
  {
  if(stream_id >= 0)
    gavl_packet_index_add(ctx->si, offset, chunk_size,
                          stream_id, timestamp, keyframe ? GAVL_PACKET_KEYFRAME : 0, duration);
  
  if(index && !ctx->si->entries[index-1].size)
    {
    /* Check whether to advance the mdat */

    if(offset >= priv->mdats[priv->current_mdat].start +
       priv->mdats[priv->current_mdat].size)
      {
      if(!ctx->si->entries[index-1].size)
        {
        ctx->si->entries[index-1].size =
          priv->mdats[priv->current_mdat].start +
          priv->mdats[priv->current_mdat].size - ctx->si->entries[index-1].position;
        }
      while(offset >= priv->mdats[priv->current_mdat].start +
            priv->mdats[priv->current_mdat].size)
        {
        priv->current_mdat++;
        }
      }
    else
      {
      if(!ctx->si->entries[index-1].size)
        {
        ctx->si->entries[index-1].size =
        offset - ctx->si->entries[index-1].position;
        }
      }
    }
  }

static int next_moof(bgav_demuxer_context_t * ctx)
  {
  qt_atom_header_t h;
  qt_priv_t * priv;
  priv = ctx->priv;
  bgav_qt_moof_free(&priv->current_moof);

  while(1)
    {
    if(!bgav_qt_atom_read_header(ctx->input, &h))
      return 0;
    if(h.fourcc == BGAV_MK_FOURCC('m','o','o','f'))
      {
      bgav_qt_moof_read(&h, ctx->input, &priv->current_moof);
      return 1;
      }
    else
      bgav_qt_atom_skip(ctx->input, &h);
    }
  return 0;
  }

static void build_index_fragmented(bgav_demuxer_context_t * ctx)
  {
  qt_priv_t * priv;
  priv = ctx->priv;

  ctx->si = gavl_packet_index_create(0);

  while(1)
    {
    /* current_moof is already loaded */
    bgav_qt_moof_to_superindex(ctx, &priv->current_moof, ctx->si);
    
    if(!(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))    
      break;

    if(!next_moof(ctx))
      break;    
    }
  }

static void build_index(bgav_demuxer_context_t * ctx)
  {
  int i, j;
  int stream_id = 0;
  int64_t chunk_offset;
  int64_t * chunk_indices;
  stream_priv_t * s;
  qt_priv_t * priv;
  int num_packets = 0;
  bgav_stream_t * bgav_s;
  int chunk_samples;
  int packet_size;
  int duration;
  qt_trak_t * trak;
  int pts_offset;
  int done = 0;
  priv = ctx->priv;
  
  if(priv->fragmented)
    {
    build_index_fragmented(ctx);
    return;
    }

  /* 1 step: Count the total number of chunks */
  for(i = 0; i < priv->moov.num_tracks; i++)
    {
    trak = &priv->moov.tracks[i];
    if(trak->mdia.minf.has_vmhd) /* One video chunk can be more packets (=frames) */
      {
      num_packets += bgav_qt_trak_samples(&priv->moov.tracks[i]);
      if(priv->streams[i].skip_first_frame)
        num_packets--;
      if(priv->streams[i].skip_last_frame)
        num_packets--;
      }
    else if(trak->mdia.minf.has_smhd)
      {
      /* Some audio frames will be read as "samples" (-> VBR audio!) */
      if(!trak->mdia.minf.stbl.stsz.sample_size)
        num_packets += bgav_qt_trak_samples(trak);
      else /* Other packets (uncompressed) will be complete quicktime chunks */
        num_packets += bgav_qt_trak_chunks(trak);
      }
    else if(trak->mdia.minf.stbl.stsd.entries &&
            (!strncmp((char*)trak->mdia.minf.stbl.stsd.entries[0].data, "text", 4) ||
             !strncmp((char*)trak->mdia.minf.stbl.stsd.entries[0].data, "tx3g", 4) ||
             !strncmp((char*)trak->mdia.minf.stbl.stsd.entries[0].data, "mp4s", 4)))
      {
      num_packets += bgav_qt_trak_samples(trak);
      }
    else // For other tracks, we count entire chunks
      {
      num_packets += bgav_qt_trak_chunks(&priv->moov.tracks[i]);
      }
    }
  if(!num_packets)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "No packets in movie");
    return;
    }
  ctx->si = gavl_packet_index_create(num_packets);
  
  chunk_indices = calloc(priv->moov.num_tracks, sizeof(*chunk_indices));

  /* Skip empty mdats */

  while(!priv->mdats[priv->current_mdat].size)
    priv->current_mdat++;
  
  i = 0;

  /* Set the dts of the streams */
  for(i = 0; i < ctx->tt->cur->num_streams; i++)
    {
    bgav_s = ctx->tt->cur->streams[i];
    if(!bgav_s->priv)
      break;
    s = bgav_s->priv;
    s->dts = bgav_s->stats.pts_start;

    //    fprintf(stderr, "Stream: %d, dts: %"PRId64"\n", i, s->dts);
    }
  
  while(i < num_packets)
    {
    /* Find the stream with the lowest chunk offset */

    chunk_offset = 9223372036854775807LL; /* Maximum */
    
    for(j = 0; j < priv->moov.num_tracks; j++)
      {
      if((priv->streams[j].stco_pos <
          priv->moov.tracks[j].mdia.minf.stbl.stco.num_entries) &&
         (priv->moov.tracks[j].mdia.minf.stbl.stco.entries[priv->streams[j].stco_pos] < chunk_offset))
        {
        stream_id = j;
        chunk_offset = priv->moov.tracks[j].mdia.minf.stbl.stco.entries[priv->streams[j].stco_pos];
        }
      }
    
    //    if(j == priv->moov.num_tracks)
    //      return;
    
    bgav_s = bgav_track_find_stream_all(ctx->tt->cur, stream_id);

    if(bgav_s && (bgav_s->type == GAVL_STREAM_AUDIO))
      {
      s = bgav_s->priv;

      if(!s->stbl->stsz.sample_size)
        {
        /* Read single packets of a chunk. We do this, when the stsz atom has more than
           zero entries */

        for(j = 0; j < s->stbl->stsc.entries[s->stsc_pos].samples_per_chunk; j++)
          {
          if(s->stsz_pos >= s->stbl->stsz.num_entries)
            {
            i++;
            continue;
            }
#if 0
          fprintf(stderr, "stsz_pos: %ld / %d, sample_in_chunk: %d, chunk_samples: %d\n",
                  s->stsz_pos, s->stbl->stsz.num_entries,
                  j, s->stbl->stsc.entries[s->stsc_pos].samples_per_chunk);
#endif
          packet_size   = s->stbl->stsz.entries[s->stsz_pos];
          s->stsz_pos++;
          
          chunk_samples = (s->stts_pos >= 0) ?
            s->stbl->stts.entries[s->stts_pos].duration :
            s->stbl->stts.entries[0].duration;

          add_packet(ctx,
                     priv,
                     bgav_s,
                     i, chunk_offset,
                     stream_id,
                     s->dts,
                     check_keyframe(s), chunk_samples, packet_size);

          s->dts += chunk_samples;
            
          chunk_offset += packet_size;
          /* Advance stts */
          if(s->stts_pos >= 0)
            {
            s->stts_count++;
            if(s->stts_count >= s->stbl->stts.entries[s->stts_pos].count)
              {
              s->stts_pos++;
              s->stts_count = 0;
              }
            }
          
          i++;
          }
        }
      else
        {
        /* Usual case: We have to guess the chunk size from subsequent chunks */
        if(s->stts_pos >= 0)
          {
          chunk_samples =
            s->stbl->stts.entries[s->stts_pos].duration *
            s->stbl->stsc.entries[s->stsc_pos].samples_per_chunk;
          }
        else
          {
          chunk_samples =
            s->stbl->stts.entries[0].duration *
            s->stbl->stsc.entries[s->stsc_pos].samples_per_chunk;
          }
      
        add_packet(ctx,
                   priv,
                   bgav_s,
                   i, chunk_offset,
                   stream_id,
                   s->dts,
                   check_keyframe(s), chunk_samples, 0);
        /* Time to sample */
        s->dts += chunk_samples;
        if(s->stts_pos >= 0)
          {
          s->stts_count++;
          if(s->stts_count >= s->stbl->stts.entries[s->stts_pos].count)
            {
            s->stts_pos++;
            s->stts_count = 0;
            }
          }
        i++;
        }
      s->stco_pos++;
      /* Update sample to chunk */
      if(s->stsc_pos < s->stbl->stsc.num_entries - 1)
        {
        if(s->stbl->stsc.entries[s->stsc_pos+1].first_chunk - 1 == s->stco_pos)
          s->stsc_pos++;
        }
      }
    else if(bgav_s && (bgav_s->type == GAVL_STREAM_VIDEO))
      {
      s = bgav_s->priv;
      for(j = 0; j < s->stbl->stsc.entries[s->stsc_pos].samples_per_chunk; j++)
        {
        if(s->stsz_pos >= s->stbl->stsz.num_entries)
          {
          done = 1;
          break;
          }
        
        packet_size = (s->stsz_pos >= 0) ? s->stbl->stsz.entries[s->stsz_pos]:
          s->stbl->stsz.sample_size;

        /* Sample size */
        if(s->stsz_pos >= 0)
          s->stsz_pos++;

        if(s->stts_pos >= 0)
          duration = s->stbl->stts.entries[s->stts_pos].duration;
        else
          duration = s->stbl->stts.entries[0].duration;

        /*
         *  We must make sure, that the pts starts at 0. This means, that
         *  ISO compliant values will be shifted to the (wrong)
         *  values produced by Apple Quicktime
         */
        
        if(s->ctts_pos >= 0)
          pts_offset =
            (int32_t)s->stbl->ctts.entries[s->ctts_pos].duration -
            (int32_t)s->stbl->ctts.entries[0].duration;
        else
          pts_offset = 0;
        
        if((s->skip_first_frame && !s->stco_pos) ||
           (s->skip_last_frame &&
            (s->stco_pos == s->stbl->stco.num_entries)))
          {
          add_packet(ctx,
                     priv,
                     bgav_s,
                     i, chunk_offset,
                     -1,
                     s->dts + pts_offset,
                     check_keyframe(s),
                     duration,
                     packet_size);
          }
        else
          {
          add_packet(ctx,
                     priv,
                     bgav_s,
                     i, chunk_offset,
                     stream_id,
                     s->dts + pts_offset,
                     check_keyframe(s),
                     duration,
                     packet_size);
          i++;
          }
        chunk_offset += packet_size;
        s->dts += duration;
        
        /* Time to sample */
        if(s->stts_pos >= 0)
          {
          s->stts_count++;
          if(s->stts_count >= s->stbl->stts.entries[s->stts_pos].count)
            {
            s->stts_pos++;
            s->stts_count = 0;
            }
          }
        /* Composition time to sample */
        if(s->ctts_pos >= 0)
          {
          s->ctts_count++;
          if(s->ctts_count >= s->stbl->ctts.entries[s->ctts_pos].count)
            {
            s->ctts_pos++;
            s->ctts_count = 0;
            }
          }
        }
      s->stco_pos++;
      /* Update sample to chunk */
      if(s->stsc_pos < s->stbl->stsc.num_entries - 1)
        {
        if(s->stbl->stsc.entries[s->stsc_pos+1].first_chunk - 1 == s->stco_pos)
          s->stsc_pos++;
        }
      }
    else if(bgav_s &&
            ((bgav_s->type == GAVL_STREAM_TEXT) ||
             (bgav_s->type == GAVL_STREAM_OVERLAY)))
      {
      s = bgav_s->priv;
      
      /* Read single samples of a chunk */
      
      for(j = 0; j < s->stbl->stsc.entries[s->stsc_pos].samples_per_chunk; j++)
        {
        packet_size   = s->stbl->stsz.entries[s->stsz_pos];
        s->stsz_pos++;

        if(s->stts_pos >= 0)
          duration = s->stbl->stts.entries[s->stts_pos].duration;
        else
          duration = s->stbl->stts.entries[0].duration;

        
        add_packet(ctx,
                   priv,
                   bgav_s,
                   i, chunk_offset,
                   stream_id,
                   s->dts,
                   check_keyframe(s), duration,
                   packet_size);
        
        chunk_offset += packet_size;

        s->dts += duration;
        
        /* Time to sample */
        if(s->stts_pos >= 0)
          {
          s->stts_count++;
          if(s->stts_count >= s->stbl->stts.entries[s->stts_pos].count)
            {
            s->stts_pos++;
            s->stts_count = 0;
            }
          }
        i++;
        }
      s->stco_pos++;
      /* Update sample to chunk */
      if(s->stsc_pos < s->stbl->stsc.num_entries - 1)
        {
        if(s->stbl->stsc.entries[s->stsc_pos+1].first_chunk - 1 == s->stco_pos)
          s->stsc_pos++;
        }
      }
    else
      {
      /* Fill in dummy packet */
      add_packet(ctx, priv, NULL, i, chunk_offset, stream_id, -1, 0, 0, 0);
      i++;
      priv->streams[stream_id].stco_pos++;
      }
    if(done)
      break;
    }
  /* Set the final packet size to the end of the mdat */

  if(ctx->si->entries[ctx->si->num_entries-1].size <= 0)
    ctx->si->entries[ctx->si->num_entries-1].size =
      priv->mdats[priv->current_mdat].start +
      priv->mdats[priv->current_mdat].size -
      ctx->si->entries[ctx->si->num_entries-1].position;
  
  free(chunk_indices);
  }

#define SET_UDTA_STRING(gavl_name, src) \
  if(!gavl_dictionary_get_string(ctx->tt->cur->metadata, gavl_name) && moov->udta.src) \
    {                                                                   \
    if(moov->udta.have_ilst)                                            \
      gavl_dictionary_set_string(ctx->tt->cur->metadata, gavl_name, moov->udta.src); \
    else                                                                \
      gavl_dictionary_set_string_nocopy(ctx->tt->cur->metadata, gavl_name, \
                              bgav_convert_string(cnv, moov->udta.src, -1, NULL)); \
    }

#define SET_UDTA_INT(gavl_name, src) \
  if(!gavl_dictionary_get_string(ctx->tt->cur->metadata, gavl_name) && moov->udta.src && \
     isdigit(*(moov->udta.src)))                                        \
    { \
    gavl_dictionary_set_int(ctx->tt->cur->metadata, gavl_name, atoi(moov->udta.src)); \
    }

static void set_metadata(bgav_demuxer_context_t * ctx)
  {
  qt_priv_t * priv;
  qt_moov_t * moov;
  
  bgav_charset_converter_t * cnv = NULL;
  
  priv = ctx->priv;
  moov = &priv->moov;

  if(!moov->udta.have_ilst)
    cnv = bgav_charset_converter_create("ISO-8859-1", BGAV_UTF8);
    
  
  SET_UDTA_STRING(GAVL_META_ARTIST,    ART);
  SET_UDTA_STRING(GAVL_META_TITLE,     nam);
  SET_UDTA_STRING(GAVL_META_ALBUM,     alb);
  SET_UDTA_STRING(GAVL_META_GENRE,     gen);
  SET_UDTA_STRING(GAVL_META_COPYRIGHT, cpy);
  SET_UDTA_INT(GAVL_META_TRACKNUMBER,  trk);
  SET_UDTA_STRING(GAVL_META_COMMENT,   cmt);
  SET_UDTA_STRING(GAVL_META_COMMENT,   inf);
  SET_UDTA_STRING(GAVL_META_AUTHOR,    aut);
  
  if(!gavl_dictionary_get_string(ctx->tt->cur->metadata, GAVL_META_TRACKNUMBER)
     && moov->udta.trkn)
    {
    gavl_dictionary_set_int(ctx->tt->cur->metadata, GAVL_META_TRACKNUMBER,
                          moov->udta.trkn);
    }

  time_to_metadata(ctx->tt->cur->metadata,
                   GAVL_META_DATE_CREATE,
                   moov->mvhd.creation_time);
  time_to_metadata(ctx->tt->cur->metadata,
                   GAVL_META_DATE_MODIFY,
                   moov->mvhd.modification_time);

  
  if(cnv)
    bgav_charset_converter_destroy(cnv);

  
  //  gavl_dictionary_dump(&ctx->tt->cur->metadata);
  }

/*
 *  This struct MUST match the channel locations assumed by
 *  ffmpeg (libavcodec/mpegaudiodec.c: mp3Frames[16], mp3Channels[16], chan_offset[9][5])
 */

static const struct
  {
  int num_channels;
  gavl_channel_id_t channels[8];
  }
mp3on4_channels[] =
  {
    { 0 }, /* Custom */
    { 1, { GAVL_CHID_FRONT_CENTER } }, // C
    { 2, { GAVL_CHID_FRONT_LEFT, GAVL_CHID_FRONT_RIGHT } }, // FLR
    { 3, { GAVL_CHID_FRONT_LEFT, GAVL_CHID_FRONT_RIGHT, GAVL_CHID_FRONT_CENTER } },
    { 4, { GAVL_CHID_FRONT_LEFT, GAVL_CHID_FRONT_RIGHT,
           GAVL_CHID_FRONT_CENTER, GAVL_CHID_REAR_CENTER } }, // C FLR BS
    { 5, { GAVL_CHID_FRONT_LEFT, GAVL_CHID_FRONT_RIGHT,
           GAVL_CHID_REAR_LEFT, GAVL_CHID_REAR_RIGHT,
           GAVL_CHID_FRONT_CENTER } }, // C FLR BLRS
    { 6, { GAVL_CHID_FRONT_LEFT, GAVL_CHID_FRONT_RIGHT,
           GAVL_CHID_REAR_LEFT, GAVL_CHID_REAR_RIGHT,
           GAVL_CHID_FRONT_CENTER, GAVL_CHID_LFE } }, // C FLR BLRS LFE a.k.a 5.1
    { 8, { GAVL_CHID_FRONT_LEFT, GAVL_CHID_FRONT_RIGHT,
           GAVL_CHID_SIDE_LEFT, GAVL_CHID_SIDE_RIGHT,
           GAVL_CHID_FRONT_CENTER, GAVL_CHID_LFE,
           GAVL_CHID_REAR_LEFT, GAVL_CHID_REAR_RIGHT } }, // C FLR BLRS BLR LFE a.k.a 7.1 
    { 4, { GAVL_CHID_FRONT_LEFT, GAVL_CHID_FRONT_RIGHT,
           GAVL_CHID_REAR_LEFT, GAVL_CHID_REAR_RIGHT } }, // FLR BLRS (Quadrophonic)
  };


static int init_mp3on4(bgav_stream_t * s)
  {
  int channel_config;
  s->fourcc = BGAV_MK_FOURCC('m', '4', 'a', 29);
  
  channel_config = (s->ci->codec_header.buf[1] >> 3) & 0x0f;
  if(!channel_config || (channel_config > 8))
    return 0;
  s->data.audio.format->num_channels = mp3on4_channels[channel_config].num_channels;
  memcpy(s->data.audio.format->channel_locations,
         mp3on4_channels[channel_config].channels,
         s->data.audio.format->num_channels * sizeof(mp3on4_channels[channel_config].channels[0]));
  return 1;
  }

/* shamelessly stolen from ffmpeg (avpriv_split_xiph_headers) */

static int split_xiph_headers(uint8_t *extradata, int extradata_size,
                              int first_header_size, uint8_t *header_start[3],
                              int header_len[3])
  {
  int i;
  
  if (extradata_size >= 6 && GAVL_PTR_2_16BE(extradata) == first_header_size)
    {
    int overall_len = 6;
    for (i=0; i<3; i++)
      {
      header_len[i] = GAVL_PTR_2_16BE(extradata);
      extradata += 2;
      header_start[i] = extradata;
      extradata += header_len[i];
      if (overall_len > extradata_size - header_len[i])
        return -1;
      overall_len += header_len[i];
      }
    }
  else if (extradata_size >= 3 && extradata_size < INT_MAX - 0x1ff && extradata[0] == 2)
    {
    int overall_len = 3;
    extradata++;
    for (i=0; i<2; i++, extradata++)
      {
      header_len[i] = 0;
      for (; overall_len < extradata_size && *extradata==0xff; extradata++)
        {
        header_len[i] += 0xff;
        overall_len   += 0xff + 1;
        }
      header_len[i] += *extradata;
      overall_len   += *extradata;
      if (overall_len > extradata_size)
        return -1;
      }
    header_len[2] = extradata_size - overall_len;
    header_start[0] = extradata;
    header_start[1] = header_start[0] + header_len[0];
    header_start[2] = header_start[1] + header_len[1];
    }
  else
    {
    return 0;
    }
  return 1;
  }

/* the audio fourcc mp4a doesn't necessarily mean, that we actually
   have AAC audio */

static const struct
  {
  int objectTypeId;
  uint32_t fourcc;
  }
audio_object_ids[] =
  {
    { 105, BGAV_MK_FOURCC('.','m','p','3') },
    { 107, BGAV_MK_FOURCC('.','m','p','2') },
    {  36, BGAV_MK_FOURCC('m','a','l','s') },
  };

static void set_audio_from_esds(bgav_stream_t * s, qt_esds_t * esds)
  {
  int i;
  int object_id;
  
  //  fprintf(stderr, "Object type ID: %d\n", esds->objectTypeId);
  //  bgav_qt_esds_dump(0, esds);

  bgav_stream_set_extradata(s, esds->decoderConfig,
                            esds->decoderConfigLen);
  
  for(i = 0; i < sizeof(audio_object_ids)/sizeof(audio_object_ids[0]); i++)
    {
    if(audio_object_ids[i].objectTypeId == esds->objectTypeId)
      {
      s->fourcc = audio_object_ids[i].fourcc;
      return;
      }
    }

  if((esds->objectTypeId == 0x40) && (esds->decoderConfigLen >= 2))
    {
    object_id = esds->decoderConfig[0] >> 3;

    if(object_id == 31)
      {
      object_id = esds->decoderConfig[0] & 0x07;
      object_id <<= 3;
      object_id |= (esds->decoderConfig[1] >> 5) & 0x07;
      object_id += 32;
      }
    
    // fprintf(stderr, "object_id: %d\n", object_id);

    for(i = 0; i < sizeof(audio_object_ids)/sizeof(audio_object_ids[0]); i++)
      {
      if(audio_object_ids[i].objectTypeId == object_id)
        {
        s->fourcc = audio_object_ids[i].fourcc;
        return;
        }
      }
    }

  /* Vorbis */
  if((esds->objectTypeId == 0xdd) && (esds->streamType == 0x15) &&
     (esds->decoderConfigLen > 8) && (s->fourcc == BGAV_MK_FOURCC('m','p','4','a')))
    {
    fprintf(stderr, "Detected Vorbis in mp4\n");
    s->fourcc = BGAV_VORBIS;
    }
                                      
  
  }

static void process_packet_subtitle_qt(bgav_stream_t * s, bgav_packet_t * p)
  {
  int i;
  uint16_t len;
  len = GAVL_PTR_2_16BE(p->buf.buf);

  if(!len)
    {
    *(p->buf.buf) = '\0'; // Empty string
    p->buf.len = 1;
    }
  else
    {
    memmove(p->buf.buf, p->buf.buf+2, len);

    /* De-Macify linebreaks */
    for(i = 0; i < len; i++)
      {
      if(p->buf.buf[i] == '\r')
        p->buf.buf[i] = '\n';
      }
    }
  p->buf.len = len;
  // p->duration = -1;
  }

static void process_packet_subtitle_tx3g(bgav_stream_t * s, bgav_packet_t * p)
  {
  //  int i;
  uint16_t len;

  len = GAVL_PTR_2_16BE(p->buf.buf);
  
  if(len)
    {
    memmove(p->buf.buf, p->buf.buf+2, len);
    p->buf.len = len;
    }
  else
    {
    *(p->buf.buf) = '\0'; // Empty string
    p->buf.len = 1;
    }
  //  p->duration = -1;
  
#if 0  
  /* De-Macify linebreaks */
  for(i = 0; i < len; i++)
    {
    if(p->buf.buf[i] == '\r')
      p->buf.buf[i] = '\n';
    }
#endif
  }

#if 0
static void process_packet_fragmented(bgav_stream_t * s, bgav_packet_t * p)
  {
  /* Load the next moof atom if necessary */
  
  if(s->index_position == s->last_index_position)
    {
    
    }
  
  }
#endif

static void setup_chapter_track(bgav_demuxer_context_t * ctx, qt_trak_t * trak)
  {
  int64_t old_pos;
  int64_t pos;
  uint8_t * buffer = NULL;
  int buffer_alloc = 0;
  
  int total_chapters;

  int chunk_index;
  int stts_index;
  int stts_count;
  int stsc_index;
  int stsc_count;
  int64_t tics = 0;
  int i;
  qt_stts_t * stts;
  qt_stsc_t * stsc;
  qt_stsz_t * stsz;
  qt_stco_t * stco;
  uint32_t len;
  bgav_charset_converter_t * cnv;
  const char * charset;
  int64_t time;
  char * label = NULL;
  gavl_dictionary_t * cl;
  
  if(!(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "Chapters detected but stream is not seekable");
    return;
    }
  if(gavl_dictionary_get_chapter_list(ctx->tt->cur->metadata))
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "More than one chapter track, choosing first");
    return;
    }

  old_pos = ctx->input->position;

  if(!(charset = bgav_qt_get_charset(trak->mdia.mdhd.language)))
    charset = "bgav_unicode";

  if(charset)
    cnv = bgav_charset_converter_create(charset, BGAV_UTF8);
  else
    {
    cnv = NULL;
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "Unknown encoding for chapter names");
    }

  stts = &trak->mdia.minf.stbl.stts;
  stsc = &trak->mdia.minf.stbl.stsc;
  stsz = &trak->mdia.minf.stbl.stsz;
  stco = &trak->mdia.minf.stbl.stco;

  if(!stsz->entries)
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "No samples in chapter names");
    return;
    }
  
  total_chapters = bgav_qt_trak_samples(trak);

  cl = gavl_dictionary_add_chapter_list(ctx->tt->cur->metadata, trak->mdia.mdhd.time_scale);
  
  chunk_index = 0;
  stts_index = 0;
  stts_count = 0;
  stsc_index = 0;
  stsc_count = 0;
  
  pos = stco->entries[chunk_index];
  
  for(i = 0; i < total_chapters; i++)
    {
    time = tics;
    
    /* Increase tics */
    tics += stts->entries[stts_index].duration;
    stts_count++;
    if(stts_count >= stts->entries[stts_index].count)
      {
      stts_index++;
      stts_count = 0;
      }
    
    /* Read sample */
    if(stsz->entries[i] > buffer_alloc)
      {
      buffer_alloc = stsz->entries[i] + 128;
      buffer = realloc(buffer, buffer_alloc);
      }
    bgav_input_seek(ctx->input, pos, SEEK_SET);
    if(bgav_input_read_data(ctx->input, buffer, stsz->entries[i]) < stsz->entries[i])
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "Read error while setting up chapter list");
      return;
      }
    /* Set chapter name */

    len = GAVL_PTR_2_16BE(buffer);
    if(len)
      label = bgav_convert_string(cnv, (char*)(buffer+2), len, NULL);
    else
      label = NULL;

    gavl_chapter_list_insert(cl, i, time, label);

    if(label)
      free(label);
    
    /* Increase file position */
    if(i < total_chapters - 1)
      {
      pos += stsz->entries[i];
      stsc_count++;
      if(stsc_count >= stsc->entries[stsc_index].samples_per_chunk)
        {
        chunk_index++;
        if((stsc_index < stsc->num_entries-1) &&
           (chunk_index +1 >= stsc->entries[stsc_index+1].first_chunk))
          {
          stsc_index++;
          }
        stsc_count = 0;
        pos = stco->entries[chunk_index];
        }
      }
    }
  
  if(buffer)
    free(buffer);
  bgav_input_seek(ctx->input, old_pos, SEEK_SET);
  }

static void init_audio(bgav_demuxer_context_t * ctx,
                       qt_trak_t * trak, int index)
  {
  char language[4];
  int user_len;
  uint8_t * user_atom;
  bgav_stream_t * bg_as;
  qt_stsd_t * stsd;
  qt_sample_description_t * desc;
  
  qt_priv_t * priv = ctx->priv;
  qt_moov_t * moov = &priv->moov;
  
  stsd = &trak->mdia.minf.stbl.stsd;
  bg_as = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);

  if(trak_has_edl(trak))
    priv->has_edl = 1;
      
  bgav_qt_mdhd_get_language(&trak->mdia.mdhd,
                            language);

  gavl_dictionary_set_string(bg_as->m, GAVL_META_LANGUAGE, language);
  
  desc = &stsd->entries[0].desc;

  bg_as->priv = &priv->streams[index];

  stream_init(bg_as, trak, moov->mvhd.time_scale);
      
  bg_as->timescale = trak->mdia.mdhd.time_scale;
  bg_as->fourcc    = desc->fourcc;
  bg_as->data.audio.format->num_channels = desc->format.audio.num_channels;
  bg_as->data.audio.format->samplerate = (int)(desc->format.audio.samplerate+0.5);
  bg_as->data.audio.bits_per_sample = desc->format.audio.bits_per_sample;
  if(desc->version == 1)
    {
    if(desc->format.audio.bytes_per_frame)
      bg_as->data.audio.block_align =
        desc->format.audio.bytes_per_frame;
    }

  /* Set channel configuration (if present) */

  if(desc->format.audio.has_chan)
    {
    bgav_qt_chan_get(&desc->format.audio.chan,
                     bg_as->data.audio.format);
    }
      
  /* Set mp4 extradata */
      
  if(desc->has_esds)
    {
    /* Check for mp3on4 */
    if((desc->esds.objectTypeId == 64) &&
       (desc->esds.decoderConfigLen >= 2) &&
       (desc->esds.decoderConfig[0] >> 3 == 29))
      {
      bgav_stream_set_extradata(bg_as, desc->esds.decoderConfig,
                                desc->esds.decoderConfigLen);
      if(!init_mp3on4(bg_as))
        {
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                 "Invalid mp3on4 channel configuration");
        bg_as->fourcc = 0;
        }
      }
    /* Vorbis */
    else if ((desc->esds.objectTypeId == 0xdd) && (desc->esds.streamType == 0x15) &&
     (desc->esds.decoderConfigLen > 8) && (bg_as->fourcc == BGAV_MK_FOURCC('m','p','4','a')))
      {
      uint8_t *header_start[3];
      int header_len[3];
      int i;
      
      if(split_xiph_headers(desc->esds.decoderConfig,
                            desc->esds.decoderConfigLen,
                            30, header_start,
                            header_len))
        {
        for(i = 0; i < 3; i++)
          gavl_append_xiph_header(&bg_as->ci->codec_header,
                                  header_start[i], header_len[i]);
        bg_as->fourcc = BGAV_VORBIS;
        }
      else
        bg_as->fourcc = 0;
      }
    else
      {
      set_audio_from_esds(bg_as, &desc->esds);
      }
    }
  else if(bg_as->fourcc == BGAV_MK_FOURCC('l', 'p', 'c', 'm'))
    {
    /* Quicktime 7 lpcm: extradata contains formatSpecificFlags
       in native byte order */

    bgav_stream_set_extradata(bg_as,
                              (uint8_t*)(&desc->format.audio.formatSpecificFlags),
                              sizeof(desc->format.audio.formatSpecificFlags));
    }
  else if(desc->format.audio.has_wave)
    {
    if((desc->format.audio.wave.has_esds) &&
       (desc->format.audio.wave.esds.decoderConfigLen))
      {
      bgav_stream_set_extradata(bg_as,
                                desc->format.audio.wave.esds.decoderConfig,
                                desc->format.audio.wave.esds.decoderConfigLen);
      }
    else if((user_atom = bgav_user_atoms_find(&desc->format.audio.wave.user,
                                              BGAV_MK_FOURCC('O','V','H','S'),
                                              &user_len)))
      bgav_stream_set_extradata(bg_as, user_atom, user_len);
    else if((user_atom = bgav_user_atoms_find(&desc->format.audio.wave.user,
                                              BGAV_MK_FOURCC('a','l','a','c'),
                                              &user_len)))
      bgav_stream_set_extradata(bg_as, user_atom, user_len);
    else if((user_atom = bgav_user_atoms_find(&desc->format.audio.wave.user,
                                              BGAV_MK_FOURCC('g','l','b','l'),
                                              &user_len)))
      bgav_stream_set_extradata(bg_as, user_atom, user_len);
        
    if(!bg_as->ci->codec_header.len)
      {
      /* Raw wave atom needed by win32 decoders (QDM2) */
      bgav_stream_set_extradata(bg_as, desc->format.audio.wave.raw,
                                desc->format.audio.wave.raw_size);
      }
    }
  else if((user_atom = bgav_user_atoms_find(&desc->format.audio.user,
                                            BGAV_MK_FOURCC('a','l','a','c'),
                                            &user_len)))
    bgav_stream_set_extradata(bg_as, user_atom, user_len);
  else if(desc->has_glbl)
    {
    bgav_stream_set_extradata(bg_as, desc->glbl.data,
                              desc->glbl.size);
    }
      
  bg_as->stream_id = index;
      

  /* Check endianess */

  if(desc->format.audio.wave.has_enda &&
     desc->format.audio.wave.enda.littleEndian)
    bg_as->data.audio.endianess = BGAV_ENDIANESS_LITTLE;
  else
    bg_as->data.audio.endianess = BGAV_ENDIANESS_BIG;

  /* Signal VBR encoding (FIXME: Stream can still be cbr) */
  if(!trak->mdia.minf.stbl.stsz.sample_size)
    bg_as->container_bitrate = GAVL_BITRATE_VBR;
  
  /* Fix channels and samplerate for AMR */

  if(bg_as->fourcc == BGAV_MK_FOURCC('s','a','m','r'))
    {
    bg_as->data.audio.format->num_channels = 1;
    bg_as->data.audio.format->samplerate = 8000;
    }
  else if(bg_as->fourcc == BGAV_MK_FOURCC('s','a','w','b'))
    {
    bg_as->data.audio.format->num_channels = 1;
    bg_as->data.audio.format->samplerate = 16000;
    }
  /* AC3 in mp4 can have multiple frames per packet */
  else if(bg_as->fourcc == BGAV_MK_FOURCC('a', 'c', '-', '3'))
    bgav_stream_set_parse_full(bg_as);

  
  }

static void init_video(bgav_demuxer_context_t * ctx,
                       qt_trak_t * trak, int index)
  {
  bgav_stream_t * bg_vs;
  int skip_first_frame = 0;
  int skip_last_frame = 0;
  qt_stsd_t * stsd;
  qt_sample_description_t * desc;
  qt_priv_t * priv = ctx->priv;
  qt_moov_t * moov = &priv->moov;
  stsd = &trak->mdia.minf.stbl.stsd;
  
  if(stsd->num_entries > 1)
    {
    if((trak->mdia.minf.stbl.stsc.num_entries >= 2) &&
       (trak->mdia.minf.stbl.stsc.entries[0].samples_per_chunk == 1) &&
       (trak->mdia.minf.stbl.stsc.entries[1].sample_description_id == 2) &&
       (trak->mdia.minf.stbl.stsc.entries[1].first_chunk == 2))
      {
      skip_first_frame = 1;
      }
    if((trak->mdia.minf.stbl.stsc.num_entries >= 2) &&
       (trak->mdia.minf.stbl.stsc.entries[trak->mdia.minf.stbl.stsc.num_entries-1].samples_per_chunk == 1) &&
       (trak->mdia.minf.stbl.stsc.entries[trak->mdia.minf.stbl.stsc.num_entries-1].sample_description_id == stsd->num_entries) &&
       (trak->mdia.minf.stbl.stsc.entries[trak->mdia.minf.stbl.stsc.num_entries-1].first_chunk == trak->mdia.minf.stbl.stco.num_entries))
      {
      skip_last_frame = 1;
      }

    if(stsd->num_entries > 1 + skip_first_frame + skip_last_frame)
      return;
    }
      
  bg_vs = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);

  if(trak_has_edl(trak))
    priv->has_edl = 1;
            
  desc = &stsd->entries[skip_first_frame].desc;
  
  bg_vs->priv = &priv->streams[index];
  stream_init(bg_vs, trak, moov->mvhd.time_scale);

  if(skip_first_frame)
    priv->streams[index].skip_first_frame = 1;
  if(skip_last_frame)
    priv->streams[index].skip_last_frame = 1;
  
  bg_vs->fourcc = desc->fourcc;

  
  bg_vs->data.video.format->image_width = desc->format.video.width;
  bg_vs->data.video.format->image_height = desc->format.video.height;
  bg_vs->data.video.format->frame_width = desc->format.video.width;
  bg_vs->data.video.format->frame_height = desc->format.video.height;

  if(!trak->mdia.minf.stbl.has_stss ||
     (bgav_qt_stts_num_samples(&trak->mdia.minf.stbl.stts) ==
      trak->mdia.minf.stbl.stss.num_entries))
    bg_vs->ci->flags &= ~GAVL_COMPRESSION_HAS_P_FRAMES;
  else if(trak->mdia.minf.stbl.has_ctts)
    bg_vs->ci->flags |= GAVL_COMPRESSION_HAS_B_FRAMES;
  
  if(desc->format.video.has_pasp)
    {
    bg_vs->data.video.format->pixel_width = desc->format.video.pasp.hSpacing;
    bg_vs->data.video.format->pixel_height = desc->format.video.pasp.vSpacing;
    }
  else
    {
    bg_vs->data.video.format->pixel_width = 1;
    bg_vs->data.video.format->pixel_height = 1;
    }
  if(desc->format.video.has_fiel)
    {
    if(desc->format.video.fiel.fields == 2)
      {
      if((desc->format.video.fiel.detail == 14) ||
         (desc->format.video.fiel.detail == 6))
        bg_vs->data.video.format->interlace_mode = GAVL_INTERLACE_BOTTOM_FIRST;
      else if((desc->format.video.fiel.detail == 9) ||
              (desc->format.video.fiel.detail == 1))
        bg_vs->data.video.format->interlace_mode = GAVL_INTERLACE_TOP_FIRST;
      }
    }
  bg_vs->data.video.depth = desc->format.video.depth;
      
  bg_vs->data.video.format->timescale = trak->mdia.mdhd.time_scale;

  /* We set the timescale here, because we need it before the demuxer sets it. */

  bg_vs->timescale = trak->mdia.mdhd.time_scale;
      
  bg_vs->data.video.format->frame_duration =
    trak->mdia.minf.stbl.stts.entries[0].duration;

  /* Some quicktime movies have just a still image */
  if((trak->mdia.minf.stbl.stts.num_entries == 1) &&
     (trak->mdia.minf.stbl.stts.entries[0].count == 1))
    bg_vs->data.video.format->framerate_mode = GAVL_FRAMERATE_STILL;
  else if((trak->mdia.minf.stbl.stts.num_entries == 1) ||
          ((trak->mdia.minf.stbl.stts.num_entries == 2) &&
           (trak->mdia.minf.stbl.stts.entries[1].count == 1)))
    bg_vs->data.video.format->framerate_mode = GAVL_FRAMERATE_CONSTANT;
  else
    bg_vs->data.video.format->framerate_mode = GAVL_FRAMERATE_VARIABLE;

  if(desc->format.video.ctab_size)
    {
    bg_vs->data.video.pal = gavl_palette_create();
    gavl_palette_alloc(bg_vs->data.video.pal, desc->format.video.ctab_size);
    memcpy(bg_vs->data.video.pal->entries, desc->format.video.ctab, desc->format.video.ctab_size *
           sizeof(bg_vs->data.video.pal->entries[0]));
    }
  
  /* Set extradata suitable for Sorenson 3 */
      
  if(bg_vs->fourcc == BGAV_MK_FOURCC('S', 'V', 'Q', '3'))
    {
    if(stsd->entries[skip_first_frame].desc.format.video.has_SMI)
      bgav_stream_set_extradata(bg_vs,
                                stsd->entries[0].data,
                                stsd->entries[0].data_size);
    }
  else if((bg_vs->fourcc == BGAV_MK_FOURCC('a', 'v', 'c', '1')) &&
          (stsd->entries[0].desc.format.video.avcC_offset))
    {
    bgav_stream_set_extradata(bg_vs,
                              stsd->entries[skip_first_frame].data +
                              stsd->entries[skip_first_frame].desc.format.video.avcC_offset,
                              stsd->entries[skip_first_frame].desc.format.video.avcC_size);
    }
      
  /* Set mp4 extradata */

  if((stsd->entries[skip_first_frame].desc.has_esds) &&
     (stsd->entries[skip_first_frame].desc.esds.decoderConfigLen))
    {
    bgav_stream_set_extradata(bg_vs,
                              stsd->entries[skip_first_frame].desc.esds.decoderConfig,
                              stsd->entries[skip_first_frame].desc.esds.decoderConfigLen);
    }
  else if(desc->has_glbl)
    bgav_stream_set_extradata(bg_vs, desc->glbl.data, desc->glbl.size);
  
  bg_vs->stream_id = index;
  
  if((bg_vs->fourcc == BGAV_MK_FOURCC('m','j','p','a')) ||
     (bg_vs->fourcc == BGAV_MK_FOURCC('j','p','e','g')) ||
     (bg_vs->fourcc == BGAV_MK_FOURCC('m', 'x', '5', 'p')) ||
     (bg_vs->fourcc == BGAV_MK_FOURCC('m', 'x', '4', 'p')) ||
     (bg_vs->fourcc == BGAV_MK_FOURCC('m', 'x', '3', 'p')) ||
     (bg_vs->fourcc == BGAV_MK_FOURCC('m', 'x', '5', 'n')) ||
     (bg_vs->fourcc == BGAV_MK_FOURCC('m', 'x', '4', 'n')) ||
     (bg_vs->fourcc == BGAV_MK_FOURCC('m', 'x', '3', 'n')) ||
     (bg_vs->fourcc == BGAV_MK_FOURCC('a', 'v', 'c', '1')) ||
     bgav_check_fourcc(bg_vs->fourcc, bgav_dv_fourccs) ||
     bgav_check_fourcc(bg_vs->fourcc, bgav_png_fourccs))
    bgav_stream_set_parse_frame(bg_vs);
  
  }

static void quicktime_init(bgav_demuxer_context_t * ctx)
  {
  int i;
  bgav_stream_t * bg_ss;
  stream_priv_t * stream_priv;
  bgav_track_t * track;
  qt_priv_t * priv = ctx->priv;
  qt_moov_t * moov = &priv->moov;

  qt_trak_t * trak;
  qt_stsd_t * stsd;
  char language[4];
  
  track = ctx->tt->cur;

  //  ctx->tt->cur->duration = 0;
  
  priv->streams = calloc(moov->num_tracks, sizeof(*(priv->streams)));
  
  for(i = 0; i < moov->num_tracks; i++)
    {
    trak = &moov->tracks[i];
    stsd = &trak->mdia.minf.stbl.stsd;
    /* Audio stream */
    if(trak->mdia.minf.has_smhd)
      {
      if(!stsd->entries)
        continue;
      init_audio(ctx, trak, i);
      }
    /* Video stream */
    else if(trak->mdia.minf.has_vmhd)
      {

      if(!stsd->entries)
        continue;
      
      init_video(ctx, trak, i);
      }
    /* Quicktime subtitles */
    else if(stsd->entries &&
            (stsd->entries[0].desc.fourcc == BGAV_MK_FOURCC('t','e','x','t')))
      {
      const char * charset;
      
      if(bgav_qt_is_chapter_track(moov, trak))
      //      if(0)
        {
        setup_chapter_track(ctx, trak);
        }
      else
        {
        charset = bgav_qt_get_charset(trak->mdia.mdhd.language);
        
        bg_ss =
          bgav_track_add_text_stream(track, ctx->opt, charset);

        if(trak_has_edl(trak))
          priv->has_edl = 1;
        
        gavl_dictionary_set_string(bg_ss->m, GAVL_META_FORMAT,
                          "Quicktime subtitles");
        bg_ss->fourcc = stsd->entries[0].desc.fourcc;
        
        bgav_qt_mdhd_get_language(&trak->mdia.mdhd,
                                  language);
        
        gavl_dictionary_set_string(bg_ss->m, GAVL_META_LANGUAGE,
                          language);
        
        
        bg_ss->timescale = trak->mdia.mdhd.time_scale;
        bg_ss->stream_id = i;
        
        stream_priv = &priv->streams[i];
        bg_ss->priv = stream_priv;
        stream_init(bg_ss, &moov->tracks[i], moov->mvhd.time_scale);
        bg_ss->process_packet = process_packet_subtitle_qt;
        }
      }
    /* MPEG-4 subtitles (3gpp timed text?) */
    else if(stsd->entries &&
            (stsd->entries[0].desc.fourcc == BGAV_MK_FOURCC('t','x','3','g')))
      {
      if(bgav_qt_is_chapter_track(moov, trak))
        setup_chapter_track(ctx, trak);
      else
        {
        bg_ss =
          bgav_track_add_text_stream(track, ctx->opt, "bgav_unicode");

        if(trak_has_edl(trak))
          priv->has_edl = 1;
        
        gavl_dictionary_set_string(bg_ss->m, GAVL_META_FORMAT,
                          "3gpp subtitles");

        bg_ss->fourcc = stsd->entries[0].desc.fourcc;
        bgav_qt_mdhd_get_language(&trak->mdia.mdhd,
                                  language);
        gavl_dictionary_set_string(bg_ss->m, GAVL_META_LANGUAGE,
                          language);
                
        bg_ss->timescale = trak->mdia.mdhd.time_scale;
        bg_ss->stream_id = i;

        stream_priv = &priv->streams[i];
        bg_ss->priv = stream_priv;
        stream_init(bg_ss, &moov->tracks[i], moov->mvhd.time_scale);
        bg_ss->process_packet = process_packet_subtitle_tx3g;
        }
      }
    else if(stsd->entries &&
            (stsd->entries[0].desc.fourcc == BGAV_MK_FOURCC('t','m','c','d')))
      {
      priv->num_timecode_tracks++;
      if(priv->num_timecode_tracks > 1)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "More than one timecode track, ignoring them all");
        priv->timecode_track = NULL;
        }
      else
        priv->timecode_track = trak;
      }
    else if(stsd->entries &&
            (stsd->entries[0].desc.fourcc == BGAV_MK_FOURCC('m','p','4','s')))
      {
      uint32_t * pal;
      uint8_t * pos;
      int j;
      if(!stsd->entries[0].desc.has_esds ||
         (stsd->entries[0].desc.esds.decoderConfigLen != 64))
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Expected 64 bytes of palette data for DVD subtitles (got %d)",
                 stsd->entries[0].desc.esds.decoderConfigLen);
        continue;
        }
      bg_ss = bgav_track_add_overlay_stream(track, ctx->opt);

      gavl_dictionary_set_string(bg_ss->m, GAVL_META_FORMAT, "DVD subtitles");
      bg_ss->fourcc = stsd->entries[0].desc.fourcc;
      
      if(trak_has_edl(trak))
        priv->has_edl = 1;

      /* Set palette */
      gavl_buffer_alloc(&bg_ss->ci->codec_header, 64);
      bg_ss->ci->codec_header.len = 64;
      pal = (uint32_t*)bg_ss->ci->codec_header.buf;
      
      pos = stsd->entries[0].desc.esds.decoderConfig;
      for(j = 0; j < 16; j++)
        {
        pal[j] = GAVL_PTR_2_32BE(pos);
        pos += 4;
        }

      bg_ss->timescale = trak->mdia.mdhd.time_scale;
      bg_ss->stream_id = i;
      
      bg_ss->priv = &priv->streams[i];
      stream_init(bg_ss, &moov->tracks[i], moov->mvhd.time_scale);

      /* Set video format */
      bg_ss->data.subtitle.video.format->image_width = (int)trak->tkhd.track_width;
      bg_ss->data.subtitle.video.format->image_height = (int)trak->tkhd.track_height;

      bg_ss->data.subtitle.video.format->frame_width  = bg_ss->data.subtitle.video.format->image_width;
      bg_ss->data.subtitle.video.format->frame_height = bg_ss->data.subtitle.video.format->image_height;

      /* Bogus values */
      bg_ss->data.subtitle.video.format->pixel_width  = 1;
      bg_ss->data.subtitle.video.format->pixel_height = 1;
      
      bg_ss->data.subtitle.video.format->timescale = trak->mdia.mdhd.time_scale;
      bg_ss->data.subtitle.video.format->framerate_mode = GAVL_FRAMERATE_VARIABLE;
      }
    
    }

  set_metadata(ctx);

  if(priv->timecode_track && (ctx->tt->cur->num_video_streams == 1))
    {
    bgav_stream_t * vs = bgav_track_get_video_stream(ctx->tt->cur, 0);
    
    stream_priv = vs->priv;
    bgav_qt_init_timecodes(ctx->input, vs,
                           priv->timecode_track, vs->stats.pts_start);
    }
  
  gavl_dictionary_update_fields(ctx->tt->cur->metadata, &priv->m_emsg);
  gavl_dictionary_reset(&priv->m_emsg);
  }

static int handle_rmra(bgav_demuxer_context_t * ctx)
  {
  int i;
  qt_priv_t * priv = ctx->priv;
  
  bgav_track_t * t;

  ctx->tt = bgav_track_table_create(0);

  for(i = 0; i < priv->moov.rmra.num_rmda; i++)
    {
    if(priv->moov.rmra.rmda[i].rdrf.fourcc == BGAV_MK_FOURCC('u','r','l',' '))
      {
      char * uri;
      t = bgav_track_table_append_track(ctx->tt);

      gavl_dictionary_set_string(t->metadata, GAVL_META_MEDIA_CLASS, GAVL_META_MEDIA_CLASS_LOCATION);

      uri = bgav_input_absolute_url(ctx->input, (char*)priv->moov.rmra.rmda[i].rdrf.data_ref);
      gavl_metadata_add_src(t->metadata, GAVL_META_SRC, NULL, uri);
      free(uri);
      }
    }
  return 1;
  }

static void set_stream_edl(qt_priv_t * priv, bgav_stream_t * s,
                           gavl_dictionary_t * es)
  {
  int i;
  qt_elst_t * elst;
  stream_priv_t * sp;
  int64_t duration = 0;
  gavl_edl_segment_t * seg;
  int64_t seg_duration;
  int mdhd_ts, mvhd_ts;
  
  sp = s->priv;
  elst = &sp->trak->edts.elst;

  mvhd_ts = priv->moov.mvhd.time_scale;
  mdhd_ts = sp->trak->mdia.mdhd.time_scale;

  gavl_dictionary_set_int(gavl_stream_get_metadata_nc(es),
                          GAVL_META_STREAM_SAMPLE_TIMESCALE, mdhd_ts);

  if(!elst->num_entries)
    {
    seg = gavl_edl_add_segment(es);

    gavl_edl_segment_set(seg,
                         0,
                         0,
                         mdhd_ts,
                         0,
                         0,
                         sp->trak->mdia.mdhd.duration);
    }
  
  
  for(i = 0; i < elst->num_entries; i++)
    {
    if((int32_t)elst->table[i].media_time > -1)
      {
      seg = gavl_edl_add_segment(es);

      seg_duration = gavl_time_rescale(mvhd_ts, mdhd_ts,
                                       elst->table[i].duration);
      
      gavl_edl_segment_set(seg,
                           0,
                           0,
                           mdhd_ts,
                           elst->table[i].media_time,
                           duration,
                           seg_duration);

      gavl_edl_segment_set_speed(seg, elst->table[i].media_rate, 65536);
      
      duration += seg_duration;
      }
    else
      duration += gavl_time_rescale(mvhd_ts, mdhd_ts,
                                    elst->table[i].duration);
    }
  
  }

static void build_edl(bgav_demuxer_context_t * ctx)
  {
  gavl_dictionary_t * es;
  gavl_dictionary_t * t;
  gavl_dictionary_t * edl;
  
  qt_priv_t * priv = ctx->priv;
 
  int i;

  if(!ctx->input->location)
    return;
  
  edl = gavl_edl_create(&ctx->tt->info);

  gavl_dictionary_set_string(edl, GAVL_META_URI, ctx->input->location);
  
  t = gavl_append_track(edl, NULL);
  
  for(i = 0; i < ctx->tt->cur->num_streams; i++)
    {
    if(ctx->tt->cur->streams[i]->flags & STREAM_EXTERN)
      continue;
    
    es = gavl_track_append_stream(t, ctx->tt->cur->streams[i]->type);
    set_stream_edl(priv, ctx->tt->cur->streams[i], es);
    }
#if 0
  for(i = 0; i < ctx->tt->cur->num_video_streams; i++)
    {
    es = gavl_track_append_video_stream(t);
    set_stream_edl(priv, &ctx->tt->cur->video_streams[i], es);
    }
  for(i = 0; i < ctx->tt->cur->num_text_streams; i++)
    {
    es = gavl_track_append_text_stream(t);
    set_stream_edl(priv, &ctx->tt->cur->text_streams[i], es);
    }
  for(i = 0; i < ctx->tt->cur->num_overlay_streams; i++)
    {
    es = gavl_track_append_overlay_stream(t);
    set_stream_edl(priv, &ctx->tt->cur->overlay_streams[i], es);
    }
#endif
  }


static void fix_index(bgav_demuxer_context_t * ctx)
  {
  int i, j;
  bgav_stream_t * s;
  stream_priv_t * sp;

  //  fprintf(stderr, "Fix index\n");
  //  gavl_packet_index_dump(ctx->si);

  
  for(i = 0; i < ctx->tt->cur->num_video_streams; i++)
    {
    s = bgav_track_get_video_stream(ctx->tt->cur, i);
    if(s->fourcc == BGAV_MK_FOURCC('d','r','a','c'))
      {
      /* Remove the last sample (the sequence end code) */
      j = ctx->si->num_entries - 1;
      while(ctx->si->entries[j].stream_id != s->stream_id)
        j--;
      /* Disable this packet */
      if(ctx->si->entries[j].size == 13)
        {
        ctx->si->entries[j].stream_id = -1;
        s->stats.pts_end -= ctx->si->entries[j].duration;
        }
      /* Update last index position */
      j--;
      while(ctx->si->entries[j].stream_id != s->stream_id)
        j--;
      s->last_index_position = j;
      
      /* Check if we have a ctts. If not, we will parse the
         whole stream for getting sample accuracy */
      sp = s->priv;

      /* If the track doesn't have a ctts, we parse the complete
         stream, except if the file was created with libquicktime */
      if(!sp->trak->mdia.minf.stbl.has_ctts &&
         strncmp(sp->trak->mdia.minf.stbl.stsd.entries[0].desc.format.video.compressor_name,
                 "libquicktime", 12))
        {
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                 "Dirac stream has no ctts");
        ctx->index_mode = INDEX_MODE_SI_PARSE;
        bgav_stream_set_parse_frame(s);
        }
      }
    }
  
  }


static int open_quicktime(bgav_demuxer_context_t * ctx)
  {
  qt_atom_header_t h;
  qt_priv_t * priv = NULL;
  int have_moov = 0;
  int have_mdat = 0;
  int done = 0;
  int i;

  /* Create track */

  
  /* Read moov atom */

  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  while(!done)
    {
    if(!bgav_qt_atom_read_header(ctx->input, &h))
      {
      break;
      }
    switch(h.fourcc)
      {
      case BGAV_MK_FOURCC('m','d','a','t'):
        /* Reached the movie data atom, stop here */
        have_mdat = 1;

        priv->mdats = realloc(priv->mdats, (priv->num_mdats+1)*sizeof(*(priv->mdats)));
        memset(&priv->mdats[priv->num_mdats], 0, sizeof(*(priv->mdats)));

        set_mdat(ctx->input, &h, priv->mdats + priv->num_mdats);
        priv->num_mdats++;
        
        /* Some files have the moov atom at the end */
        if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
          {
          bgav_qt_atom_skip(ctx->input, &h);
          }

        else if(!have_moov)
          {
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                   "Non streamable file on non seekable source");
          return 0;
          }
        
        break;
      case BGAV_MK_FOURCC('m','o','o','v'):
        if(!bgav_qt_moov_read(&h, ctx->input, &priv->moov))
          {
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                   "Reading moov atom failed");
          return 0;
          }
        have_moov = 1;
        bgav_qt_atom_skip(ctx->input, &h);
        if(ctx->opt->dump_headers)
          bgav_qt_moov_dump(0, &priv->moov);
        break;
      case BGAV_MK_FOURCC('f','r','e','e'):
      case BGAV_MK_FOURCC('w','i','d','e'):
        bgav_qt_atom_skip(ctx->input, &h);
        break;
      case BGAV_MK_FOURCC('f','t','y','p'):
        if(!bgav_input_read_fourcc(ctx->input, &priv->ftyp_fourcc))
          return 0;
        bgav_qt_atom_skip(ctx->input, &h);
        break;
      case BGAV_MK_FOURCC('m','o','o','f'):
        {
        if(!bgav_qt_moof_read(&h, ctx->input, &priv->current_moof))
          {
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                   "Reading moof atom failed");
          return 0;
          }
        // bgav_qt_moof_dump(0, &priv->current_moof);
        priv->fragmented = 1;
        priv->first_moof = h.start_position;
        // fprintf(stderr, "First moof: %ld\n", h.start_position);
        priv->fragment_mdat.start = -1; // Set invalid
        }
        break;
      case BGAV_MK_FOURCC('e','m','s','g'):
        if(handle_emsg(ctx, &h, &priv->m_emsg) != GAVL_SOURCE_OK)
          return 0;
        break;
      default:
        bgav_qt_atom_skip_unknown(ctx->input, &h, 0);
      }

    if(priv->first_moof)
      done = 1;    
    else if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
      {
      if(ctx->input->position >= ctx->input->total_bytes)
        done = 1;
      }
    else if(have_moov && have_mdat)
      done = 1;
    }

  /* Check for redirecting */
  if(!have_mdat && !priv->fragmented)
    {
    if(priv->moov.has_rmra)
      {
      /* Redirector!!! */
      handle_rmra(ctx);
      return 1;
      }
    else
      return 0;
    }
  /* Initialize streams */
  ctx->tt = bgav_track_table_create(1);
  quicktime_init(ctx);

  /* Build index */
  build_index(ctx);
  
  /* No packets are found */
  if(!ctx->si)
    return 0;

  /* Quicktime is almost always sample accurate */
  
  ctx->flags |= BGAV_DEMUXER_SAMPLE_ACCURATE;
  
  /* Fix index (probably changing index mode) */
  fix_index(ctx);
  
  /* Check if we have an EDL */
  if(priv->has_edl)
    build_edl(ctx);
  
  priv->current_mdat = 0;


  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    {
    bgav_input_seek(ctx->input, ctx->si->entries[0].position, SEEK_SET);
    }
  else if(ctx->input->position < ctx->si->entries[0].position)
    bgav_input_skip(ctx->input, ctx->si->entries[0].position - ctx->input->position);

#if 0 // ?
  if(priv->fragmented)
    {
    /* Read first mdat */
    }
#endif
  
  i = 0;

  while(ftyps[i].format)
    {
    if(ftyps[i].fourcc == priv->ftyp_fourcc)
      {
      bgav_track_set_format(ctx->tt->cur, ftyps[i].format, ftyps[i].mimetype);
      break;
      }
    
    i++;
    }
  
  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;

  if(!priv->fragmented || (ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    {
    bgav_demuxer_check_interleave(ctx);
    bgav_demuxer_set_durations_from_superindex(ctx, ctx->tt->cur);
    }
  return 1;
  }

static void close_quicktime(bgav_demuxer_context_t * ctx)
  {
  qt_priv_t * priv;

  priv = ctx->priv;

  if(priv->streams)
    free(priv->streams);
    
  if(priv->mdats)
    free(priv->mdats);
  bgav_qt_moov_free(&priv->moov);
  free(ctx->priv);
  }

static gavl_source_status_t handle_emsg(bgav_demuxer_context_t * ctx,
                                        qt_atom_header_t * h, gavl_dictionary_t * m_msg)
  {
  qt_emsg_t emsg;
  qt_priv_t * priv;
  
  priv = ctx->priv;
  
  bgav_qt_emsg_init(&emsg);
  if(!bgav_qt_emsg_read(h, ctx->input, &emsg))
    return GAVL_SOURCE_EOF;

  if((emsg.message_data.len > 3) &&
     (emsg.message_data.buf[0] == 'I') &&
     (emsg.message_data.buf[1] == 'D') &&
     (emsg.message_data.buf[2] == '3'))
    {
    bgav_input_context_t * input_mem;

    input_mem = bgav_input_open_memory(emsg.message_data.buf,
                                       emsg.message_data.len);

    if(bgav_id3v2_probe(input_mem))
      {
      bgav_id3v2_tag_t * tag;
                
      if((tag = bgav_id3v2_read(input_mem)))
        {
        const char * artist;
        const char * title;
        
        //        fprintf(stderr, "Got ID3 tag\n");
        //        bgav_id3v2_dump(tag);

        if(!ctx->tt)
          {
          bgav_id3v2_2_metadata(tag, &priv->m_emsg);

          if((artist = gavl_dictionary_get_string(&priv->m_emsg, GAVL_META_ARTIST)) &&
             (title = gavl_dictionary_get_string(&priv->m_emsg, GAVL_META_TITLE)))
            gavl_dictionary_set_string_nocopy(&priv->m_emsg,
                                              GAVL_META_LABEL, gavl_sprintf("%s - %s", artist, title));
          
          //          fprintf(stderr, "Got initial metadata:\n");
          //          gavl_dictionary_dump(&priv->m_emsg, 2);
          }
        else
          {
          gavl_dictionary_t m;
          gavl_dictionary_init(&m);
          bgav_id3v2_2_metadata(tag, &m);

          if((artist = gavl_dictionary_get_string(&m, GAVL_META_ARTIST)) &&
             (title = gavl_dictionary_get_string(&m, GAVL_META_TITLE)))
            gavl_dictionary_set_string_nocopy(&m, GAVL_META_LABEL, gavl_sprintf("%s - %s", artist, title));
          
          gavl_dictionary_merge2(&m, ctx->tt->cur->metadata);
          
          if(gavl_dictionary_compare(&m, ctx->tt->cur->metadata))
            {
            //            fprintf(stderr, "Got metadata:\n");
            //            gavl_dictionary_dump(&m, 2);

            gavl_dictionary_reset(ctx->tt->cur->metadata);
            gavl_dictionary_move(ctx->tt->cur->metadata, &m);
            bgav_metadata_changed(ctx->b, ctx->tt->cur->metadata);
            }
          else
            gavl_dictionary_free(&m);
          }
        }
      }
    }
  else
    {
    bgav_qt_emsg_dump(0, &emsg);
    }
  bgav_qt_emsg_free(&emsg);
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t next_packet_quicktime(bgav_demuxer_context_t * ctx)
  {
  qt_priv_t * priv;
  priv = ctx->priv;

  if(priv->fragmented && !(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    {
    if(ctx->index_position >= ctx->si->num_entries)
      {
      qt_atom_header_t h;
      int done = 0;
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Next segment");

      while(!done)
        {
        if(!bgav_qt_atom_read_header(ctx->input, &h))
          return GAVL_SOURCE_EOF;

        switch(h.fourcc)
          {
          case BGAV_MK_FOURCC('e','m','s','g'):
            handle_emsg(ctx, &h, NULL);
            break;
          case BGAV_MK_FOURCC('m','o','o','f'):
            bgav_qt_moof_free(&priv->current_moof);
            bgav_qt_moof_read(&h, ctx->input, &priv->current_moof);
            gavl_packet_index_clear(ctx->si);
            ctx->index_position = 0;
            bgav_qt_moof_to_superindex(ctx, &priv->current_moof, ctx->si);
            break;
          case BGAV_MK_FOURCC('m','d','a','t'):
            done = 1;
            break;
          default:
            fprintf(stderr, "Ignoring atom: ");
            bgav_qt_atom_dump_header(0, &h);
            bgav_qt_atom_skip(ctx->input, &h);
            break;
          }
        
        }
      
      if(done)
        return bgav_demuxer_next_packet_si(ctx);
      else
        return GAVL_SOURCE_EOF;
      }
    else
      {
      // fprintf(stderr, "next_packet_fragmented %d %d\n", ctx->si->current_position, ctx->si->num_entries);
      return bgav_demuxer_next_packet_si(ctx);
      }
    }
  else
    {
    return bgav_demuxer_next_packet_si(ctx);
    }
  
  }

const bgav_demuxer_t bgav_demuxer_quicktime =
  {
    .probe =       probe_quicktime,
    .open =        open_quicktime,
    .next_packet = next_packet_quicktime,
    .close =       close_quicktime
  };

/* ftyps */

static const ftyp_t ftyps[] =
  {
    { BGAV_MK_FOURCC('3', 'g', '2', 'a'), "3GPP2 Media",                                   "video/3gpp2" },
    { BGAV_MK_FOURCC('3', 'g', '2', 'b'), "3GPP2 Media",                                   "video/3gpp2" },
    { BGAV_MK_FOURCC('3', 'g', '2', 'c'), "3GPP2 Media",                                   "video/3gpp2" },
    { BGAV_MK_FOURCC('3', 'g', 'e', '6'), "3GPP",                                          "video/3gpp" },
    { BGAV_MK_FOURCC('3', 'g', 'e', '7'), "3GPP",                                          "video/3gpp" },
    { BGAV_MK_FOURCC('3', 'g', 'g', '6'), "3GPP",                                          "video/3gpp" },
    { BGAV_MK_FOURCC('3', 'g', 'p', '1'), "3GPP Media",                                    "video/3gpp" },
    { BGAV_MK_FOURCC('3', 'g', 'p', '2'), "3GPP Media",                                    "video/3gpp" },
    { BGAV_MK_FOURCC('3', 'g', 'p', '3'), "3GPP Media",                                    "video/3gpp" },
    { BGAV_MK_FOURCC('3', 'g', 'p', '4'), "3GPP Media",                                    "video/3gpp" },
    { BGAV_MK_FOURCC('3', 'g', 'p', '5'), "3GPP Media",                                    "video/3gpp" },
    { BGAV_MK_FOURCC('3', 'g', 'p', '6'), "3GPP Media",                                    "video/3gpp" },
    { BGAV_MK_FOURCC('3', 'g', 'p', '6'), "3GPP Media",                                    "video/3gpp" },
    { BGAV_MK_FOURCC('3', 'g', 'p', '6'), "3GPP Media",                                    "video/3gpp" },
    { BGAV_MK_FOURCC('3', 'g', 's', '7'), "3GPP Media",                                    "video/3gpp" },
    { BGAV_MK_FOURCC('a', 'v', 'c', '1'), "MP4 Base",                                      "video/mp4" },
    { BGAV_MK_FOURCC('C', 'A', 'E', 'P'), "Canon Digital Camera",                          }, 	 
    { BGAV_MK_FOURCC('c', 'a', 'q', 'v'), "Casio Digital Camera",                          }, 	 
    { BGAV_MK_FOURCC('C', 'D', 'e', 's'), "Convergent Design",                             },	 
    { BGAV_MK_FOURCC('d', 'a', '0', 'a'), "DMB MAF",                                       },
    { BGAV_MK_FOURCC('d', 'a', '0', 'b'), "DMB MAF",                                       },
    { BGAV_MK_FOURCC('d', 'a', '1', 'a'), "DMB MAF",                                       },
    { BGAV_MK_FOURCC('d', 'a', '1', 'b'), "DMB MAF",                                       },
    { BGAV_MK_FOURCC('d', 'a', '2', 'a'), "DMB MAF",                                       },
    { BGAV_MK_FOURCC('d', 'a', '2', 'b'), "DMB MAF",                                       },
    { BGAV_MK_FOURCC('d', 'a', '3', 'a'), "DMB MAF",                                       },
    { BGAV_MK_FOURCC('d', 'a', '3', 'b'), "DMB MAF",                                       },
    { BGAV_MK_FOURCC('d', 'm', 'b', '1'), "DMB MAF",                                       },
    { BGAV_MK_FOURCC('d', 'm', 'p', 'f'), "Digital Media Project",                         },
    { BGAV_MK_FOURCC('d', 'r', 'c', '1'), "Dirac",                                         },
    { BGAV_MK_FOURCC('d', 'v', '1', 'a'), "DMB MAF",                                       },
    { BGAV_MK_FOURCC('d', 'v', '1', 'b'), "DMB MAF",                                       },
    { BGAV_MK_FOURCC('d', 'v', '2', 'a'), "DMB MAF",                                       },
    { BGAV_MK_FOURCC('d', 'v', '2', 'b'), "DMB MAF",                                       },
    { BGAV_MK_FOURCC('d', 'v', '3', 'a'), "DMB MAF",                                       },
    { BGAV_MK_FOURCC('d', 'v', '3', 'b'), "DMB MAF",                                       },
    { BGAV_MK_FOURCC('d', 'v', 'r', '1'), "DVB",                                           "video/vnd.dvb.file" },
    { BGAV_MK_FOURCC('d', 'v', 't', '1'), "DVB",                                           "video/vnd.dvb.file" },
    { BGAV_MK_FOURCC('F', '4', 'V', ' '), "Video for Adobe Flash Player",                  "video/mp4" },
    { BGAV_MK_FOURCC('F', '4', 'P', ' '), "Protected Video for Adobe Flash Player",        "video/mp4" },
    { BGAV_MK_FOURCC('F', '4', 'A', ' '), "Audio for Adobe Flash Player",                  "audio/mp4" },
    { BGAV_MK_FOURCC('F', '4', 'B', ' '), "Audio Book for Adobe Flash Player",             "audio/mp4" }, 	 
    { BGAV_MK_FOURCC('i', 's', 'c', '2'), "ISMACryp 2.0",                                  }, 	 
    { BGAV_MK_FOURCC('i', 's', 'o', '2'), "MP4 Base Media v2",                             "video/mp4" },
    { BGAV_MK_FOURCC('i', 's', 'o', 'm'), "MP4 Base Media v1",                             "video/mp4" },
    { BGAV_MK_FOURCC('J', 'P', '2', ' '), "JPEG 2000 Image",                               "image/jp2" }, 	 
      //    { BGAV_MK_FOURCC('JP20'), "Unknown, from GPAC samples (prob non-existent) 	GPAC 	NO 	unknown 	[4]
    { BGAV_MK_FOURCC('j', 'p', 'm', ' '), "JPEG 2000 Compound Image",                      "image/jpm" }, 
    { BGAV_MK_FOURCC('j', 'p', 'x', ' '), "JPEG 2000 w/ extensions",                       "image/jpx" }, 	 
    { BGAV_MK_FOURCC('K', 'D', 'D', 'I'), "3GPP2 EZmovie for KDDI 3G cellphones",          "video/3gpp2" }, 	 
    { BGAV_MK_FOURCC('M', '4', 'A', ' '), "Apple iTunes AAC-LC Audio",                     "audio/x-m4a" },
    { BGAV_MK_FOURCC('M', '4', 'B', ' '), "Apple iTunes AAC-LC Audio Book",                "audio/mp4" },
    { BGAV_MK_FOURCC('M', '4', 'P', ' '), "Apple iTunes AAC-LC AES Protected Audio",       "audio/mp4" },
    { BGAV_MK_FOURCC('M', '4', 'V', ' '), "Apple iTunes Video",                            "video/x-m4v" },
    { BGAV_MK_FOURCC('M', '4', 'V', 'H'), "Apple TV",                                      "video/x-m4v"  },	 
    { BGAV_MK_FOURCC('M', '4', 'V', 'P'), "Apple iPhone",                                  "video/x-m4v"  },	 
    { BGAV_MK_FOURCC('m', 'j', '2', 's'), "Motion JPEG 2000",                              "video/mj2" 	 },
    { BGAV_MK_FOURCC('m', 'j', 'p', '2'), "Motion JPEG 2000",                              "video/mj2" 	 },
    { BGAV_MK_FOURCC('m', 'm', 'p', '4'), "MPEG-4/3GPP Mobile Profile",                    "video/mp4" },	
    { BGAV_MK_FOURCC('m', 'p', '2', '1'), "MPEG-21"      	                           },
    { BGAV_MK_FOURCC('m', 'p', '4', '1'), "MP4 v1",                                        "video/mp4" },
    { BGAV_MK_FOURCC('m', 'p', '4', '2'), "MP4 v2",                                        "video/mp4" }, 	
    { BGAV_MK_FOURCC('m', 'p', '7', '1'), "MP4 w/ MPEG-7 Metadata"      	           },
    { BGAV_MK_FOURCC('M', 'P', 'P', 'I'), "Photo Player, MAF",                             },	
    { BGAV_MK_FOURCC('m', 'q', 't', ' '), "Sony / Mobile QuickTime",                        "video/quicktime" }, 	
    { BGAV_MK_FOURCC('M', 'S', 'N', 'V'), "MPEG-4 (.MP4) for SonyPSP",                     "audio/mp4" },
    { BGAV_MK_FOURCC('N', 'D', 'A', 'S'), "MP4 v2 [ISO 14496-14] Nero Digital AAC Audio",  "audio/mp4" },
    { BGAV_MK_FOURCC('N', 'D', 'S', 'C'), "MPEG-4 (.MP4) Nero Cinema Profile",             "video/mp4" },
    { BGAV_MK_FOURCC('N', 'D', 'S', 'H'), "MPEG-4 (.MP4) Nero HDTV Profile",               "video/mp4" }, 	
    { BGAV_MK_FOURCC('N', 'D', 'S', 'M'), "MPEG-4 (.MP4) Nero Mobile Profile",             "video/mp4" },
    { BGAV_MK_FOURCC('N', 'D', 'S', 'P'), "MPEG-4 (.MP4) Nero Portable Profile",           "video/mp4" },
    { BGAV_MK_FOURCC('N', 'D', 'S', 'S'), "MPEG-4 (.MP4) Nero Standard Profile",           "video/mp4" },
    { BGAV_MK_FOURCC('N', 'D', 'X', 'C'), "H.264/MPEG-4 AVC (.MP4) Nero Cinema Profile",   "video/mp4" },
    { BGAV_MK_FOURCC('N', 'D', 'X', 'H'), "H.264/MPEG-4 AVC (.MP4) Nero HDTV Profile",     "video/mp4" },
    { BGAV_MK_FOURCC('N', 'D', 'X', 'M'), "H.264/MPEG-4 AVC (.MP4) Nero Mobile Profile",   "video/mp4" },
    { BGAV_MK_FOURCC('N', 'D', 'X', 'P'), "H.264/MPEG-4 AVC (.MP4) Nero Portable Profile", "video/mp4" },
    { BGAV_MK_FOURCC('N', 'D', 'X', 'S'), "H.264/MPEG-4 AVC (.MP4) Nero Standard Profile", "video/mp4" },
    { BGAV_MK_FOURCC('o', 'd', 'c', 'f'), "OMA DCF DRM Format 2.0"                         },
    { BGAV_MK_FOURCC('o', 'p', 'f', '2'), "OMA PDCF DRM Format 2.1" 	                   },
    { BGAV_MK_FOURCC('o', 'p', 'x', '2'), "OMA PDCF DRM + XBS extensions" 	           },
    { BGAV_MK_FOURCC('p', 'a', 'n', 'a'), "Panasonic Digital Camera" 	                   }, 
    { BGAV_MK_FOURCC('q', 't', ' ', ' '), "Apple QuickTime",                               "video/quicktime" },
    { BGAV_MK_FOURCC('R', 'O', 'S', 'S'), "Ross Video"                                     },
    { BGAV_MK_FOURCC('s', 'd', 'v', ' '), "SD Memory Card Video"                           },
    { BGAV_MK_FOURCC('s', 's', 'c', '1'), "Samsung stereoscopic, single stream"            },
    { BGAV_MK_FOURCC('s', 's', 'c', '2'), "Samsung stereoscopic, dual stream"              },
    { 0,                                  "Apple QuickTime",                               "video/quicktime" },
    { /* */ },
  };
