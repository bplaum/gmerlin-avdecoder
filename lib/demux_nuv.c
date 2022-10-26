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

#define NUV_VIDEO     'V'
#define NUV_EXTRADATA 'D'
#define NUV_AUDIO     'A'
#define NUV_SEEKP     'R'
#define NUV_MYTHEXT   'X'

#define HDRSIZE 12


static const char * const nuppel_sig = "NuppelVideo";
static const char * const mythtv_sig = "MythTVVideo";
#define SIG_LEN 12

#define AUDIO_ID 0
#define VIDEO_ID 1

static int probe_nuv(bgav_input_context_t * input)
  {
  char probe_data[SIG_LEN];
  if(bgav_input_get_data(input, (uint8_t*)probe_data, SIG_LEN) < SIG_LEN)
    return 0;

  if(!strncmp(probe_data, nuppel_sig, SIG_LEN) ||
     !strncmp(probe_data, mythtv_sig, SIG_LEN))
    return 1;
  
  return 0;
  }


static int open_nuv(bgav_demuxer_context_t * ctx)
  {
  uint8_t tmp_8, subtype;
  uint32_t tmp_32;
  double aspect, fps;
  int done;
  char signature[SIG_LEN];
  int is_mythtv;
  int interlaced;
  uint32_t width, height;
  uint32_t a_packs, v_packs;

  uint32_t size;
  
  bgav_stream_t * as = NULL;
  bgav_stream_t * vs = NULL;
  
  ctx->tt = bgav_track_table_create(1);
  
  if(bgav_input_read_data(ctx->input, (uint8_t*)signature, SIG_LEN) < SIG_LEN)
    return 0;
  if(!strncmp(signature, mythtv_sig, SIG_LEN))
    is_mythtv = 1;
  else
    is_mythtv = 0;

  bgav_input_skip(ctx->input, 8); // 5 byte version + 3 byte padding
  if(!bgav_input_read_32_le(ctx->input, &width) ||
     !bgav_input_read_32_le(ctx->input, &height))
    return 0;
  bgav_input_skip(ctx->input, 8); // Desired width + height
  
  if(!bgav_input_read_data(ctx->input, &tmp_8, 1))
    return 0;
  if(tmp_8 == 'P')
    interlaced = 0;
  else
    interlaced = 1;

  bgav_input_skip(ctx->input, 3); // Padding
  
  if(!bgav_input_read_double_64_le(ctx->input, &aspect) ||
     !bgav_input_read_double_64_le(ctx->input, &fps) ||
     !bgav_input_read_32_le(ctx->input, &v_packs) ||
     !bgav_input_read_32_le(ctx->input, &a_packs))
    return 0;

  bgav_input_skip(ctx->input, 8); // Text + Keyframe distance (?)

  /* Initialize streams */

  if(v_packs)
    {
    vs = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
    vs->stream_id = VIDEO_ID;
    vs->fourcc = BGAV_MK_FOURCC('N', 'U', 'V', ' ');
    vs->timescale = 1000;
    vs->data.video.format->image_width = width;
    vs->data.video.format->frame_width = width;
    vs->data.video.format->image_height = height;
    vs->data.video.format->frame_height = height;
    vs->data.video.format->pixel_width  = aspect * 10000;
    vs->data.video.format->pixel_height = 10000;
    vs->data.video.format->timescale      = 1000;
    vs->data.video.format->frame_duration = 1000.0 / fps;
    vs->flags |= STREAM_NO_DURATIONS;

    if(interlaced)
      vs->data.video.format->interlace_mode = GAVL_INTERLACE_BOTTOM_FIRST;
    }

  if(a_packs)
    {
    as = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
    as->stream_id = AUDIO_ID;
    as->fourcc = BGAV_WAVID_2_FOURCC(0x0001);
    as->timescale = 1000;
    as->data.audio.bits_per_sample = 16;
    as->data.audio.format->num_channels = 2;
    as->data.audio.format->samplerate = 44100;
    as->container_bitrate = as->data.audio.format->samplerate *
      as->data.audio.format->num_channels * as->data.audio.bits_per_sample;
    as->data.audio.block_align = as->data.audio.format->num_channels * 2;
    }
  
  /* Get codec data */

  if(!is_mythtv && !vs)
    done = 1;
  else
    done = 0;

  while(!done)
    {
    if(!bgav_input_read_data(ctx->input, &tmp_8, 1))
      return 0;

    switch(tmp_8)
      {
      case NUV_EXTRADATA:
        if(!bgav_input_read_data(ctx->input, &subtype, 1))
          return 0;
        bgav_input_skip(ctx->input, 6);

        if(!bgav_input_read_32_le(ctx->input, &size))
          return 0;
        size &= 0xffffff;
        if(vs && (subtype == 'R'))
          {
          gavl_buffer_alloc(&vs->ci->codec_header, size);
          if(bgav_input_read_data(ctx->input, vs->ci->codec_header.buf, size) < size)
            return 0;
          
          vs->ci->codec_header.len = size;
          size = 0;
          if(!is_mythtv)
            done = 1;
          }
        break;
      case NUV_MYTHEXT:
        bgav_input_skip(ctx->input, 7);
        if(!bgav_input_read_32_le(ctx->input, &size))
          return 0;
        size &= 0xffffff;
        if(size != 128 * 4)
          break;
        bgav_input_skip(ctx->input, 4); // Version

        if(vs)
          {
          if(!bgav_input_read_fourcc(ctx->input, &vs->fourcc))
            return 0;
          }
        else
          bgav_input_skip(ctx->input, 4);

        if(as)
          {
          if(!bgav_input_read_fourcc(ctx->input, &as->fourcc))
            return 0;

          if(as->fourcc == BGAV_MK_FOURCC('L','A','M','E'))
            as->flags |= STREAM_PARSE_FULL;
          
          if(!bgav_input_read_32_le(ctx->input, &tmp_32))
            return 0;
          as->data.audio.format->samplerate = tmp_32;

          if(!bgav_input_read_32_le(ctx->input, &tmp_32))
            return 0;
          as->data.audio.bits_per_sample = tmp_32;

          if(!bgav_input_read_32_le(ctx->input, &tmp_32))
            return 0;
          as->data.audio.format->num_channels = tmp_32;
          as->container_bitrate = 0;
          }
        else
          bgav_input_skip(ctx->input, 4*4);

        size -= 6*4;
        done = 1;
        break;
      case NUV_SEEKP:
        size = 11;
        break;
      default:
        bgav_input_skip(ctx->input, 7);
        if(!bgav_input_read_32_le(ctx->input, &size))
          return 0;
        size &= 0xffffff;
        
        break;
      }
    if(size)
      bgav_input_skip(ctx->input, size);
    }
  
  /* Set description */
  if(is_mythtv)
    bgav_track_set_format(ctx->tt->cur, "MythTV", NULL);
  else
    bgav_track_set_format(ctx->tt->cur, "NuppelVideo", NULL);
  
  ctx->data_start = ctx->input->position;
  ctx->flags |= BGAV_DEMUXER_HAS_DATA_START;
  
  return 1;
  }

static gavl_source_status_t next_packet_nuv(bgav_demuxer_context_t * ctx)
  {
  uint8_t hdr[HDRSIZE];
  uint32_t size;
  int32_t pts;
  bgav_stream_t * s;
  bgav_packet_t * p;
  
  if(bgav_input_read_data(ctx->input, hdr, HDRSIZE) < HDRSIZE)
    return GAVL_SOURCE_EOF;

  size = GAVL_PTR_2_32LE(&hdr[8]);
  size &= 0xffffff;
  
  pts  = GAVL_PTR_2_32LE(&hdr[4]);
  
  switch(hdr[0])
    {
    case NUV_VIDEO:
    case NUV_EXTRADATA:
      s = bgav_track_find_stream(ctx,
                                 VIDEO_ID);
      if(!s)
        {
        bgav_input_skip(ctx->input, size);
        break;
        }
        
      p = bgav_stream_get_packet_write(s);
      bgav_packet_alloc(p, size + HDRSIZE);
      memcpy(p->buf.buf, hdr, HDRSIZE);

      if(bgav_input_read_data(ctx->input, p->buf.buf + HDRSIZE,
                              size) < size)
        return GAVL_SOURCE_EOF;
      
      p->buf.len = HDRSIZE + size;
      p->pts = pts;
      bgav_stream_done_packet_write(s, p);
      break;
    case NUV_AUDIO:
      s = bgav_track_find_stream(ctx,
                                 AUDIO_ID);
      if(!s)
        {
        bgav_input_skip(ctx->input, size);
        break;
        }
      
      p = bgav_stream_get_packet_write(s);
      bgav_packet_alloc(p, size);

      if(bgav_input_read_data(ctx->input, p->buf.buf,
                              size) < size)
        return GAVL_SOURCE_EOF;
      
      p->buf.len = size;
      p->pts = pts;
      
      bgav_stream_done_packet_write(s, p);
      break;
    case NUV_SEEKP:
      // contains no data, size value is invalid
      break;
    default:
      bgav_input_skip(ctx->input, size);
      break;
    }
  return GAVL_SOURCE_OK;
  }


static void close_nuv(bgav_demuxer_context_t * ctx)
  {

  }
     
const bgav_demuxer_t bgav_demuxer_nuv =
  {
    .probe =       probe_nuv,
    .open =        open_nuv,
    .next_packet = next_packet_nuv,
    .close =       close_nuv
  };
