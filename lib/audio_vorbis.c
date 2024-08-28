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



#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>

#include <config.h>
#include <avdec_private.h>
#include <codecs.h>
#include <vorbis_comment.h>

#include <stdio.h>

#define LOG_DOMAIN "vorbis"

// #define DUMP_OUTPUT
// #define DUMP_PACKET

typedef struct
  {
  ogg_sync_state   dec_oy; /* sync and verify incoming physical bitstream */
  ogg_stream_state dec_os; /* take physical pages, weld into a logical
                              stream of packets */
  ogg_page         dec_og; /* one Ogg bitstream page.  Vorbis packets are inside */
  ogg_packet       dec_op; /* one raw packet of data for decode */

  vorbis_info      dec_vi; /* struct that stores all the static vorbis bitstream
                                settings */
  vorbis_comment   dec_vc; /* struct that stores all the bitstream user comments */
  vorbis_dsp_state dec_vd; /* central working state for the packet->PCM decoder */
  vorbis_block     dec_vb; /* local working space for packet->PCM decode */
  int stream_initialized;

  bgav_packet_t p;
  uint8_t * packet_ptr;
  int64_t packetno;
  } vorbis_audio_priv;


/* Put raw streams into the sync engine */

static gavl_source_status_t read_data(bgav_stream_t * s)
  {
  gavl_source_status_t st;
  char * buffer;
  bgav_packet_t * p;
  vorbis_audio_priv * priv;
  
  priv = s->decoder_priv;
  
  if((st = bgav_stream_get_packet_read(s, &p)) != GAVL_SOURCE_OK)
    return st;
  
  buffer = ogg_sync_buffer(&priv->dec_oy, p->buf.len);
  memcpy(buffer, p->buf.buf, p->buf.len);
  ogg_sync_wrote(&priv->dec_oy, p->buf.len);
  bgav_stream_done_packet_read(s, p);
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t next_page(bgav_stream_t * s)
  {
  gavl_source_status_t st;
  int result = 0;
  vorbis_audio_priv * priv;
  priv = s->decoder_priv;
  
  while(result < 1)
    {
    result = ogg_sync_pageout(&priv->dec_oy, &priv->dec_og);
    
    if(result == 0)
      {
      if((st = read_data(s)) != GAVL_SOURCE_OK)
        return st;
      }
    else
      {
      /* Initialitze stream state */
      if(!priv->stream_initialized)
        {
        ogg_stream_init(&priv->dec_os, ogg_page_serialno(&priv->dec_og));
        priv->stream_initialized = 1;
        }
      ogg_stream_pagein(&priv->dec_os, &priv->dec_og);
      }

    }
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t next_packet(bgav_stream_t * s)
  {
  gavl_source_status_t st;
  int result = 0;
  vorbis_audio_priv * priv;
  
  priv = s->decoder_priv;
  
  if(s->fourcc == BGAV_VORBIS)
    {
    if(!priv->dec_op.bytes)
      {
      bgav_packet_t * p = NULL;
      if((st = bgav_stream_get_packet_read(s, &p)) != GAVL_SOURCE_OK)
        return st;
#ifdef DUMP_PACKET
      gavl_dprintf("vorbis: Got packet: %p ", p);
      gavl_packet_dump(p);
#endif    
      gavl_packet_reset(&priv->p);
      gavl_packet_copy(&priv->p, p);
    
      memset(&priv->dec_op, 0, sizeof(priv->dec_op));
      priv->dec_op.bytes  = priv->p.buf.len;
      priv->dec_op.packet = priv->p.buf.buf;
    
      priv->dec_op.granulepos = priv->p.pts + priv->p.duration;
    
      priv->dec_op.packetno = priv->packetno++;
      priv->dec_op.e_o_s = 0;
      priv->packetno++;

      bgav_stream_done_packet_read(s, p);

      }

    
    st = bgav_stream_peek_packet_read(s, NULL);

    switch(st)
      {
      case GAVL_SOURCE_AGAIN:
        return GAVL_SOURCE_AGAIN;
        break;
      case GAVL_SOURCE_EOF:
        priv->dec_op.e_o_s = 1;
      case GAVL_SOURCE_OK: // Fall through
        return GAVL_SOURCE_OK;
        break;
      }
    }
  else
    {
    while(result < 1)
      {
      result = ogg_stream_packetout(&priv->dec_os, &priv->dec_op);
      
      if(result == 0)
        {
        if((st = next_page(s)) != GAVL_SOURCE_OK)
          return st;
        }
      }
    }
  return GAVL_SOURCE_OK;
  }


static int init_vorbis(bgav_stream_t * s)
  {
  uint8_t * ptr;
  char * buffer;

  uint32_t header_sizes[3];
  uint32_t len;
  uint32_t fourcc;
  vorbis_audio_priv * priv;
  priv = calloc(1, sizeof(*priv));
  ogg_sync_init(&priv->dec_oy);

  
  vorbis_info_init(&priv->dec_vi);
  vorbis_comment_init(&priv->dec_vc);

  s->decoder_priv = priv;
  
  /* Heroine Virtual way:
     The 3 header packets are in the first audio chunk */
  
  if(s->fourcc == BGAV_MK_FOURCC('O','g', 'g', 'S'))
    {
    if(!next_page(s))
      return 0;
    
    if(!next_packet(s))
      return 0;
    
    /* Initialize vorbis */
    
    if(vorbis_synthesis_headerin(&priv->dec_vi, &priv->dec_vc,
                                 &priv->dec_op) < 0)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "decode: vorbis_synthesis_headerin: not a vorbis header");
      return 0;
      }
    
    if(!next_packet(s))
      return 0;
    vorbis_synthesis_headerin(&priv->dec_vi, &priv->dec_vc, &priv->dec_op);
    if(!next_packet(s))
      return 0;
    vorbis_synthesis_headerin(&priv->dec_vi, &priv->dec_vc, &priv->dec_op);
    }
  /*
   * AVI way:
   * Header packets are in extradata:
   * starting with byte 22 if ext_data, we first have
   * 3 uint32_ts for the packet sizes, followed by the raw
   * packets
   */
  else if(s->fourcc == BGAV_MK_FOURCC('V', 'O', 'R', 'B'))
    {
    if(s->ci->codec_header.len < 3 * sizeof(uint32_t))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Vorbis decoder: Init data too small (%d bytes)",
               s->ci->codec_header.len);
      return 0;
      }
    //    gavl_hexdump(s->ext_data, s->ci.global_header_len, 16);

    ptr = s->ci->codec_header.buf;
    header_sizes[0] = GAVL_PTR_2_32LE(ptr);ptr+=4;
    header_sizes[1] = GAVL_PTR_2_32LE(ptr);ptr+=4;
    header_sizes[2] = GAVL_PTR_2_32LE(ptr);ptr+=4;

    priv->dec_op.packet = ptr;
    priv->dec_op.b_o_s  = 1;
    priv->dec_op.bytes  = header_sizes[0];

    if(vorbis_synthesis_headerin(&priv->dec_vi, &priv->dec_vc,
                                 &priv->dec_op) < 0)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "decode: vorbis_synthesis_headerin: not a vorbis header");
      return 1;
      }
    ptr += header_sizes[0];

    priv->dec_op.packet = ptr;
    priv->dec_op.b_o_s  = 0;
    priv->dec_op.bytes  = header_sizes[1];

    vorbis_synthesis_headerin(&priv->dec_vi, &priv->dec_vc,
                              &priv->dec_op);
    ptr += header_sizes[1];

    priv->dec_op.packet = ptr;
    priv->dec_op.b_o_s  = 0;
    priv->dec_op.bytes  = header_sizes[2];

    vorbis_synthesis_headerin(&priv->dec_vi, &priv->dec_vc,
                              &priv->dec_op);
    //    ptr += header_sizes[1];
    }
  /* AVI Vorbis mode 2: Codec data in extradata starting with byte 8  */
  /* (bytes 0 - 7 are acm version and libvorbis version, both 32 bit) */

  else if((s->fourcc == BGAV_WAVID_2_FOURCC(0x6750)) ||
          (s->fourcc == BGAV_WAVID_2_FOURCC(0x6770)))
    {
    if(s->ci->codec_header.len <= 8)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "ext size too small");
      return 0;
      }
    buffer = ogg_sync_buffer(&priv->dec_oy, s->ci->codec_header.len - 8);
    memcpy(buffer, s->ci->codec_header.buf + 8, s->ci->codec_header.len - 8);
    ogg_sync_wrote(&priv->dec_oy, s->ci->codec_header.len - 8);

    if(!next_page(s))
      return 0;
    if(!next_packet(s))
      return 0;
    /* Initialize vorbis */
    if(vorbis_synthesis_headerin(&priv->dec_vi, &priv->dec_vc,
                                 &priv->dec_op) < 0)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "decode: vorbis_synthesis_headerin: not a vorbis header");
      return 0;
      }
    
    if(!next_packet(s))
      return 0;
    vorbis_synthesis_headerin(&priv->dec_vi, &priv->dec_vc, &priv->dec_op);
    if(!next_packet(s))
      return 0;
    vorbis_synthesis_headerin(&priv->dec_vi, &priv->dec_vc, &priv->dec_op);
    }

  /*
   *  OggV method (qtcomponents.sf.net):
   *  In the sample description, we have an atom of type
   *  'wave', which is parsed by the demuxer.
   *  Inside this atom, there is an atom 'OVHS', which
   *  containes the header packets encapsulated in
   *  ogg pages
   */
  
  else if(s->fourcc == BGAV_MK_FOURCC('O','g','g','V'))
    {
    ptr = s->ci->codec_header.buf;
    len = GAVL_PTR_2_32BE(ptr);ptr+=4;
    fourcc = BGAV_PTR_2_FOURCC(ptr);ptr+=4;
    if(fourcc != BGAV_MK_FOURCC('O','V','H','S'))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "No OVHS Atom found");
      return 0;
      }

    buffer = ogg_sync_buffer(&priv->dec_oy, len - 8);
    memcpy(buffer, ptr, len - 8);
    ogg_sync_wrote(&priv->dec_oy, len - 8);

    if(!next_page(s))
      return 0;
    
    if(!next_packet(s))
      return 0;
    
    /* Initialize vorbis */
    
    if(vorbis_synthesis_headerin(&priv->dec_vi, &priv->dec_vc,
                                 &priv->dec_op) < 0)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "decode: vorbis_synthesis_headerin: not a vorbis header");
      return 0;
      }
    
    if(!next_packet(s))
      return 0;
    vorbis_synthesis_headerin(&priv->dec_vi, &priv->dec_vc, &priv->dec_op);
    if(!next_packet(s))
      return 0;
    vorbis_synthesis_headerin(&priv->dec_vi, &priv->dec_vc, &priv->dec_op);
    }
  
  /* BGAV Way: Header packets are in extradata in a segemented packet */
  
  else if(s->fourcc == BGAV_VORBIS)
    {
    ogg_packet op;
    int i;
    int len;
    memset(&op, 0, sizeof(op));
    
    if(!s->ci->codec_header.buf)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "No extradata found");
      return 0;
      }
    
    ptr = s->ci->codec_header.buf;

    op.b_o_s = 1;

    //    gavl_hexdump(s->ci.global_header, 64, 16);
    
    for(i = 0; i < 3; i++)
      {
      if(i)
        op.b_o_s = 0;
      
      op.packet = gavl_extract_xiph_header(&s->ci->codec_header,
                                           i, &len);
      op.bytes = len;
      
      if(vorbis_synthesis_headerin(&priv->dec_vi, &priv->dec_vc,
                                   &op) < 0)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Packet %d is not a vorbis header", i+1);
        return 0;
        }
      op.packetno++;
      }
    }
  
  vorbis_synthesis_init(&priv->dec_vd, &priv->dec_vi);
  vorbis_block_init(&priv->dec_vd, &priv->dec_vb);

  // #ifdef HAVE_VORBIS_SYNTHESIS_RESTART
  //  vorbis_synthesis_restart(&priv->dec_vd);
  // #endif  
  s->data.audio.format->sample_format   = GAVL_SAMPLE_FLOAT;
  s->data.audio.format->interleave_mode = GAVL_INTERLEAVE_NONE;
  s->data.audio.format->samples_per_frame = 2048;

  /* Set up audio format from the vorbis header overriding previous values */
  s->data.audio.format->samplerate = priv->dec_vi.rate;
  s->data.audio.format->num_channels = priv->dec_vi.channels;

  /* Samples per frame is just the maximum */
  s->src_flags |= GAVL_SOURCE_SRC_FRAMESIZE_MAX;
  
  /* Vorbis 5.1 mapping */
  
  bgav_vorbis_set_channel_setup(s->data.audio.format);
  gavl_set_channel_setup(s->data.audio.format);

  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT, "Ogg Vorbis");
  
  /* Preroll */

  /* Samples of packet N is (blocksize[n-1]+blocksize[n])/4
     Largest Block size: bz[1]
     Largest Packet size: (bz[1]+bz[1])/4 = bz[1]/2
     2 packets: bz[1]
   */
  s->data.audio.sync_samples = vorbis_info_blocksize(&priv->dec_vi, 1);
  return 1;
  }

static gavl_source_status_t decode_frame_vorbis(bgav_stream_t * s)
  {
  vorbis_audio_priv * priv;
  float ** channels;
  int i;
  gavl_source_status_t st;
  int samples_decoded = 0;
  
  priv = s->decoder_priv;
    
  /* Decode stuff */
  
  while(1)
    {
    samples_decoded =
      vorbis_synthesis_pcmout(&priv->dec_vd, &channels);

    if(samples_decoded > 0)
      break;
    
    //    fprintf(stderr, "decode_frame_vorbis %ld\n", priv->dec_op.granulepos);
    
    if((st = next_packet(s)) != GAVL_SOURCE_OK)
      return st;
    
    if(vorbis_synthesis(&priv->dec_vb, &priv->dec_op) == 0)
      {
      vorbis_synthesis_blockin(&priv->dec_vd,
                               &priv->dec_vb);
      }
    /* Reset bytes so we know that we ate this packet */
    priv->dec_op.bytes = 0;
    }
  
  for(i = 0; i < s->data.audio.format->num_channels; i++)
    s->data.audio.frame->channels.f[i] = channels[i];
  
  vorbis_synthesis_read(&priv->dec_vd, samples_decoded);
  s->data.audio.frame->valid_samples = samples_decoded;

  /* This can happen for the last packet */
  if(priv->dec_op.e_o_s && (s->data.audio.frame->valid_samples > priv->p.duration))
    s->data.audio.frame->valid_samples = priv->p.duration;

#ifdef DUMP_OUTPUT
  gavl_dprintf("Vorbis samples decoded: %d\n",
               s->data.audio.frame->valid_samples);
#endif
  
  return GAVL_SOURCE_OK;
  }

static void resync_vorbis(bgav_stream_t * s)
  {
  vorbis_audio_priv * priv;
  priv = s->decoder_priv;
  priv->dec_op.bytes = 0;
  if(s->fourcc == BGAV_VORBIS)
    {
    priv->packetno = 0;
    }
  else
    {
    ogg_stream_clear(&priv->dec_os);
    ogg_sync_reset(&priv->dec_oy);
    priv->stream_initialized = 0;
    if(!next_page(s))
      return;
    ogg_sync_init(&priv->dec_oy);
    ogg_stream_init(&priv->dec_os, ogg_page_serialno(&priv->dec_og));
    }
#ifdef HAVE_VORBIS_SYNTHESIS_RESTART
  vorbis_synthesis_restart(&priv->dec_vd);
#else
  vorbis_dsp_clear(&priv->dec_vd);
  vorbis_block_clear(&priv->dec_vb);
  vorbis_synthesis_init(&priv->dec_vd, &priv->dec_vi);
  vorbis_block_init(&priv->dec_vd, &priv->dec_vb);
#endif  

  /* Skip until we can cleanly decode */

  if(s->fourcc == BGAV_VORBIS)
    {
    bgav_packet_t * p;
    int samples_decoded;
    float ** channels;
    gavl_source_status_t st;
    
    if(!next_packet(s))
      return;
    
    if(vorbis_synthesis(&priv->dec_vb, &priv->dec_op) == 0)
      {
      // fprintf(stderr, "Resync: blockin\n");
      vorbis_synthesis_blockin(&priv->dec_vd,
                               &priv->dec_vb);
      }

    samples_decoded =
      vorbis_synthesis_pcmout(&priv->dec_vd, &channels);
    // fprintf(stderr, "Samples decoded after resync %d\n", samples_decoded);

    vorbis_synthesis_read(&priv->dec_vd, samples_decoded);
    
    /* Synchronize output time to the next packet */
    p = NULL;
    if((st = bgav_stream_peek_packet_read(s, &p)) == GAVL_SOURCE_OK)
      {
      s->out_time = p->pts;
      //      fprintf(stderr, "Vorbis resync PTS: %"PRId64"\n", p->pts);
      }
    }
  
  }

static void close_vorbis(bgav_stream_t * s)
  {
  vorbis_audio_priv * priv;
  priv = s->decoder_priv;

  ogg_stream_clear(&priv->dec_os);
  ogg_sync_clear(&priv->dec_oy);
  vorbis_block_clear(&priv->dec_vb);
  vorbis_dsp_clear(&priv->dec_vd);
  vorbis_comment_clear(&priv->dec_vc);
  vorbis_info_clear(&priv->dec_vi);

  gavl_packet_free(&priv->p);
  
  free(priv);
  }

static bgav_audio_decoder_t decoder =
  {
    .fourccs = (uint32_t[]){ BGAV_MK_FOURCC('O','g', 'g', 'S'),
                             BGAV_MK_FOURCC('O','g', 'g', 'V'),
                             BGAV_VORBIS,
                             BGAV_MK_FOURCC('V', 'O', 'R', 'B'),
                             BGAV_WAVID_2_FOURCC(0x674f), // Mode 1  (header in first packet)
                             BGAV_WAVID_2_FOURCC(0x676f), // Mode 1+
                             BGAV_WAVID_2_FOURCC(0x6750), // Mode 2  (header in extradata)
                             BGAV_WAVID_2_FOURCC(0x6770), // Mode 2+
                             //                           BGAV_WAVID_2_FOURCC(0x6751), // Mode 3  (no header)
                           //                           BGAV_WAVID_2_FOURCC(0x6771), // Mode 3+
                           0x00 },
    .name = "Ogg vorbis audio decoder",
    .init =   init_vorbis,
    .close =  close_vorbis,
    .resync = resync_vorbis,
    .decode_frame = decode_frame_vorbis
  };

void bgav_init_audio_decoders_vorbis()
  {
  bgav_audio_decoder_register(&decoder);
  }

