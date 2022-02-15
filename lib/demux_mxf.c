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
#include <mxf.h>

/* Based on the ffmpeg MXF demuxer with lots of changes */

#define LOG_DOMAIN "mxf"

#define FRAME_WRAPPED      0
#define CLIP_WRAPPED_CBR   1
#define CLIP_WRAPPED_PARSE 2 /* Unsupported for now */


static void build_edl_mxf(bgav_demuxer_context_t * ctx);

/* TODO: Find a better way */
static int probe_mxf(bgav_input_context_t * input)
  {
  char * pos;
  if(input->filename)
    {
    pos = strrchr(input->filename, '.');
    if(!pos)
      return 0;
    if(!strcasecmp(pos, ".mxf"))
      return 1;
    }
  return 0;
  }

typedef struct
  {
  int64_t pts_counter;
  int eof;
  /* Constant frame size for clip-wrapped streams, which are TRUE CBR */
  int frame_size;
  //  int wrap_mode;
  
  int64_t start;
  int64_t length;
  int64_t pos;
  
  int (*next_packet)(bgav_demuxer_context_t * ctx, bgav_stream_t * s);

  int track_id;
  
  } stream_priv_t;

typedef struct
  {
  mxf_file_t mxf;
  } mxf_t;

static void set_pts(bgav_stream_t * s, stream_priv_t * sp,
                    bgav_packet_t * p)
  {
  if(s->type == GAVL_STREAM_VIDEO)
    {
    p->pts = sp->pts_counter;
    p->duration = s->data.video.format->frame_duration;
    sp->pts_counter += p->duration;
    if(sp->frame_size)
      PACKET_SET_KEYFRAME(p);
    }
  else if(s->type == GAVL_STREAM_AUDIO)
    {
    p->pts = sp->pts_counter;
    if(s->data.audio.block_align)
      p->duration = p->data_size / s->data.audio.block_align;
    sp->pts_counter += p->duration;
    PACKET_SET_KEYFRAME(p);
    }
  }

static int next_packet_clip_wrapped_const(bgav_demuxer_context_t * ctx, bgav_stream_t * s)
  {
  int bytes_to_read;
  mxf_klv_t klv;
  bgav_stream_t * tmp_stream = NULL;
  stream_priv_t * sp;
  bgav_packet_t * p;
  mxf_t * priv;
  priv = ctx->priv;
  sp = s->priv;

  /* Need the KLV packet for this stream */
  if(!sp->start)
    {
    bgav_input_seek(ctx->input, ctx->data_start, SEEK_SET);
    while(1)
      {
      if(!bgav_mxf_klv_read(ctx->input, &klv))
        return 0;

      tmp_stream = bgav_mxf_find_stream(&priv->mxf, ctx, klv.key);
      if(tmp_stream == s)
        {
        sp->start  = ctx->input->position;
        sp->pos    = ctx->input->position;
        sp->length = klv.length;
        break;
        }
      else
        bgav_input_skip(ctx->input, klv.length);
      }
    }
  /* No packets */
  if(!sp->start)
    return 0;
  /* Out of data */
  if(sp->pos >= sp->start + sp->length)
    return 0;

  if(ctx->input->position != sp->pos)
    bgav_input_seek(ctx->input, sp->pos, SEEK_SET);
  
  bytes_to_read = sp->frame_size;
  if(sp->pos + bytes_to_read >= sp->start + sp->length)
    bytes_to_read = sp->start + sp->length - sp->pos;

  p = bgav_stream_get_packet_write(s);
  p->position = ctx->input->position;
  bgav_packet_alloc(p, bytes_to_read);
  p->data_size = bgav_input_read_data(ctx->input, p->data, bytes_to_read);

  sp->pos += bytes_to_read;

  if(p->data_size < bytes_to_read)
    return 0;
  
  set_pts(s, sp, p);
  bgav_stream_done_packet_write(s, p);
  return 1;
  }

static int process_packet_frame_wrapped(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;
  bgav_packet_t * p;
  mxf_klv_t klv;
  int64_t position;
  stream_priv_t * sp;
  mxf_t * priv;

  priv = ctx->priv;
  position = ctx->input->position;

  if(position > ((partition_t*)(ctx->tt->cur->priv))->end_pos)
    return 0;
  
  if(!bgav_mxf_klv_read(ctx->input, &klv))
    return 0;
  s = bgav_mxf_find_stream(&priv->mxf, ctx, klv.key);
  if(!s)
    {
    bgav_input_skip(ctx->input, klv.length);
    return 1;
    }
  
  sp = s->priv;

  p = bgav_stream_get_packet_write(s);
  p->position = position;
  /* check for 8 channels AES3 element */
  if(klv.key[12] == 0x06 && klv.key[13] == 0x01 && klv.key[14] == 0x10)
    {
    int i, j;
    int32_t sample;
    int64_t end_pos = ctx->input->position + klv.length;
    int num_samples;
    uint8_t * ptr;

    /* Skip  SMPTE 331M header */
    bgav_input_skip(ctx->input, 4);

    num_samples = (end_pos - ctx->input->position) / 32; /* 8 channels*4 bytes/channel */
    
    bgav_packet_alloc(p, num_samples * s->data.audio.block_align);
    ptr = p->data;
    p->data_size = 0;
    for(i = 0; i < num_samples; i++)
      {
      for(j = 0; j < s->data.audio.format->num_channels; j++)
        {
        if(!bgav_input_read_32_le(ctx->input, (uint32_t*)(&sample)))
          return 0;
        if(s->data.audio.bits_per_sample == 24)
          {
          sample = (sample >> 4) & 0xffffff;
          GAVL_24LE_2_PTR(sample, ptr);
          ptr += 3;
          p->data_size += 3;
          }
        else if(s->data.audio.bits_per_sample == 16)
          {
          sample = (sample >> 12) & 0xffff;
          GAVL_16LE_2_PTR(sample, ptr);
          ptr += 2;
          p->data_size += 2;
          }
        }
      bgav_input_skip(ctx->input, 32 - s->data.audio.format->num_channels * 4);
      }
    p->pts = sp->pts_counter;
    p->duration = num_samples;
    sp->pts_counter += num_samples;
    }
  else
    {
    bgav_packet_alloc(p, klv.length);
    if((p->data_size = bgav_input_read_data(ctx->input, p->data, klv.length)) < klv.length)
      return 0;

    set_pts(s, sp, p);
    }
  
  if(p)
    {
#if 0
    if(s->type == GAVL_STREAM_AUDIO)
      {
      fprintf(stderr, "Got audio packet\n");
      bgav_packet_dump(p);
      }
    //    if(s->type == GAVL_STREAM_VIDEO)
    //      fprintf(stderr, "Got video packet\n");
#endif
    bgav_stream_done_packet_write(s, p);
    }
  return 1;
  }

static int next_packet_frame_wrapped(bgav_demuxer_context_t * ctx, bgav_stream_t * dummy)
  {
  if(ctx->next_packet_pos)
    {
    int ret = 0;
    while(1)
      {
      if(!process_packet_frame_wrapped(ctx))
        return ret;
      else
        ret = 1;
      if(ctx->input->position >= ctx->next_packet_pos)
        return ret;
      }
    }
  else
    {
    return process_packet_frame_wrapped(ctx);
    }
  return 0;
  }


static void init_stream_common(bgav_demuxer_context_t * ctx, bgav_stream_t * s,
                               mxf_track_t * st, mxf_descriptor_t * sd,
                               uint32_t fourcc)
  {
  stream_priv_t * sp;
  mxf_t * priv;
  /* Common initialization */
  priv = ctx->priv;
  sp = calloc(1, sizeof(*priv));
  s->priv = sp;
  s->fourcc = fourcc;

  sp->track_id = st->track_id;
  
  /* Detect wrap mode */

  if(priv->mxf.index_segments &&
     priv->mxf.index_segments[0]->edit_unit_byte_count)
    sp->frame_size = priv->mxf.index_segments[0]->edit_unit_byte_count;

#if 0  
  /* Hack: This makes P2 audio files clip wrapped */
  if(!sd->clip_wrapped &&
     (((mxf_preface_t*)(priv->mxf.header.preface))->operational_pattern == MXF_OP_ATOM) &&
     sp->frame_size &&
     (sp->frame_size < st->max_packet_size) &&
     (st->num_packets == 1) && (s->type == GAVL_STREAM_AUDIO))
    sd->clip_wrapped = 1;
#endif
  
  switch(sd->wrapping_type)
    {
    case WRAP_FRAME:
      sp->next_packet = next_packet_frame_wrapped;
      break;
    case WRAP_CLIP:
      if(sp->frame_size)
        sp->next_packet = next_packet_clip_wrapped_const;
      else
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Clip wrapped tracks with nonconstant framesize not supported");
      break;
    case WRAP_CUSTOM:
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Custom wrapping not supported");
      break;
    case WRAP_UNKNOWN:
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Unknown wrapping");
    }
  
  }

static const uint32_t pcm_fourccs[] =
  {
    BGAV_MK_FOURCC('s', 'o', 'w', 't'),
    BGAV_MK_FOURCC('t', 'w', 'o', 's'),
    BGAV_MK_FOURCC('a', 'l', 'a', 'w'),
  };

static int is_pcm(uint32_t fourcc)
  {
  int i;
  for(i = 0; i < sizeof(pcm_fourccs)/sizeof(pcm_fourccs[0]); i++)
    {
    if(fourcc == pcm_fourccs[i])
      return 1;
    }
  return 0;
  }

static void init_audio_stream(bgav_demuxer_context_t * ctx, bgav_stream_t * s,
                              mxf_track_t * st, mxf_descriptor_t * sd,
                              uint32_t fourcc)
  {
  stream_priv_t * priv;
  init_stream_common(ctx, s, st, sd, fourcc);
  priv = s->priv;
  if(sd->sample_rate_num % sd->sample_rate_den)
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Rounding fractional audio samplerate");
  s->data.audio.format->samplerate = sd->sample_rate_num / sd->sample_rate_den;
  s->data.audio.format->num_channels = sd->channels;
  s->data.audio.bits_per_sample = sd->bits_per_sample;

  if(is_pcm(fourcc))
    {
    s->data.audio.block_align = sd->channels * ((sd->bits_per_sample+7)/8);
    /* Make a more sensible frame size for clip wrapped PCM streams */
    if(priv->frame_size == s->data.audio.block_align)
      priv->frame_size = 1024 * s->data.audio.block_align; /* 1024 samples */
    s->index_mode = INDEX_MODE_SIMPLE;
    }
  }

static void init_timecode_track(bgav_stream_t * vs, mxf_track_t * timecode_track)
  {
  mxf_sequence_t * seq;
  mxf_timecode_component_t * tc;
  
  seq = (mxf_sequence_t *)(timecode_track->sequence);
  tc = (mxf_timecode_component_t *)(seq->structural_components[0]);

  /* Timecode format */
  vs->data.video.format->timecode_format.int_framerate =
    tc->rounded_timecode_base;

  if(tc->drop_frame)
    vs->data.video.format->timecode_format.flags |= GAVL_TIMECODE_DROP_FRAME;
  
  /* First timecode only */
  vs->timecode_table = bgav_timecode_table_create(1);
  vs->timecode_table->entries[0].pts = 0;
  vs->timecode_table->entries[0].timecode =
    gavl_timecode_from_framecount(&vs->data.video.format->timecode_format,
                                  tc->start_timecode);
  }

static void init_video_stream(bgav_demuxer_context_t * ctx, bgav_stream_t * s,
                              mxf_track_t * st, mxf_descriptor_t * sd,
                              uint32_t fourcc)
  {
  init_stream_common(ctx, s, st, sd, fourcc);
  
  if((s->fourcc == BGAV_MK_FOURCC('m','p','g','v')) ||
     (s->fourcc == BGAV_MK_FOURCC('m','x','5','p')) ||
     (s->fourcc == BGAV_MK_FOURCC('m','x','4','p')) ||
     (s->fourcc == BGAV_MK_FOURCC('m','x','3','p')) ||
     (s->fourcc == BGAV_MK_FOURCC('m','x','5','n')) ||
     (s->fourcc == BGAV_MK_FOURCC('m','x','4','n')) ||
     (s->fourcc == BGAV_MK_FOURCC('m','x','3','n')))
    {
    s->index_mode = INDEX_MODE_SIMPLE;
    s->flags |= STREAM_PARSE_FRAME;
    }
  else if(s->fourcc == BGAV_MK_FOURCC('m','p','4','v'))
    {
    s->index_mode = INDEX_MODE_SIMPLE;
    s->flags |= STREAM_PARSE_FULL; 
    }
  else
    s->index_mode = INDEX_MODE_SIMPLE;
  
  s->data.video.format->timescale      = sd->sample_rate_num;
  s->data.video.format->frame_duration = sd->sample_rate_den;
  s->data.video.format->image_width    = sd->width;
  s->data.video.format->image_height   = sd->height;
  s->data.video.format->frame_width    = sd->width;
  s->data.video.format->frame_height   = sd->height;

  if(sd->ext_size)
    bgav_stream_set_extradata(s, sd->ext_data, sd->ext_size);
  
  /* Todo: Aspect ratio */
  s->data.video.format->pixel_width    = 1;
  s->data.video.format->pixel_height   = 1;
  }


static void cleanup_stream_mxf(bgav_stream_t * s)
  {
  if(s->priv) free(s->priv);
  }

static void
handle_source_track_simple(bgav_demuxer_context_t * ctx,
                           mxf_package_t * sp, mxf_track_t * t,
                           bgav_track_t * bt)
  {
  mxf_descriptor_t * sd;
  mxf_sequence_t * ss;
  mxf_t * priv;
  uint32_t fourcc;
  bgav_stream_t * s = NULL;
  
  priv = ctx->priv;
  
  ss = (mxf_sequence_t*)(t->sequence);
  
  if(!ss)
    return;
  
  if(ss->is_timecode)
    {
    /* TODO */
    return;
    }
  else if(ss->stream_type == GAVL_STREAM_NONE)
    {
    return;
    }
  else
    {
    if(!ss->structural_components)
      return;
    
    if(ss->structural_components[0]->type != MXF_TYPE_SOURCE_CLIP)
      return;
    
    sd = bgav_mxf_get_source_descriptor(&priv->mxf, sp, t);
    if(!sd)
      {
      return;
      }
    
    if(ss->stream_type == GAVL_STREAM_AUDIO)
      {

      fourcc = bgav_mxf_get_audio_fourcc(sd);
      if(!fourcc)
        return;
      s = bgav_track_add_audio_stream(bt, ctx->opt);
      s->cleanup = cleanup_stream_mxf;
      init_audio_stream(ctx, s, t, sd, fourcc);
      }
    else if(ss->stream_type == GAVL_STREAM_VIDEO)
      {
      fourcc = bgav_mxf_get_video_fourcc(sd);
      if(!fourcc)
        return;
      s = bgav_track_add_video_stream(bt, ctx->opt);
      s->cleanup = cleanup_stream_mxf;
      init_video_stream(ctx, s, t, sd, fourcc);

      if(sp->timecode_track && (sp->num_timecode_tracks == 1))
        init_timecode_track(s, (mxf_track_t*)sp->timecode_track);
      }

    /* Should not happen */
    if(!s)
      return;

    s->stream_id =
      t->track_number[0] << 24 |
      t->track_number[1] << 16 |
      t->track_number[2] <<  8 |
      t->track_number[3];
    }
  return;
  }

static int get_body_sid(mxf_file_t * f, mxf_package_t * p, uint32_t * ret)
  {
  int i;
  mxf_essence_container_data_t * ec;
  mxf_content_storage_t * cs;
  
  cs = (mxf_content_storage_t*)(((mxf_preface_t*)(f->header.preface))->content_storage);
  
  for(i = 0; i < cs->num_essence_container_data_refs; i++)
    {
    if(!cs->essence_containers[i])
      continue;
    ec = (mxf_essence_container_data_t*)cs->essence_containers[i];

    if(p == (mxf_package_t*)ec->linked_package)
      {
      *ret = ec->body_sid;
      return 1;
      }
    }
  return 0;
  }

static partition_t * get_body_partition(mxf_file_t * f, mxf_package_t * p)
  {
  int i;
  uint32_t body_sid;
  if(!get_body_sid(f, p, &body_sid))
    return NULL;
  if(f->header.p.body_sid == body_sid)
    return &f->header;

  for(i = 0; i < f->num_body_partitions; i++)
    {
    if(f->body_partitions[i].p.body_sid == body_sid)
      return &f->body_partitions[i];
    }
  return NULL;
  }

/* Simple initialization */
static int init_simple(bgav_demuxer_context_t * ctx)
  {
  mxf_t * priv;
  int i, j, num_tracks = 0;
  mxf_package_t * sp = NULL;
  int index = 0;
  
  priv = ctx->priv;
  /* We simply open the Source packages */
  
  for(i = 0; i < priv->mxf.header.num_metadata; i++)
    {
    if(priv->mxf.header.metadata[i]->type == MXF_TYPE_SOURCE_PACKAGE)
      num_tracks++;
    }

  ctx->tt = bgav_track_table_create(num_tracks);
  
  for(i = 0; i < priv->mxf.header.num_metadata; i++)
    {
    if(priv->mxf.header.metadata[i]->type == MXF_TYPE_SOURCE_PACKAGE)
      {
      sp = (mxf_package_t*)(priv->mxf.header.metadata[i]);

      ctx->tt->tracks[index]->priv = get_body_partition(&priv->mxf, sp);
      
      if(!ctx->tt->tracks[index]->priv)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Couldn't find partition for source package %d", index);
        return 0;
        }
      /* Loop over tracks */
      for(j = 0; j < sp->num_track_refs; j++)
        {
        handle_source_track_simple(ctx, sp, (mxf_track_t*)sp->tracks[j], ctx->tt->tracks[index]);
        }
      index++;
      }
    }
  
  return 1;
  }

static int open_mxf(bgav_demuxer_context_t * ctx)
  {
  int i;
  mxf_t * priv;
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  if(!bgav_mxf_file_read(ctx->input, &priv->mxf))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Parsing MXF file failed, please report");
    return 0;
    }
  if(ctx->opt->dump_headers)
    bgav_mxf_file_dump(&priv->mxf);
  
  if(priv->mxf.header.max_source_sequence_components == 1)
    {
    if(!init_simple(ctx))
      return 0;
    }
  else
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Unsupported MXF type, please report");
    return 0;
    }

  if(priv->mxf.header.max_material_sequence_components >= 1)
    build_edl_mxf(ctx);    
  
  ctx->data_start = priv->mxf.data_start;
  ctx->flags |= BGAV_DEMUXER_HAS_DATA_START;

  /* Decide index mode */
  ctx->index_mode = INDEX_MODE_MIXED;
  for(i = 0; i < ctx->tt->cur->num_streams; i++)
    {
    if(ctx->tt->cur->streams[i].index_mode == INDEX_MODE_NONE)
      {
      ctx->index_mode = INDEX_MODE_NONE;
      break;
      }
    }

  bgav_track_set_format(ctx->tt->cur, "MXF", NULL);
  
  
  return 1;
  }

static bgav_stream_t * next_stream(bgav_stream_t * s, int num)
  {
  stream_priv_t * sp;
  int i;
  for(i = 0; i < num; i++)
    {
    if(s[i].flags & STREAM_EXTERN)
      continue;
    
    if(s[i].action != BGAV_STREAM_MUTE)
      {
      sp = s[i].priv;
      if(!sp->eof)
        return s + i;
      }
    }
  return NULL;
  }

static int next_packet_mxf(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;
  stream_priv_t * sp;
  
  s = ctx->request_stream;
  if(s)
    sp = s->priv;
  else
    sp = NULL;
  
  while(!s || !sp->next_packet(ctx, s))
    {
    if(sp)
      sp->eof = 1;

    if(!(s = next_stream(ctx->tt->cur->streams, ctx->tt->cur->num_streams)))
      return 0;
    sp = s->priv;
    }
  return 1;
  }

static void seek_mxf(bgav_demuxer_context_t * ctx, int64_t time,
                    int scale)
  {
  
  }

#if 1

static void reset_streams(bgav_stream_t * streams, int num)
  {
  int j;
  stream_priv_t * sp;

  for(j = 0; j < num; j++)
    {
    if(streams[j].flags & STREAM_EXTERN)
      continue;
    
    sp = streams[j].priv;
    if(sp)
      {
      sp->pos = sp->start;
      sp->eof = 0;
      sp->pts_counter = 0;
      }
    }
  }

static int select_track_mxf(bgav_demuxer_context_t * ctx, int track)
  {
  bgav_input_seek(ctx->input, ((partition_t*)(ctx->tt->cur->priv))->start_pos, SEEK_SET);

  reset_streams(ctx->tt->cur->streams, ctx->tt->cur->num_streams);
  
  return 1;
  }
#endif


static void close_mxf(bgav_demuxer_context_t * ctx)
  {
  mxf_t * priv;
  priv = ctx->priv;
  bgav_mxf_file_free(&priv->mxf);
  free(priv);
  }

static void resync_mxf(bgav_demuxer_context_t * ctx, bgav_stream_t * s)
  {
  stream_priv_t * sp = s->priv;
  sp->pts_counter = STREAM_GET_SYNC(s);
  
  if(s->file_index)
    sp->pos = s->file_index->entries[s->index_position].position;
  
  }

const bgav_demuxer_t bgav_demuxer_mxf =
  {
    .probe        = probe_mxf,
    .open         = open_mxf,
    .select_track = select_track_mxf,
    .next_packet  = next_packet_mxf,
    .resync       = resync_mxf,
    .seek         = seek_mxf,
    .close        = close_mxf
  };

static int find_source_stream(bgav_stream_t * streams, int num, int track_id)
  {
  int i;
  stream_priv_t * p;
  
  for(i = 0; i < num; i++)
    {
    if(streams[i].flags & STREAM_EXTERN)
      continue;
    
    switch(streams[i].type)
      {
      case GAVL_STREAM_AUDIO:
      case GAVL_STREAM_VIDEO:
      case GAVL_STREAM_TEXT:
      case GAVL_STREAM_OVERLAY:
        break;
      default:
        continue;
        break;
      }
    
    p = streams[i].priv;
    if(p->track_id == track_id)
      return i;
    }
  return -1;
  }

static int get_source_stream(bgav_track_table_t * tt,
                             mxf_file_t * f,
                             int * track_index, int * stream_index,
                             gavl_stream_type_t type,
                             mxf_package_t * sp, int track_id)
  {
  int i;
  mxf_content_storage_t * cs;
  int found;

  cs = (mxf_content_storage_t*)(((mxf_preface_t*)(f->header.preface))->content_storage);
  found = 0;
  *track_index = 0;
  
  for(i = 0; i < cs->num_package_refs; i++)
    {
    if(cs->packages[i]->type == MXF_TYPE_SOURCE_PACKAGE)
      {
      if(cs->packages[i] == (mxf_metadata_t*)sp)
        {
        found = 1;
        break;
        }
      else
        (*track_index)++;
      }
    }

  if(!found)
    return 0;
  
  *stream_index = find_source_stream(tt->tracks[*track_index]->streams,
                                     tt->tracks[*track_index]->num_streams,
                                     track_id);
  if(*stream_index < 0)
    return 0;
  return 1;
  }

static void handle_material_track(bgav_demuxer_context_t * ctx, mxf_package_t * p,
                                  mxf_track_t * mt, gavl_dictionary_t * et)
  {
  int i;
  mxf_sequence_t * ss;
  mxf_source_clip_t * sc;
  mxf_t * priv;
  gavl_dictionary_t * es = NULL;
  int track_index, stream_index = 0;
  gavl_edl_segment_t * seg;
  int64_t duration = 0;
  priv = ctx->priv;

  if(!mt)
    return;
  
  ss = (mxf_sequence_t*)(mt->sequence);
  
  if(!ss)
    return;
  
  if(ss->is_timecode)
    {
    /* TODO */
    return;
    }
  else if(ss->stream_type == GAVL_STREAM_NONE)
    {
    return;
    }
  else
    {
    if(!ss->structural_components ||
       (ss->structural_components[0]->type != MXF_TYPE_SOURCE_CLIP))
      return;

    es = NULL;
    
    switch(ss->stream_type)
      {
      case GAVL_STREAM_AUDIO:
        es = gavl_track_append_audio_stream(et);
        break;
      case GAVL_STREAM_VIDEO:
        es = gavl_track_append_video_stream(et);
        break;
      case GAVL_STREAM_TEXT:
      case GAVL_STREAM_OVERLAY:
      case GAVL_STREAM_NONE:
      case GAVL_STREAM_MSG:
        break;
      }
    if(!es)
      return;
    
    gavl_dictionary_set_int(gavl_stream_get_metadata_nc(es),
                            GAVL_META_STREAM_SAMPLE_TIMESCALE, mt->edit_rate_num);
    
    for(i = 0; i < ss->num_structural_component_refs; i++)
      {
      sc = (mxf_source_clip_t*)(ss->structural_components[i]);
      
      /*  */
      
      if(get_source_stream(ctx->tt,
                            &priv->mxf,
                            &track_index, &stream_index,
                            ss->stream_type,
                            (mxf_package_t*)sc->source_package,
                            sc->source_track_id))
        {
        seg = gavl_edl_add_segment(es);

        gavl_edl_segment_set(seg,
                             track_index,
                             stream_index,
                             mt->edit_rate_num,
                             sc->start_position * mt->edit_rate_den,
                             duration,
                             sc->duration * mt->edit_rate_den);
        }
      
      duration += sc->duration * mt->edit_rate_den;
      }
    }
  }

static void build_edl_mxf(bgav_demuxer_context_t * ctx)
  {
  mxf_t * priv;
  int i, j;
  mxf_package_t * sp = NULL;
  gavl_dictionary_t * t;
  gavl_dictionary_t * edl;
  
  priv = ctx->priv;

  if(!ctx->input->filename)
    return;
  
  edl = gavl_edl_create(&ctx->tt->info);
  gavl_dictionary_set_string(edl, GAVL_META_URI, ctx->input->filename);
  
  /* We simply open the Material packages */
  
  for(i = 0; i < priv->mxf.header.num_metadata; i++)
    {
    if(priv->mxf.header.metadata[i]->type == MXF_TYPE_MATERIAL_PACKAGE)
      {
      t = gavl_append_track(edl, NULL);
      sp = (mxf_package_t*)(priv->mxf.header.metadata[i]);
      
      /* Loop over tracks */
      for(j = 0; j < sp->num_track_refs; j++)
        {
        handle_material_track(ctx, sp, (mxf_track_t*)sp->tracks[j], t);
        }
      }
    }
  
  }
