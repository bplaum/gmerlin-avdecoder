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
#include <string.h>
#include <stdio.h>

#define PKT_MAP   0xbc
#define PKT_MEDIA 0xbf
#define PKT_EOS   0xfb
#define PKT_FLT   0xfc
#define PKT_UMF   0xfd

typedef struct
  {
  uint32_t fields_per_entry;
  uint32_t num_offsets;
  uint32_t * offsets;
  } flt_t;

static int flt_read(bgav_input_context_t * input,
                    flt_t * ret, uint32_t len)
  {
  uint32_t i;

  if(!bgav_input_read_32_le(input, &ret->fields_per_entry) ||
     !bgav_input_read_32_le(input, &ret->num_offsets))
    return 0;

  len -= 8;
  
  ret->offsets = malloc(ret->num_offsets * sizeof(*ret->offsets));
  for(i = 0; i < ret->num_offsets; i++)
    {
    if(!bgav_input_read_32_le(input, &ret->offsets[i]))
      return 0;
    len -= 4;
    }
  if(len)
    bgav_input_skip(input, len);
  return 1;
  }

static void flt_free(flt_t * flt)
  {
  if(flt->offsets)
    free(flt->offsets);
  }

#if 0
static void flt_dump(flt_t * flt)
  {
  uint32_t i;
  bgav_dprintf("GXF field locator table\n");
  bgav_dprintf("  fields_per_entry: %d\n", flt->fields_per_entry);
  bgav_dprintf("  num_offsets:      %d\n", flt->num_offsets);
  for(i = 0; i < flt->num_offsets; i++)
    {
    bgav_dprintf("  offsets[%04d]:  %d\n", i, flt->offsets[i]);
    }
  }
#endif

typedef struct
  {
  uint8_t type;
  uint8_t id;
  uint32_t field_nr;
  uint32_t field_information;
  uint32_t timeline_field_nr;
  uint8_t flags;
  uint8_t reserved;
  } media_header_t;

static int read_media_header(bgav_input_context_t * input,
                             media_header_t * ret)
  {
  return(bgav_input_read_data(input, &ret->type, 1) &&
         bgav_input_read_data(input, &ret->id, 1) &&
         bgav_input_read_32_be(input, &ret->field_nr) &&
         bgav_input_read_32_be(input, &ret->field_information) &&
         bgav_input_read_32_be(input, &ret->timeline_field_nr) &&
         bgav_input_read_data(input, &ret->flags, 1) &&
         bgav_input_read_data(input, &ret->reserved, 1));
  }

#if 0
static void dump_media_header(media_header_t * h)
  {
  bgav_dprintf("GXF media header\n");
  bgav_dprintf("  type:              %d\n", h->type);
  bgav_dprintf("  id:                %d\n", h->id);
  bgav_dprintf("  field_nr:          %d\n", h->field_nr);
  bgav_dprintf("  field_information: %08x\n", h->field_information);
  bgav_dprintf("  timeline_field_nr: %d\n", h->timeline_field_nr);
  bgav_dprintf("  flags:             %d\n", h->flags);
  bgav_dprintf("  reserved:          %d\n", h->reserved);
  
  }
#endif

typedef struct
  {
  uint32_t first_field;
  uint32_t last_field;
  int num_fields;
  flt_t flt;
  
  /* Taken from video format */
  int timescale;
  int frame_duration;
  int do_sync;
  int64_t sync_field;
  } gxf_priv_t;

static const uint8_t startcode[] =
  {0, 0, 0, 0, 1 }; // start with map packet

static const uint8_t endcode[] =
  {0, 0, 0, 0, 0xe1, 0xe2};

static int is_start_code(uint8_t * data, int * type, uint32_t * length)
  {
  if(memcmp(data, startcode, 5))
    return 0;
  if(memcmp(data + 10, endcode, 6))
    return 0;

  *type = data[5];
  *length = GAVL_PTR_2_32BE(&data[6]);
  return 1;
  }

static int probe_gxf(bgav_input_context_t * input)
  {
  int type;
  uint32_t length;
  uint8_t test_data[16];
  if(bgav_input_get_data(input, test_data, 16) < 16)
    return 0;

  if(!is_start_code(test_data, &type, &length) ||
     (type != PKT_MAP))
    return 0;
  return 1;
  }

static int read_packet_header(bgav_input_context_t * input,
                               int * type, uint32_t * length)
  {
  uint8_t test_data[16];
  if(bgav_input_read_data(input, test_data, 16) < 16)
    return 0;

  if(!is_start_code(test_data, type, length))
    return 0;
  *length -= 16;
  return 1;
  }

static int peek_packet_header(bgav_input_context_t * input,
                               int * type, uint32_t * length)
  {
  uint8_t test_data[16];
  if(bgav_input_get_data(input, test_data, 16) < 16)
    return 0;

  if(!is_start_code(test_data, type, length))
    return 0;
  *length -= 16;
  return 1;
  }

static const struct
  {
  int timescale;
  int frame_duration;
  }
framerate_table[] =
  {
    { 0, 0 },
    { 60, 1 },
    { 60000, 1001 },
    { 50, 1 },
    { 30, 1 },
    { 30000, 1001 },
    { 25, 1 },
    { 24, 1 },
    { 24000, 1001 }
  };

static int parse_track(bgav_input_context_t * input,
                       bgav_track_t * t, const bgav_options_t * opt)
  {
  bgav_stream_t * as = NULL;
  bgav_stream_t * vs = NULL;
  uint32_t val_32;
  uint8_t tag;
  uint8_t value_length;
  uint16_t data_length;
  
  int64_t start_pos;
  uint8_t type, id;
  
  start_pos = input->position;
  
  if(!bgav_input_read_data(input, &type, 1) ||
     !bgav_input_read_data(input, &id, 1))
    return 0;

  type -= 0x80;
  switch(type)
    {
    case 4:
    case 5:
      vs = bgav_track_add_video_stream(t, opt);
      vs->fourcc = BGAV_MK_FOURCC('M','J','P','G');
      vs->ci->flags &= ~GAVL_COMPRESSION_HAS_P_FRAMES;
      break;
    case 13:
    case 14:
    case 15:
    case 16:
      vs = bgav_track_add_video_stream(t, opt);
      if(type & 0x01)      
        vs->fourcc = BGAV_MK_FOURCC('d','v','c',' ');
      else
        vs->fourcc = BGAV_MK_FOURCC('d','v','c','p');
      vs->ci->flags &= ~GAVL_COMPRESSION_HAS_P_FRAMES;
      break;
    case 11:
    case 12:
    case 20:
      vs = bgav_track_add_video_stream(t, opt);
      vs->fourcc = BGAV_MK_FOURCC('m','p','v','2');
      vs->flags |= STREAM_PARSE_FRAME;
      break;
    case 22:
    case 23:
      vs = bgav_track_add_video_stream(t, opt);
      vs->fourcc = BGAV_MK_FOURCC('m','p','v','1');
      vs->flags |= STREAM_PARSE_FRAME;
      break;
    case 9:
      as = bgav_track_add_audio_stream(t, opt);
      as->data.audio.format->num_channels = 1;
      as->data.audio.format->samplerate = 48000;
      as->data.audio.bits_per_sample = 24;
      as->fourcc = BGAV_WAVID_2_FOURCC(0x0001);
      break;
    case 10:
      as = bgav_track_add_audio_stream(t, opt);
      as->data.audio.format->num_channels = 1;
      as->data.audio.format->samplerate = 48000;
      as->data.audio.bits_per_sample = 16;
      as->fourcc = BGAV_WAVID_2_FOURCC(0x0001);
      break;
    case 17:
      as = bgav_track_add_audio_stream(t, opt);
      vs->fourcc = BGAV_MK_FOURCC('.','a','c','3');
      break;
    }
  if(as) as->stream_id = id - 0xc0;
  else if(vs) vs->stream_id = id - 0xc0;

  /* Parse tags */
  bgav_input_read_16_be(input, &data_length);

  while(data_length)
    {
    bgav_input_read_data(input, &tag, 1);
    bgav_input_read_data(input, &value_length, 1);
    data_length-=2;
    
    switch(tag)
      {
      case 0x50: // Frame rate
        if(vs && (value_length == 4))
          {
          bgav_input_read_32_be(input, &val_32);
          if((val_32 >= 1) && (val_32 <= 8))
            {
            vs->data.video.format->timescale =
              framerate_table[val_32].timescale;
            vs->data.video.format->frame_duration =
              framerate_table[val_32].frame_duration;
            }
          }
        else
          bgav_input_skip(input, value_length);
        break;
      case 0x52: // Fields per frame
        if(vs && (value_length == 4))
          {
          bgav_input_read_32_be(input, &val_32);
          if(val_32 == 2)
            vs->data.video.format->interlace_mode =
              GAVL_INTERLACE_BOTTOM_FIRST;
          }
        else
          bgav_input_skip(input, value_length);
        break;
      default:
        bgav_input_skip(input, value_length);
      }
    
    data_length-=value_length;

    }
  return input->position - start_pos;
  }

static int get_previous_startcode(bgav_demuxer_context_t * ctx,
                                  media_header_t * h)
  {
  int type;
  uint32_t length;
  while(1)
    {
    if(ctx->input->position <= 0)
      return 0;
    bgav_input_seek(ctx->input, -1, SEEK_CUR);
    if(peek_packet_header(ctx->input, &type, &length) &&
       (type == PKT_MEDIA))
      break;
    }
  read_packet_header(ctx->input, &type, &length);
  read_media_header(ctx->input, h);
  bgav_input_seek(ctx->input, -32, SEEK_CUR);
  return 1;
  }

static int get_next_startcode(bgav_demuxer_context_t * ctx,
                              media_header_t * h)
  {
  int type;
  uint32_t length;
  while(1)
    {
    if(ctx->input->total_bytes &&
       (ctx->input->position >= ctx->input->total_bytes - 31))
      return 0;
    if(peek_packet_header(ctx->input, &type, &length) &&
       (type == PKT_MEDIA))
      break;
    bgav_input_seek(ctx->input, 1, SEEK_CUR);
    }
  read_packet_header(ctx->input, &type, &length);
  read_media_header(ctx->input, h);
  bgav_input_seek(ctx->input, -32, SEEK_CUR);
  return 1;
  }

static int open_gxf(bgav_demuxer_context_t * ctx)
  {
  uint32_t length;
  int type;
  uint8_t tag;
  uint8_t value_length;
  uint16_t data_length;
  uint16_t track_desc_length;
  bgav_stream_t * vs;
  bgav_stream_t * as;
  gxf_priv_t * priv;
  media_header_t mh;
  int64_t last_pos;
  int i;
  
  /* Parse map packet */
  if(!read_packet_header(ctx->input, &type, &length) ||
     (type != PKT_MAP))
    return 0;

  bgav_input_skip(ctx->input, 2); // 0xE0, 0xFF
  if(!bgav_input_read_16_be(ctx->input, &data_length))
    return 0;

  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
    
  while(data_length)
    {
    if(!bgav_input_read_data(ctx->input, &tag, 1))
      return 0;
    if(!bgav_input_read_data(ctx->input, &value_length, 1))
      return 0;
    data_length-=2;

    switch(tag)
      {
      case 0x41: // number of first field in stream
        if(value_length == 4)
          {
          if(!bgav_input_read_32_be(ctx->input, &priv->first_field))
            return 0;
          }
        else
          bgav_input_skip(ctx->input, value_length);
        break;
      case 0x42: // number of last field in stream + 1
        if(value_length == 4)
          {
          if(!bgav_input_read_32_be(ctx->input, &priv->last_field))
            return 0;
          }
        else
          bgav_input_skip(ctx->input, value_length);
        break;
      default:
        bgav_input_skip(ctx->input, value_length);
        break;
      }
    data_length-=value_length;
    }

  ctx->tt = bgav_track_table_create(1);
  
  /* Track descriptions */

  if(!bgav_input_read_16_be(ctx->input, &track_desc_length))
    return 0;

  while(track_desc_length)
    {
    track_desc_length -= parse_track(ctx->input, ctx->tt->cur,
                                     ctx->opt);
    }

  bgav_track_set_format(ctx->tt->cur, "GXF", NULL);
  
  /* Skip remaining stuff of the map packet */
  if(ctx->input->position < length + 16)
    {
    bgav_input_skip(ctx->input, length + 16 - ctx->input->position);
    }
  
  /* Check for other packets */

  while(1)
    {
    if(!peek_packet_header(ctx->input, &type, &length))
      {
      return 0;
      }
    if(type == PKT_MEDIA)
      break;

    else if(type == PKT_EOS)
      return 0;
    
    else if(type == PKT_FLT)
      {
      bgav_input_skip(ctx->input, 16);
      if(!flt_read(ctx->input, &priv->flt, length))
        return 0;
      //      flt_dump(&priv->flt);
      }
    else if(type == PKT_UMF)
      {
      bgav_input_skip(ctx->input, length + 16);
      }
    else
      {
      bgav_input_skip(ctx->input, length + 16);
      }
    }
  
  ctx->data_start = ctx->input->position;
  ctx->flags |= BGAV_DEMUXER_HAS_DATA_START;
  
  /* Get number of fields and timescales */
  vs = bgav_track_get_video_stream(ctx->tt->cur, 0);
  
  priv->timescale      = vs->data.video.format->timescale;
  priv->frame_duration = vs->data.video.format->frame_duration;
  
  if(vs)
    {
    /* Set audio timescales */
    for(i = 0; i < ctx->tt->cur->num_audio_streams; i++)
      {
      as = bgav_track_get_audio_stream(ctx->tt->cur, i);
      as->timescale = priv->timescale;
      }
    if(vs->data.video.format->interlace_mode != GAVL_INTERLACE_NONE)
      priv->num_fields = 2;
    else
      priv->num_fields = 1;
    
    if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
      {
      last_pos = ctx->input->position;
      if(!get_next_startcode(ctx, &mh))
        return 0;
      priv->first_field = mh.field_nr;
      bgav_input_seek(ctx->input, -32, SEEK_END);
      if(!get_previous_startcode(ctx, &mh))
        return 0;
      priv->last_field = mh.field_nr;
      bgav_input_seek(ctx->input, last_pos, SEEK_SET);
      ctx->flags |= (BGAV_DEMUXER_CAN_SEEK | BGAV_DEMUXER_SEEK_ITERATIVE);
      }

    if(priv->last_field > priv->first_field)
      vs->stats.pts_end = (int64_t)vs->data.video.format->frame_duration *
        (priv->last_field - priv->first_field) / priv->num_fields;
    }
  
  return 1;
  }


static int next_packet_gxf(bgav_demuxer_context_t * ctx)
  {
  uint32_t length;
  int type;
  media_header_t mh;  
  bgav_packet_t * p;
  bgav_stream_t * s;
  gxf_priv_t * priv;
  int64_t position;
  
  priv = ctx->priv;

  position = ctx->input->position;
  
  if(!read_packet_header(ctx->input, &type, &length))
    return 0;

  if(type == PKT_EOS)
    {
    return 0;
    }
  else if(type == PKT_MEDIA)
    {
    if(!read_media_header(ctx->input, &mh))
      return 0;
    length -= 16;

    s = bgav_track_find_stream(ctx, mh.id);
    
    if(!s)
      {
      bgav_input_skip(ctx->input, length);
      }
    else
      {
      //      if(s->type == GAVF_STREAM_VIDEO)
      //        dump_media_header(&mh);
      if(priv->do_sync)
        {
        if(mh.field_nr - priv->first_field < priv->sync_field)
          {
          bgav_input_skip(ctx->input, length);
          return 1;
          }
        else if(!STREAM_HAS_SYNC(s))
          {
          STREAM_SET_SYNC(s, (mh.field_nr - priv->first_field) / priv->num_fields *
                          priv->frame_duration);
          }
        }
      
      p = bgav_stream_get_packet_write(s);
      bgav_packet_alloc(p,length);
      
      p->pts = (mh.field_nr - priv->first_field) / priv->num_fields *
            priv->frame_duration ;

      p->position = position;
      
      if(bgav_input_read_data(ctx->input, p->data, length) < length)
        return 0;
      p->data_size = length;
#if 0
      if(s->type == GAVF_STREAM_VIDEO)
        {
        fprintf(stderr, "Got video packet %d:\n", priv->num_fields);
        bgav_packet_dump(p);
        }
#endif
      bgav_stream_done_packet_write(s, p);
      }
    
    }
  else
    {
    bgav_input_skip(ctx->input, length);
    }
  
  return 1;
  }

static void seek_gxf(bgav_demuxer_context_t * ctx, int64_t time,
                     int scale)
  {
  media_header_t mh;
  gxf_priv_t * priv;
  int64_t pos;
  
  int64_t field_index;

  priv = ctx->priv;

  field_index = gavl_time_rescale(scale, priv->timescale, time);
  
  field_index /= priv->frame_duration;
  field_index *= priv->num_fields;
  
  pos = ctx->input->total_bytes *
    field_index / (priv->last_field - priv->first_field);

  
  bgav_input_seek(ctx->input, pos, SEEK_SET);

  if(!get_next_startcode(ctx, &mh))
    return;

  priv->do_sync = 1;
  priv->sync_field = field_index;

  
  while(!bgav_track_has_sync(ctx->tt->cur))
    {
    if(!next_packet_gxf(ctx))
      break;
    }
  
  //  if(bgav_track_has_sync(ctx->tt->cur))
    
  priv->do_sync = 0;
  }

static void close_gxf(bgav_demuxer_context_t * ctx)
  {
  gxf_priv_t * priv;
  priv = ctx->priv;
  flt_free(&priv->flt);
  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_gxf =
  {
    .probe =       probe_gxf,
    .open =        open_gxf,
    .next_packet = next_packet_gxf,
    .seek =        seek_gxf,
    .close =       close_gxf
  };
