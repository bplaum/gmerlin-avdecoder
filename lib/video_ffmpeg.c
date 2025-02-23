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



#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <bswap.h>
#include <avdec_private.h>
#include <codecs.h>
// #include <ptscache.h>

#include <stdio.h>
#include <pthread.h>

#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>

// #undef HAVE_LIBVA


#ifdef HAVE_LIBVA
#include <va/va.h>
#include <libavutil/hwcontext_vaapi.h>
#include <gavl/hw_vaapi.h>
#include <gavl/hw_vaapi_x11.h>
#endif

/* TODO: configure check */
// #define HAVE_DRM


#include <gavl/gavldsp.h>


#include <dvframe.h>
#include <mpeg4_header.h>

#include <libavutil/pixdesc.h>


#define LOG_DOMAIN "ffmpeg_video"

// #define DUMP_DECODE
// #define DUMP_EXTRADATA
// #define DUMP_PACKET

#define HAS_DELAY       (1<<0)
#define SWAP_FIELDS_IN  (1<<1)
#define SWAP_FIELDS_OUT (1<<2)
#define MERGE_FIELDS    (1<<3)
#define FLIP_Y          (1<<4)
#define B_REFERENCE     (1<<5) // B-frames can be reference frames (H.264 only for now)
#define GOT_EOS         (1<<6) // Got end of sequence
#define NEED_FORMAT     (1<<7)
#define FLUSH_EOF       (1<<8)
#define HAVE_DR          (1<<9)

/* Skip handling */

#define SKIP_MODE_NONE 0 // No Skipping
#define SKIP_MODE_FAST 1 // Skip all B frames
#define SKIP_MODE_SLOW 2 // 

#define COUNT_PACKETS  
#define COUNT_FRAMES  

static gavl_pixelformat_t get_pixelformat(enum AVPixelFormat p,
                                          gavl_pixelformat_t pixelformat);


typedef struct
  {
  const char * decoder_name;
  const char * format_name;
  enum AVCodecID ffmpeg_id;
  const uint32_t * fourccs;
  } codec_info_t;


typedef struct
  {
  AVBufferRef *refs[GAVL_MAX_PLANES];
  } pool_el_t;

typedef struct
  {
  AVCodecContext * ctx;
  AVFrame * frame;
  //  enum CodecID ffmpeg_id;
  codec_info_t * info;
  gavl_video_frame_t * gavl_frame;
  gavl_video_frame_t * gavl_frame_priv;

  pool_el_t * pool_el;
  int pool_el_alloc;
  
  /* Real video ugliness */

  uint32_t rv_extradata[2+AV_INPUT_BUFFER_PADDING_SIZE/4];

#if LIBAVCODEC_VERSION_MAJOR < 54
  AVPaletteControl palette;
#endif
  

  uint8_t * extradata;
  int extradata_size;
  
  /* State variables */
  int flags;


  
  gavl_video_frame_t * flip_frame; /* Only used if we flip AND do postprocessing */
  
  /* Swap fields for MJPEG-A/B bottom first */
  gavl_video_frame_t  * src_field;
  gavl_video_frame_t  * dst_field;
  gavl_video_format_t field_format[2];
  
  /* */

  gavl_packet_pts_cache_t * pts_cache;
  
  //  int64_t picture_timestamp;
  //  int     picture_duration;
  //  gavl_timecode_t picture_timecode;
  gavl_interlace_mode_t picture_interlace;
  
  int64_t skip_time;
  int skip_mode;
  
  AVPacket * pkt;

  bgav_packet_t * p;

  void (*put_frame)(bgav_stream_t * s, gavl_video_frame_t * f);

  gavl_hw_context_t * hwctx;

  AVBufferRef * devctx;


#ifdef COUNT_PACKETS
  int packet_count_o;
  int packet_count_n;
#endif

#ifdef COUNT_FRAMES
  int frame_count;
#endif
  
  gavl_dsp_context_t * dsp;

  gavl_video_frame_pool_t * fp;
  
  } ffmpeg_video_priv;


#ifdef HAVE_LIBVA // NEW libva API
static int vaapi_supported(const AVCodec *codec)
  {
  const AVCodecHWConfig * cfg = NULL;
  int idx = 0;
  
  //  const AVHWAccel *hwaccel = NULL;

  /*
   *  Right now, libva and MPEG video (DVD) makes screwed up pictures.
   *  Retry with later ffmpegs!
   */
  if((codec->id == AV_CODEC_ID_MPEG1VIDEO) ||
     (codec->id == AV_CODEC_ID_MPEG2VIDEO))
    return 0;

  while((cfg = avcodec_get_hw_config(codec, idx)))
    {
    if((cfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
       (cfg->device_type == AV_HWDEVICE_TYPE_VAAPI) &&
       (cfg->pix_fmt == AV_PIX_FMT_VAAPI))
      {
      return 1;
      }
    
    idx++;
    }
  return 0;
  }

static int init_vaapi(bgav_stream_t * s)
  {
  AVHWDeviceContext* c;
  AVVAAPIDeviceContext * vaapi_ctx;
  ffmpeg_video_priv * priv = s->decoder_priv;
  
  if(!priv->hwctx &&
     !(priv->hwctx = gavl_hw_ctx_create_vaapi_x11(NULL)))
    return 0;

  priv->devctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);

  c = (AVHWDeviceContext*)priv->devctx->data;
  
  vaapi_ctx = c->hwctx;
  
  vaapi_ctx->display = gavl_hw_ctx_vaapi_x11_get_va_display(priv->hwctx);
  
  if(av_hwdevice_ctx_init(priv->devctx))
    return 0;

  priv->ctx->hw_device_ctx = av_buffer_ref(priv->devctx);
  
  return 1;
  
  }

static void put_frame_vaapi(bgav_stream_t * s, gavl_video_frame_t * f1)
  {
  //  VAStatus result;
  ffmpeg_video_priv * priv = s->decoder_priv;

  //  id = (VASurfaceID)(uintptr_t)priv->frame->data[0];

  s->vframe = priv->gavl_frame;
  priv->gavl_frame->storage = (VASurfaceID*)(&priv->frame->data[3]);

  // fprintf(stderr, "put_frame_vaapi %08x\n", *((VASurfaceID*)priv->gavl_frame->user_data));
  
  priv->gavl_frame->hwctx = priv->hwctx;
  }


#endif

static void put_frame_fp(bgav_stream_t * s, gavl_video_frame_t * f1)
  {
  //  VAStatus result;
  ffmpeg_video_priv * priv = s->decoder_priv;

  //  id = (VASurfaceID)(uintptr_t)priv->frame->data[0];

  s->vframe = priv->gavl_frame;
  }


static codec_info_t * lookup_codec(bgav_stream_t * s);

static gavl_source_status_t 
get_data(bgav_stream_t * s, bgav_packet_t ** ret_p)
  {
  bgav_packet_t * ret;
  gavl_source_status_t st;
  ffmpeg_video_priv * priv = s->decoder_priv;
  
  if((st = bgav_stream_get_packet_read(s, ret_p)) != GAVL_SOURCE_OK)
    return st;
  
  ret = *ret_p;

#ifdef COUNT_PACKETS
  if(ret->flags & GAVL_PACKET_NOOUTPUT)
    priv->packet_count_n++;
  else
    priv->packet_count_o++;
#endif
  
#ifdef DUMP_PACKET
  fprintf(stderr, "video_ffmpeg: Got packet ");
  gavl_packet_dump(ret);
#endif
  
  if((priv->flags & SWAP_FIELDS_IN) && (ret->field2_offset))
    {
    if(!priv->p)
      priv->p = bgav_packet_create();
    
    gavl_packet_alloc(priv->p, ret->buf.len);

    priv->p->field2_offset =
      ret->buf.len - ret->field2_offset;
    
    /* Second field -> first field */
    memcpy(priv->p->buf.buf,
           ret->buf.buf + ret->field2_offset,
           ret->buf.len - ret->field2_offset);

    /* First field -> second field */
    memcpy(priv->p->buf.buf + priv->p->field2_offset,
           ret->buf.buf,
           ret->field2_offset);
    gavl_packet_copy_metadata(priv->p, ret);
    priv->p->buf.len = ret->buf.len;
    bgav_stream_done_packet_read(s, ret);
    *ret_p = priv->p;
    }
  
  if(priv->flags & MERGE_FIELDS)
    ret->field2_offset = 0;
  
  return GAVL_SOURCE_OK;
  }

static void done_data(bgav_stream_t * s, bgav_packet_t * p)
  {
  ffmpeg_video_priv * priv = s->decoder_priv;
  if(p != priv->p)
    bgav_stream_done_packet_read(s, p);
  }

static void get_format(AVCodecContext * ctx, gavl_video_format_t * format);
static void init_put_frame(bgav_stream_t * s);


/* Codec specific hacks */

#if 0
static int frame_dumped = 0;
static void dump_frame(uint8_t * data, int len)
  {
  FILE * out;
  if(frame_dumped)
    return;
  frame_dumped = 1;
  out = fopen("frame.dat", "wb");
  fwrite(data, 1, len, out);
  fclose(out);
  }
#endif

static void update_palette(bgav_stream_t * s, gavl_palette_t * palette)
  {
  uint32_t * pal_i;
  int i, imax;
  ffmpeg_video_priv * priv;
  
  priv = s->decoder_priv;

  imax =
    (palette->num_entries > AVPALETTE_COUNT)
    ? AVPALETTE_COUNT : palette->num_entries;
  
  gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN,
           "Got palette %d entries", palette->num_entries);
      
  pal_i =
    (uint32_t*)av_packet_new_side_data(priv->pkt, AV_PKT_DATA_PALETTE,
                                       AVPALETTE_COUNT * 4);

  for(i = 0; i < imax; i++)
    {
    pal_i[i] =
      ((palette->entries[i].a >> 8) << 24) |
      ((palette->entries[i].r >> 8) << 16) |
      ((palette->entries[i].g >> 8) << 8) |
      ((palette->entries[i].b >> 8));
    }
  for(i = imax; i < AVPALETTE_COUNT; i++)
    pal_i[i] = 0;
  
  }

static gavl_source_status_t get_packet(bgav_stream_t * s)
  {
  bgav_packet_t * p;
  gavl_source_status_t st;
  ffmpeg_video_priv * priv;
  //  bgav_pts_cache_entry_t * e;
  gavl_palette_t * palette;
  
  priv = s->decoder_priv;

  while(1)
    {
    priv->pkt->data = NULL;
    priv->pkt->size = 0;
    
    /* Read data */
    p = NULL;
 
    if(!(priv->flags & GOT_EOS))
      {
      st = get_data(s, &p);

      switch(st)
        {
        case GAVL_SOURCE_EOF:
          //          fprintf(stderr, "*** EOF 1\n");
          p = NULL;
          break;
        case GAVL_SOURCE_AGAIN:
          return st;
          break;
        case GAVL_SOURCE_OK:
          break;
        }
      }
    else
      p = NULL;
    
    /* Early EOF detection */
    if(!p)
      {
      //   fprintf(stderr, "Flushing decoder\n");
      avcodec_send_packet(priv->ctx, NULL);
      priv->flags |= FLUSH_EOF;
      return GAVL_SOURCE_EOF;
      }
    /* Check what to skip */
    
    if(p->pts == GAVL_TIME_UNDEFINED)
      {
      done_data(s, p);
      // fprintf(stderr, "Skipping frame (fast)\n");
      continue;
      }
    
    if(priv->skip_mode == SKIP_MODE_FAST)
      {
      /* Didn't have "our" I/P-frame yet: Don't even look at this */
      if((PACKET_GET_CODING_TYPE(p) == GAVL_PACKET_TYPE_B) &&
         !PACKET_GET_REF(p))
        {
        done_data(s, p);
        // fprintf(stderr, "Skipping frame (fast)\n");
        continue;
        }
      else if(p->pts + p->duration >= priv->skip_time)
        priv->skip_mode = SKIP_MODE_SLOW;
      }

    if(priv->skip_mode)
      {
      if(PACKET_GET_CODING_TYPE(p) == GAVL_PACKET_TYPE_B)
        {
        if(!(priv->flags & B_REFERENCE) &&
           (p->pts + p->duration < priv->skip_time))
          {
          done_data(s, p);
          // fprintf(stderr, "Skipping frame (fast)\n");
          continue;
          }
        }
      }
    
    break;
    }

  if((priv->ctx->skip_frame == AVDISCARD_DEFAULT) &&
     !(p->flags & GAVL_PACKET_NOOUTPUT))
    gavl_packet_pts_cache_push_packet(priv->pts_cache, p);
  
  priv->pkt->data = p->buf.buf;
  if(p->field2_offset)
    priv->pkt->size = p->field2_offset;
  else
    priv->pkt->size = p->buf.len;
      
  /* Palette handling */

  if((palette = gavl_packet_get_extradata(p, GAVL_PACKET_EXTRADATA_PALETTE)))
    {
    update_palette(s, palette);
    }
  
  /* Check for EOS */
  if(p->sequence_end_pos > 0)
    priv->flags |= GOT_EOS;

  avcodec_send_packet(priv->ctx, priv->pkt);

#ifdef DUMP_DECODE
  gavl_dprintf("Used %d bytes", priv->pkt->size);
#endif

  done_data(s, p);
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t decode_picture(bgav_stream_t * s)
  {
  ffmpeg_video_priv * priv;
  //  bgav_pts_cache_entry_t * e;
  gavl_source_status_t st;
  int result;

  priv = s->decoder_priv;
  
  if(priv->flags & GOT_EOS)
    {
    avcodec_flush_buffers(priv->ctx);
    priv->flags &= ~GOT_EOS;
    }
  
  while(1)
    {
    result = avcodec_receive_frame(priv->ctx, priv->frame);
    
    if(!result)
      {
      /* Got frame */
      st = GAVL_SOURCE_OK;
      break;
      }
    else if(result == AVERROR(EAGAIN))
      {
      /* Get data, flush the decoder after the last packet */
      st = get_packet(s);

      /* Nothing we can do right now */
      if(st == GAVL_SOURCE_AGAIN)
        return st;
      }
    else if(result == AVERROR_EOF)
      {
      st = GAVL_SOURCE_EOF;
      break;
      }
    else
      {
      char errbuf[128];
      av_strerror(result, errbuf, 128);
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Got decoder error: %s\n", errbuf);
      return GAVL_SOURCE_EOF;
      }
    }
  
#ifdef DUMP_DECODE
  gavl_dprintf("Got frame: Interlaced: %d TFF: %d Repeat: %d, framerate: %f\n",
               priv->frame->interlaced_frame,
               priv->frame->top_field_first,
               priv->frame->repeat_pict,
               (float)(priv->ctx->time_base.den) / (float)(priv->ctx->time_base.num)
               );
#endif

#ifdef COUNT_FRAMES
  priv->frame_count++;
#endif
  
    /* Ugly hack: Need to free the side data elements manually because
       ffmpeg has no public API for that */
#if LIBAVCODEC_VERSION_MAJOR >= 54
  if(priv->pkt->side_data_elems)
    {
    av_free(priv->pkt->side_data[0].data);
    av_freep(&priv->pkt->side_data);
    priv->pkt->side_data_elems = 0;
    }
#endif

#if 0 // TODO: Decode second field ()if we need this at all */
    
  /* Decode 2nd field for field pictures */
  if(p && p->field2_offset && (bytes_used > 0))
    {
    priv->pkt->data = p->buf.buf + p->field2_offset;
    priv->pkt->size = p->buf.len - p->field2_offset;
    
#ifdef DUMP_DECODE
    gavl_dprintf("Decode (f2): out_time: %" PRId64 " len: %d\n", s->out_time,
                 priv->pkt.size);
    if(priv->pkt.data)
      gavl_hexdump(priv->pkt.data, 16, 16);
#endif
    
    bytes_used = avcodec_decode_video2(priv->ctx,
                                       priv->frame,
                                       &have_picture,
                                       &priv->pkt);
#ifdef DUMP_DECODE
    gavl_dprintf("Used %d/%d bytes, got picture: %d ",
                 bytes_used, priv->pkt.size, have_picture);
    if(!have_picture)
      gavl_dprintf("\n");
    else
      {
      gavl_dprintf("Interlaced: %d TFF: %d Repeat: %d, framerate: %f",
                   priv->frame->interlaced_frame,
                   priv->frame->top_field_first,
                   priv->frame->repeat_pict,
                   (float)(priv->ctx->time_base.den) / (float)(priv->ctx->time_base.num)
                   );
      
      gavl_dprintf("\n");
      }
#endif
    }
#endif // End (second field
    
  if(!result)
    {
    int i;
    s->flags |= STREAM_HAVE_FRAME; 

    if(priv->flags & HAVE_DR)
      {
      priv->gavl_frame = priv->frame->opaque;
      }
    else
      {
      /* Set our internal frame */
      for(i = 0; i < 3; i++)
        {
        priv->gavl_frame->planes[i]  = priv->frame->data[i];
        priv->gavl_frame->strides[i] = priv->frame->linesize[i];
        }
      }

    gavl_packet_pts_cache_get_first(priv->pts_cache, priv->gavl_frame);
    
    if(gavl_interlace_mode_is_mixed(s->data.video.format->interlace_mode))
      {
/* Newer version */
#if defined (AV_FRAME_FLAG_INTERLACED) && defined (AV_FRAME_FLAG_TOP_FIELD_FIRST)
      if(priv->frame->flags & AV_FRAME_FLAG_INTERLACED)
        {
        if(priv->frame->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST)
          priv->gavl_frame->interlace_mode = GAVL_INTERLACE_TOP_FIRST;
        else
          priv->gavl_frame->interlace_mode = GAVL_INTERLACE_BOTTOM_FIRST;
        
        }
#else
      if(priv->frame->interlaced_frame)
        {
        if(priv->frame->top_field_first)
          priv->gavl_frame->interlace_mode = GAVL_INTERLACE_TOP_FIRST;
        else
          priv->gavl_frame->interlace_mode = GAVL_INTERLACE_BOTTOM_FIRST;
        }
#endif
      else
        priv->gavl_frame->interlace_mode = GAVL_INTERLACE_NONE;
      }
    }
  return GAVL_SOURCE_OK;
  }

static int skipto_ffmpeg(bgav_stream_t * s, int64_t time)
  {
  ffmpeg_video_priv * priv;
  
  priv = s->decoder_priv;
  priv->skip_time = time;
  priv->skip_mode = SKIP_MODE_FAST;
  
  while(1)
    {
    if(decode_picture(s) != GAVL_SOURCE_OK)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Got EOF while skipping");
      s->flags &= ~STREAM_HAVE_FRAME;
      return 0;
      }
#if 0
    fprintf(stderr, "Skipto ffmpeg %"PRId64" %"PRId64" %d\n",
            priv->picture_timestamp, time, exact);
#endif
    
    if(priv->gavl_frame->timestamp + priv->gavl_frame->duration > time)
      break;
    }
  priv->skip_time = GAVL_TIME_UNDEFINED;
  priv->skip_mode = SKIP_MODE_NONE;
  s->out_time = priv->gavl_frame->timestamp;
  return 1;
  }

static gavl_source_status_t 
decode_ffmpeg(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  gavl_source_status_t st;
  ffmpeg_video_priv * priv;
  /* We get the DV format info ourselfes, since the values
     ffmpeg returns are not reliable */
  priv = s->decoder_priv;
  
  if(!(s->flags & STREAM_HAVE_FRAME))
    {
    if((st = decode_picture(s)) != GAVL_SOURCE_OK)
      {
      //      fprintf(stderr, "*** EOF 5\n");
      return st;
      }
    }
  if(s->flags & STREAM_HAVE_FRAME)
    {
    if(priv->put_frame)
      {
      priv->put_frame(s, f);
      /* Set frame metadata */
      if(f)
        gavl_video_frame_copy_metadata(f, priv->gavl_frame);
      }
    }
  else if(!(priv->flags & NEED_FORMAT))
    {
    //    fprintf(stderr, "*** EOF 4\n");
    return GAVL_SOURCE_EOF; /* EOF */
    }
  return GAVL_SOURCE_OK;
  }

// #define find_decoder(id, s) avcodec_find_decoder(id)

static const AVCodec * find_decoder(bgav_stream_t * s, enum AVCodecID id)
  {
  return avcodec_find_decoder(id);
  }

#if 0


static void buffer_free_func(void *opaque, uint8_t *data)
  {
  /* NOP */
  }

static int get_buffer(struct AVCodecContext *ctx, AVFrame *frame, int flags)
  {
  gavl_video_frame_t * vframe;
  ffmpeg_video_priv * priv;
  bgav_stream_t * s = ctx->opaque;
  int i;
  int frame_height;
  const gavl_video_format_t * vfmt;

  priv = s->decoder_priv;
  
  if(!priv->fp)
    {
    gavl_video_format_t fmt;
    priv->fp = gavl_video_frame_pool_create(priv->hwctx);

    /* Set format */
    fmt.image_width = priv->ctx->width;
    fmt.image_height = priv->ctx->height;
    fmt.pixelformat = get_pixelformat(priv->ctx->pix_fmt, GAVL_PIXELFORMAT_NONE);
    
    gavl_video_frame_pool_set_format(priv->fp, &fmt);
    }
  
  memset(frame->data, 0, sizeof(frame->data));
  frame->extended_data = frame->data;

  vframe = gavl_video_frame_pool_get(priv->fp);

  if(vframe->buf_idx >= priv->pool_el_alloc)
    {
    int inc = vframe->buf_idx - priv->pool_el_alloc + 1;
    inc = ((inc + 7)/8)*8;
    
    priv->pool_el = realloc(priv->pool_el,
                            (priv->pool_el_alloc + inc)*sizeof(*priv->pool_el));
    memset(priv->pool_el + priv->pool_el_alloc, 0, inc*sizeof(*priv->pool_el));
    priv->pool_el_alloc += inc;
    }

  if(!priv->pool_el[vframe->buf_idx].refs[0])
    {
    vfmt = gavl_video_frame_pool_get_format(priv->fp);
    frame_height = vfmt->frame_height;
  
    for(i = 0; i < GAVL_MAX_PLANES; i++)
      {
      if(!vframe->strides[i])
        break;

      if(i == 1)
        {
        int sub_v;
        gavl_pixelformat_chroma_sub(vfmt->pixelformat, NULL, &sub_v);
        frame_height /= sub_v;
        }
    
      frame->linesize[i] = vframe->strides[i];

      priv->pool_el[vframe->buf_idx].refs[i] = 
        av_buffer_create(vframe->planes[i],
                         vframe->strides[i] * frame_height,
                         buffer_free_func,
                         NULL, 0);
    
      }
    }
  else
    {
    for(i = 0; i < GAVL_MAX_PLANES; i++)
      {
      if(!vframe->strides[i])
        break;

      frame->buf[i]  = av_buffer_ref(priv->pool_el[vframe->buf_idx].refs[i]);
      frame->data[i] = frame->buf[i]->data;
      }
    }
  return 0;
  }
#endif

static int init_ffmpeg(bgav_stream_t * s)
  {
  const AVCodec * codec;
  
  ffmpeg_video_priv * priv;

  AVDictionary * options = NULL;
  
  //  av_log_set_level(AV_LOG_DEBUG);
  
  priv = calloc(1, sizeof(*priv));
  priv->skip_time = GAVL_TIME_UNDEFINED;

  priv->pts_cache = gavl_packet_pts_cache_create(-1);
  
  s->decoder_priv = priv;
  
  /* Set up coded specific details */
  
  if(s->fourcc == BGAV_MK_FOURCC('W','V','1','F'))
    s->flags |= FLIP_Y;
  
  priv->info = lookup_codec(s);

  priv->ctx = avcodec_alloc_context3(NULL);

  codec = find_decoder(s, priv->info->ffmpeg_id);

  
  priv->ctx->width = s->data.video.format->frame_width;
  priv->ctx->height = s->data.video.format->frame_height;
  priv->ctx->bits_per_coded_sample = s->data.video.depth;

  /* Setting codec tag with Nuppelvideo crashes */
  //  if(s->fourcc != BGAV_MK_FOURCC('R', 'J', 'P', 'G'))
#if 1
    {
    priv->ctx->codec_tag   =
      ((s->fourcc & 0x000000ff) << 24) |
      ((s->fourcc & 0x0000ff00) << 8) |
      ((s->fourcc & 0x00ff0000) >> 8) |
      ((s->fourcc & 0xff000000) >> 24);
    }
#endif
  priv->ctx->codec_id = codec->id;

  /* Threads (disabled for VAAPI) */
  priv->ctx->thread_count = gavl_num_cpus();
  
#ifdef HAVE_LIBVA
  if(s->opt->vaapi && vaapi_supported(codec) && init_vaapi(s))
    {
    priv->ctx->thread_count = 1;
    }
  else
#endif

#if 0    
  if(codec->capabilities & AV_CODEC_CAP_DR1)
    {
    fprintf(stderr, "Direct rendering supported\n");

    priv->ctx->get_buffer2 = get_buffer;
    priv->flags |= HAVE_DR;
    
    /* TODO: Allocate HW context */
    
    }
#endif
  
  /* Check if there might be B-frames */
  if(codec->capabilities & AV_CODEC_CAP_DELAY)
    priv->flags |= HAS_DELAY;

  /* Check if B-frames might be references */
  if(codec->id == AV_CODEC_ID_H264)
    priv->flags |= B_REFERENCE;
  
  priv->ctx->opaque = s;
  
  if(s->ci->codec_header.len)
    {
    priv->extradata = calloc(s->ci->codec_header.len + AV_INPUT_BUFFER_PADDING_SIZE, 1);
    memcpy(priv->extradata, s->ci->codec_header.buf, s->ci->codec_header.len);
    priv->extradata_size = s->ci->codec_header.len;

    //    if(bgav_video_is_divx4(s->fourcc))
    //      bgav_mpeg4_remove_packed_flag(priv->extradata, &priv->extradata_size, &priv->extradata_size);
    
    priv->ctx->extradata      = priv->extradata;
    priv->ctx->extradata_size = priv->extradata_size;
    
#ifdef DUMP_EXTRADATA
    gavl_dprintf("video_ffmpeg: Adding extradata %d bytes\n",
                 priv->ctx->extradata_size);
    gavl_hexdump(priv->ctx->extradata, priv->ctx->extradata_size, 16);
#endif
    }
    
  priv->ctx->codec_type = AVMEDIA_TYPE_VIDEO;
  
  priv->ctx->bit_rate = 0;

  priv->ctx->time_base.den = s->data.video.format->timescale;
  priv->ctx->time_base.num = s->data.video.format->frame_duration;

  /* Build the palette from the stream info */
  
  //  if(s->data.video.palette_size)

  
  //  gavl_hexdump(s->ci->global_header, s->ci->global_header_len, 16);
  
  priv->frame = av_frame_alloc();

  if(!(priv->flags & HAVE_DR))
    {
    priv->gavl_frame_priv = gavl_video_frame_create(NULL);
    priv->gavl_frame_priv = priv->gavl_frame_priv;
    priv->gavl_frame = priv->gavl_frame_priv;
    }
  
  priv->pkt = av_packet_alloc();
  
  /* Some codecs need extra stuff */

  /* Swap fields for Quicktime Motion JPEG */
  if(s->fourcc == BGAV_MK_FOURCC('m','j','p','a'))
    {
    priv->flags |= MERGE_FIELDS;
    if(s->data.video.format->interlace_mode == GAVL_INTERLACE_BOTTOM_FIRST)
      priv->flags |= SWAP_FIELDS_IN;
    }
  
  if((s->fourcc == BGAV_MK_FOURCC('m','j','p','b')))
    {
    if(s->data.video.format->interlace_mode == GAVL_INTERLACE_BOTTOM_FIRST)
      {
#if 1
      // #if LIBAVCODEC_VERSION_INT < ((53<<16)|(32<<8)|2)
      priv->flags |= SWAP_FIELDS_OUT;
      priv->src_field = gavl_video_frame_create(NULL);
      priv->dst_field = gavl_video_frame_create(NULL);
#else
      priv->ctx->field_order = AV_FIELD_BB;
#endif
      }
    }

  
  /* Huffman tables for Motion jpeg */

  if(((s->fourcc == BGAV_MK_FOURCC('A','V','R','n')) ||
      (s->fourcc == BGAV_MK_FOURCC('M','J','P','G'))) &&
     priv->ctx->extradata_size)
    {
    av_dict_set(&options, "extern_huff", "1", 0);
    }
  
  priv->ctx->workaround_bugs = FF_BUG_AUTODETECT;
  priv->ctx->error_concealment = 3;
  
  //  priv->ctx->error_resilience = 3;
  
  /* Open this thing */

  bgav_ffmpeg_lock();
  
  if(avcodec_open2(priv->ctx, codec, &options) != 0)
    {
    bgav_ffmpeg_unlock();
    return 0;
    }
  bgav_ffmpeg_unlock();
  
  //  priv->ctx->skip_frame = AVDISCARD_NONREF;
  //  priv->ctx->skip_loop_filter = AVDISCARD_ALL;
  //  priv->ctx->skip_idct = AVDISCARD_ALL;
  
  /* Set missing format values */
  
  priv->flags |= NEED_FORMAT;
  
  if(decode_picture(s) != GAVL_SOURCE_OK)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Could not get initial frame");
    return 0;
    }

     
  get_format(priv->ctx, s->data.video.format);

  priv->flags &= ~NEED_FORMAT;
      
  
  /* Handle unsupported colormodels */
  if(s->data.video.format->pixelformat == GAVL_PIXELFORMAT_NONE)
    {
    const AVPixFmtDescriptor * d = av_pix_fmt_desc_get(priv->ctx->pix_fmt);

    if(d)
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Unsupported pixelformat %s", d->name);
    else
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Codec initialization failed");
    
    return 0;
    }

  if(priv->flags & SWAP_FIELDS_OUT)
    {
    gavl_get_field_format(s->data.video.format,
                          &priv->field_format[0], 0);
    gavl_get_field_format(s->data.video.format,
                          &priv->field_format[1], 1);
    }
  
  av_dict_free(&options);

  if(!gavl_dictionary_get_string(s->m, GAVL_META_FORMAT))
    gavl_dictionary_set_string(s->m, GAVL_META_FORMAT,
                      priv->info->format_name);

  init_put_frame(s);

  if(!priv->put_frame)
    s->vframe = priv->gavl_frame;
  
  return 1;
  }

static void resync_ffmpeg(bgav_stream_t * s)
  {
  ffmpeg_video_priv * priv;
  priv = s->decoder_priv;
  
  avcodec_flush_buffers(priv->ctx);
  
  gavl_packet_pts_cache_clear(priv->pts_cache);
  
  }

static void close_ffmpeg(bgav_stream_t * s)
  {
  ffmpeg_video_priv * priv;
  priv= s->decoder_priv;

  if(!priv)
    return;

#ifdef COUNT_PACKETS
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got %d normal packets", priv->packet_count_o);
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got %d noout packets", priv->packet_count_n);
#endif

#ifdef COUNT_FRAMES
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got %d frames", priv->frame_count);
#endif
  
  if(priv->frame)
    {
    //    av_frame_unref(priv->frame); // Not necessary? At least valgrind doesn't complain
    av_frame_free(&priv->frame);
    }

  if(priv->pkt)
    {
    //    av_frame_unref(priv->frame); // Not necessary? At least valgrind doesn't complain
    av_packet_free(&priv->pkt);
    }
  
  if(priv->ctx)
    {
    bgav_ffmpeg_lock();
    avcodec_close(priv->ctx);
    bgav_ffmpeg_unlock();
    av_free(priv->ctx);
    }
  if(priv->gavl_frame_priv)
    {
    gavl_video_frame_null(priv->gavl_frame_priv);

    priv->gavl_frame_priv->storage = NULL;
    priv->gavl_frame_priv->hwctx = NULL;
    gavl_video_frame_destroy(priv->gavl_frame_priv);
    }

  if(priv->fp)
    gavl_video_frame_pool_destroy(priv->fp);

  if(priv->pool_el)
    {
    int i, j;
    for(i = 0; i < priv->pool_el_alloc; i++)
      {
      for(j = 0; j < GAVL_MAX_PLANES; j++)
        {
        if(priv->pool_el[i].refs[j])
          av_buffer_unref(&priv->pool_el[i].refs[j]);
        }
      }
    free(priv->pool_el);
    }
  
  if(priv->src_field)
    {
    gavl_video_frame_null(priv->src_field);
    gavl_video_frame_destroy(priv->src_field);
    }
  
  if(priv->dst_field)
    {
    gavl_video_frame_null(priv->dst_field);
    gavl_video_frame_destroy(priv->dst_field);
    }

  
  if(priv->hwctx)
    gavl_hw_ctx_destroy(priv->hwctx);

  if(priv->devctx)
    av_buffer_unref(&priv->devctx);
  
  if(priv->p)
    bgav_packet_destroy(priv->p);
  
  if(priv->extradata)
    free(priv->extradata);

  if(priv->dsp)
    gavl_dsp_context_destroy(priv->dsp);

  if(priv->pts_cache)
    gavl_packet_pts_cache_destroy(priv->pts_cache);

  free(priv);
  }

/* Map of ffmpeg codecs to fourccs (from ffmpeg's avienc.c) */

static codec_info_t codec_infos[] =
  {

/*     AV_CODEC_ID_MPEG1VIDEO */
#if 0        
    { "FFmpeg Mpeg1 decoder", "MPEG-1", AV_CODEC_ID_MPEG1VIDEO,
      (uint32_t[]){ BGAV_MK_FOURCC('m', 'p', 'g', '1'), 
               BGAV_MK_FOURCC('m', 'p', 'g', '2'),
               BGAV_MK_FOURCC('P', 'I', 'M', '1'), 
               BGAV_MK_FOURCC('V', 'C', 'R', '2'),
               0x00 } }, 
#endif    
/*     AV_CODEC_ID_MPEG2VIDEO, /\* preferred ID for MPEG-1/2 video decoding *\/ */

/*     AV_CODEC_ID_MPEG2VIDEO_XVMC, */

/*     AV_CODEC_ID_H261, */
#if 0 // http://samples.mplayerhq.hu/V-codecs/h261/h261test.avi: Grey image
      // http://samples.mplayerhq.hu/V-codecs/h261/lotr.mov: Messed up image then crash
      // MPlayer can't play these either
    /************************************************************
     * H261 Variants
     ************************************************************/
    
    { "FFmpeg H261 decoder", "H261", AV_CODEC_ID_H261,
      (uint32_t[]){ BGAV_MK_FOURCC('H', '2', '6', '1'),
               BGAV_MK_FOURCC('h', '2', '6', '1'),
               0x00 } },
#endif    
    
    /*     AV_CODEC_ID_H263, */
    { "FFmpeg H263 decoder", "H263", AV_CODEC_ID_H263,
      (uint32_t[]){ BGAV_MK_FOURCC('H', '2', '6', '3'),
                    BGAV_MK_FOURCC('h', '2', '6', '3'),
                    BGAV_MK_FOURCC('s', '2', '6', '3'),
                    BGAV_MK_FOURCC('u', '2', '6', '3'),
                    BGAV_MK_FOURCC('U', '2', '6', '3'),
                    BGAV_MK_FOURCC('v', 'i', 'v', '1'),
                    0x00 } },

    /*     AV_CODEC_ID_RV10, */
    { "FFmpeg Real Video 1.0 decoder", "Real Video 1.0", AV_CODEC_ID_RV10,
      (uint32_t[]){ BGAV_MK_FOURCC('R', 'V', '1', '0'),
               BGAV_MK_FOURCC('R', 'V', '1', '3'),
               0x00 } },

    /*     AV_CODEC_ID_RV20, */
    { "FFmpeg Real Video 2.0 decoder", "Real Video 2.0", AV_CODEC_ID_RV20,
      (uint32_t[]){ BGAV_MK_FOURCC('R', 'V', '2', '0'),
               0x00 } },

    /*     AV_CODEC_ID_MJPEG, */
    { "FFmpeg motion JPEG decoder", "Motion JPEG", AV_CODEC_ID_MJPEG,
      (uint32_t[]){ BGAV_MK_FOURCC('L', 'J', 'P', 'G'),
                    BGAV_MK_FOURCC('A', 'V', 'R', 'n'),
                    BGAV_MK_FOURCC('j', 'p', 'e', 'g'),
                    BGAV_MK_FOURCC('m', 'j', 'p', 'a'),
                    BGAV_MK_FOURCC('A', 'V', 'D', 'J'),
                    BGAV_MK_FOURCC('M', 'J', 'P', 'G'),
                    BGAV_MK_FOURCC('I', 'J', 'P', 'G'),
                    BGAV_MK_FOURCC('J', 'P', 'G', 'L'),
                    BGAV_MK_FOURCC('L', 'J', 'P', 'G'),
                    BGAV_MK_FOURCC('M', 'J', 'L', 'S'),
                    BGAV_MK_FOURCC('d', 'm', 'b', '1'),
                    BGAV_MK_FOURCC('J', 'F', 'I', 'F'), // SMJPEG
                    0x00 },
    },
    
    /*     AV_CODEC_ID_MJPEGB, */
    { "FFmpeg motion Jpeg-B decoder", "Motion Jpeg B", AV_CODEC_ID_MJPEGB,
      (uint32_t[]){ BGAV_MK_FOURCC('m', 'j', 'p', 'b'),
                    0x00 } },
    
/*     AV_CODEC_ID_LJPEG, */

/*     AV_CODEC_ID_SP5X, */
    { "FFmpeg SP5X decoder", "SP5X Motion JPEG", AV_CODEC_ID_SP5X,
      (uint32_t[]){ BGAV_MK_FOURCC('S', 'P', '5', '4'),
               0x00 } },

/*     AV_CODEC_ID_JPEGLS, */

/*     AV_CODEC_ID_MPEG4, */
    { "FFmpeg MPEG-4 decoder", "MPEG-4", AV_CODEC_ID_MPEG4,
      (uint32_t[]){ BGAV_MK_FOURCC('D', 'I', 'V', 'X'),
               BGAV_MK_FOURCC('d', 'i', 'v', 'x'),
               BGAV_MK_FOURCC('D', 'X', '5', '0'),
               BGAV_MK_FOURCC('X', 'V', 'I', 'D'),
               BGAV_MK_FOURCC('x', 'v', 'i', 'd'),
               BGAV_MK_FOURCC('M', 'P', '4', 'S'),
               BGAV_MK_FOURCC('M', '4', 'S', '2'),
               BGAV_MK_FOURCC(0x04, 0, 0, 0), /* some broken avi use this */
               BGAV_MK_FOURCC('D', 'I', 'V', '1'),
               BGAV_MK_FOURCC('B', 'L', 'Z', '0'),
               BGAV_MK_FOURCC('m', 'p', '4', 'v'),
               BGAV_MK_FOURCC('U', 'M', 'P', '4'),
               BGAV_MK_FOURCC('3', 'I', 'V', '2'),
               BGAV_MK_FOURCC('W', 'V', '1', 'F'),
               BGAV_MK_FOURCC('R', 'M', 'P', '4'),
               BGAV_MK_FOURCC('S', 'E', 'D', 'G'),
               BGAV_MK_FOURCC('S', 'M', 'P', '4'),
               BGAV_MK_FOURCC('3', 'I', 'V', 'D'),
               BGAV_MK_FOURCC('F', 'M', 'P', '4'),
               0x00 } },

    /*     AV_CODEC_ID_RAWVIDEO, */
    { "FFmpeg Raw decoder", "Uncompressed", AV_CODEC_ID_RAWVIDEO,
      (uint32_t[]){ BGAV_MK_FOURCC('Y', '4', '2', '2'),
                    BGAV_MK_FOURCC('I', '4', '2', '0'),
                    0x00 } },
    
    /*     AV_CODEC_ID_MSMPEG4V1, */
    { "FFmpeg MSMPEG4V1 decoder", "Microsoft MPEG-4 V1", AV_CODEC_ID_MSMPEG4V1,
      (uint32_t[]){ BGAV_MK_FOURCC('M', 'P', 'G', '4'),
               BGAV_MK_FOURCC('D', 'I', 'V', '4'),
               0x00 } },

    /*     AV_CODEC_ID_MSMPEG4V2, */
    { "FFmpeg MSMPEG4V2 decoder", "Microsoft MPEG-4 V2", AV_CODEC_ID_MSMPEG4V2,
      (uint32_t[]){ BGAV_MK_FOURCC('M', 'P', '4', '2'),
               BGAV_MK_FOURCC('D', 'I', 'V', '2'),
               0x00 } },

    /*     AV_CODEC_ID_MSMPEG4V3, */
    { "FFmpeg MSMPEG4V3 decoder", "Microsoft MPEG-4 V3", AV_CODEC_ID_MSMPEG4V3,
      (uint32_t[]){ BGAV_MK_FOURCC('D', 'I', 'V', '3'),
               BGAV_MK_FOURCC('M', 'P', '4', '3'), 
               BGAV_MK_FOURCC('m', 'p', '4', '3'), 
               BGAV_MK_FOURCC('M', 'P', 'G', '3'), 
               BGAV_MK_FOURCC('D', 'I', 'V', '5'), 
               BGAV_MK_FOURCC('D', 'I', 'V', '6'), 
               BGAV_MK_FOURCC('D', 'I', 'V', '4'), 
               BGAV_MK_FOURCC('A', 'P', '4', '1'),
               BGAV_MK_FOURCC('C', 'O', 'L', '1'),
               BGAV_MK_FOURCC('C', 'O', 'L', '0'),
               0x00 } },
    
    /*     AV_CODEC_ID_WMV1, */
    { "FFmpeg WMV1 decoder", "Window Media Video 7", AV_CODEC_ID_WMV1,
      (uint32_t[]){ BGAV_MK_FOURCC('W', 'M', 'V', '1'),
               0x00 } }, 

    /*     AV_CODEC_ID_WMV2, */
    { "FFmpeg WMV2 decoder", "Window Media Video 8", AV_CODEC_ID_WMV2,
      (uint32_t[]){ BGAV_MK_FOURCC('W', 'M', 'V', '2'),
               0x00 } }, 
    
    /*     AV_CODEC_ID_H263P, */

    /*     AV_CODEC_ID_H263I, */
    { "FFmpeg H263I decoder", "I263", AV_CODEC_ID_H263I,
      (uint32_t[]){ BGAV_MK_FOURCC('I', '2', '6', '3'), /* intel h263 */
               0x00 } },
    
    /*     AV_CODEC_ID_FLV1, */
    { "FFmpeg Flash video decoder", "Flash Video 1", AV_CODEC_ID_FLV1,
      (uint32_t[]){ BGAV_MK_FOURCC('F', 'L', 'V', '1'),
                    0x00 } },

    /*     AV_CODEC_ID_SVQ1, */
    { "FFmpeg Sorenson 1 decoder", "Sorenson Video 1", AV_CODEC_ID_SVQ1,
      (uint32_t[]){ BGAV_MK_FOURCC('S', 'V', 'Q', '1'),
               BGAV_MK_FOURCC('s', 'v', 'q', '1'),
               BGAV_MK_FOURCC('s', 'v', 'q', 'i'),
               0x00 } },

    /*     AV_CODEC_ID_SVQ3, */
    { "FFmpeg Sorenson 3 decoder", "Sorenson Video 3", AV_CODEC_ID_SVQ3,
      (uint32_t[]){ BGAV_MK_FOURCC('S', 'V', 'Q', '3'),
                    0x00 } },
    
    /*     AV_CODEC_ID_DVVIDEO, */
    { "FFmpeg DV decoder", "DV Video", AV_CODEC_ID_DVVIDEO,
      bgav_dv_fourccs,
    },
    /*     AV_CODEC_ID_HUFFYUV, */
    { "FFmpeg Hufyuv decoder", "Huff YUV", AV_CODEC_ID_HUFFYUV,
      (uint32_t[]){ BGAV_MK_FOURCC('H', 'F', 'Y', 'U'),
                    0x00 } },

    /*     AV_CODEC_ID_CYUV, */
    { "FFmpeg Creative YUV decoder", "Creative YUV", AV_CODEC_ID_CYUV,
      (uint32_t[]){ BGAV_MK_FOURCC('C', 'Y', 'U', 'V'),
                    BGAV_MK_FOURCC('c', 'y', 'u', 'v'),
               0x00 } },
    
    /*     AV_CODEC_ID_H264, */
    { "FFmpeg H264 decoder", "H264", AV_CODEC_ID_H264,
      (uint32_t[]){ BGAV_MK_FOURCC('a', 'v', 'c', '1'),
               BGAV_MK_FOURCC('H', '2', '6', '4'),
               BGAV_MK_FOURCC('h', '2', '6', '4'),
               0x00 } },

    /*     AV_CODEC_ID_H264, */
    { "FFmpeg H265 decoder", "H265", AV_CODEC_ID_HEVC,
      (uint32_t[]){ BGAV_MK_FOURCC('h', 'e', 'v', '1'),
                    BGAV_MK_FOURCC('h', 'v', 'c', '1'),
                    BGAV_MK_FOURCC('H', 'E', 'V', 'C'),
                    0x00 } },

    /*     AV_CODEC_ID_INDEO3, */
    { "FFmpeg Inteo 3 decoder", "Intel Indeo 3", AV_CODEC_ID_INDEO3,
      (uint32_t[]){ BGAV_MK_FOURCC('I', 'V', '3', '1'),
                    BGAV_MK_FOURCC('I', 'V', '3', '2'),
                    BGAV_MK_FOURCC('i', 'v', '3', '1'),
                    BGAV_MK_FOURCC('i', 'v', '3', '2'),
                    0x00 } },

    /*     AV_CODEC_ID_VP3, */
    { "FFmpeg VP3 decoder", "On2 VP3", AV_CODEC_ID_VP3,
      (uint32_t[]){ BGAV_MK_FOURCC('V', 'P', '3', '1'),
                    BGAV_MK_FOURCC('V', 'P', '3', ' '),
               0x00 } },

    /*     AV_CODEC_ID_THEORA, */

    /*     AV_CODEC_ID_ASV1, */
    { "FFmpeg ASV1 decoder", "Asus v1", AV_CODEC_ID_ASV1,
      (uint32_t[]){ BGAV_MK_FOURCC('A', 'S', 'V', '1'),
               0x00 } },
    
    /*     AV_CODEC_ID_ASV2, */
    { "FFmpeg ASV2 decoder", "Asus v2", AV_CODEC_ID_ASV2,
      (uint32_t[]){ BGAV_MK_FOURCC('A', 'S', 'V', '2'),
                    0x00 } },
    
    /*     AV_CODEC_ID_FFV1, */
    
    { "FFmpeg Video 1 (FFV1) decoder", "FFV1", AV_CODEC_ID_FFV1,
      (uint32_t[]){ BGAV_MK_FOURCC('F', 'F', 'V', '1'),
               0x00 } },
    
    /*     AV_CODEC_ID_4XM, */
    { "FFmpeg 4XM video decoder", "4XM Video", AV_CODEC_ID_4XM,
      (uint32_t[]){ BGAV_MK_FOURCC('4', 'X', 'M', 'V'),

                    0x00 } },

    /*     AV_CODEC_ID_VCR1, */
    { "FFmpeg VCR1 decoder", "ATI VCR1", AV_CODEC_ID_VCR1,
      (uint32_t[]){ BGAV_MK_FOURCC('V', 'C', 'R', '1'),
               0x00 } },

    /*     AV_CODEC_ID_CLJR, */
    { "FFmpeg CLJR decoder", "Cirrus Logic AccuPak", AV_CODEC_ID_CLJR,
      (uint32_t[]){ BGAV_MK_FOURCC('C', 'L', 'J', 'R'),
               0x00 } },

    /*     AV_CODEC_ID_MDEC, */
    { "FFmpeg MPEC video decoder", "Playstation MDEC", AV_CODEC_ID_MDEC,
      (uint32_t[]){ BGAV_MK_FOURCC('M', 'D', 'E', 'C'),
                    0x00 } },
    
    /*     AV_CODEC_ID_ROQ, */
    { "FFmpeg ID Roq Video Decoder", "ID Roq Video", AV_CODEC_ID_ROQ,
      (uint32_t[]){ BGAV_MK_FOURCC('R', 'O', 'Q', 'V'),
                    0x00 } },

    /*     AV_CODEC_ID_INTERPLAY_VIDEO, */
    { "FFmpeg Interplay Video Decoder", "Interplay Video", AV_CODEC_ID_INTERPLAY_VIDEO,
      (uint32_t[]){ BGAV_MK_FOURCC('I', 'P', 'V', 'D'),
                    0x00 } },
    
    /*     AV_CODEC_ID_XAN_WC3, */
    
    /*     AV_CODEC_ID_XAN_WC4, */
#if 0 /* Commented out in libavcodec as well */
    { "FFmpeg Xxan decoder", "Xan/WC3", AV_CODEC_ID_XAN_WC4,
      (uint32_t[]){ BGAV_MK_FOURCC('X', 'x', 'a', 'n'),
               0x00 } },
#endif

    /*     AV_CODEC_ID_RPZA, */
    
    { "FFmpeg rpza decoder", "Apple Video", AV_CODEC_ID_RPZA,
      (uint32_t[]){ BGAV_MK_FOURCC('r', 'p', 'z', 'a'),
               BGAV_MK_FOURCC('a', 'z', 'p', 'r'),
               0x00 } },

    /*     AV_CODEC_ID_CINEPAK, */
    { "FFmpeg cinepak decoder", "Cinepak", AV_CODEC_ID_CINEPAK,
      (uint32_t[]){ BGAV_MK_FOURCC('c', 'v', 'i', 'd'),
               0x00 } },

    /*     AV_CODEC_ID_WS_VQA, */
    { "FFmpeg Westwood VQA decoder", "Westwood VQA", AV_CODEC_ID_WS_VQA,
      (uint32_t[]){ BGAV_MK_FOURCC('W', 'V', 'Q', 'A'),
                    0x00 } },
    
    /*     AV_CODEC_ID_MSRLE, */
    { "FFmpeg MSRLE Decoder", "Microsoft RLE", AV_CODEC_ID_MSRLE,
      (uint32_t[]){ BGAV_MK_FOURCC('W', 'R', 'L', 'E'),
               BGAV_MK_FOURCC('m', 'r', 'l', 'e'),
               BGAV_MK_FOURCC(0x1, 0x0, 0x0, 0x0),
               0x00 } },

    /*     AV_CODEC_ID_MSVIDEO1, */
    { "FFmpeg MSVideo 1 decoder", "Microsoft Video 1", AV_CODEC_ID_MSVIDEO1,
      (uint32_t[]){ BGAV_MK_FOURCC('M', 'S', 'V', 'C'),
               BGAV_MK_FOURCC('m', 's', 'v', 'c'),
               BGAV_MK_FOURCC('C', 'R', 'A', 'M'),
               BGAV_MK_FOURCC('c', 'r', 'a', 'm'),
               BGAV_MK_FOURCC('W', 'H', 'A', 'M'),
               BGAV_MK_FOURCC('w', 'h', 'a', 'm'),
               0x00 } },

    /*     AV_CODEC_ID_IDCIN, */
#if 1 // Crashes
    { "FFmpeg ID CIN decoder", "ID CIN", AV_CODEC_ID_IDCIN,
      (uint32_t[]){ BGAV_MK_FOURCC('I', 'D', 'C', 'I'),
               0x00 } },
#endif
    /*     AV_CODEC_ID_8BPS, */
    { "FFmpeg 8BPS decoder", "Quicktime Planar RGB (8BPS)", AV_CODEC_ID_8BPS,
      (uint32_t[]){ BGAV_MK_FOURCC('8', 'B', 'P', 'S'),
               0x00 } },
    /*     AV_CODEC_ID_SMC, */
    { "FFmpeg SMC decoder", "Apple Graphics", AV_CODEC_ID_SMC,
      (uint32_t[]){ BGAV_MK_FOURCC('s', 'm', 'c', ' '),
               0x00 } },


    /*     AV_CODEC_ID_FLIC, */
    { "FFmpeg FLI/FLC Decoder", "FLI/FLC Animation", AV_CODEC_ID_FLIC,
      (uint32_t[]){ BGAV_MK_FOURCC('F', 'L', 'I', 'C'),
               0x00 } },

    /*     AV_CODEC_ID_TRUEMOTION1, */
    { "FFmpeg DUCK TrueMotion 1 decoder", "Duck TrueMotion 1", AV_CODEC_ID_TRUEMOTION1,
      (uint32_t[]){ BGAV_MK_FOURCC('D', 'U', 'C', 'K'),
               0x00 } },

    /*     AV_CODEC_ID_VMDVIDEO, */
    { "FFmpeg Sierra VMD video decoder", "Sierra VMD video",
      AV_CODEC_ID_VMDVIDEO,
      (uint32_t[]){ BGAV_MK_FOURCC('V', 'M', 'D', 'V'),
                    0x00 } },
    
    /*     AV_CODEC_ID_MSZH, */
    { "FFmpeg MSZH decoder", "LCL MSZH", AV_CODEC_ID_MSZH,
      (uint32_t[]){ BGAV_MK_FOURCC('M', 'S', 'Z', 'H'),
               0x00 } },

    /*     AV_CODEC_ID_ZLIB, */
    { "FFmpeg ZLIB decoder", "LCL ZLIB", AV_CODEC_ID_ZLIB,
      (uint32_t[]){ BGAV_MK_FOURCC('Z', 'L', 'I', 'B'),
               0x00 } },
    /*     AV_CODEC_ID_QTRLE, */
    { "FFmpeg QT rle Decoder", "Quicktime RLE", AV_CODEC_ID_QTRLE,
      (uint32_t[]){ BGAV_MK_FOURCC('r', 'l', 'e', ' '),
               0x00 } },

    /*     AV_CODEC_ID_SNOW, */
    { "FFmpeg Snow decoder", "Snow", AV_CODEC_ID_SNOW,
      (uint32_t[]){ BGAV_MK_FOURCC('S', 'N', 'O', 'W'),
                    0x00 } },
    
    /*     AV_CODEC_ID_TSCC, */
    { "FFmpeg TSCC decoder", "TechSmith Camtasia", AV_CODEC_ID_TSCC,
      (uint32_t[]){ BGAV_MK_FOURCC('T', 'S', 'C', 'C'),
               BGAV_MK_FOURCC('t', 's', 'c', 'c'),
               0x00 } },
    /*     AV_CODEC_ID_ULTI, */
    { "FFmpeg ULTI decoder", "IBM Ultimotion", AV_CODEC_ID_ULTI,
      (uint32_t[]){ BGAV_MK_FOURCC('U', 'L', 'T', 'I'),
               0x00 } },

    /*     AV_CODEC_ID_QDRAW, */
    { "FFmpeg QDraw decoder", "Apple QuickDraw", AV_CODEC_ID_QDRAW,
      (uint32_t[]){ BGAV_MK_FOURCC('q', 'd', 'r', 'w'),
               0x00 } },
    /*     AV_CODEC_ID_VIXL, */
    { "FFmpeg Video XL Decoder", "Video XL", AV_CODEC_ID_VIXL,
      (uint32_t[]){ BGAV_MK_FOURCC('V', 'I', 'X', 'L'),
               0x00 } },
    /*     AV_CODEC_ID_QPEG, */
    { "FFmpeg QPEG decoder", "QPEG", AV_CODEC_ID_QPEG,
      (uint32_t[]){ BGAV_MK_FOURCC('Q', '1', '.', '0'),
                    BGAV_MK_FOURCC('Q', '1', '.', '1'),
                    0x00 } },

    /*     AV_CODEC_ID_XVID, */

    /*     AV_CODEC_ID_PNG, */

    /*     AV_CODEC_ID_PPM, */

    /*     AV_CODEC_ID_PBM, */

    /*     AV_CODEC_ID_PGM, */

    /*     AV_CODEC_ID_PGMYUV, */

    /*     AV_CODEC_ID_PAM, */

    /*     AV_CODEC_ID_FFVHUFF, */
    { "FFmpeg FFVHUFF decoder", "FFmpeg Huffman", AV_CODEC_ID_FFVHUFF,
      (uint32_t[]){ BGAV_MK_FOURCC('F', 'F', 'V', 'H'),
               0x00 } },
    
    /*     AV_CODEC_ID_RV30, */

    { "FFmpeg Real video 3.0 decoder", "Real video 3.0", AV_CODEC_ID_RV30,
      (uint32_t[]){ BGAV_MK_FOURCC('R', 'V', '3', '0'),
               0x00 } },
    
    /*     AV_CODEC_ID_RV40, */
    { "FFmpeg Real video 4.0 decoder", "Real video 4.0", AV_CODEC_ID_RV40,
      (uint32_t[]){ BGAV_MK_FOURCC('R', 'V', '4', '0'),
               0x00 } },
    
    /*     AV_CODEC_ID_VC1, */
    { "FFmpeg VC1 decoder", "VC1", AV_CODEC_ID_VC1,
      (uint32_t[]){ BGAV_MK_FOURCC('W', 'V', 'C', '1'),
                    BGAV_MK_FOURCC('V', 'C', '-', '1'),
                    0x00 } },
    
    /*     AV_CODEC_ID_WMV3, */
    // #ifndef HAVE_W32DLL
    
    // [wmv3 @ 0xb63fe128]Old WMV3 version detected, only I-frames will be decoded
    { "FFmpeg WMV3 decoder", "Window Media Video 9", AV_CODEC_ID_WMV3,
      (uint32_t[]){ BGAV_MK_FOURCC('W', 'M', 'V', '3'),
               0x00 } }, 
    // #endif

    /*     AV_CODEC_ID_LOCO, */
    { "FFmpeg LOCO decoder", "LOCO", AV_CODEC_ID_LOCO,
      (uint32_t[]){ BGAV_MK_FOURCC('L', 'O', 'C', 'O'),
               0x00 } },
    
    /*     AV_CODEC_ID_WNV1, */
    { "FFmpeg WNV1 decoder", "Winnow Video 1", AV_CODEC_ID_WNV1,
      (uint32_t[]){ BGAV_MK_FOURCC('W', 'N', 'V', '1'),
               0x00 } },

    /*     AV_CODEC_ID_AASC, */
    { "FFmpeg AASC decoder", "Autodesk Animator Studio Codec", AV_CODEC_ID_AASC,
      (uint32_t[]){ BGAV_MK_FOURCC('A', 'A', 'S', 'C'),
               0x00 } },

    /*     AV_CODEC_ID_INDEO2, */
    { "FFmpeg Inteo 2 decoder", "Intel Indeo 2", AV_CODEC_ID_INDEO2,
      (uint32_t[]){ BGAV_MK_FOURCC('R', 'T', '2', '1'),
                    0x00 } },
    /*     AV_CODEC_ID_FRAPS, */
    { "FFmpeg Fraps 1 decoder", "Fraps 1", AV_CODEC_ID_FRAPS,
      (uint32_t[]){ BGAV_MK_FOURCC('F', 'P', 'S', '1'),
                    0x00 } },

    /*     AV_CODEC_ID_TRUEMOTION2, */
    { "FFmpeg DUCK TrueMotion 2 decoder", "Duck TrueMotion 2", AV_CODEC_ID_TRUEMOTION2,
      (uint32_t[]){ BGAV_MK_FOURCC('T', 'M', '2', '0'),
               0x00 } },
    /*     AV_CODEC_ID_BMP, */
    /*     AV_CODEC_ID_CSCD, */
    { "FFmpeg CSCD decoder", "CamStudio Screen Codec", AV_CODEC_ID_CSCD,
      (uint32_t[]){ BGAV_MK_FOURCC('C', 'S', 'C', 'D'),
               0x00 } },
    
    /*     AV_CODEC_ID_MMVIDEO, */
    { "FFmpeg MM video decoder", "American Laser Games MM", AV_CODEC_ID_MMVIDEO,
      (uint32_t[]){ BGAV_MK_FOURCC('M', 'M', 'V', 'D'),
                    0x00 } },
    /*     AV_CODEC_ID_ZMBV, */
    { "FFmpeg ZMBV decoder", "Zip Motion Blocks Video", AV_CODEC_ID_ZMBV,
      (uint32_t[]){ BGAV_MK_FOURCC('Z', 'M', 'B', 'V'),
               0x00 } },

    /*     AV_CODEC_ID_AVS, */
    { "FFmpeg AVS Video Decoder", "AVS Video", AV_CODEC_ID_AVS,
      (uint32_t[]){ BGAV_MK_FOURCC('A', 'V', 'S', ' '),
                    0x00 } },
    /*     AV_CODEC_ID_SMACKVIDEO, */
    { "FFmpeg Smacker Video Decoder", "Smacker Video", AV_CODEC_ID_SMACKVIDEO,
      (uint32_t[]){ BGAV_MK_FOURCC('S', 'M', 'K', '2'),
                    BGAV_MK_FOURCC('S', 'M', 'K', '4'),
                    0x00 } },
    /*     AV_CODEC_ID_NUV, */
    { "FFmpeg NuppelVideo decoder", "NuppelVideo (rtjpeg)", AV_CODEC_ID_NUV,
      (uint32_t[]){ BGAV_MK_FOURCC('R', 'J', 'P', 'G'),
                    BGAV_MK_FOURCC('N', 'U', 'V', ' '),
               0x00 } },
    /*     AV_CODEC_ID_KMVC, */
    { "FFmpeg KMVC decoder", "Karl Morton's video codec", AV_CODEC_ID_KMVC,
      (uint32_t[]){ BGAV_MK_FOURCC('K', 'M', 'V', 'C'),
               0x00 } },
    /*     AV_CODEC_ID_FLASHSV, */
    { "FFmpeg Flash screen video decoder", "Flash Screen Video", AV_CODEC_ID_FLASHSV,
      (uint32_t[]){ BGAV_MK_FOURCC('F', 'L', 'V', 'S'),
                    0x00 } },
    /*     AV_CODEC_ID_CAVS, */
    { "FFmpeg Chinese AVS decoder", "Chinese AVS", AV_CODEC_ID_CAVS,
      (uint32_t[]){ BGAV_MK_FOURCC('C', 'A', 'V', 'S'),
                    0x00 } },
    /*     AV_CODEC_ID_JPEG2000, */

    /*     AV_CODEC_ID_VMNC, */
    { "FFmpeg VMware video decoder", "VMware video", AV_CODEC_ID_VMNC,
      (uint32_t[]){ BGAV_MK_FOURCC('V', 'M', 'n', 'c'),
                    0x00 } },
    /*     AV_CODEC_ID_VP5, */
    { "FFmpeg VP5 decoder", "On2 VP5", AV_CODEC_ID_VP5,
      (uint32_t[]){ BGAV_MK_FOURCC('V', 'P', '5', '0'),
                    0x00 } },
    /*     AV_CODEC_ID_VP6, */

    { "FFmpeg VP6.2 decoder", "On2 VP6.2", AV_CODEC_ID_VP6,
      (uint32_t[]){ BGAV_MK_FOURCC('V', 'P', '6', '2'),
                    0x00 } },

    { "FFmpeg VP6.0 decoder", "On2 VP6.0", AV_CODEC_ID_VP6,
      (uint32_t[]){ BGAV_MK_FOURCC('V', 'P', '6', '0'),
                    0x00 } },
    { "FFmpeg VP6.1 decoder", "On2 VP6.1", AV_CODEC_ID_VP6,
      (uint32_t[]){ BGAV_MK_FOURCC('V', 'P', '6', '1'),
                    0x00 } },
    { "FFmpeg VP6.2 decoder (flash variant)", "On2 VP6.2 (flash variant)", AV_CODEC_ID_VP6F,
      (uint32_t[]){ BGAV_MK_FOURCC('V', 'P', '6', 'F'),
                    0x00 } },
    /*     AV_CODEC_ID_TARGA, */
    /*     AV_CODEC_ID_DSICINVIDEO, */
    { "FFmpeg Delphine CIN video decoder", "Delphine CIN Video", AV_CODEC_ID_DSICINVIDEO,
      (uint32_t[]){ BGAV_MK_FOURCC('d', 'c', 'i', 'n'),
               0x00 } },
    /*     AV_CODEC_ID_TIERTEXSEQVIDEO, */
    { "FFmpeg Tiertex Video Decoder", "Tiertex Video", AV_CODEC_ID_TIERTEXSEQVIDEO,
      (uint32_t[]){ BGAV_MK_FOURCC('T', 'I', 'T', 'X'),
                    0x00 } },
    /*     AV_CODEC_ID_TIFF, */
    /*     AV_CODEC_ID_GIF, */
    { "FFmpeg GIF Video Decoder", "GIF", AV_CODEC_ID_GIF,
      (uint32_t[]){ BGAV_MK_FOURCC('g', 'i', 'f', ' '),
                    0x00 } },
    /*     AV_CODEC_ID_FFH264, */
    /*     AV_CODEC_ID_DXA, */
    { "FFmpeg DXA decoder", "DXA", AV_CODEC_ID_DXA,
      (uint32_t[]){ BGAV_MK_FOURCC('D', 'X', 'A', ' '),
                    0x00 } },
    /*     AV_CODEC_ID_DNXHD, */
    { "FFmpeg DNxHD Video decoder", "DNxHD", AV_CODEC_ID_DNXHD,
      (uint32_t[]){ BGAV_MK_FOURCC('A', 'V', 'd', 'n'),
               0x00 } },
    /*     AV_CODEC_ID_THP, */
    { "FFmpeg THP Video decoder", "THP Video", AV_CODEC_ID_THP,
      (uint32_t[]){ BGAV_MK_FOURCC('T', 'H', 'P', 'V'),
               0x00 } },
    /*     AV_CODEC_ID_SGI, */
    /*     AV_CODEC_ID_C93, */
    { "FFmpeg C93 decoder", "C93", AV_CODEC_ID_C93,
      (uint32_t[]){ BGAV_MK_FOURCC('C', '9', '3', 'V'),
                    0x00 } },
    /*     AV_CODEC_ID_BETHSOFTVID, */
    { "FFmpeg Bethsoft VID decoder", "Bethsoft VID",
      AV_CODEC_ID_BETHSOFTVID,
      (uint32_t[]){ BGAV_MK_FOURCC('B','S','D','V'), 0x00 } },

    /*     AV_CODEC_ID_PTX, */
    
    /*     AV_CODEC_ID_TXD, */
    
    /*     AV_CODEC_ID_VP6A, */
    { "FFmpeg VP6 yuva decoder", "On2 VP6.0 with alpha", AV_CODEC_ID_VP6A,
      (uint32_t[]){ BGAV_MK_FOURCC('V', 'P', '6', 'A'),
                    0x00 } },
    /*     AV_CODEC_ID_AMV, */
    /*     AV_CODEC_ID_VB, */
    { "FFmpeg Beam Software VB decoder", "Beam Software VB",
      AV_CODEC_ID_VB,
      (uint32_t[]){ BGAV_MK_FOURCC('V','B','V','1'), 0x00 } },

    { "FFmpeg Indeo 5 decoder", "Indeo 5", AV_CODEC_ID_INDEO5,
      (uint32_t[]){ BGAV_MK_FOURCC('I', 'V', '5', '0'),
                    0x00 } },

    { "FFmpeg VP8 decoder", "VP8", AV_CODEC_ID_VP8,
      (uint32_t[]){ BGAV_MK_FOURCC('V', 'P', '8', '0'),
                    0x00 } },

    { "FFmpeg VP9 decoder", "VP9", AV_CODEC_ID_VP9,
      (uint32_t[]){ BGAV_MK_FOURCC('V', 'P', '9', '0'),
        BGAV_MK_FOURCC('v', 'p', '0', '9'),
        0x00 } },
    
    { "Ffmpeg MPEG-1 decoder", "MPEG-1", AV_CODEC_ID_MPEG1VIDEO,
      (uint32_t[])
      { /* Set by MPEG demuxers */
        BGAV_MK_FOURCC('m','p','v','1'), // MPEG-1
        0x00,
      }
    },
    { "Ffmpeg MPEG-2 decoder", "MPEG-2", AV_CODEC_ID_MPEG2VIDEO,
      (uint32_t[]){ /* Set by MPEG demuxers */
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
    },
    {
      "Ffmpeg ProRes decoder", "ProRes", AV_CODEC_ID_PRORES,
      (uint32_t[]){
        BGAV_MK_FOURCC('a', 'p', 'c', 'h'),
        BGAV_MK_FOURCC('a', 'p', 'c', 'n'),
        BGAV_MK_FOURCC('a', 'p', 'c', 's'),
        BGAV_MK_FOURCC('a', 'p', 'c', 'o'),
        BGAV_MK_FOURCC('a', 'p', '4', 'h'),
        BGAV_MK_FOURCC('a', 'p', '4', 'x'),
        0x00 },
    },
    
  };

#define NUM_CODECS sizeof(codec_infos)/sizeof(codec_infos[0])

static int real_num_codecs;

static struct
  {
  codec_info_t * info;
  bgav_video_decoder_t decoder;
  } codecs[NUM_CODECS];

static codec_info_t * lookup_codec(bgav_stream_t * s)
  {
  int i;
  for(i = 0; i < real_num_codecs; i++)
    {
    if(s->data.video.decoder == &codecs[i].decoder)
      return codecs[i].info;
    }
  return NULL;
  }

void bgav_init_video_decoders_ffmpeg(bgav_options_t * opt)
  {
  int i;
  const AVCodec * c;
  
  real_num_codecs = 0;
  for(i = 0; i < NUM_CODECS; i++)
    {
    if((c = avcodec_find_decoder(codec_infos[i].ffmpeg_id)))
      {
      codecs[real_num_codecs].info = &codec_infos[i];
      codecs[real_num_codecs].decoder.name = codecs[real_num_codecs].info->decoder_name;
      
      if(c->capabilities & AV_CODEC_CAP_DELAY) 
        codecs[real_num_codecs].decoder.skipto = skipto_ffmpeg;
      codecs[real_num_codecs].decoder.fourccs =
        codecs[real_num_codecs].info->fourccs;
      codecs[real_num_codecs].decoder.init = init_ffmpeg;
      codecs[real_num_codecs].decoder.decode = decode_ffmpeg;
      codecs[real_num_codecs].decoder.close = close_ffmpeg;
      codecs[real_num_codecs].decoder.resync = resync_ffmpeg;
      
      bgav_video_decoder_register(&codecs[real_num_codecs].decoder);
      real_num_codecs++;
      }
    else
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "Cannot find %s", codec_infos[i].decoder_name);
      }
    }
  }

static void pal8_to_rgb24(gavl_video_frame_t * dst, AVFrame * src,
                          int width, int height)
  {
  int i, j;
  uint32_t pixel;
  uint8_t * dst_ptr;
  uint8_t * dst_save;

  uint8_t * src_ptr;
  uint8_t * src_save;

  uint32_t * palette;

  int dst_stride;
    
  palette = (uint32_t*)(src->data[1]);
  
  dst_save = dst->planes[0];
  dst_stride = dst->strides[0];
  
  src_save = src->data[0];
  for(i = 0; i < height; i++)
    {
    src_ptr = src_save;
    dst_ptr = dst_save;
    
    for(j = 0; j < width; j++)
      {
      pixel = palette[*src_ptr];
      dst_ptr[0] = (pixel & 0x00FF0000) >> 16;
      dst_ptr[1] = (pixel & 0x0000FF00) >> 8;
      dst_ptr[2] = (pixel & 0x000000FF);
      
      dst_ptr+=3;
      src_ptr++;
      }

    src_save += src->linesize[0];
    dst_save += dst_stride;
    }
  }

static void pal8_to_rgba32(gavl_video_frame_t * dst, AVFrame * src,
                           int width, int height)
  {
  int i, j;
  uint32_t pixel;
  uint8_t * dst_ptr;
  uint8_t * dst_save;

  uint8_t * src_ptr;
  uint8_t * src_save;

  uint32_t * palette;

  int dst_stride;
    
  palette = (uint32_t*)(src->data[1]);
  
  dst_save = dst->planes[0];
  dst_stride = dst->strides[0];
  
  src_save = src->data[0];
  for(i = 0; i < height; i++)
    {
    src_ptr = src_save;
    dst_ptr = dst_save;
    
    for(j = 0; j < width; j++)
      {
      pixel = palette[*src_ptr];
      dst_ptr[0] = (pixel & 0x00FF0000) >> 16;
      dst_ptr[1] = (pixel & 0x0000FF00) >> 8;
      dst_ptr[2] = (pixel & 0x000000FF);
      dst_ptr[3] = (pixel & 0xFF000000) >> 24;
      
      dst_ptr+=4;
      src_ptr++;
      }

    src_save += src->linesize[0];
    dst_save += dst_stride;
    }
  }

static void yuva420_to_yuva32(gavl_video_frame_t * dst, AVFrame * src,
                              int width, int height)
  {
  int i, j;
  uint8_t * dst_ptr;
  uint8_t * dst_save;

  uint8_t * src_ptr_y;
  uint8_t * src_save_y;

  uint8_t * src_ptr_u;
  uint8_t * src_save_u;

  uint8_t * src_ptr_v;
  uint8_t * src_save_v;

  uint8_t * src_ptr_a;
  uint8_t * src_save_a;

  int dst_stride;
  
  dst_save = dst->planes[0];
  dst_stride = dst->strides[0];
  
  src_save_y = src->data[0];
  src_save_u = src->data[1];
  src_save_v = src->data[2];
  src_save_a = src->data[3];

  for(i = 0; i < height/2; i++)
    {
    src_ptr_y = src_save_y;
    src_ptr_u = src_save_u;
    src_ptr_v = src_save_v;
    src_ptr_a = src_save_a;
    dst_ptr = dst_save;
    
    for(j = 0; j < width/2; j++)
      {
      dst_ptr[0] = *src_ptr_y;
      dst_ptr[1] = *src_ptr_u;
      dst_ptr[2] = *src_ptr_v;
      dst_ptr[3] = *src_ptr_a;
      
      dst_ptr+=4;
      src_ptr_y++;
      src_ptr_a++;

      dst_ptr[0] = *src_ptr_y;
      dst_ptr[1] = *src_ptr_u;
      dst_ptr[2] = *src_ptr_v;
      dst_ptr[3] = *src_ptr_a;
      
      dst_ptr+=4;
      src_ptr_y++;
      src_ptr_a++;

      src_ptr_u++;
      src_ptr_v++;
      
      }

    src_save_y += src->linesize[0];
    dst_save += dst_stride;

    src_ptr_y = src_save_y;
    src_ptr_u = src_save_u;
    src_ptr_v = src_save_v;
    src_ptr_a = src_save_a;
    dst_ptr = dst_save;
    
    for(j = 0; j < width/2; j++)
      {
      dst_ptr[0] = *src_ptr_y;
      dst_ptr[1] = *src_ptr_u;
      dst_ptr[2] = *src_ptr_v;
      dst_ptr[3] = *src_ptr_a;
      
      dst_ptr+=4;
      src_ptr_y++;
      src_ptr_a++;

      dst_ptr[0] = *src_ptr_y;
      dst_ptr[1] = *src_ptr_u;
      dst_ptr[2] = *src_ptr_v;
      dst_ptr[3] = *src_ptr_a;
      
      dst_ptr+=4;
      src_ptr_y++;
      src_ptr_a++;

      src_ptr_u++;
      src_ptr_v++;
      
      }
    
    src_save_y += src->linesize[0];
    src_save_u += src->linesize[1];
    src_save_v += src->linesize[2];
    src_save_a += src->linesize[3];
    
    dst_save += dst_stride;
    }
  }


/* Real stupid rgba format conversion */

static void rgba32_to_rgba32(gavl_video_frame_t * dst, AVFrame * src,
                             int width, int height)
  {
  int i, j;
  uint32_t r, g, b, a;
  uint8_t * dst_ptr;
  uint8_t * dst_save;
  int dst_stride;
  
  uint32_t * src_ptr;
  uint8_t  * src_save;

  dst_save = dst->planes[0];
  dst_stride = dst->strides[0];
    
  src_save = src->data[0];
  for(i = 0; i < height; i++)
    {
    src_ptr = (uint32_t*)src_save;
    dst_ptr = dst_save;
    
    for(j = 0; j < width; j++)
      {
      a = ((*src_ptr) & 0xff000000) >> 24;
      r = ((*src_ptr) & 0x00ff0000) >> 16;
      g = ((*src_ptr) & 0x0000ff00) >> 8;
      b = ((*src_ptr) & 0x000000ff);
      dst_ptr[0] = r;
      dst_ptr[1] = g;
      dst_ptr[2] = b;
      dst_ptr[3] = a;
      
      dst_ptr+=4;
      src_ptr++;
      }
    src_save += src->linesize[0];
    dst_save += dst_stride;
    }
  }

/* Pixel formats */

static const struct
  {
  enum AVPixelFormat  ffmpeg_csp;
  gavl_pixelformat_t gavl_csp;
  } pixelformats[] =
  {
    { AV_PIX_FMT_YUV420P,       GAVL_YUV_420_P },  ///< Planar YUV 4:2:0 (1 Cr & Cb sample per 2x2 Y samples)
    { AV_PIX_FMT_YUYV422,       GAVL_YUY2      },
    { AV_PIX_FMT_RGB24,         GAVL_RGB_24    },  ///< Packed pixel, 3 bytes per pixel, RGBRGB...
    { AV_PIX_FMT_BGR24,         GAVL_BGR_24    },  ///< Packed pixel, 3 bytes per pixel, BGRBGR...
    { AV_PIX_FMT_YUV422P,       GAVL_YUV_422_P },  ///< Planar YUV 4:2:2 (1 Cr & Cb sample per 2x1 Y samples)
    { AV_PIX_FMT_YUV444P,       GAVL_YUV_444_P }, ///< Planar YUV 4:4:4 (1 Cr & Cb sample per 1x1 Y samples)
    { AV_PIX_FMT_RGB32,         GAVL_RGBA_32   },  ///< Packed pixel, 4 bytes per pixel, BGRABGRA..., stored in cpu endianness
    { AV_PIX_FMT_YUV410P,       GAVL_YUV_410_P }, ///< Planar YUV 4:1:0 (1 Cr & Cb sample per 4x4 Y samples)
    { AV_PIX_FMT_YUV411P,       GAVL_YUV_411_P }, ///< Planar YUV 4:1:1 (1 Cr & Cb sample per 4x1 Y samples)
    { AV_PIX_FMT_RGB565,        GAVL_RGB_16 }, ///< always stored in cpu endianness
    { AV_PIX_FMT_RGB555,        GAVL_RGB_15 }, ///< always stored in cpu endianness, most significant bit to 1
    { AV_PIX_FMT_GRAY8,         GAVL_PIXELFORMAT_NONE },
    { AV_PIX_FMT_MONOWHITE,     GAVL_PIXELFORMAT_NONE }, ///< 0 is white
    { AV_PIX_FMT_MONOBLACK,     GAVL_PIXELFORMAT_NONE }, ///< 0 is black
    // { PIX_FMT_PAL8,          GAVL_RGB_24     }, ///< 8 bit with RGBA palette
    { AV_PIX_FMT_YUVJ420P,      GAVL_YUVJ_420_P }, ///< Planar YUV 4:2:0 full scale (jpeg)
    { AV_PIX_FMT_YUVJ422P,      GAVL_YUVJ_422_P }, ///< Planar YUV 4:2:2 full scale (jpeg)
    { AV_PIX_FMT_YUVJ444P,      GAVL_YUVJ_444_P }, ///< Planar YUV 4:4:4 full scale (jpeg)

    { AV_PIX_FMT_YUVA420P,      GAVL_YUVA_32 },
    { AV_PIX_FMT_VAAPI,         GAVL_YUV_420_P },  ///< Planar YUV 4:2:0 (1 Cr & Cb sample per 2x2 Y samples)
    /* Higher accuracy */
    { AV_PIX_FMT_YUV422P10,     GAVL_YUV_422_P_16 },
    { AV_PIX_FMT_YUV422P12,     GAVL_YUV_422_P_16 },
    { AV_PIX_FMT_YUV422P14,     GAVL_YUV_422_P_16 },
    { AV_PIX_FMT_YUV422P16,     GAVL_YUV_422_P_16 },
    { AV_PIX_FMT_YUV444P10,     GAVL_YUV_444_P_16 },
    { AV_PIX_FMT_YUV444P12,     GAVL_YUV_444_P_16 },
    { AV_PIX_FMT_YUV444P14,     GAVL_YUV_444_P_16 },
    { AV_PIX_FMT_YUV444P16,     GAVL_YUV_444_P_16 },
    
    { AV_PIX_FMT_NB, GAVL_PIXELFORMAT_NONE },


  };


static gavl_pixelformat_t get_pixelformat(enum AVPixelFormat p,
                                          gavl_pixelformat_t pixelformat)
  {
  int i;
  if(p == AV_PIX_FMT_PAL8)
    {
    if(pixelformat == GAVL_RGBA_32)
      return GAVL_RGBA_32;
    else
      return GAVL_RGB_24;
    }
  
  for(i = 0; i < sizeof(pixelformats)/sizeof(pixelformats[0]); i++)
    {
    if(pixelformats[i].ffmpeg_csp == p)
      return pixelformats[i].gavl_csp;
    }
  return GAVL_PIXELFORMAT_NONE;
  }


/* Static functions (moved here, to make the above mess more readable) */
static void get_format(AVCodecContext * ctx, gavl_video_format_t * format)
  {
  
  if(format->pixelformat == GAVL_PIXELFORMAT_NONE)
    {
    format->pixelformat =
      get_pixelformat(ctx->pix_fmt, format->pixelformat);
    }
  
  if(ctx->codec_id == AV_CODEC_ID_DVVIDEO)
    {
    if(format->interlace_mode == GAVL_INTERLACE_UNKNOWN)
      format->interlace_mode = GAVL_INTERLACE_BOTTOM_FIRST;

    /* We completely ignore the frame size of the container */
    format->image_width = ctx->width;
    format->frame_width = ctx->width;
        
    format->image_height = ctx->height;
    format->frame_height = ctx->height;
    }
  else
    {
    if((ctx->sample_aspect_ratio.num > 1) ||
       (ctx->sample_aspect_ratio.den > 1))
      {
      format->pixel_width  = ctx->sample_aspect_ratio.num;
      format->pixel_height = ctx->sample_aspect_ratio.den;
      }
    /* Some demuxers don't know the frame dimensions */
    if(!format->image_width)
      {
      format->image_width = ctx->width;
      format->frame_width = ctx->width;

      format->image_height = ctx->height;
      format->frame_height = ctx->height;
      }
    
    if((format->pixel_width <= 0) || (format->pixel_height <= 0))
      {
      format->pixel_width  = 1;
      format->pixel_height = 1;
      }
    
    /* Sometimes, the size encoded in some mp4 (vol?) headers is different from
       what is found in the container. In this case, the image must be scaled. */
    else if(format->image_width &&
            (ctx->width < format->image_width))
      {
      format->pixel_width  = format->image_width;
      format->pixel_height = ctx->width;
      format->image_width = ctx->width;
      }
    }

  if(format->pixelformat == GAVL_YUV_420_P)
    {
    switch(ctx->chroma_sample_location)
      {
      case AVCHROMA_LOC_LEFT:
        format->chroma_placement = GAVL_CHROMA_PLACEMENT_MPEG2;
        break;
      case AVCHROMA_LOC_TOPLEFT:
        format->chroma_placement = GAVL_CHROMA_PLACEMENT_DVPAL;
        break;
      case AVCHROMA_LOC_CENTER:
      case AVCHROMA_LOC_UNSPECIFIED:
      default: // There are more in the enum but unused
        format->chroma_placement = GAVL_CHROMA_PLACEMENT_DEFAULT;
        break;
      }
    }

  
  if(!format->timescale)
    {
    format->timescale = ctx->time_base.den;
    format->frame_duration = ctx->time_base.num;
    }
  }


static void put_frame_palette(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  ffmpeg_video_priv * priv;
  priv = s->decoder_priv;
  
  if(s->data.video.format->pixelformat == GAVL_RGBA_32)
    pal8_to_rgba32(f, priv->frame,
                   s->data.video.format->image_width,
                   s->data.video.format->image_height);
  else
    pal8_to_rgb24(f, priv->frame,
                  s->data.video.format->image_width,
                  s->data.video.format->image_height);
  }


static void put_frame_rgba32(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  ffmpeg_video_priv * priv = s->decoder_priv;
  rgba32_to_rgba32(f, priv->frame,
                   s->data.video.format->image_width,
                   s->data.video.format->image_height);
  }

static void put_frame_yuva420(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  ffmpeg_video_priv * priv = s->decoder_priv;
  yuva420_to_yuva32(f, priv->frame,
                    s->data.video.format->image_width,
                    s->data.video.format->image_height);
  }


static void put_frame_flip(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  ffmpeg_video_priv * priv = s->decoder_priv;
  priv->gavl_frame->planes[0]  = priv->frame->data[0];
  priv->gavl_frame->planes[1]  = priv->frame->data[1];
  priv->gavl_frame->planes[2]  = priv->frame->data[2];
          
  priv->gavl_frame->strides[0] = priv->frame->linesize[0];
  priv->gavl_frame->strides[1] = priv->frame->linesize[1];
  priv->gavl_frame->strides[2] = priv->frame->linesize[2];
  gavl_video_frame_copy_flip_y(s->data.video.format,
                               f, priv->gavl_frame);
  
  }

static void put_frame_swapfields(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  ffmpeg_video_priv * priv = s->decoder_priv;
  priv->gavl_frame->planes[0]  = priv->frame->data[0];
  priv->gavl_frame->planes[1]  = priv->frame->data[1];
  priv->gavl_frame->planes[2]  = priv->frame->data[2];
          
  priv->gavl_frame->strides[0] = priv->frame->linesize[0];
  priv->gavl_frame->strides[1] = priv->frame->linesize[1];
  priv->gavl_frame->strides[2] = priv->frame->linesize[2];

  /* src field (top) -> dst field (bottom) */
  gavl_video_frame_get_field(s->data.video.format->pixelformat,
                             priv->gavl_frame,
                             priv->src_field,
                             0);

  gavl_video_frame_get_field(s->data.video.format->pixelformat,
                             f,
                             priv->dst_field,
                             1);
        
  gavl_video_frame_copy(&priv->field_format[1], priv->dst_field, priv->src_field);

  /* src field (bottom) -> dst field (top) */
  gavl_video_frame_get_field(s->data.video.format->pixelformat,
                             priv->gavl_frame,
                             priv->src_field,
                             1);

  gavl_video_frame_get_field(s->data.video.format->pixelformat,
                             f,
                             priv->dst_field,
                             0);
  gavl_video_frame_copy(&priv->field_format[0], priv->dst_field, priv->src_field);
  
  }

static void put_frame_yuvp10_nocopy(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  ffmpeg_video_priv * priv = s->decoder_priv;
  if(!priv->dsp)
    priv->dsp = gavl_dsp_context_create();

  gavl_dsp_video_frame_shift_bits(priv->dsp, s->vframe, s->data.video.format, 6);
  }

static void put_frame_yuvp12_nocopy(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  ffmpeg_video_priv * priv = s->decoder_priv;
  if(!priv->dsp)
    priv->dsp = gavl_dsp_context_create();

  gavl_dsp_video_frame_shift_bits(priv->dsp, s->vframe, s->data.video.format, 4);
  }

static void put_frame_yuvp14_nocopy(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  ffmpeg_video_priv * priv = s->decoder_priv;
  if(!priv->dsp)
    priv->dsp = gavl_dsp_context_create();

  gavl_dsp_video_frame_shift_bits(priv->dsp, s->vframe, s->data.video.format, 2);
  }

/* Copy */
static void put_frame_yuvp10(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  ffmpeg_video_priv * priv = s->decoder_priv;
  if(!priv->dsp)
    priv->dsp = gavl_dsp_context_create();

  gavl_dsp_video_frame_shift_bits_copy(priv->dsp, f, priv->gavl_frame, s->data.video.format, 6);
  }

static void put_frame_yuvp12(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  ffmpeg_video_priv * priv = s->decoder_priv;
  if(!priv->dsp)
    priv->dsp = gavl_dsp_context_create();

  gavl_dsp_video_frame_shift_bits_copy(priv->dsp, f, priv->gavl_frame, s->data.video.format, 4);
  }

static void put_frame_yuvp14(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  ffmpeg_video_priv * priv = s->decoder_priv;
  if(!priv->dsp)
    priv->dsp = gavl_dsp_context_create();

  gavl_dsp_video_frame_shift_bits_copy(priv->dsp, f, priv->gavl_frame, s->data.video.format, 2);
  }

/* Copy/flip internal frame to output */
static void init_put_frame(bgav_stream_t * s)
  {
  
  ffmpeg_video_priv * priv;
  priv = s->decoder_priv;
  if(priv->hwctx)
    {
    s->data.video.format->hwctx = priv->hwctx;
    s->src_flags |= GAVL_SOURCE_SRC_ALLOC;
    }

  if(priv->fp)
    {
    priv->put_frame = put_frame_fp;
    return;
    }
  switch(priv->ctx->pix_fmt)
    {
    case AV_PIX_FMT_PAL8:
      priv->put_frame = put_frame_palette;
      break;
#ifdef HAVE_LIBVA
    case AV_PIX_FMT_VAAPI:
      priv->put_frame = put_frame_vaapi;
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Using VAAPI");
      break;
#endif
    case AV_PIX_FMT_RGB32:
      priv->put_frame = put_frame_rgba32;
      break;
    case AV_PIX_FMT_YUVA420P:
      priv->put_frame = put_frame_yuva420;
      break;
    case AV_PIX_FMT_YUV422P10:
    case AV_PIX_FMT_YUV444P10:
      if(!(s->ci->flags & GAVL_COMPRESSION_HAS_P_FRAMES))
        {
        priv->put_frame = put_frame_yuvp10_nocopy;
        s->vframe = priv->gavl_frame;
        }
      else
        priv->put_frame = put_frame_yuvp10;
      break;
    case AV_PIX_FMT_YUV422P12:
    case AV_PIX_FMT_YUV444P12:
      if(!(s->ci->flags & GAVL_COMPRESSION_HAS_P_FRAMES))
        {
        priv->put_frame = put_frame_yuvp12_nocopy;
        s->vframe = priv->gavl_frame;
        }
      else
        priv->put_frame = put_frame_yuvp12;
      break;
    case AV_PIX_FMT_YUV422P14:
    case AV_PIX_FMT_YUV444P14:
      if(!(s->ci->flags & GAVL_COMPRESSION_HAS_P_FRAMES))
        {
        priv->put_frame = put_frame_yuvp14_nocopy;
        s->vframe = priv->gavl_frame;
        }
      else
        priv->put_frame = put_frame_yuvp14;
      break;
    default:
      if(priv->flags & FLIP_Y)
        priv->put_frame = put_frame_flip;
      else if(priv->flags & SWAP_FIELDS_OUT)
        priv->put_frame = put_frame_swapfields;
      else
        priv->put_frame = NULL;
      break;
    }
  }

/* Global locking */

static pthread_mutex_t ffmpeg_mutex = PTHREAD_MUTEX_INITIALIZER;

void bgav_ffmpeg_lock()
  {
  pthread_mutex_lock(&ffmpeg_mutex);
  }

void bgav_ffmpeg_unlock()
  {
  pthread_mutex_unlock(&ffmpeg_mutex);
  }

