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


#include <config.h>

#include <avdec_private.h>
#include <codecs.h>

#include <linux/videodev2.h>
#include <gavl/hw_v4l2.h>
#include <gavl/log.h>
#define LOG_DOMAIN "v4l2_m2m"

static gavl_array_t * v4l_devices = NULL;

typedef struct
  {
  gavl_v4l2_device_t * dev;
  gavl_hw_context_t * hwctx;
  
  //  gavl_packet_source_t * psrc;
  
  } v4l2_t;

static int probe_v4l2(const gavl_dictionary_t * dict)
  {
  return !!gavl_v4l2_get_decoder(v4l_devices, GAVL_CODEC_ID_NONE, dict);
  }

static int init_v4l2(bgav_stream_t * s)
  {
  const gavl_dictionary_t * dev_file;

  v4l2_t * priv;
  
  if(!(dev_file = gavl_v4l2_get_decoder(v4l_devices, s->ci->id, s->info)))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Couldn't find decoder for %d", s->ci->id);
    return 0;
    }
  
  //  fprintf(stderr, "Got device %s\n", gavl_dictionary_get_string(dev_file, GAVL_META_URI));

  priv = calloc(1, sizeof(*priv));

  priv->hwctx = gavl_hw_ctx_create_v4l2(dev_file);
  
  priv->dev = gavl_hw_ctx_v4l2_get_device(priv->hwctx);
  
  
  if(!gavl_v4l2_device_init_decoder(priv->dev, s->info_ext, s->psrc))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Initializing decoder failed");
    return 0;
    }
  s->data.video.vsrc = gavl_v4l2_device_get_video_source(priv->dev);
  s->decoder_priv = priv;
  
  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT, gavl_compression_get_short_name(s->ci->id));
  return 1;
  }

/*
  Unused because we use the video source
*/

static gavl_source_status_t 
decode_v4l2(bgav_stream_t * s, gavl_video_frame_t * frame)
  {
  return GAVL_SOURCE_EOF;
  }

static void close_v4l2(bgav_stream_t * s)
  {
  v4l2_t * priv;
  
  priv = s->decoder_priv;

  /* dev is owned by hwctx */
  //  if(priv->dev)
  //    gavl_v4l2_device_close(priv->dev);
  
  if(priv->hwctx)
    gavl_hw_ctx_destroy(priv->hwctx);
  
  free(priv);
     
  }

static void resync_v4l2(bgav_stream_t * s)
  {
  v4l2_t * priv;
  
  priv = s->decoder_priv;

  gavl_v4l2_device_resync_decoder(priv->dev);
  
  }

#define H264_FOURCCS \
  BGAV_MK_FOURCC('a', 'v', 'c', '1'), \
  BGAV_MK_FOURCC('H', '2', '6', '4'), \
  BGAV_MK_FOURCC('h', '2', '6', '4')

#define JPEG_FOURCCS                  \
  BGAV_MK_FOURCC('M', 'J', 'P', 'G')

#define V4L_DECODER(n, f) \
  { \
  .name = "V4L2 M2M " n " Decoder",             \
  .fourccs = (uint32_t[]){ f, 0x00 },         \
  .probe = probe_v4l2,                        \
  .init =   init_v4l2,             \
  .decode = decode_v4l2,           \
  .close =  close_v4l2,            \
  .resync = resync_v4l2,         \
  }

static struct
  {
  gavl_codec_id_t codec_id;
  bgav_video_decoder_t decoder;
  }
decoders[] =
  {
   {
    GAVL_CODEC_ID_H264,
    V4L_DECODER("H.264", H264_FOURCCS)
   },
#if 1
   {
    GAVL_CODEC_ID_JPEG,
    V4L_DECODER("MJPEG", JPEG_FOURCCS)
   },
#endif
   { /* End */ }
  };

void bgav_init_video_decoders_v4l2()
  {
  int idx = 0;
  
  v4l_devices = gavl_array_create();

  gavl_v4l2_devices_scan_by_type(GAVL_V4L2_DEVICE_DECODER, v4l_devices);
  
  while(decoders[idx].codec_id)
    {
    if(gavl_v4l2_get_decoder(v4l_devices, decoders[idx].codec_id, NULL))
      {
      //      fprintf(stderr, "Registering %s\n", decoders[idx].decoder.name);
      bgav_video_decoder_register(&decoders[idx].decoder);
      }
    idx++;
    }
  
  }

#if defined(__GNUC__) && defined(__ELF__)
static void __cleanup() __attribute__ ((destructor));
 
static void __cleanup()
  {
  if(v4l_devices)
    gavl_array_destroy(v4l_devices);
  }

#endif
