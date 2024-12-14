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
#include <stdio.h>
#include <string.h>

#include <avdec_private.h>
#include <parser.h>
#include <bsf.h>

#define LOG_DOMAIN "audio"

int bgav_num_audio_streams(bgav_t * bgav, int track)
  {
  return bgav->tt->tracks[track]->num_audio_streams;
  }

const gavl_audio_format_t * bgav_get_audio_format(bgav_t *  bgav, int stream)
  {
  bgav_stream_t * s;

  if(!(s = bgav_track_get_audio_stream(bgav->tt->cur, stream)))
    return NULL;
  
  return s->data.audio.format;
  }

const gavl_audio_format_t * bgav_get_audio_format_t(bgav_t *  bgav, int track, int stream)
  {
  bgav_stream_t * s;

  if(!(s = bgav_track_get_audio_stream(bgav->tt->tracks[track], stream)))
    return NULL;
  
  
  return s->data.audio.format;
  }

int bgav_set_audio_stream(bgav_t * b, int stream, bgav_stream_action_t action)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_audio_stream(b->tt->cur, stream)))
    return 0;
  s->action = action;
  return 1;
  }

static gavl_source_status_t get_frame(void * sp, gavl_audio_frame_t ** frame)
  {
  bgav_stream_t * s = sp;
  
  if(!(s->flags & STREAM_HAVE_FRAME) &&
     !s->data.audio.decoder->decode_frame(s))
    {
    s->flags |= STREAM_EOF_C;
    return GAVL_SOURCE_EOF;
    }
  s->flags &= ~STREAM_HAVE_FRAME; 
  s->data.audio.frame->timestamp = s->out_time;
  s->out_time += s->data.audio.frame->valid_samples;

  if((s->ci->pre_skip > 0) &&
     (s->data.audio.frame->timestamp - s->stats.pts_start < s->ci->pre_skip))
    {
    gavl_audio_frame_skip(s->data.audio.format,
                          s->data.audio.frame,
                          s->ci->pre_skip - (s->data.audio.frame->timestamp - s->stats.pts_start));
    }
  
  *frame = s->data.audio.frame;
  return GAVL_SOURCE_OK;
  }

int bgav_audio_init(bgav_stream_t * s)
  {
  bgav_set_audio_compression_info(s);
  
  if((s->flags & STREAM_FILTER_PACKETS) && !s->pf)
    {
    s->pf = bgav_packet_filter_create(s->fourcc);
    s->psrc = bgav_packet_filter_connect(s->pf, s->psrc);

    gavl_packet_source_peek_packet(s->psrc, NULL);
    }

  gavl_dictionary_reset(s->info_ext);
  gavl_dictionary_copy(s->info_ext, gavl_packet_source_get_stream(s->psrc));

  gavl_compression_info_free(s->ci);
  gavl_compression_info_init(s->ci);
  gavl_stream_get_compression_info(s->info_ext, s->ci);

  //  fprintf(stderr, "Audio initialized:\n");
  //  gavl_dictionary_dump(s->info_ext, 2);
  
  if(s->stats.pts_start == GAVL_TIME_UNDEFINED)
    {
    bgav_packet_t * p = NULL;
    char tmp_string[128];
    
    if(bgav_stream_peek_packet_read(s, &p) != GAVL_SOURCE_OK)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "EOF while getting start time");
      return 0;
      }
    s->stats.pts_start = p->pts;
    sprintf(tmp_string, "%" PRId64, s->stats.pts_start);
    gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Got initial audio timestamp: %s",
             tmp_string);
    } /* Else */
  //  else if(s->data.audio.pre_skip > 0)
  //    s->stats.pts_start = -s->data.audio.pre_skip;

  s->out_time = s->stats.pts_start;
  
  if(!s->timescale && s->data.audio.format->samplerate)
    s->timescale = s->data.audio.format->samplerate;

  if(s->container_bitrate)
    gavl_dictionary_set_int(s->m, GAVL_META_BITRATE,
                            s->container_bitrate);

  if(s->data.audio.bits_per_sample)
    gavl_dictionary_set_int(s->m, GAVL_META_AUDIO_BITS ,
                            s->data.audio.bits_per_sample);

  return 1;
  }

int bgav_audio_start(bgav_stream_t * s)
  {
  bgav_audio_decoder_t * dec;
  
  
  if(s->action == BGAV_STREAM_DECODE)
    {
    dec = bgav_find_audio_decoder(s->fourcc);
    if(!dec)
      {
      if(!(s->fourcc & 0xffff0000))
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                 "No audio decoder found for WAV ID 0x%04x", s->fourcc);
      else
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                 "No audio decoder found for fourcc %c%c%c%c (0x%08x)",
                 (s->fourcc & 0xFF000000) >> 24,
                 (s->fourcc & 0x00FF0000) >> 16,
                 (s->fourcc & 0x0000FF00) >> 8,
                 (s->fourcc & 0x000000FF),
                 s->fourcc);
      return 0;
      }
    s->data.audio.decoder = dec;
    s->data.audio.frame = gavl_audio_frame_create(NULL);
    
    if(!dec->init(s))
      return 0;

    /* might need to (re)set this here */
    s->out_time = s->stats.pts_start;

    if(s->ci->flags & GAVL_COMPRESSION_SBR)
      s->out_time *= 2;
    
    //    if(!s->timescale)
    //      s->timescale = s->data.audio.format->samplerate;

    //    s->out_time -= s->data.audio.pre_skip;
    }

  //  if(s->container_bitrate == GAVL_BITRATE_VBR)
  //    gavl_dictionary_set_string(&s->m, GAVL_META_BITRATE,
  //                      "VBR");
  else if(s->codec_bitrate)
    gavl_dictionary_set_int(s->m, GAVL_META_BITRATE,
                          s->codec_bitrate);
  
  if(s->action == BGAV_STREAM_DECODE)
    s->data.audio.source =
      gavl_audio_source_create(get_frame, s,
                               GAVL_SOURCE_SRC_ALLOC | s->src_flags,
                               s->data.audio.format);
  
  //  if(s->data.audio.pre_skip && s->data.audio.source)
  //    gavl_audio_source_skip_src(s->data.audio.source, s->data.audio.pre_skip);
  
  return 1;
  }

void bgav_audio_stop(bgav_stream_t * s)
  {
  if(s->data.audio.decoder)
    {
    s->data.audio.decoder->close(s);
    s->data.audio.decoder = NULL;
    }
  if(s->data.audio.frame)
    {
    gavl_audio_frame_null(s->data.audio.frame);
    gavl_audio_frame_destroy(s->data.audio.frame);
    s->data.audio.frame = NULL;
    }
  if(s->data.audio.source)
    {
    gavl_audio_source_destroy(s->data.audio.source);
    s->data.audio.source = NULL;
    }
  }

const char * bgav_get_audio_description(bgav_t * b, int stream)
  {
  bgav_stream_t * s;
  
  if(!(s = bgav_track_get_audio_stream(b->tt->cur, stream)))
    return NULL;
  
  return gavl_dictionary_get_string(s->m, GAVL_META_FORMAT);
  }

const char * bgav_get_audio_info(bgav_t * b, int stream)
  {
  bgav_stream_t * s;
  
  if(!(s = bgav_track_get_audio_stream(b->tt->cur, stream)))
    return NULL;

  return gavl_dictionary_get_string(s->m,
                                    GAVL_META_LABEL);
  }

const bgav_metadata_t *
bgav_get_audio_metadata(bgav_t * b, int stream)
  {
  bgav_stream_t * s;
  
  if(!(s = bgav_track_get_audio_stream(b->tt->cur, stream)))
    return NULL;

  return s->m;
  }

const bgav_metadata_t *
bgav_get_audio_metadata_t(bgav_t * b, int track, int stream)
  {
  bgav_stream_t * s;
  
  if(!(s = bgav_track_get_audio_stream(b->tt->tracks[track], stream)))
    return NULL;

  return s->m;
  }


const char * bgav_get_audio_language(bgav_t * b, int stream)
  {
  bgav_stream_t * s;
  
  if(!(s = bgav_track_get_audio_stream(b->tt->cur, stream)))
    return NULL;
  
  return gavl_dictionary_get_string(s->m, GAVL_META_LANGUAGE);
  }

int bgav_get_audio_bitrate(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s;
  
  if(!(s = bgav_track_get_audio_stream(bgav->tt->cur, stream)))
    return 0;
  
  if(s->codec_bitrate)
    return s->codec_bitrate;
  else if(s->container_bitrate)
    return s->container_bitrate;
  else
    return 0;
  }

int bgav_read_audio(bgav_t * b, gavl_audio_frame_t * frame,
                    int stream, int num_samples)
  {
  bgav_stream_t * s;
  
  if(!(s = bgav_track_get_audio_stream(b->tt->cur, stream)))
    return 0;
  return gavl_audio_source_read_samples(s->data.audio.source,
                                        frame, num_samples);
  }

void bgav_audio_dump(bgav_stream_t * s)
  {
  gavl_dprintf("  Bits per sample:   %d\n", s->data.audio.bits_per_sample);
  gavl_dprintf("Format:\n");
  gavl_audio_format_dump(s->data.audio.format);
  }


void bgav_audio_resync(bgav_stream_t * s)
  {
  gavl_packet_t * p = NULL;
  
  if(s->data.audio.frame)
    s->data.audio.frame->valid_samples = 0;

  if(bgav_stream_peek_packet_read(s, &p) != GAVL_SOURCE_EOF)
    {
    s->out_time = p->pts;
    if(s->ci->flags & GAVL_COMPRESSION_SBR)
      s->out_time *= 2;
    }
  
  if(s->data.audio.decoder &&
     s->data.audio.decoder->resync)
    s->data.audio.decoder->resync(s);
  
  if(s->data.audio.source)
    gavl_audio_source_reset(s->data.audio.source);

  //  fprintf(stderr, "audio resync %"PRId64"\n", gavl_time_unscale(s->data.audio.format->samplerate, s->out_time));

  }

int bgav_audio_skipto(bgav_stream_t * s, int64_t * t, int scale)
  {
  int64_t num_samples;
  // int samples_skipped = 0;  
  int64_t skip_time;

  skip_time = gavl_time_rescale(scale,
                                s->data.audio.format->samplerate,
                                *t);
  
  num_samples = skip_time - s->out_time;
  
  if(num_samples < 0)
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "Cannot skip backwards: Stream time: %"PRId64" skip time: %"PRId64" difference: %"PRId64,
             s->out_time, skip_time, num_samples);
    return 1;
    }

  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Skipping: %"PRId64" samples\n", num_samples);
    
  //  fprintf(stderr, "bgav_audio_skipto... %"PRId64"...", num_samples);
  gavl_audio_source_skip(s->data.audio.source, num_samples);
  //  fprintf(stderr, "done\n");
  return 1;
  }

static uint32_t alaw_fourccs[] =
  {
  BGAV_MK_FOURCC('a', 'l', 'a', 'w'),
  BGAV_MK_FOURCC('A', 'L', 'A', 'W'),
  BGAV_WAVID_2_FOURCC(0x06),
  0x00
  };

static uint32_t ulaw_fourccs[] =
  {
  BGAV_MK_FOURCC('u', 'l', 'a', 'w'),
  BGAV_MK_FOURCC('U', 'L', 'A', 'W'),
  BGAV_WAVID_2_FOURCC(0x07),
  0x00
  };

static uint32_t mp2_fourccs[] =
  {
  BGAV_MK_FOURCC('.', 'm', 'p', '2'),
  BGAV_WAVID_2_FOURCC(0x0050),
  0x00
  };

static uint32_t mp3_fourccs[] =
  {
  BGAV_MK_FOURCC('.', 'm', 'p', '3'),
  BGAV_WAVID_2_FOURCC(0x0055),
  0x00
  };

static uint32_t ac3_fourccs[] =
  {
    BGAV_WAVID_2_FOURCC(0x2000),
    BGAV_MK_FOURCC('.', 'a', 'c', '3'),
    BGAV_MK_FOURCC('d', 'n', 'e', 't'), 
    BGAV_MK_FOURCC('a', 'c', '-', '3'),
    0x00
  };

static uint32_t vorbis_fourccs[] =
  {
    BGAV_MK_FOURCC('V','B','I','S'),
    0x00
  };

static uint32_t aac_fourccs[] =
  {
    BGAV_MK_FOURCC('m','p','4','a'),
    0x00
  };

static uint32_t adts_fourccs[] =
  {
    BGAV_MK_FOURCC('A','D','T','S'),
    0x00
  };

static uint32_t flac_fourccs[] =
  {
    BGAV_MK_FOURCC('F','L','A','C'),
    0x00
  };

static uint32_t opus_fourccs[] =
  {
    BGAV_MK_FOURCC('O','P','U','S'),
    0x00
  };

static uint32_t speex_fourccs[] =
  {
    BGAV_MK_FOURCC('S','P','E','X'),
    0x00
  };


static uint32_t dts_fourccs[] =
  {
    BGAV_MK_FOURCC('d', 't', 's', ' '),
    0x00,
  };

int bgav_get_audio_compression_info(bgav_t * bgav, int stream,
                                    gavl_compression_info_t * info)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_audio_stream(bgav->tt->cur, stream)))
    return 0;
  return gavl_stream_get_compression_info(s->info, info);
  
  }

int bgav_set_audio_compression_info(bgav_stream_t * s)
  {
  int need_header = 0;
  //  int need_bitrate = 1;
  uint32_t codec_tag = 0;
  gavl_codec_id_t id = GAVL_CODEC_ID_NONE;
  
  //  bgav_track_get_compression(bgav->tt->cur);
  
  if(bgav_check_fourcc(s->fourcc, alaw_fourccs))
    id = GAVL_CODEC_ID_ALAW;
  else if(bgav_check_fourcc(s->fourcc, ulaw_fourccs))
    id = GAVL_CODEC_ID_ULAW;
  else if(bgav_check_fourcc(s->fourcc, ac3_fourccs))
    id = GAVL_CODEC_ID_AC3;
  else if(bgav_check_fourcc(s->fourcc, mp2_fourccs))
    id = GAVL_CODEC_ID_MP2;
  else if(bgav_check_fourcc(s->fourcc, mp3_fourccs))
    id = GAVL_CODEC_ID_MP3;
  else if(bgav_check_fourcc(s->fourcc, dts_fourccs))
    id = GAVL_CODEC_ID_DTS;
  else if(bgav_check_fourcc(s->fourcc, aac_fourccs))
    {
    id = GAVL_CODEC_ID_AAC;
    need_header = 1;
    }
  else if(bgav_check_fourcc(s->fourcc, speex_fourccs))
    {
    id = GAVL_CODEC_ID_SPEEX;
    need_header = 1;
    }
  else if(bgav_check_fourcc(s->fourcc, vorbis_fourccs))
    {
    id = GAVL_CODEC_ID_VORBIS;
    need_header = 1;
    }
  else if(bgav_check_fourcc(s->fourcc, flac_fourccs))
    {
    id = GAVL_CODEC_ID_FLAC;
    need_header = 1;
    }
  else if(bgav_check_fourcc(s->fourcc, opus_fourccs))
    {
    id = GAVL_CODEC_ID_OPUS;
    need_header = 1;
    }
  else if(bgav_check_fourcc(s->fourcc, adts_fourccs) &&
          (s->flags & STREAM_FILTER_PACKETS))
    {
    id = GAVL_CODEC_ID_AAC;
    //    need_header = 1;
    }
  
  if(id == GAVL_CODEC_ID_NONE)
    {
    id = GAVL_CODEC_ID_EXTENDED;
    codec_tag = s->fourcc;
    }
  
  if(need_header && !s->ci->codec_header.len)
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "Cannot output compressed audio stream: Global header missing");
    s->flags |= STREAM_GOT_NO_CI;
    return 0;
    }
  s->ci->id = id;
  s->ci->codec_tag = codec_tag;
  
  if(s->codec_bitrate)
    s->ci->bitrate = s->codec_bitrate;
  else if(s->container_bitrate)
    s->ci->bitrate = s->container_bitrate;
  
  gavl_stream_set_compression_info(s->info, s->ci);
  s->flags |= STREAM_GOT_CI;
  
  return 1;
  }

int bgav_read_audio_packet(bgav_t * bgav, int stream, gavl_packet_t * p)
  {
  bgav_stream_t * s;
  
  if(!(s = bgav_track_get_audio_stream(bgav->tt->cur, stream)))
    return 0;
  return (gavl_packet_source_read_packet(s->psrc, &p) == GAVL_SOURCE_OK);
  }

gavl_audio_source_t * bgav_get_audio_source(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_audio_stream(bgav->tt->cur, stream)))
    return NULL;
  return s->data.audio.source;
  }

gavl_packet_source_t * bgav_get_audio_packet_source(bgav_t * bgav, int stream)
  {
  bgav_stream_t * s;
  if(!(s = bgav_track_get_audio_stream(bgav->tt->cur, stream)))
    return NULL;
  return s->psrc;
  }

void bgav_stream_set_sbr(bgav_stream_t * s)
  {
  s->ci->flags |= GAVL_COMPRESSION_SBR;

#if 0
  if(s->stats.pts_start != GAVL_TIME_UNDEFINED)
    s->stats.pts_start *= 2;

  if(s->stats.pts_end != GAVL_TIME_UNDEFINED)
    s->stats.pts_end *= 2;

  if(s->stats.duration_min != GAVL_TIME_UNDEFINED)
    s->stats.duration_min *= 2;

  if(s->stats.duration_max != GAVL_TIME_UNDEFINED)
    s->stats.duration_max *= 2;
#endif
  
  }

