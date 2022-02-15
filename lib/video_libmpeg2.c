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

#include <inttypes.h>
#include <mpeg2dec/mpeg2.h>

#include <config.h>

#include <avdec_private.h>
#include <codecs.h>
#include <ptscache.h>

#define LOG_DOMAIN "video_libmpeg2"

// #define DUMP_SEQUENCE_HEADER
// #define DUMP_PACKETS

static const char picture_types[] = { "?IPB????" };

/* Debug function */
#ifdef DUMP_SEQUENCE_HEADER
static void dump_sequence_header(const mpeg2_sequence_t * s)
  {
  bgav_dprintf("Sequence header:\n");
  bgav_dprintf("size:         %d x %d\n", s->width, s->height);
  bgav_dprintf("chroma_size:  %d x %d\n", s->chroma_width,
          s->chroma_height);

  bgav_dprintf("picture_size: %d x %d\n", s->picture_width,
          s->picture_height);
  bgav_dprintf("display_size: %d x %d\n", s->display_width,
          s->display_height);
  bgav_dprintf("pixel_size:   %d x %d\n", s->pixel_width,
          s->pixel_height);
  }
#endif

#define FLAG_INIT              (1<<0)
#define FLAG_EOF               (1<<1)
#define FLAG_HAS_GOP_TIMECODE  (1<<2)
#define FLAG_NEED_SEQUENCE     (1<<3)
#define FLAG_EXTERN_TIMECODES  (1<<4)
#define FLAG_EXTERN_ASPECT     (1<<5)

typedef struct
  {
  const mpeg2_info_t * info;
  mpeg2dec_t * dec;
  gavl_video_frame_t * frame;
  bgav_packet_t * p;
  int flags;
  
  int64_t picture_duration;
  int64_t picture_timestamp;

  /*
   *  Specify how many non-B frames we saw since the
   *  last seek. We needs this to skip old B-Frames
   *  which are output by libmpeg2 by accident
   */
  
  int non_b_count;
  uint8_t sequence_end_code[4];
  
  gavl_timecode_t gop_timecode;

  bgav_pts_cache_t pts_cache;

  int y_offset;
  int sub_v;
  } mpeg2_priv_t;

static int get_data(bgav_stream_t*s)
  {
  mpeg2_priv_t * priv;
  int cache_index;
  priv = s->data.video.decoder->priv;

  if(priv->p)
    {
    bgav_stream_done_packet_read(s, priv->p);
    priv->p = NULL;
    }
  
  if(!bgav_stream_peek_packet_read(s, 1))
    {
    if(!(priv->flags & FLAG_EOF))
      {
      /* Flush the last picture */
      priv->sequence_end_code[0] = 0x00;
      priv->sequence_end_code[1] = 0x00;
      priv->sequence_end_code[2] = 0x01;
      priv->sequence_end_code[3] = 0xb7;

      mpeg2_buffer(priv->dec,
                   &priv->sequence_end_code[0],
                   &priv->sequence_end_code[4]);
      priv->flags |= FLAG_EOF;
      return 1;
      }
    else
      return 0;
    }

  if(priv->flags & FLAG_NEED_SEQUENCE)
    {
    bgav_packet_t * tmp;
    priv->flags &= ~FLAG_NEED_SEQUENCE;

    tmp = bgav_stream_peek_packet_read(s, 1);
    
    if((s->ext_size > 0) && (tmp->data[3] != 0xb3))
      {
      mpeg2_buffer(priv->dec, s->ext_data, s->ext_data + s->ext_size);
      return 1;
      }
    }
  
  priv->p = bgav_stream_get_packet_read(s);
  
#ifdef DUMP_PACKETS
  fprintf(stderr, "Got packet ");
  bgav_packet_dump(priv->p);
#endif
  
  priv->flags &= ~FLAG_EOF;
  mpeg2_buffer(priv->dec, priv->p->data, priv->p->data + priv->p->data_size);

  //  fprintf(stderr, "mpeg2_buffer %d bytes\n", priv->p->data_size);
  //  gavl_hexdump(priv->p->data, 32, 16);
  
  bgav_pts_cache_push(&priv->pts_cache,
                      priv->p->pts, priv->p->duration,
                      priv->p->tc,
                      &cache_index,
                      NULL);
  
  mpeg2_tag_picture(priv->dec, cache_index, 0);
  
  return 1;
  }

static void get_format(bgav_stream_t*s,
                       gavl_video_format_t * ret,
                       const mpeg2_sequence_t * sequence)
  {
  mpeg2_priv_t * priv;
  int container_time;
  priv = s->data.video.decoder->priv;

  container_time = (ret->timescale > 0);

  if(!container_time)
    {
    switch(sequence->frame_period)
      {
      /* Original timscale is 27.000.000, a bit too much for us */

      /* We choose duration/scale so that the duration is always even.
         Since the MPEG-2 repeat stuff happens at half-frametime level,
         we nevertheless get 100% precise timestamps
      */
    
      case 1126125: /* 24000 / 1001 */
        ret->timescale = 48000;
        ret->frame_duration = 2002;
        break;
      case 1125000: /* 24 / 1 */
        ret->timescale = 48;
        ret->frame_duration = 2;
        break;
      case 1080000: /* 25 / 1 */
        ret->timescale = 50;
        ret->frame_duration = 2;
        break;
      case 900900: /* 30000 / 1001 */
        ret->timescale = 60000;
        ret->frame_duration = 2002;
        break;
      case 900000: /* 30 / 1 */
        ret->timescale = 60;
        ret->frame_duration = 2;
        break;
      case 540000: /* 50 / 1 */
        ret->timescale = 100;
        ret->frame_duration = 2;
        break;
      case 450450: /* 60000 / 1001 */
        ret->timescale = 120000;
        ret->frame_duration = 2002;
        break;
      case 450000: /* 60 / 1 */
        ret->timescale = 120;
        ret->frame_duration = 2;
        break;
      }
    }
  
  ret->image_width  = sequence->picture_width;
  ret->image_height = sequence->picture_height;

  /* Handle D-10 */
  if((s->fourcc == BGAV_MK_FOURCC('m','x','3','p')) || // IMX PAL 30 MBps
     (s->fourcc == BGAV_MK_FOURCC('m','x','4','p')) || // IMX PAL 40 MBps
     (s->fourcc == BGAV_MK_FOURCC('m','x','5','p'))) // IMX PAL 50 MBps
    {
    if(ret->image_height == 608)
      ret->image_height = 576;
    priv->y_offset = 32;
    ret->interlace_mode = GAVL_INTERLACE_TOP_FIRST;
    }
  else if((s->fourcc == BGAV_MK_FOURCC('m','x','3','n')) || // IMX NTSC 30 MBps
          (s->fourcc == BGAV_MK_FOURCC('m','x','4','n')) || // IMX NTSC 40 MBps
          (s->fourcc == BGAV_MK_FOURCC('m','x','5','n'))) // IMX NTSC 50 MBps
    {
    if(ret->image_height == 512)
      ret->image_height = 486;
    priv->y_offset = 26;
    ret->interlace_mode = GAVL_INTERLACE_TOP_FIRST;
    }
  
  ret->frame_width  = sequence->width;
  ret->frame_height = sequence->height;

  //  if(!ret->pixel_width)
    {
    ret->pixel_width  = sequence->pixel_width;
    ret->pixel_height = sequence->pixel_height;
    }
    //  else
    priv->flags |= FLAG_EXTERN_ASPECT;
  
  if(sequence->chroma_height == sequence->height/2)
    {
    ret->pixelformat = GAVL_YUV_420_P;
    if(sequence->flags & SEQ_FLAG_MPEG2)
      ret->chroma_placement = GAVL_CHROMA_PLACEMENT_MPEG2;
    priv->sub_v = 2;
    }
  else if(sequence->chroma_height == sequence->height)
    {
    ret->pixelformat = GAVL_YUV_422_P;
    priv->sub_v = 1;
    }
  if(!container_time)
    {
    if((sequence->flags & SEQ_FLAG_MPEG2) && (ret->framerate_mode == GAVL_FRAMERATE_UNKNOWN))
      ret->framerate_mode = GAVL_FRAMERATE_VARIABLE;
    else /* MPEG-1 is always constant framerate */
      {
      ret->timescale /= 2;
      ret->frame_duration /= 2;
      }
    }
#ifdef DUMP_SEQUENCE_HEADER
  dump_sequence_header(sequence);
#endif
  
  // Get interlace mode

  if(ret->interlace_mode == GAVL_INTERLACE_UNKNOWN)
    {
    if((sequence->flags & SEQ_FLAG_MPEG2) &&
       !(sequence->flags & SEQ_FLAG_PROGRESSIVE_SEQUENCE))
      ret->interlace_mode = GAVL_INTERLACE_MIXED;
    else
      ret->interlace_mode = GAVL_INTERLACE_NONE;
    }
  }

static int parse(bgav_stream_t*s, mpeg2_state_t * state)
  {
  mpeg2_priv_t * priv;
  priv = s->data.video.decoder->priv;

  while(1)
    {
    *state = mpeg2_parse(priv->dec);

    //    fprintf(stderr, "mpeg2_parse %d\n", *state);
    
    if(*state == STATE_BUFFER)
      {
      if(!get_data(s))
        return 0;
      continue;
      }
    
    if((*state == STATE_GOP) && !(s->flags & FLAG_EXTERN_TIMECODES) &&
       s->data.video.format.frame_duration)
      {
      if(!s->data.video.format.timecode_format.int_framerate)
        {
        s->data.video.format.timecode_format.int_framerate =
          s->data.video.format.timescale / s->data.video.format.frame_duration;
        if(s->data.video.format.timescale % s->data.video.format.frame_duration)
          s->data.video.format.timecode_format.int_framerate++;
        if(priv->info->gop->flags & GOP_FLAG_DROP_FRAME)
          s->data.video.format.timecode_format.flags |=
            GAVL_TIMECODE_DROP_FRAME;
        }
      gavl_timecode_from_hmsf(&priv->gop_timecode,
                              priv->info->gop->hours,
                              priv->info->gop->minutes,
                              priv->info->gop->seconds,
                              priv->info->gop->pictures);
      priv->flags |= FLAG_HAS_GOP_TIMECODE;
      }
    return 1;
    }
  }

/* Decode one picture, frame will be in priv->info->display_picture
   after */

static int decode_picture(bgav_stream_t*s)
  {
  int done;
  gavl_video_format_t new_format;
  int cache_index;
  mpeg2_priv_t * priv;
  mpeg2_state_t state;
  priv = s->data.video.decoder->priv;
  done = 0;
  while(1)
    {
    if(!parse(s, &state))
      return 0;
    
    if(((state == STATE_END) || (state == STATE_SLICE) ||
        (state == STATE_INVALID_END)))
      {
      if(state == STATE_END)
        {
        priv->flags |= FLAG_EOF;
        if(priv->info->display_picture)
          done = 1;
        }
      if(priv->info->current_picture)
        {
        switch(priv->info->current_picture->flags & PIC_MASK_CODING_TYPE)
          {
          case PIC_FLAG_CODING_TYPE_I:
          case PIC_FLAG_CODING_TYPE_P:
            priv->non_b_count++;
            done = 1;
            break;
          case PIC_FLAG_CODING_TYPE_B:
            if(priv->non_b_count >= 2)
              done = 1;
            break;
          }
        }      
      if(!priv->info->display_fbuf)
        done = 0;
      if(done)
        break;
      }
#if MPEG2_RELEASE >= MPEG2_VERSION(0,5,0)
    else if((state == STATE_SEQUENCE) ||
            (state == STATE_SEQUENCE_REPEATED) ||
            (state == STATE_SEQUENCE_MODIFIED))
#else
    else if((state == STATE_SEQUENCE) ||
            (state == STATE_SEQUENCE_REPEATED))
#endif
      {
      memset(&new_format, 0, sizeof(new_format));
      get_format(s, &new_format, priv->info->sequence);
      
      if((new_format.image_width != s->data.video.format.image_width) ||
         (new_format.image_height != s->data.video.format.image_height))
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                 "Detected change of image size, not handled yet");

      if(!(priv->flags & FLAG_EXTERN_ASPECT))
        {
        if((s->data.video.format.pixel_width != new_format.pixel_width) ||
           (s->data.video.format.pixel_height != new_format.pixel_height))
          {
          gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
                   "Detected change of pixel aspect ratio: %dx%d",
                   new_format.pixel_width,
                   new_format.pixel_height);
          if(s->opt->aspect_callback)
            {
            s->opt->aspect_callback(s->opt->aspect_callback_data,
                                    bgav_stream_get_index(s),
                                    new_format.pixel_width,
                                    new_format.pixel_height);
            }
          s->data.video.format.pixel_width = new_format.pixel_width;
          s->data.video.format.pixel_height = new_format.pixel_height;
          }
        }
      }

    }
  
  if((state == STATE_END) && (priv->flags & FLAG_INIT))
    {
    /* If we have a sequence end code after the first frame,
       force still mode */
    gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Detected MPEG still image");
    s->flags |= STREAM_STILL_MODE;
    // s->data.video.format.framerate_mode = GAVL_FRAMERATE_STILL;
    if(priv->p)
      {
      bgav_stream_done_packet_read(s, priv->p);
      priv->p = NULL;
      }
    s->data.video.format.framerate_mode = GAVL_FRAMERATE_STILL;
    }

  /* Calculate timestamp */
    
  if(priv->info->display_picture->flags & PIC_FLAG_TAGS)
    {
    cache_index = priv->info->display_picture->tag;

    priv->picture_timestamp = priv->pts_cache.entries[cache_index].pts;
    priv->picture_duration  = priv->pts_cache.entries[cache_index].duration;
    priv->pts_cache.entries[cache_index].used = 0;
    }
  else /* Should never happen */
    {
    priv->picture_timestamp += priv->picture_duration;
    }
  
  return 1;
  }


static int decode_mpeg2(bgav_stream_t*s, gavl_video_frame_t*f)
  {
  mpeg2_priv_t * priv;

  priv = s->data.video.decoder->priv;
  
  /* Decode frame */
  
#if 0  
  if(f)
    mpeg2_skip(priv->dec, 0);
  else
    mpeg2_skip(priv->dec, 1);
#endif

  if(!(s->flags & STREAM_HAVE_FRAME))
    {
    if(!decode_picture(s))
      return 0;

    s->flags |= STREAM_HAVE_FRAME;
    }
  
  if(priv->flags & FLAG_INIT)
    {
    s->out_time = priv->picture_timestamp;
    return 1;
    }
  if(f)
    {
    priv->frame->planes[0] = priv->info->display_fbuf->buf[0] + priv->y_offset * priv->frame->strides[0];
    priv->frame->planes[1] = priv->info->display_fbuf->buf[1] + priv->y_offset/priv->sub_v * priv->frame->strides[1];
    priv->frame->planes[2] = priv->info->display_fbuf->buf[2] + priv->y_offset/priv->sub_v * priv->frame->strides[2];
    gavl_video_frame_copy(&s->data.video.format, f, priv->frame);
    if(s->data.video.format.interlace_mode == GAVL_INTERLACE_MIXED)
      {
      if(priv->info->display_picture->flags & PIC_FLAG_PROGRESSIVE_FRAME)
        f->interlace_mode = GAVL_INTERLACE_NONE;
      else
        f->interlace_mode =
          (priv->info->display_picture->flags & PIC_FLAG_TOP_FIELD_FIRST) ?
          GAVL_INTERLACE_TOP_FIRST : GAVL_INTERLACE_BOTTOM_FIRST;
      }
    f->timestamp = priv->picture_timestamp;
    f->duration  = priv->picture_duration;
    }

  /* Set timecodes */  
  if(((priv->info->display_picture->flags & PIC_MASK_CODING_TYPE) ==
      PIC_FLAG_CODING_TYPE_I) &&
     (priv->flags & FLAG_HAS_GOP_TIMECODE))
    {
    s->codec_timecode = priv->gop_timecode;
    s->has_codec_timecode = 1;
    priv->flags &= ~FLAG_HAS_GOP_TIMECODE;
    }
  return 1;
  }


static int init_mpeg2(bgav_stream_t*s)
  {
  mpeg2_state_t state;
  mpeg2_priv_t * priv;
  
  priv = calloc(1, sizeof(*priv));
  s->data.video.decoder->priv = priv;
  
  if(s->data.video.format.timecode_format.int_framerate)
    s->flags |= FLAG_EXTERN_TIMECODES;
  
  priv->dec  = mpeg2_init();
  priv->info = mpeg2_info(priv->dec);
  priv->non_b_count = 0;

  priv->flags |= FLAG_NEED_SEQUENCE;
  
  while(1)
    {
    if(!parse(s, &state))
      return 0;
    if(state == STATE_SEQUENCE)
      break;
    }

  priv->flags &= ~FLAG_NEED_SEQUENCE;
  
  /* Get format */
  
  get_format(s, &s->data.video.format, priv->info->sequence);
  priv->frame = gavl_video_frame_create(NULL);

  priv->frame->strides[0] = priv->info->sequence->width;
  priv->frame->strides[1] = priv->info->sequence->chroma_width;
  priv->frame->strides[2] = priv->info->sequence->chroma_width;


  gavl_dictionary_set_string_nocopy(&s->m, GAVL_META_FORMAT,
                          bgav_sprintf("MPEG-%d",
                                       (priv->info->sequence->flags & SEQ_FLAG_MPEG2) ? 2 : 1));

#if 0
  s->description =
    bgav_sprintf("MPEG-%d, %d kb/sec", 
                 (priv->info->sequence->flags & SEQ_FLAG_MPEG2) ? 2 : 1,
                 (priv->info->sequence->byte_rate * 8) / 1000);
#endif
  
  s->codec_bitrate = priv->info->sequence->byte_rate * 8;
  
  if(!s->timescale)
    s->timescale = s->data.video.format.timescale;
  
  while(1)
    {
    if(!parse(s, &state))
      return 0;
    if(state == STATE_PICTURE)
      break;
    }
  
  /* Decode first frame to check for still mode */
  
  priv->flags |= FLAG_INIT;
  decode_mpeg2(s, NULL);
  priv->flags &= ~FLAG_INIT;
  s->out_time = priv->picture_timestamp;
  return 1;
  }

static void resync_mpeg2(bgav_stream_t*s)
  {
  mpeg2_priv_t * priv;
  bgav_packet_t * p;
  priv = s->data.video.decoder->priv;

  if(priv->p)
    {
    bgav_stream_done_packet_read(s, priv->p);
    priv->p = NULL;
    }
  priv->non_b_count = 0;
  priv->flags &= ~FLAG_EOF;
  priv->flags |= FLAG_NEED_SEQUENCE;
    
  bgav_pts_cache_clear(&priv->pts_cache);

  mpeg2_reset(priv->dec, 0);
  //  mpeg2_buffer(priv->dec, NULL, NULL);

  //  fprintf(stderr, "resync_mpeg2\n");
  
  if(!(s->flags & STREAM_STILL_MODE))
    {
    //  mpeg2_skip(priv->dec, 1);
    
    while(1)
      {
      /* Skip pictures until we have the next keyframe */
      p = bgav_stream_peek_packet_read(s, 1);
      if(!p)
        return;

      if(PACKET_GET_KEYFRAME(p))
        {
        s->out_time = p->pts;
        break;
        }
      /* Skip this packet */
      p = bgav_stream_get_packet_read(s);
      bgav_stream_done_packet_read(s, p);
      }
    }
  //  mpeg2_skip(priv->dec, 0);
  }
  
static void close_mpeg2(bgav_stream_t*s)
  {
  mpeg2_priv_t * priv;
  priv = s->data.video.decoder->priv;

  if(priv->p)
    bgav_packet_pool_put(s->pp, priv->p);
  
  if(priv->frame)
    {
    gavl_video_frame_null(priv->frame);
    gavl_video_frame_destroy(priv->frame);
    }
  
  if(priv->dec)
    mpeg2_close(priv->dec);
  
  free(priv);

  }

static int skipto_mpeg2(bgav_stream_t * s, int64_t time, int exact)
  {
  mpeg2_priv_t * priv = s->data.video.decoder->priv;
  while(1)
    {
    /* TODO: Skip B-frames */
    //    fprintf(stderr, "skipto_mpeg2 %ld %ld\n", time, priv->picture_timestamp);
    if(!decode_picture(s))
      return 0;

    s->flags |= STREAM_HAVE_FRAME;

    if(priv->picture_timestamp + priv->picture_duration > time)
      {
      s->out_time = priv->picture_timestamp;
      return 1;
      }
    }
  return 1;
  }

static bgav_video_decoder_t decoder =
  {
    .fourccs =
    (uint32_t[]){ /* Set by MPEG demuxers */
    BGAV_MK_FOURCC('m','p','v','1'), // MPEG-1
    BGAV_MK_FOURCC('m','p','v','2'), // MPEG-2
    BGAV_MK_FOURCC('m','p','g','v'), // MPEG-1/2
    /* Quicktime fourccs */
    BGAV_MK_FOURCC('h','d','v','1'), // HDV 720p30
    BGAV_MK_FOURCC('h','d','v','2'), // 1080i60 25 Mbps CBR
    BGAV_MK_FOURCC('h','d','v','3'), // 1080i50 25 Mbps CBR
    BGAV_MK_FOURCC('h','d','v','5'), // HDV 720p25
    BGAV_MK_FOURCC('h','d','v','6'), // 1080p24 25 Mbps CBR
    BGAV_MK_FOURCC('h','d','v','7'), // 1080p25 25 Mbps CBR
    BGAV_MK_FOURCC('h','d','v','8'), // 1080p30 25 Mbps CBR
    BGAV_MK_FOURCC('x','d','v','1'), // XDCAM EX 720p30 VBR
    BGAV_MK_FOURCC('x','d','v','2'), // XDCAM HD 1080i60 VBR
    BGAV_MK_FOURCC('x','d','v','3'), // XDCAM HD 1080i50 VBR
    BGAV_MK_FOURCC('x','d','v','4'), // XDCAM EX 720p24 VBR
    BGAV_MK_FOURCC('x','d','v','5'), // XDCAM EX 720p25 VBR
    BGAV_MK_FOURCC('x','d','v','6'), // XDCAM HD 1080p24 VBR
    BGAV_MK_FOURCC('x','d','v','7'), // XDCAM HD 1080p25 VBR
    BGAV_MK_FOURCC('x','d','v','8'), // XDCAM HD 1080p30 VBR
    BGAV_MK_FOURCC('x','d','v','9'), // XDCAM EX 720p60 VBR
    BGAV_MK_FOURCC('x','d','v','a'), // XDCAM EX 720p50 VBR
    BGAV_MK_FOURCC('x','d','v','b'), // XDCAM EX 1080i60 VBR
    BGAV_MK_FOURCC('x','d','v','c'), // XDCAM EX 1080i50 VBR
    BGAV_MK_FOURCC('x','d','v','d'), // XDCAM EX 1080p24 VBR
    BGAV_MK_FOURCC('x','d','v','e'), // XDCAM EX 1080p25 VBR
    BGAV_MK_FOURCC('x','d','v','f'), // XDCAM EX 1080p30 VBR
    BGAV_MK_FOURCC('m','x','3','p'), // IMX PAL 30 MBps
    BGAV_MK_FOURCC('m','x','4','p'), // IMX PAL 40 MBps
    BGAV_MK_FOURCC('m','x','5','p'), // IMX PAL 50 MBps
    BGAV_MK_FOURCC('m','x','3','n'), // IMX NTSC 30 MBps
    BGAV_MK_FOURCC('m','x','4','n'), // IMX NTSC 40 MBps
    BGAV_MK_FOURCC('m','x','5','n'), // IMX NTSC 50 MBps
    0x00 },
    .name =    "libmpeg2 decoder",
    .init =    init_mpeg2,
    .decode =  decode_mpeg2,
    .close =   close_mpeg2,
    .resync =  resync_mpeg2,
    .skipto =  skipto_mpeg2,
  };

void bgav_init_video_decoders_libmpeg2()
  {
  bgav_video_decoder_register(&decoder);
  }
