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

#include <config.h>
#include <avdec_private.h>
#include <codecs.h>
#include <aac_frame.h>

#define LOG_DOMAIN "faad2"

// #define DUMP_DECODE

typedef struct
  {
  bgav_bytebuffer_t buf;
  faacDecHandle dec;
  float * sample_buffer;
  
  int init;

  int64_t last_duration;
  
  } faad_priv_t;

static gavl_source_status_t get_data(bgav_stream_t * s)
  {
  gavl_source_status_t st;
  faad_priv_t * priv;
  bgav_packet_t * p = NULL;
  
  priv = s->decoder_priv;

  if((st = bgav_stream_get_packet_read(s, &p)) != GAVL_SOURCE_OK)
    return st;
  
  //  fprintf(stderr, "get_packet_read %ld\n", p->position);
  
  /* If we know the number of samples (i.e. when the packet comes
     from an mp4/mov container), flush the buffer to remove previous
     padding bytes */
  if(p->duration > 0)
    bgav_bytebuffer_flush(&priv->buf);

  priv->last_duration = p->duration * (1 + !!(s->ci.flags & GAVL_COMPRESSION_SBR));
  
  bgav_bytebuffer_append(&priv->buf, p, 0);
  bgav_stream_done_packet_read(s, p);
  
  return 1;
  }

static gavl_source_status_t decode_frame_faad2(bgav_stream_t * s)
  {
  gavl_source_status_t st;
  faacDecFrameInfo frame_info;
  faad_priv_t * priv;
    
  priv = s->decoder_priv;

  memset(&frame_info, 0, sizeof(frame_info));
  
  if(priv->buf.size < FAAD_MIN_STREAMSIZE)
    {
    if((st = get_data(s)) != GAVL_SOURCE_OK)
      return st;
    }
  while(1)
    {
#ifdef DUMP_DECODE
    gavl_dprintf("faacDecDecode %d bytes\n", priv->buf.size);
    gavl_hexdump(priv->buf.buffer, 7, 7);
#endif
    
    s->data.audio.frame->samples.f = faacDecDecode(priv->dec,
                                                   &frame_info,
                                                   priv->buf.buffer,
                                                   priv->buf.size);
    
#ifdef DUMP_DECODE
    gavl_dprintf("Used %ld bytes, ptr: %p, samples: %ld\n",
                 frame_info.bytesconsumed, s->data.audio.frame->samples.f,
                 frame_info.samples);
#endif
   
    bgav_bytebuffer_remove(&priv->buf, frame_info.bytesconsumed);
    
    if(!s->data.audio.frame->samples.f)
      {
      if(frame_info.error == 14) /* Too little data */
        {
        if((st = get_data(s)) != GAVL_SOURCE_OK)
          return st;
        }
      else
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "faacDecDecode failed %s",
                 faacDecGetErrorMessage(frame_info.error));
        bgav_bytebuffer_flush(&priv->buf);
        if((st = get_data(s)) != GAVL_SOURCE_OK)
          return st;
        //    priv->data_size = 0;
        //    priv->frame->valid_samples = 0;
        // return 0; /* Recatching the stream is doomed to failure, so we end here */
        }
      }
    else
      break;
    }

  /* If decoding didn't fail, we might have some silent samples.
     faad2 seems to be reporting 0 samples in this case :( */
  
  if(!frame_info.samples)
    s->data.audio.frame->valid_samples = s->data.audio.format->samples_per_frame;
  else
    s->data.audio.frame->valid_samples = frame_info.samples  / s->data.audio.format->num_channels;

  if((priv->last_duration >= 0) &&
     (s->data.audio.frame->valid_samples > priv->last_duration))
    s->data.audio.frame->valid_samples = priv->last_duration;
  
  s->flags |= STREAM_HAVE_FRAME;

  if(!priv->init)
    return GAVL_SOURCE_OK;
  
  if(s->data.audio.format->channel_locations[0] == GAVL_CHID_NONE)
    {
    bgav_faad_set_channel_setup(&frame_info,
                                s->data.audio.format);
    }
  
  if(!gavl_dictionary_get_string(s->m, GAVL_META_FORMAT))
    {
    switch(frame_info.object_type)
      {
      case MAIN:
        gavl_dictionary_set_string(s->m, GAVL_META_FORMAT, "AAC Main");
        break;
      case LC:
        gavl_dictionary_set_string(s->m, GAVL_META_FORMAT, "AAC LC");
        break;
      case SSR:
        gavl_dictionary_set_string(s->m, GAVL_META_FORMAT, "AAC SSR");
        break;
      case LTP:
        gavl_dictionary_set_string(s->m, GAVL_META_FORMAT, "AAC LTP");
        break;
      case HE_AAC:
        gavl_dictionary_set_string(s->m, GAVL_META_FORMAT, "HE-AAC");
        break;
      case ER_LC:
      case ER_LTP:
      case LD:
      case DRM_ER_LC: /* special object type for DRM */
        gavl_dictionary_set_string(s->m, GAVL_META_FORMAT, "MPEG_2/4 AAC");
        break;
      }
    }

    
  return GAVL_SOURCE_OK;
  }

static int init_faad2(bgav_stream_t * s)
  {
  faad_priv_t * priv;
  unsigned long samplerate = 0;
  unsigned char channels;
  char result;
  faacDecConfigurationPtr cfg;
  
  priv = calloc(1, sizeof(*priv));
  priv->dec = faacDecOpen();
  s->decoder_priv = priv;
  
  if(!s->ext_size)
    {
    /* Init the library From an ADTS header */
    if(get_data(s) != GAVL_SOURCE_OK)
      return 0;

    result = faacDecInit(priv->dec, priv->buf.buffer,
                         priv->buf.size,
                         &samplerate, &channels);
    bgav_bytebuffer_remove(&priv->buf, result);
    }
  else
    {
    /* Init the library using a DecoderSpecificInfo */
    result = faacDecInit2(priv->dec, s->ext_data,
                          s->ext_size,
                          &samplerate, &channels);
    }

  faacDecPostSeekReset(priv->dec, 1); // Don't skip the first frame
    
  /* Some mp4 files have a wrong samplerate in the sample description,
     so we correct it here */
  if(samplerate == 2 * s->data.audio.format->samplerate)
    {
    //    fprintf(stderr, "Detected HE-AAC\n");
    //    gavl_hexdump(s->ext_data, s->ext_size, 16);

    if(!s->data.audio.format->samples_per_frame)
      s->data.audio.format->samples_per_frame = 2048;

    s->ci.flags |= GAVL_COMPRESSION_SBR;
    
    if(s->stats.pts_end)
      s->stats.pts_end *= 2;
    }
  else
    {
    if(!s->data.audio.format->samples_per_frame)
      s->data.audio.format->samples_per_frame = 1024;
    //    fprintf(stderr, "Detected NO HE-AAC\n");
    //    gavl_hexdump(s->ext_data, s->ext_size, 16);
    }

  s->data.audio.preroll = s->data.audio.format->samples_per_frame;
  
  s->data.audio.format->samplerate = samplerate;
  
  s->data.audio.format->num_channels = channels;
  s->data.audio.format->sample_format = GAVL_SAMPLE_FLOAT;
  //  s->data.audio.format->sample_format = GAVL_SAMPLE_S16;
  s->data.audio.format->interleave_mode = GAVL_INTERLEAVE_ALL;
  
  cfg = faacDecGetCurrentConfiguration(priv->dec);
  cfg->outputFormat = FAAD_FMT_FLOAT;
  // cfg->outputFormat = FAAD_FMT_16BIT;
  faacDecSetConfiguration(priv->dec, cfg);

  /* Decode a first frame to get the channel setup and the description */

  priv->init = 1;
  
  if(decode_frame_faad2(s) != GAVL_SOURCE_OK)
    return 0;
  
  priv->init = 0;
  return 1;
  }


// static int frame_number = 0;


static void close_faad2(bgav_stream_t * s)
  {
  faad_priv_t * priv;
  priv = s->decoder_priv;
  if(priv->dec)
    faacDecClose(priv->dec);

  bgav_bytebuffer_free(&priv->buf);
  free(priv);
  }

static void resync_faad2(bgav_stream_t * s)
  {
  faad_priv_t * priv;
  priv = s->decoder_priv;

  bgav_bytebuffer_flush(&priv->buf);

  faacDecPostSeekReset(priv->dec, 1); // Don't skip the first frame
  }

static bgav_audio_decoder_t decoder =
  {
    .name =   "FAAD AAC audio decoder",
    .fourccs = (uint32_t[]){ BGAV_MK_FOURCC('m','p','4','a'),
                           BGAV_MK_FOURCC('A','D','T','S'),
                           BGAV_MK_FOURCC('A','D','I','F'),
                           BGAV_MK_FOURCC('a','a','c',' '),
                           BGAV_MK_FOURCC('A','A','C',' '),
                           BGAV_MK_FOURCC('A','A','C','P'),
                           BGAV_MK_FOURCC('r','a','a','c'),
                           BGAV_MK_FOURCC('r','a','c','p'),
                           BGAV_WAVID_2_FOURCC(0x00ff),
                           BGAV_WAVID_2_FOURCC(0x706d),
                      0x0 },
    
    .init =   init_faad2,
    .decode_frame = decode_frame_faad2,
    .close =  close_faad2,
    .resync =  resync_faad2
  };

void bgav_init_audio_decoders_faad2()
  {
  bgav_audio_decoder_register(&decoder);
  }

