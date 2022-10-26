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
#include <string.h>
#include <ctype.h>

#define AUDIO_STREAM_ID 0
#define VIDEO_STREAM_ID 1

#define LOG_DOMAIN "demux_vivo" 

/*
 *  VIVO demuxer
 *  Written with MPlayer source as reference.
 *  cleaned up a lot and made reentrant
 *  (actually rewritten from scratch)
 */

/* VIVO audio standards from vivog723.acm:

    G.723:
        FormatTag = 0x111
        Channels = 1 - mono
        SamplesPerSec = 8000 - 8khz
        AvgBytesPerSec = 800
        BlockAlign (bytes per block) = 24
        BitsPerSample = 8

    Siren:
        FormatTag = 0x112
        Channels = 1 - mono
        SamplesPerSec = 16000 - 16khz
        AvgBytesPerSec = 2000
        BlockAlign (bytes per block) = 40
        BitsPerSample = 8
*/

typedef struct
  {
  /* Main stuff */

  int version;
  float fps;
  uint32_t duration;
  uint32_t length;
  uint32_t rate;
  uint32_t vid_rate;
  uint32_t playtime1;
  uint32_t playtime2;
  int buffer;
  uint32_t preroll;
  
  char * title;
  char * author;
  char * copyright;
  char * producer;

  /* These seem to be present in VIVO 2 only */
    
  int width;
  int height;
  int display_width;
  int display_height;

  /* Additional headers */
  
  int have_record_2;
    
  struct
    {
    int time_unit_num;
    int time_unit_den;
    } record_2;

  /* Record type 3 and 4 seem to be identical */

  int have_record_3_4;
  
  struct
    {
    int type; /* 3 or 4 */
    uint32_t length;
    uint32_t initial_frame_length;
    uint32_t nominal_bitrate;
    uint32_t sampling_frequency;
    uint32_t gain_factor;
    } record_3_4;
      
  } vivo_header_t;

static int read_length(bgav_input_context_t * input)
  {
  uint8_t c;
  int len;
  
  if(!bgav_input_read_data(input, &c, 1))
    return -1;

  len = c;
  while(c & 0x80)
    {
    len = 0x80 * (len - 0x80);

    if(!bgav_input_read_data(input, &c, 1))
      return -1;
    len += c;
    }
  return len;
  }

static int check_key(char * buffer, const char * key, char ** pos)
  {
  char * pos1;
  
  if(!strncmp(buffer, key, strlen(key)))
    {
    pos1 = strchr(buffer, ':');
    if(pos1)
      {
      pos1++;
      while(isspace(*pos1) && (*pos1 != '\0'))
        pos1++;

      if(*pos1 == '\0')
        return 0;
      *pos = pos1;
      return 1;
      }
    }
  return 0;
  }

static void vivo_header_dump(vivo_header_t * h)
  {
  bgav_dprintf( "Main VIVO header\n");
  bgav_dprintf( "  Version:       %d\n", h->version);
  bgav_dprintf( "  FPS:           %f\n", h->fps);
  bgav_dprintf( "  Duration:      %d\n", h->duration);
  bgav_dprintf( "  Length:        %d\n", h->length);
  bgav_dprintf( "  Rate:          %d\n", h->rate);
  bgav_dprintf( "  VidRate:       %d\n", h->vid_rate);
  bgav_dprintf( "  Playtime1:     %d\n", h->playtime1);
  bgav_dprintf( "  Playtime2:     %d\n", h->playtime2);
  bgav_dprintf( "  Buffer:        %d\n", h->buffer);
  bgav_dprintf( "  Preroll:       %d\n", h->preroll);
  bgav_dprintf( "  Title:         %s\n", h->title);
  bgav_dprintf( "  Author:        %s\n", h->author);
  bgav_dprintf( "  Copyright:     %s\n", h->copyright);
  bgav_dprintf( "  Producer:      %s\n", h->producer);
  bgav_dprintf( "  Width:         %d\n", h->width);
  bgav_dprintf( "  Height:        %d\n", h->height);
  bgav_dprintf( "  DisplayWidth:  %d\n", h->display_width);
  bgav_dprintf( "  DisplayHeight: %d\n", h->display_height);

  if(h->have_record_2)
    {
    bgav_dprintf( "RecordType 2\n");
    bgav_dprintf( "  TimeUnitNumerator:   %d\n", h->record_2.time_unit_num);
    bgav_dprintf( "  TimeUnitDenominator: %d\n", h->record_2.time_unit_den);
    }

  if(h->have_record_3_4)
    {
    bgav_dprintf( "RecordType %d\n", h->record_3_4.type);
    bgav_dprintf( "  Length:             %d\n", h->record_3_4.length);
    bgav_dprintf( "  InitialFrameLength: %d\n", h->record_3_4.initial_frame_length);
    bgav_dprintf( "  SamplingFrequency:  %d\n", h->record_3_4.sampling_frequency);
    bgav_dprintf( "  GainFactor:         %d\n", h->record_3_4.gain_factor);
    }
  
  }

static int vivo_header_read(vivo_header_t * ret, bgav_input_context_t * input)
  {
  uint8_t c;
  int len;
  int64_t header_start;
  char * str = NULL;
  char * pos = NULL;
  int result = 0;
  int record_type;

  gavl_buffer_t buf;
  gavl_buffer_init(&buf);
  
  /* First, read the main stuff */

  if(!bgav_input_read_data(input, &c, 1))
    goto fail;

  if((c & 0xf0) != 0x00)
    goto fail;

  len = read_length(input);
  if(len < 0)
    goto fail;

  header_start = input->position;
  
  while(input->position < header_start + len)
    {
    if(!bgav_input_read_line(input, &buf))
      goto fail;
    str = (char*)buf.buf;
    if(check_key(str, "Version", &pos))
      {
      while(!isdigit(*pos) && (*pos != '\0'))
        pos++;
      if(*pos == '\0')
        goto fail;
      ret->version = atoi(pos);
      }
    else if(check_key(str, "FPS", &pos))
      ret->fps = atof(pos);
    else if(check_key(str, "Duration", &pos))
      ret->duration = strtoul(pos, NULL, 10);
    else if(check_key(str, "Rate", &pos))
      ret->rate = strtoul(pos, NULL, 10);
    else if(check_key(str, "VidRate", &pos))
      ret->vid_rate = strtoul(pos, NULL, 10);
    else if(check_key(str, "Playtime1", &pos))
      ret->playtime1 = strtoul(pos, NULL, 10);
    else if(check_key(str, "Playtime2", &pos))
      ret->playtime2 = strtoul(pos, NULL, 10);
    else if(check_key(str, "Buffer", &pos))
      ret->buffer = strtol(pos, NULL, 10);
    else if(check_key(str, "Preroll", &pos))
      ret->preroll = strtoul(pos, NULL, 10);
    else if(check_key(str, "Title", &pos))
      ret->title = gavl_strdup(pos);
    else if(check_key(str, "Author", &pos))
      ret->author = gavl_strdup(pos);
    else if(check_key(str, "Copyright", &pos))
      ret->copyright = gavl_strdup(pos);
    else if(check_key(str, "Producer", &pos))
      ret->producer = gavl_strdup(pos);
    else if(check_key(str, "Width", &pos))
      ret->width = strtoul(pos, NULL, 10);
    else if(check_key(str, "Height", &pos))
      ret->height = strtoul(pos, NULL, 10);
    else if(check_key(str, "DisplayWidth", &pos))
      ret->display_width = strtoul(pos, NULL, 10);
    else if(check_key(str, "DisplayHeight", &pos))
      ret->display_height = strtoul(pos, NULL, 10);
    }

  /* How, check if we have additional headers */

  while(1)
    {
    if(!bgav_input_get_data(input, &c, 1))
      goto fail;
    
    if((c & 0xf0) != 0x00)
      break; /* Reached non header data */

    bgav_input_skip(input, 1);
        
    len = read_length(input);
    if(len < 0)
      goto fail;
    
    header_start = input->position;

    /* Get the record type */

    record_type = -1;
    while(input->position < header_start + len)
      {
      if(!bgav_input_read_line(input, &buf))
        goto fail;
      str = (char*)buf.buf;
      
      /* Skip empty lines */
      if(*str == '\0')
        continue;
      
      if(!check_key(str, "RecordType", &pos))
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Unknown extended header: %s", str);
        break;
        }
      record_type = atoi(pos);
      break;
      }

    if(record_type == -1)
      goto fail;
        
    switch(record_type)
      {
      case 2:
        while(input->position < header_start + len)
          {
          if(!bgav_input_read_line(input, &buf))
            goto fail;
          str = (char*)buf.buf;
          
          if(check_key(str, "TimestampType", &pos))
            {
            if(strcmp(pos, "relative"))
              gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                       "Unknown timestamp type: %s",
                      pos);
            }
          else if(check_key(str, "TimeUnitNumerator", &pos))
            {
            ret->record_2.time_unit_num = strtoul(pos, NULL, 10);
            }
          else if(check_key(str, "TimeUnitDenominator", &pos))
            {
            ret->record_2.time_unit_den = strtoul(pos, NULL, 10);
            }
          }
        ret->have_record_2 = 1;
        break;
      case 3:
      case 4:
        while(input->position < header_start + len)
          {
          if(!bgav_input_read_line(input, &buf))
            goto fail;
          str = (char*)buf.buf;
          
          if(check_key(str, "Length", &pos))
            ret->record_3_4.length = strtoul(pos, NULL, 10);

          else if(check_key(str, "InitialFrameLength", &pos))
            ret->record_3_4.initial_frame_length = strtoul(pos, NULL, 10);

          else if(check_key(str, "NominalBitrate", &pos))
            ret->record_3_4.nominal_bitrate = strtoul(pos, NULL, 10);

          else if(check_key(str, "SamplingFrequency", &pos))
            ret->record_3_4.sampling_frequency = strtoul(pos, NULL, 10);

          else if(check_key(str, "GainFactor", &pos))
            ret->record_3_4.gain_factor = strtoul(pos, NULL, 10);
          
          }
        ret->record_3_4.type = record_type;
        ret->have_record_3_4 = 1;
        break;
      }
    }

  result = 1;
  if(input->opt->dump_headers)
    vivo_header_dump(ret);
  fail:
  gavl_buffer_free(&buf);
  return result;
  }

#define MY_FREE(p) if(p) free(p);

static void vivo_header_free(vivo_header_t * h)
  {
  MY_FREE(h->title);
  MY_FREE(h->author);
  MY_FREE(h->copyright);
  MY_FREE(h->producer);
  }

#undef MY_FREE

typedef struct
  {
  vivo_header_t header;
  uint32_t audio_pos;
  } vivo_priv_t;

static int probe_vivo(bgav_input_context_t * input)
  {
  /* We look, if we have the string "Version:Vivo/" in the first 32 bytes */
    
  int i;
  uint8_t probe_data[32];

  if(bgav_input_get_data(input, probe_data, 32) < 32)
    return 0;

  for(i = 0; i < 32-13; i++)
    {
    if(!strncmp((char*)(&probe_data[i]), "Version:Vivo/", 13))
      return 1;
    }
  return 0;
  }

static int open_vivo(bgav_demuxer_context_t * ctx)
  {
  
  vivo_priv_t * priv;
  bgav_stream_t * audio_stream = NULL;
  bgav_stream_t * video_stream = NULL;
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
  
  /* Read header */

  if(!vivo_header_read(&priv->header, ctx->input))
    goto fail;

  /* Create track */

  ctx->tt = bgav_track_table_create(1);
  
  /* Set up audio stream */
  
  audio_stream = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
  audio_stream->stream_id = AUDIO_STREAM_ID;

  if(priv->header.version == 1)
    {
    /* G.723 */
    audio_stream->fourcc = BGAV_WAVID_2_FOURCC(0x0111);
    audio_stream->data.audio.format->samplerate = 8000;
    audio_stream->container_bitrate = 800 * 8;
    audio_stream->data.audio.block_align = 24;
    audio_stream->data.audio.bits_per_sample = 8;
    }
  else if(priv->header.version == 2)
    {
    /* Siren */
    audio_stream->fourcc = BGAV_WAVID_2_FOURCC(0x0112);
    audio_stream->data.audio.format->samplerate = 16000;
    audio_stream->container_bitrate = 2000 * 8;
    audio_stream->data.audio.block_align = 40;
    audio_stream->data.audio.bits_per_sample = 16;
    }
  audio_stream->data.audio.format->num_channels = 1;
  audio_stream->codec_bitrate = audio_stream->container_bitrate;
  /* Set up video stream */
  
  video_stream = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
  video_stream->flags |= STREAM_NO_DURATIONS;

  if(priv->header.version == 1)
    {
    video_stream->fourcc = BGAV_MK_FOURCC('v', 'i', 'v', '1');
    }
  else if(priv->header.version == 2)
    {
    video_stream->fourcc = BGAV_MK_FOURCC('v', 'i', 'v', 'o');
    video_stream->data.video.format->image_width = priv->header.width;
    video_stream->data.video.format->frame_width = priv->header.width;

    video_stream->data.video.format->image_height = priv->header.height;
    video_stream->data.video.format->frame_height = priv->header.height;
    }
  video_stream->data.video.format->pixel_width = 1;
  video_stream->data.video.format->pixel_height = 1;
  video_stream->data.video.format->framerate_mode = GAVL_FRAMERATE_VARIABLE;
  
  video_stream->stream_id = VIDEO_STREAM_ID;
  
  //  video_stream->data.video.format->timescale = (int)(priv->header.fps * 1000.0);
  
  video_stream->data.video.format->timescale = 1000;
  video_stream->timescale = 1000;
  
  //  video_stream->data.video.format->frame_duration = 1000;
  video_stream->data.video.depth = 24;
  video_stream->data.video.image_size = video_stream->data.video.format->image_width *
    video_stream->data.video.format->image_height * 3;
  /* Set up metadata */

  gavl_dictionary_set_string(ctx->tt->cur->metadata,
                    GAVL_META_TITLE, priv->header.title);
  gavl_dictionary_set_string(ctx->tt->cur->metadata,
                    GAVL_META_AUTHOR, priv->header.author);
  gavl_dictionary_set_string(ctx->tt->cur->metadata,
                    GAVL_META_COPYRIGHT, priv->header.copyright);
  gavl_dictionary_set_string(ctx->tt->cur->metadata,
                    GAVL_META_SOFTWARE, priv->header.producer);

  bgav_track_set_format(ctx->tt->cur, "Vivo", NULL);
  
  gavl_track_set_duration(ctx->tt->cur->info, (GAVL_TIME_SCALE * (int64_t)(priv->header.duration)) / 1000);

  ctx->data_start = ctx->input->position;
  ctx->flags |= BGAV_DEMUXER_HAS_DATA_START;
  
  return 1;
  
  fail:
  return 0;
  }

static gavl_source_status_t next_packet_vivo(bgav_demuxer_context_t * ctx)
  {
  uint8_t c, h;
  int prefix = 0;
  int len;
  int seq;
  bgav_stream_t * stream = NULL;
  vivo_priv_t * priv;
  int do_audio = 0;
  int do_video = 0;
  
  priv = ctx->priv;

  if(!bgav_input_read_data(ctx->input, &c, 1))
    return GAVL_SOURCE_EOF;

  if(c == 0x82)
    {
    prefix = 1;
    
    if(!bgav_input_read_data(ctx->input, &c, 1))
      return GAVL_SOURCE_EOF;
    
    }

  h = c;
  
  switch(h & 0xf0)
    {
    case 0x00: /* Thought we already have all headers */
      len = read_length(ctx->input);
      if(len < 0)
        return GAVL_SOURCE_EOF;
      bgav_input_skip(ctx->input, len);
      return GAVL_SOURCE_OK;
      break;
    case 0x10: /* Video packet */
    case 0x20:
      if(prefix || ((h & 0xf0) == 0x20))
        {
        if(!bgav_input_read_data(ctx->input, &c, 1))
          return GAVL_SOURCE_EOF;
        len = c;
        }
      else
        len = 128;
      do_video = 1;
      break;
    case 0x30:
    case 0x40:
      if(prefix)
        {
        if(!bgav_input_read_data(ctx->input, &c, 1))
          return GAVL_SOURCE_EOF;
        len = c;
        }
      else if((h & 0xf0) == 0x30)
        len = 40;
      else
        len = 24;
      do_audio = 1;
      priv->audio_pos += len;
      break;
    default:
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Unknown packet type");
      return GAVL_SOURCE_EOF;
    }
    
  if(do_audio)
    stream = bgav_track_find_stream(ctx, AUDIO_STREAM_ID);
  else if(do_video)
    stream = bgav_track_find_stream(ctx, VIDEO_STREAM_ID);
  
  if(!stream)
    {
    bgav_input_skip(ctx->input, len);
    return GAVL_SOURCE_OK;
    }
  
  seq = (h & 0x0f);
  /* Finish packet */

  if(stream->packet && (stream->packet_seq != seq))
    {
    bgav_stream_done_packet_write(stream, stream->packet);

    if(do_video)
      {
      bgav_stream_t * as = bgav_track_get_audio_stream(ctx->tt->cur, 0);
      
      stream->packet->pts = (priv->audio_pos * 8000) /
        as->container_bitrate;
      }

    stream->packet = NULL;
    }

  /* Get new packet */
  
  if(!stream->packet)
    {
    stream->packet = bgav_stream_get_packet_write(stream);
    stream->packet_seq = seq;
    stream->packet->buf.len = 0;
    }

  /* Append data */
  bgav_packet_alloc(stream->packet,
                    stream->packet->buf.len + len);
  if(bgav_input_read_data(ctx->input,
                          stream->packet->buf.buf + stream->packet->buf.len,
                          len) < len)
    {
    return GAVL_SOURCE_EOF;
    }
  stream->packet->buf.len += len;
  if((h & 0xf0) == 0x20)
    stream->packet_seq--;
  return GAVL_SOURCE_OK;
  }

static int select_track_vivo(bgav_demuxer_context_t * ctx, int t)
  {
  vivo_priv_t * priv;
  priv = ctx->priv;
  priv->audio_pos = 0;
  return 1;
  }

static void close_vivo(bgav_demuxer_context_t * ctx)
  {
  vivo_priv_t * priv;
  priv = ctx->priv;
  
  vivo_header_free(&priv->header);
  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_vivo =
  {
    .probe =        probe_vivo,
    .open =         open_vivo,
    .select_track = select_track_vivo,
    .next_packet =  next_packet_vivo,
    .close =        close_vivo
  };

