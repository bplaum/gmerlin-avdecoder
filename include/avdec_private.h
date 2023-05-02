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

#ifndef BGAV_AVDEDEC_PRIVATE_H_INCLUDED
#define BGAV_AVDEDEC_PRIVATE_H_INCLUDED

#include "config.h"

#include <avdec.h>
#include <libavcodec/version.h>
#include <gavl/gavf.h>
#include <gavl/log.h>

#include <stdio.h> /* Needed for fileindex stuff */

#include <libintl.h>

#include <os.h>
#include <gavl/metatags.h>
#include <gavl/compression.h>
#include <gavl/numptr.h>
#include <gavl/utils.h>
#include <gavl/trackinfo.h>

#include <bsf.h>


#define BGAV_MK_FOURCC(a, b, c, d) ((a<<24)|(b<<16)|(c<<8)|d)

#define BGAV_VORBIS BGAV_MK_FOURCC('V','B','I','S')

typedef struct bgav_demuxer_s         bgav_demuxer_t;
typedef struct bgav_demuxer_context_s bgav_demuxer_context_t;

typedef struct bgav_redirector_s         bgav_redirector_t;

#define bgav_packet_t gavl_packet_t

typedef struct bgav_file_index_s      bgav_file_index_t;
typedef struct bgav_packet_parser_s   bgav_packet_parser_t;

typedef struct bgav_input_s                    bgav_input_t;
typedef struct bgav_input_context_s            bgav_input_context_t;
typedef struct bgav_audio_decoder_s            bgav_audio_decoder_t;
typedef struct bgav_video_decoder_s            bgav_video_decoder_t;
typedef struct bgav_subtitle_converter_s bgav_subtitle_converter_t;
typedef struct bgav_stream_s   bgav_stream_t;

typedef struct bgav_charset_converter_s bgav_charset_converter_t;

typedef struct bgav_track_s bgav_track_t;

typedef struct bgav_timecode_table_s bgav_timecode_table_t;

#include <id3.h>
#include <yml.h>
#include <packettimer.h>
#include <frametype.h>

/* Decoder structures */

struct bgav_audio_decoder_s
  {
  const uint32_t * fourccs;
  const char * name;
  int (*init)(bgav_stream_t*);
  gavl_source_status_t (*decode_frame)(bgav_stream_t*);
  void (*close)(bgav_stream_t*);
  void (*resync)(bgav_stream_t*);
  bgav_audio_decoder_t * next;
  };

struct bgav_video_decoder_s
  {
  const uint32_t * fourccs;
  const char * name;
  int flags;

  int (*init)(bgav_stream_t*);


  /*
   *  Decodes one frame. If frame is NULL;
   *  the frame is skipped.
   *  If this function is NULL, the codec must create a
   *  video source for the stream
   */
  gavl_source_status_t (*decode)(bgav_stream_t*, gavl_video_frame_t*);
  
  void (*close)(bgav_stream_t*);
  
  void (*resync)(bgav_stream_t*);
  
  /* Skip to a specified time. Only needed for
     decoders which are not synchronous
     (not one packet in, one frame out) */
  int (*skipto)(bgav_stream_t*, int64_t dest);
  
  bgav_video_decoder_t * next;
  };

/* These map a palette entry to a gavl format frame */

#define BGAV_PALETTE_2_RGB24(pal, dst) \
dst[0] = pal.r >> 8;\
dst[1] = pal.g >> 8;\
dst[2] = pal.b >> 8;

#define BGAV_PALETTE_2_RGBA32(pal, dst) \
dst[0] = pal.r >> 8;\
dst[1] = pal.g >> 8;\
dst[2] = pal.b >> 8;\
dst[3] = pal.a >> 8;

#define BGAV_PALETTE_2_BGR24(pal, dst) \
dst[2] = pal.r >> 8;\
dst[1] = pal.g >> 8;\
dst[0] = pal.b >> 8;

/* Packet */

#define BGAV_CODING_TYPE_I GAVL_PACKET_TYPE_I
#define BGAV_CODING_TYPE_P GAVL_PACKET_TYPE_P
#define BGAV_CODING_TYPE_B GAVL_PACKET_TYPE_B
// #define BGAV_CODING_TYPE_D 'D' /* Unsupported */


/* If these flags are changed, the flags of the superindex must be
   changed as well */

#define PACKET_SET_CODING_TYPE(p, t) (p)->flags |= t
#define PACKET_SET_KEYFRAME(p)       (p)->flags |= GAVL_PACKET_KEYFRAME
#define PACKET_SET_SKIP(p)           (p)->flags |= GAVL_PACKET_SKIP
#define PACKET_SET_LAST(p)           (p)->flags |= GAVL_PACKET_LAST
#define PACKET_SET_REF(p)            (p)->flags |= GAVL_PACKET_REF
#define PACKET_SET_FIELD_PIC(p)      (p)->flags |= GAVL_PACKET_FIELD_PIC

#define PACKET_GET_CODING_TYPE(p)    ((p)->flags & GAVL_PACKET_TYPE_MASK)
#define PACKET_GET_KEYFRAME(p)       ((p)->flags & GAVL_PACKET_KEYFRAME)
#define PACKET_GET_SKIP(p)           ((p)->flags & GAVL_PACKET_SKIP)
#define PACKET_GET_LAST(p)           ((p)->flags & GAVL_PACKET_LAST)
#define PACKET_GET_REF(p)            ((p)->flags & GAVL_PACKET_REF)
// #define PACKET_GET_FIELD_PIC(p)      ((p)->flags & PACKET_FLAG_FIELD_PIC)

/* packet.c */

bgav_packet_t * bgav_packet_create();

void bgav_packet_destroy(bgav_packet_t*);
void bgav_packet_free(bgav_packet_t*);

#define bgav_packet_reset(p) gavl_packet_reset(p)
#define bgav_packet_alloc(p, s) gavl_packet_alloc(p, s)

#define bgav_packet_dump(p) gavl_packet_dump(p)

void bgav_packet_dump_data(bgav_packet_t * p, int bytes);

void bgav_packet_pad(bgav_packet_t * p);
// void bgav_packet_reset(bgav_packet_t * p);

// void bgav_packet_alloc_palette(bgav_packet_t * p, int size);
void bgav_packet_copy_metadata(bgav_packet_t * dst,
                               const bgav_packet_t * src);

void bgav_packet_copy(bgav_packet_t * dst,
                      const bgav_packet_t * src);

/* Stream structure */ 

#define BGAV_ENDIANESS_NONE   0 // Unspecified
#define BGAV_ENDIANESS_BIG    1
#define BGAV_ENDIANESS_LITTLE 2

#define STREAM_PARSE_FULL         (1<<0) /* Not frame aligned */
#define STREAM_PARSE_FRAME        (1<<1) /* Frame aligned but no keyframes */
#define STREAM_DTS_ONLY           (1<<2)
#define STREAM_STILL_SHOWN        (1<<3)  /* Still image already shown */
#define STREAM_EOF_D              (1<<4)  /* End of file at demuxer    */
#define STREAM_EOF_C              (1<<5)  /* End of file at codec      */
// #define STREAM_NEED_FRAMETYPES    (1<<6) /* Need frame types          */

/* Picture is available for immediate output */
#define STREAM_HAVE_FRAME         (1<<7)

/* Already got the format from the parser */

#define STREAM_RAW_PACKETS           (1<<9)
#define STREAM_FILTER_PACKETS        (1<<10)
#define STREAM_NO_DURATIONS          (1<<11)
#define STREAM_HAS_DTS               (1<<12)
#define STREAM_B_PYRAMID             (1<<13)

#define STREAM_GOT_CI                (1<<14) // Compression info present
#define STREAM_GOT_NO_CI             (1<<15) // Compression info tested but not present
#define STREAM_DISCONT               (1<<16) // Stream is discontinuous
#define STREAM_STANDALONE            (1<<18) // Standalone decoder
#define STREAM_EXTERN                (1<<19) // Exteral to the demultiplexer (subtitle file, message stream)

#define STREAM_STARTED               (1<<22) /* Stream started already */

#define STREAM_WRITE_STARTED         (1<<23)

// Set by the ogg demultiplexer to specify that the pts_end is set by the demuxer backend
#define STREAM_DEMUXER_SETS_PTS_END  (1<<24)


/* Stream could not get extract compression info from the
 * demuxer
 */

#define STREAM_SET_SYNC(s, t)  (s)->sync_time = t; if((s)->pbuffer) gavl_packet_buffer_set_out_pts((s)->pbuffer, t)
#define STREAM_GET_SYNC(s)     (s)->sync_time

#define STREAM_UNSET_SYNC(s)   (s)->sync_time = GAVL_TIME_UNDEFINED

#define STREAM_HAS_SYNC(s)     ((s)->sync_time != GAVL_TIME_UNDEFINED)

#define STREAM_SET_STILL(s) \
  s->data.video.format->framerate_mode = GAVL_FRAMERATE_STILL; \
  s->ci->flags &= ~(GAVL_COMPRESSION_HAS_P_FRAMES|GAVL_COMPRESSION_HAS_B_FRAMES);

#define STREAM_IS_STILL(s) \
    ((s->type == GAVL_STREAM_VIDEO) && (s->data.video.format->framerate_mode == GAVL_FRAMERATE_STILL))

typedef struct
  {
  int depth;
  int planes;     /* For M$ formats only */
  int image_size; /* For M$ formats only */
      
  bgav_video_decoder_t * decoder;
  gavl_video_format_t * format;
  //      int palette_changed;
      
  // bgav_keyframe_table_t * kft;
      
  //  int max_ref_frames; /* Needed for VDPAU */
      
  //  bgav_video_format_tracker_t * ft;
  /* Palette */
  int pal_sent;
  gavl_palette_t * pal;
  
  gavl_video_source_t * vsrc;
  gavl_video_source_t * vsrc_priv;
  
  } bgav_stream_video_t;
  
struct bgav_stream_s
  {
  gavl_dictionary_t in_info;
  
  gavl_dictionary_t * info;
  gavl_dictionary_t * info_ext;
  
  gavl_packet_source_t * psrc_priv; // Packet source coming right after the packet buffer
  
  bgav_packet_parser_t * parser;
  
  
  void * priv;
  void * decoder_priv;
  
  //  int initialized; /* Mostly means, that the format is valid */

  int64_t dts; // Auxillary variable for generating timestamps on the fly
  
  const bgav_options_t * opt;

  bgav_stream_action_t action;
  int stream_id; /* Format specific stream id */
  gavl_stream_type_t type;

  //  bgav_packet_buffer_t * packet_buffer;
  
  uint32_t fourcc;

  uint32_t subformat; /* Real flavors, sub_ids.... */
  
  int64_t in_position;  /* In packets */
  
  /*
   *  Support for custom timescales
   *  Default timescales are the samplerate for audio
   *  streams and the timescale from the format for video streams.
   *  Demuxers can, however, define other timescales.
   */
  int timescale;
  
   /*
    * Sync time:
    * 
    * - *Only* valid for resynchronization during seeking
    *
    * - If the demuxer seeks, it sets the sync_time in
    *   *packet* timescale
    *
    * - If we seek sample accurately, it's the output time
    *   in *sample* timescale
    */
  
  int64_t sync_time;

  int64_t out_time; /* In codec timescale */
  
  /* Positions in the superindex */
  
  int first_index_position;
  int last_index_position;
  int index_position;
  
  /* Where to get data */
  bgav_demuxer_context_t * demuxer;

  /* Some demuxers can only read incomplete packets at once,
     so they can store the packets here */

  bgav_packet_t * packet;
  int             packet_seq;
  
  gavl_dictionary_t * m;
  
  /*
   *  Sometimes, the bitrates important for codecs 
   *  and the bitrates set in the container format
   *  differ, so we save both here
   */
  
  int container_bitrate;
  int codec_bitrate;
  
  /*
   *  See STREAM_ defines above
   */

  int flags;
  
  /* Passed to gavl_[audio|video]_source_create() */
  int src_flags;
  
  gavl_stream_stats_t stats;
  
  /*
   *  Timestamp of the first frame in *output* timescale
   *  must be set by bgav_start()
   */
  
  /* The track, where this stream belongs */
  bgav_track_t * track;
  //   bgav_file_index_t * file_index;

  gavl_seek_index_t index;
  
  void (*process_packet)(bgav_stream_t * s, bgav_packet_t * p);

  /* Cleanup function (can be set by demuxers) */
  void (*cleanup)(bgav_stream_t * s);
  
  /* Set for INDEX_MODE_SI_PARSE for all streams, which need parsing */
  int index_mode;

  /* timecode table (for video streams only for now) */
  bgav_timecode_table_t * timecode_table;
  
  bgav_packet_filter_t * pf;
  
  /* Compression info (the ci-pointer might be changed by a bitstream filter) */
  gavl_compression_info_t * ci;
  gavl_compression_info_t ci_orig;
  
  /* If this is set, we will pass this to the
     source */
  gavl_video_frame_t * vframe;
  
  /* New gavlized packet handling */

  gavl_packet_source_t * psrc; /* Output packets for the public API */
  gavl_packet_sink_t * psink;
  gavl_packet_buffer_t * pbuffer;

  gavl_packet_sink_t * psink_parse; // Packet sink used with index building or duration scanning
  
  union
    {
    struct
      {
      gavl_audio_format_t * format;
      bgav_audio_decoder_t * decoder;
      int bits_per_sample; /* In some cases, this must be set from the
                              Container to distinguish between 8 and 16 bit
                              PCM codecs. For compressed codecs like mp3, this
                              field is nonsense*/
      
      /* The following ones are mainly for Microsoft formats and codecs */
      int block_align;

      /* This is ONLY used for codecs, which can be both little-
         and big-endian. In this case, the endianess is set by
         the demuxer */
      
      int endianess;

      /* Number of *samples* which must be decoded before
         the next frame after seeking. Codecs set this, demuxers
         can honour it when seeking. */
      
      int preroll;
      
      gavl_audio_frame_t * frame;
      
      gavl_audio_source_t * source;
      
      } audio;
    bgav_stream_video_t video;
    struct
      {
      bgav_stream_video_t video; // Must be first element
      /* Charset converter for text subtitles */
      bgav_subtitle_converter_t * cnv;
      
      char * charset;
      
      /* The video stream, onto which the subtitles will be
         displayed */
      // bgav_stream_t * vs;
      // int vs_index;
      
      } subtitle;
    struct
      {
      gavl_handle_msg_func msg_callback;
      void * msg_callback_data;
      } msg;
    } data;
  };

/* stream.c */

void bgav_stream_flush(bgav_stream_t * s);
// void bgav_stream_set_continuous(bgav_stream_t * s);

gavl_sink_status_t bgav_stream_put_packet_get_duration(void * priv, gavl_packet_t * p);
gavl_sink_status_t bgav_stream_put_packet_parse(void * priv, gavl_packet_t * p);

int bgav_stream_start(bgav_stream_t * stream);
void bgav_stream_stop(bgav_stream_t * stream);
void bgav_stream_create_packet_buffer(bgav_stream_t * stream);
void bgav_stream_create_packet_pool(bgav_stream_t * stream);
void bgav_stream_init(bgav_stream_t * stream, const bgav_options_t * opt);
void bgav_stream_free(bgav_stream_t * stream);
void bgav_stream_dump(bgav_stream_t * s);
void bgav_stream_set_extradata(bgav_stream_t * s, const uint8_t * data, int len);

void bgav_stream_set_parse_full(bgav_stream_t * s);
void bgav_stream_set_parse_frame(bgav_stream_t * s);

void bgav_stream_set_from_gavl(bgav_stream_t * s,
                               gavl_dictionary_t * dict);

int bgav_streams_foreach(bgav_stream_t * s, int num,
                         int (*action)(void * priv, bgav_stream_t * s), void * priv);

int64_t bgav_stream_get_duration(bgav_stream_t * s);


/* Top level packet functions */
bgav_packet_t * bgav_stream_get_packet_write(bgav_stream_t * s);
void bgav_stream_done_packet_write(bgav_stream_t * s, bgav_packet_t * p);

/* Callbacks for packet sources */

gavl_source_status_t
bgav_stream_read_func_continuous(bgav_stream_t * s, gavl_packet_t ** p);

/* Read one packet from a discontinuous (e.g. subtitle-) stream */

gavl_source_status_t
bgav_stream_read_func_discontinuous(bgav_stream_t * s, gavl_packet_t ** p);


/* TODO */
gavl_source_status_t
bgav_stream_get_packet_read(bgav_stream_t * s, bgav_packet_t ** ret);
gavl_source_status_t
bgav_stream_peek_packet_read(bgav_stream_t * s, bgav_packet_t ** ret);

void bgav_stream_done_packet_read(bgav_stream_t * s, bgav_packet_t * p);


/* Which timestamp would come if we would decode right now? */

gavl_time_t bgav_stream_next_timestamp(bgav_stream_t *);

/* Set timescales and stuff */
void bgav_stream_set_timimg(bgav_stream_t * s);

/* Clear the packet buffer, called before seeking */

void bgav_stream_clear(bgav_stream_t * s);

/*
 *  Initialize for reading packets, get the compression info,
 *  called after a track is selected
 */

int bgav_stream_init_read(bgav_stream_t * s);

/*
 * Skip to a specific point which must be larger than the current stream time
 */

int bgav_stream_skipto(bgav_stream_t * s, int64_t * time, int scale);


#define TRACK_SAMPLE_ACCURATE (1<<0)
#define TRACK_HAS_COMPRESSION (1<<2)

struct bgav_track_s
  {
  // char * name;
  bgav_metadata_t * metadata;

  int num_streams;
  bgav_stream_t * streams;
  
  int num_audio_streams;
  int num_video_streams;
  int num_text_streams;
  int num_overlay_streams;
  int num_msg_streams;
  int streams_alloc;
  
  void * priv; /* For storing private data */  

  int flags;

  gavl_dictionary_t * info;
  
  /* Data start (set intially to -1, maybe set to
     other values by demuxers). It can be used by the core to
     seek to the position, where the demuxer can start working
  */
  int64_t data_start;
  int64_t data_end; // Can be set by the demuxer to the end of the packet data section
  
  };

/* track.c */

void bgav_track_export_infos(bgav_track_t * t);

void bgav_track_set_format(bgav_track_t * track, const char * format, const char * mimetype);

int bgav_track_set_stream_action(bgav_track_t * track, gavl_stream_type_t type, int stream,
                                 bgav_stream_action_t action);

bgav_stream_t *
bgav_track_add_audio_stream(bgav_track_t * t, const bgav_options_t * opt);

bgav_stream_t *
bgav_track_add_video_stream(bgav_track_t * t, const bgav_options_t * opt);

bgav_stream_t *
bgav_track_add_msg_stream(bgav_track_t * t, const bgav_options_t * opt, int id);

void
bgav_track_remove_stream(bgav_track_t * track, int stream);

void
bgav_track_remove_audio_stream(bgav_track_t * track, int stream);

void
bgav_track_remove_video_stream(bgav_track_t * track, int stream);

void
bgav_track_remove_text_stream(bgav_track_t * track, int stream);

void
bgav_track_remove_overlay_stream(bgav_track_t * track, int stream);



bgav_stream_t * bgav_track_get_stream(bgav_track_t * track, gavl_stream_type_t type,
                                      int stream);

bgav_stream_t * bgav_track_get_audio_stream(bgav_track_t * track, int stream);
bgav_stream_t * bgav_track_get_video_stream(bgav_track_t * track, int stream);
bgav_stream_t * bgav_track_get_text_stream(bgav_track_t * track, int stream);
bgav_stream_t * bgav_track_get_overlay_stream(bgav_track_t * track, int stream);
bgav_stream_t * bgav_track_get_msg_stream(bgav_track_t * track, int stream);

bgav_stream_t * bgav_track_get_msg_stream_by_id(bgav_track_t * track, int id);

bgav_stream_t *
bgav_track_add_text_stream(bgav_track_t * t, const bgav_options_t * opt,
                           const char * encoding);

bgav_stream_t *
bgav_track_add_overlay_stream(bgav_track_t * t, const bgav_options_t * opt);

bgav_stream_t *
bgav_track_find_stream(bgav_demuxer_context_t * t, int stream_id);

bgav_stream_t * bgav_track_get_subtitle_stream(bgav_track_t * t, int index);

int bgav_track_foreach(bgav_track_t * t,
                     int (*action)(void * priv, bgav_stream_t * s), void * priv);

const gavl_dictionary_t * bgav_track_get_priv(const bgav_track_t * t);
gavl_dictionary_t * bgav_track_get_priv_nc(bgav_track_t * t);

void bgav_track_compute_info(bgav_track_t * t);

/* Clear all buffers (call BEFORE seeking) */

void bgav_track_clear(bgav_track_t * track);

/* Initialize track (called after the track is selected */
void bgav_track_init_read(bgav_track_t * track);

/* Call after the track becomes current */
void bgav_track_mute(bgav_track_t * t);

/* Resync the decoders, update output times */

void bgav_track_resync(bgav_track_t*);

/* Skip to a specific point */

int bgav_track_skipto(bgav_track_t*, int64_t * time, int scale);

/* Find stream among ALL streams, also switched off ones */

bgav_stream_t *
bgav_track_find_stream_all(bgav_track_t * t, int stream_id);

int bgav_track_start(bgav_track_t * t, bgav_demuxer_context_t * demuxer);
void bgav_track_stop(bgav_track_t * t);

int64_t bgav_track_sync_time(bgav_track_t * t, int scale);
int64_t bgav_track_out_time(bgav_track_t * t, int scale);

void bgav_track_set_eof_d(bgav_track_t * t);
void bgav_track_clear_eof_d(bgav_track_t * t);
int bgav_track_eof_d(bgav_track_t * t);


/* Remove unsupported streams */

void bgav_track_remove_unsupported(bgav_track_t * t);

void bgav_track_free(bgav_track_t * t);

void bgav_track_dump(bgav_track_t * t);

int bgav_track_has_sync(bgav_track_t * t);


void bgav_track_reset_index_positions(bgav_track_t * t);


/* Tracktable */

typedef struct
  {
  int num_tracks;
  bgav_track_t ** tracks;
  bgav_track_t * cur;
  int cur_idx;
  int refcount;
  
  gavl_dictionary_t info;
  } bgav_track_table_t;

bgav_track_table_t * bgav_track_table_create(int num_tracks);
bgav_track_t * bgav_track_table_append_track(bgav_track_table_t * t);
void bgav_track_table_remove_track(bgav_track_table_t * t, int idx);

void bgav_track_table_unref(bgav_track_table_t*);
void bgav_track_table_ref(bgav_track_table_t*);

void bgav_track_table_select_track(bgav_track_table_t*,int);
void bgav_track_table_dump(bgav_track_table_t*);

void bgav_track_table_merge_metadata(bgav_track_table_t*,
                                     bgav_metadata_t * m);

void bgav_track_table_remove_unsupported(bgav_track_table_t * t);

void bgav_track_table_compute_info(bgav_track_table_t * t);
void bgav_track_table_create_message_streams(bgav_track_table_t * t, const bgav_options_t * opt);
void bgav_track_table_export_infos(bgav_track_table_t * t);


/* Options (shared between inputs, demuxers and decoders) */

struct bgav_options_s
  {
  /* Try sample accurate processing */
  int sample_accurate;
  int cache_time;
  int cache_size;
  
  /* Generic network options */
  int connect_timeout;
  int read_timeout;

  int network_bandwidth;

  //  int network_buffer_size;

  /* 0..1024:     Randomize       */
  /* 1025..65536: Fixed port base */
  
  int rtp_port_base;
  int rtp_try_tcp; /* try TCP before falling back to UDP */
  
  /* ftp options */
    
  char * ftp_anonymous_password;
  int ftp_anonymous;

  /* Default character set for text subtitles */
  char * default_subtitle_encoding;
  
  int audio_dynrange;
  int seamless;

  /* Postprocessing level (0.0 .. 1.0) */
  
  float pp_level;

  char * dvb_channels_file;

  /* Prefer ffmpeg demuxers over native demuxers */
  int prefer_ffmpeg_demuxers;

  int dv_datetime;
  
  int shrink;

  int vaapi;

  int log_level;

  int dump_headers;
  int dump_indices;
  int dump_packets; 
  /* Callbacks */
  
  bgav_metadata_change_callback metadata_callback;
  void * metadata_callback_data;
  
  bgav_buffer_callback buffer_callback;
  void * buffer_callback_data;

  bgav_user_pass_callback user_pass_callback;
  void * user_pass_callback_data;

  bgav_aspect_callback aspect_callback;
  void * aspect_callback_data;
  
  bgav_index_callback index_callback;
  void * index_callback_data;
  };

BGAV_PUBLIC void bgav_options_set_defaults(bgav_options_t*opt);

void bgav_options_free(bgav_options_t*opt);

#if 0
void bgav_options_metadata_changed(const bgav_options_t * opt,
                                   const gavl_dictionary_t * new_metadata,
                                   gavl_time_t pts);
#endif

/* Overloadable input module */

struct bgav_input_s
  {
  const char * name;
  int     (*open)(bgav_input_context_t*, const char * url, char ** redirect_url);
  int     (*read)(bgav_input_context_t*, uint8_t * buffer, int len);

  /* Attempts to read data but returns immediately if there is nothing
     available */
  
  int64_t (*seek_byte)(bgav_input_context_t*, int64_t pos, int whence);
  void    (*close)(bgav_input_context_t*);

  /* Some inputs support multiple tracks */

  int    (*select_track)(bgav_input_context_t*, int);

  /* Alternate API: Block based read and seek access */
  
  int (*read_block)(bgav_input_context_t*);
  int (*seek_block)(bgav_input_context_t*, int64_t block);
  
  /*
   * Time based seek function for media, which are not stored
   * stricktly linear. Time is changed to the actual seeked time.
   */
  
  void (*seek_time)(bgav_input_context_t*, gavl_time_t * time);
  
  /* Some inputs autoscan the available devices */
  bgav_device_info_t (*find_devices)();

  void (*pause)(bgav_input_context_t*);
  void (*resume)(bgav_input_context_t*);

  /* Non-Blocking API */
  int (*can_read)(bgav_input_context_t*, int timeout);
  int  (*read_nonblock)(bgav_input_context_t*, uint8_t * buffer, int len);
  };

// #define BGAV_INPUT_DO_BUFFER      (1<<0)
#define BGAV_INPUT_CAN_PAUSE      (1<<1)
#define BGAV_INPUT_CAN_SEEK_BYTE  (1<<2)
#define BGAV_INPUT_CAN_SEEK_TIME  (1<<3)
#define BGAV_INPUT_SEEK_SLOW      (1<<4)
#define BGAV_INPUT_PAUSED         (1<<5)

struct bgav_input_context_s
  {
  gavl_buffer_t buf;

  /* ID3V2 tags can be prepended to many types of files,
     so we read them globally */

  bgav_id3v2_tag_t * id3v2;
  
  void * priv;
  int64_t total_bytes; /* Maybe 0 for non seekable streams */
  int64_t position;    /* Updated also for non seekable streams */
  const bgav_input_t * input;

  /* Some input modules already fire up a demuxer */
    
  bgav_demuxer_context_t * demuxer;
  
  char * location;

  /* For reading textfiles */
  char * charset;
  
  bgav_charset_converter_t * cnv;
  
  /* For multiple track support */

  bgav_track_table_t * tt;

  bgav_metadata_t m;

  /* This is set by the modules to signal that we
     need to prebuffer data */

  int flags;

  /* For sector based access */
#if 0
  int sector_size;
  int sector_size_raw;
  int sector_header_size;
  int64_t total_sectors;
  int64_t sector_position;
#endif

  /* Set by read_block() */
  int block_size;
  const uint8_t * block;
  const uint8_t * block_ptr;
  
  bgav_options_t opt;
  
  // Stream ID, which will be used for syncing (for DVB)
  int sync_id;
  
  /* Inputs set this, if indexing is supported */
  char * index_file;

  bgav_yml_node_t * yml;
  
  bgav_t * b;

  /* Set by the HLS input, read by the adts demuxer */
  int64_t input_pts;
  
  /*
   *  Set by the mpegts and vtt demuxers, read by the HLS input to have a
   *  PTS <-> Segment assosiation
   */
  
  int64_t demuxer_pts;
  int demuxer_scale;
  
  /*
   *  Set by the HLS input, converted to metadata
   *  by the mpegts and adts demultiplexers
   */
  
  int64_t clock_time;
  };

/* input.c */

/* Read functions return FALSE on error */

BGAV_PUBLIC int bgav_input_read_data(bgav_input_context_t*, uint8_t*, int);

void bgav_input_set_demuxer_pts(bgav_input_context_t*, int64_t pts, int scale);

/* Non-blocking i/o */

int bgav_input_can_read(bgav_input_context_t*, int milliseconds);
int bgav_input_read_nonblock(bgav_input_context_t*, uint8_t*, int);

int bgav_input_read_string_pascal(bgav_input_context_t*, char*);

int bgav_input_read_8(bgav_input_context_t*,uint8_t*);
int bgav_input_read_16_le(bgav_input_context_t*,uint16_t*);
int bgav_input_read_24_le(bgav_input_context_t*,uint32_t*);
int bgav_input_read_32_le(bgav_input_context_t*,uint32_t*);
int bgav_input_read_64_le(bgav_input_context_t*,uint64_t*);

int bgav_input_read_16_be(bgav_input_context_t*,uint16_t*);
int bgav_input_read_24_be(bgav_input_context_t*,uint32_t*);
int bgav_input_read_32_be(bgav_input_context_t*,uint32_t*);
int bgav_input_read_64_be(bgav_input_context_t*,uint64_t*);

int bgav_input_read_float_32_be(bgav_input_context_t * ctx, float * ret);
int bgav_input_read_float_32_le(bgav_input_context_t * ctx, float * ret);

int bgav_input_read_double_64_be(bgav_input_context_t * ctx, double * ret);
int bgav_input_read_double_64_le(bgav_input_context_t * ctx, double * ret);

int bgav_input_get_data(bgav_input_context_t*, uint8_t*,int);

int bgav_input_get_8(bgav_input_context_t*,uint8_t*);
int bgav_input_get_16_le(bgav_input_context_t*,uint16_t*);
int bgav_input_get_24_le(bgav_input_context_t*,uint32_t*);
int bgav_input_get_32_le(bgav_input_context_t*,uint32_t*);
int bgav_input_get_64_le(bgav_input_context_t*,uint64_t*);

int bgav_input_get_16_be(bgav_input_context_t*,uint16_t*);
int bgav_input_get_24_be(bgav_input_context_t*,uint32_t*);
int bgav_input_get_32_be(bgav_input_context_t*,uint32_t*);
int bgav_input_get_64_be(bgav_input_context_t*,uint64_t*);

int bgav_input_get_float_32_be(bgav_input_context_t * ctx, float * ret);
int bgav_input_get_float_32_le(bgav_input_context_t * ctx, float * ret);

int bgav_input_get_double_64_be(bgav_input_context_t * ctx, double * ret);
int bgav_input_get_double_64_le(bgav_input_context_t * ctx, double * ret);

bgav_yml_node_t * bgav_input_get_yml(bgav_input_context_t * ctx);

#define bgav_input_read_fourcc(a,b) bgav_input_read_32_be(a,b)
#define bgav_input_get_fourcc(a,b)  bgav_input_get_32_be(a,b)

char * bgav_input_absolute_url(bgav_input_context_t * ctx, const char * rel_url);


/*
 *  Read one line from the input. Linebreak characters
 *  (\r, \n) are NOT stored in the buffer, return values is
 *  the number of characters in the line
 */

int bgav_input_read_line(bgav_input_context_t*,
                         gavl_buffer_t * ret);

void bgav_input_detect_charset(bgav_input_context_t*);
int bgav_utf8_validate(const uint8_t * str, const uint8_t * end);


int bgav_input_read_convert_line(bgav_input_context_t*,
                                 gavl_buffer_t * ret);


BGAV_PUBLIC int bgav_input_open(bgav_input_context_t *, const char * url);

BGAV_PUBLIC void bgav_input_close(bgav_input_context_t * ctx);

void bgav_input_destroy(bgav_input_context_t * ctx);

void bgav_input_skip(bgav_input_context_t *, int64_t);

/* Reopen  the input. Not all inputs can do this */
int bgav_input_reopen(bgav_input_context_t*);

BGAV_PUBLIC bgav_input_context_t * bgav_input_create(bgav_t * b, const bgav_options_t *);

/* For debugging purposes only: if you encounter data,
   hexdump them to stderr and skip them */

void bgav_input_skip_dump(bgav_input_context_t *, int);

void bgav_input_get_dump(bgav_input_context_t *, int);

void bgav_input_seek(bgav_input_context_t * ctx,
                     int64_t position,
                     int whence);




// void bgav_input_buffer(bgav_input_context_t * ctx);

void bgav_input_ensure_buffer_size(bgav_input_context_t * ctx, int len);

/* Input module to read from memory */

bgav_input_context_t * bgav_input_open_memory(uint8_t * data,
                                              uint32_t data_size);

bgav_input_context_t * bgav_input_open_sub(bgav_input_context_t * src,
                                           int64_t start,
                                           int64_t end);

/* Reopen a memory input with new data and minimal CPU overhead */

bgav_input_context_t * bgav_input_open_as_buffer(bgav_input_context_t * input);

void bgav_input_reopen_memory(bgav_input_context_t * ctx,
                              uint8_t * data,
                              uint32_t data_size);


/* Input module to read from a filedescriptor */

bgav_input_context_t *
bgav_input_open_fd(int fd, int64_t total_bytes, const char * mimetype);

/*
 *  Some demuxer will create a superindex. If this is the case,
 *  generic next_packet() and seek() functions will be used
 */

typedef struct 
  {
  int num_entries;
  int entries_alloc;

  int current_position;
  
  struct
    {
    int64_t offset;
    uint32_t size;
    int stream_id;
    int flags;
    int64_t pts;  /* Time is scaled with the timescale of the stream */
    int duration;  /* In timescale tics, can be 0 if unknown */
    } * entries;
  } bgav_superindex_t;

/* Create superindex, nothing will be allocated if size == 0 */

bgav_superindex_t * bgav_superindex_create(int size);
void bgav_superindex_destroy(bgav_superindex_t *);


void bgav_superindex_add_packet(bgav_superindex_t * idx,
                                bgav_stream_t * s,
                                int64_t offset,
                                uint32_t size,
                                int stream_id,
                                int64_t timestamp,
                                int keyframe, int duration);

void bgav_superindex_seek(bgav_superindex_t * idx,
                          bgav_stream_t * s,
                          int64_t * time, int scale);

BGAV_PUBLIC void bgav_superindex_dump(bgav_superindex_t * idx);

void bgav_superindex_set_durations(bgav_superindex_t * idx, bgav_stream_t * s);

void bgav_superindex_merge_fileindex(bgav_superindex_t * idx, bgav_stream_t * s);
void bgav_superindex_set_size(bgav_superindex_t * ret, int size);

void bgav_superindex_set_coding_types(bgav_superindex_t * idx,
                                      bgav_stream_t * s);

void bgav_superindex_set_stream_stats(bgav_superindex_t * idx,
                                      bgav_stream_t * s);

/* timecode.c */

typedef struct
  {
  int64_t pts;
  gavl_timecode_t timecode;
  } bgav_timecode_table_entry_t;

struct bgav_timecode_table_s
  {
  int num_entries;
  int entries_alloc;
  bgav_timecode_table_entry_t * entries;
  };

bgav_timecode_table_t *
bgav_timecode_table_create(int num);

void
bgav_timecode_table_append_entry(bgav_timecode_table_t *,
                                 int64_t pts,
                                 gavl_timecode_t timecode);

void
bgav_timecode_table_destroy(bgav_timecode_table_t *);

gavl_timecode_t
bgav_timecode_table_get_timecode(bgav_timecode_table_t * table,
                                 int64_t pts);

#if 0

/*
 * File index
 *
 * fileindex.c
 */

typedef struct
  {
  uint32_t flags;     /* Packet flags */
  /*
   * Seek positon:
   * 
   * For 1-layer muxed files, it's the
   * fseek() position, where the demuxer
   * can start to parse the packet header.
   *
   * For 2-layer muxed files, it's the
   * fseek() position of the lowest level
   * paket inside which the subpacket *starts*
   *
   * For superindex formats, it's the position
   * inside the superindex
   */
  
  uint64_t position; 

  /*
   *  Presentation time of the frame in
   *  format-based timescale (*not* in
   *  stream timescale)
   */

  int64_t pts;
  
  } bgav_file_index_entry_t;

/* Per stream structure */

struct bgav_file_index_s
  {
  /* Infos stored to speed up loading */
  uint32_t stream_id;
  uint32_t fourcc;
  
  uint32_t max_packet_size;
  
  /* Video infos stored by the format tracker */

  uint32_t interlace_mode;
  uint32_t framerate_mode;
  
  uint32_t num_entries;
  uint32_t entries_alloc;
  bgav_file_index_entry_t * entries;
  
  bgav_timecode_table_t tt;
  };

bgav_file_index_t * bgav_file_index_create();
void bgav_file_index_destroy(bgav_file_index_t *);
#endif

gavl_source_status_t bgav_demuxer_next_packet_fileindex(bgav_demuxer_context_t * ctx);
gavl_source_status_t bgav_demuxer_next_packet_interleaved(bgav_demuxer_context_t * ctx);

BGAV_PUBLIC void bgav_file_index_dump(bgav_t * b);


void
bgav_file_index_append_packet(bgav_file_index_t * idx,
                              int64_t position,
                              int64_t time,
                              int keyframe, gavl_timecode_t tc);

int bgav_file_index_read_header(const char * filename,
                                bgav_input_context_t * input,
                                int * num_tracks);

void bgav_file_index_write_header(const char * filename,
                                  FILE * output,
                                  int num_tracks);

int bgav_read_file_index(bgav_t*);

void bgav_write_file_index(bgav_t*);

int bgav_build_file_index(bgav_t * b, gavl_time_t * time_needed);

/* Demuxer class */

struct bgav_demuxer_s
  {
  int  (*probe)(bgav_input_context_t*);

  int  (*open)(bgav_demuxer_context_t * ctx);
  
  gavl_source_status_t (*next_packet)(bgav_demuxer_context_t*);

  /*
   *  Seeking sets either the position- or the time
   *  member of the stream
   */
  
  void (*seek)(bgav_demuxer_context_t*, int64_t time, int scale);
  void (*close)(bgav_demuxer_context_t*);

  /* Some demuxers support multiple tracks. This can fail e.g.
     if the input must be seekable but isn't */

  int (*select_track)(bgav_demuxer_context_t*, int track);

  /* After the input seeked to an arbitrary position, resync the stream
     and generate valid packet timestamps after that. This enables
     generic seeking support for MPEG-PS, MPEG-TS, OGG and ASF */
  
  int (*post_seek_resync)(bgav_demuxer_context_t*);
  
  /* Some demuxers have their own magic to build a file index */
  //  void (*build_index)(bgav_demuxer_context_t*);

  /* Demuxers might need this function to update the internal state
     after seeking with the fileindex */
  
  void (*resync)(bgav_demuxer_context_t*, bgav_stream_t * s);
  };

/* Demuxer flags */

#define BGAV_DEMUXER_CAN_SEEK             (1<<0)
#define BGAV_DEMUXER_PEEK_FORCES_READ     (1<<2) /* This is set if only subtitle streams are read */
#define BGAV_DEMUXER_SI_SEEKING           (1<<3) /* Demuxer is seeking */
#define BGAV_DEMUXER_SI_PRIVATE_FUNCS     (1<<4) /* We have a suprindex but use private seek/demux funcs */

#define BGAV_DEMUXER_BUILD_INDEX          (1<<8) /* We're just building
                                                    an index */

/* Discontionus demuxer: Read_packet *might* return GAVL_SOURCE_AGAIN */
#define BGAV_DEMUXER_DISCONT              (1<<10) /*
                                                   * True if we have just one active subtitle stream with attached subreader
                                                   */

/* Use generic code to get the duration */
#define BGAV_DEMUXER_GET_DURATION          (1<<11)

#define BGAV_DEMUXER_HAS_CLOCK_TIME        (1<<12)

/* Set if packets allow to build a seek index */
#define BGAV_DEMUXER_BUILD_SEEK_INDEX       (1<<13)

/* Set if a seek index is already there */
#define BGAV_DEMUXER_HAS_SEEK_INDEX         (1<<14)


#define INDEX_MODE_NONE   0 /* Default: No sample accuracy */
/* Packets have precise timestamps and durations and are adjacent in the file */
#define INDEX_MODE_SIMPLE 1
/* For PCM soundfiles: Sample accuracy is already there */
#define INDEX_MODE_PCM    4
/* File has a global index and codecs, which allow sample accuracy */
#define INDEX_MODE_SI_SA  5
/* File has a global index but codecs, which need complete parsing */
#define INDEX_MODE_SI_PARSE  6

/* Stream must be completely parsed, streams can have
   INDEX_MODE_SIMPLE, INDEX_MODE_MPEG or INDEX_MODE_PTS */
#define INDEX_MODE_MIXED  7

// #define INDEX_MODE_CUSTOM 4 /* Demuxer builds index */


#define DEMUX_MODE_STREAM 0
#define DEMUX_MODE_SI_I   1 /* Interleaved with superindex */
#define DEMUX_MODE_SI_NI  2 /* Non-interleaved with superindex */

struct bgav_demuxer_context_s
  {
  const bgav_options_t * opt;
  void * priv;
  const bgav_demuxer_t * demuxer;
  bgav_input_context_t * input;
  
  bgav_track_table_t * tt;

  int packet_size; /* Optional, if it's fixed */
  
  int index_mode;
  int demux_mode;
  uint32_t flags;
  
  /*
   *  The stream, which requested the next_packet function.
   *  Can come handy sometimes
   */
  bgav_stream_t * request_stream;

  /*
   *  If demuxer creates a superindex, generic get_packet() and
   *  seek() functions will be used
   */
  bgav_superindex_t * si;
  
  bgav_t * b;
  };

/* demuxer.c */

/*
 *  Create a demuxer.
 */

bgav_demuxer_context_t *
bgav_demuxer_create(bgav_t * b,
                    const bgav_demuxer_t * demuxer,
                    bgav_input_context_t * input);

const bgav_demuxer_t * bgav_demuxer_probe(bgav_input_context_t * input);

void bgav_demuxer_create_buffers(bgav_demuxer_context_t * demuxer);
void bgav_demuxer_destroy(bgav_demuxer_context_t * demuxer);

/*
 *  Get the duration of the current track for demuxers which, have the
 *  post_seek_resync method
 */

int bgav_demuxer_get_duration(bgav_demuxer_context_t * ctx);

void bgav_demuxer_set_clock_time(bgav_demuxer_context_t * ctx,
                                 int64_t pts, int scale, gavl_time_t clock_time);

/* Generic get/peek functions */

void
bgav_demuxer_seek(bgav_demuxer_context_t * demuxer,
                  int64_t time, int scale);

gavl_source_status_t 
bgav_demuxer_next_packet(bgav_demuxer_context_t * demuxer);

/*
 *  Start a demuxer. Some demuxers (most notably quicktime)
 *  can contain nothing but urls for the real streams.
 *  In this case, redir (if not NULL) will contain the
 *  redirector context
 */

int bgav_demuxer_start(bgav_demuxer_context_t * ctx);
void bgav_demuxer_stop(bgav_demuxer_context_t * ctx);

#define BGAV_DEMUXER_STREAM_ID_RAW 1
gavl_source_status_t bgav_demuxer_next_packet_raw(bgav_demuxer_context_t * ctx);

// bgav_packet_t *
// bgav_demuxer_get_packet_write(bgav_demuxer_context_t * demuxer, int stream);

// bgav_stream_t * bgav_track_find_stream(bgav_track_t * ctx, int stream_id);

/* Redirector */

struct bgav_redirector_s
  {
  const char * name;
  int (*probe)(bgav_input_context_t*);
  bgav_track_table_t * (*parse)(bgav_input_context_t*);
  };


const bgav_redirector_t * bgav_redirector_probe(bgav_input_context_t * input);

int bgav_is_redirector(bgav_t * bgav);


/* Actual decoder */

#define BGAV_FLAG_EOF              (1<<0)
#define BGAV_FLAG_IS_RUNNING       (1<<1)
#define BGAV_FLAG_STATE_SENT       (1<<2)
#define BGAV_FLAG_PAUSED           (1<<3)

struct bgav_s
  {
  char * location;
  
  /* Configuration parameters */

  bgav_options_t opt;
  
  bgav_input_context_t * input;
  bgav_demuxer_context_t * demuxer;
  bgav_track_table_t * tt;
  
  
  bgav_metadata_t metadata;

  /* Set by the seek function */
  
  int flags;

  gavl_dictionary_t state;
  };

/* bgav.c */

void bgav_stop(bgav_t * b);
int bgav_init(bgav_t * b);

void bgav_metadata_changed(bgav_t * b,
                           const gavl_dictionary_t * new_metadata);
void bgav_signal_restart(bgav_t * b, int reason);

void bgav_seek_window_changed(bgav_t * b,
                              gavl_time_t start, gavl_time_t end);

// void bgav_abs_time_offset_changed(bgav_t * b, gavl_time_t off);
//void bgav_start_time_absolute_changed(bgav_t * b, gavl_time_t off);

void bgav_send_state(bgav_t * b);

/* Bytestream utilities */

/* ptr -> integer */

#if 0

#define BGAV_PTR_2_16LE(p) GAVL_PTR_2_16LE(p) 
#define BGAV_PTR_2_24LE(p) GAVL_PTR_2_24LE(p)
#define BGAV_PTR_2_32LE(p) GAVL_PTR_2_32LE(p)
#define BGAV_PTR_2_64LE(p) GAVL_PTR_2_64LE(p)
#define BGAV_PTR_2_16BE(p) GAVL_PTR_2_16BE(p) 
#define BGAV_PTR_2_32BE(p) GAVL_PTR_2_32BE(p) 
#define BGAV_PTR_2_24BE(p) GAVL_PTR_2_24BE(p)
#define BGAV_PTR_2_64BE(p) GAVL_PTR_2_64BE(p)

/* integer -> ptr */

#define BGAV_16LE_2_PTR(i, p) GAVL_16LE_2_PTR(i, p) 
#define BGAV_24LE_2_PTR(i, p) GAVL_24LE_2_PTR(i, p) 
#define BGAV_32LE_2_PTR(i, p) GAVL_32LE_2_PTR(i, p)
#define BGAV_64LE_2_PTR(i, p) GAVL_64LE_2_PTR(i, p) 
#define BGAV_16BE_2_PTR(i, p) GAVL_16BE_2_PTR(i, p) 
#define BGAV_32BE_2_PTR(i, p) GAVL_32BE_2_PTR(i, p) 
#define BGAV_24BE_2_PTR(i, p) GAVL_24BE_2_PTR(i, p) 
#define BGAV_64BE_2_PTR(i, p) GAVL_64BE_2_PTR(i, p) 

#endif

#define BGAV_PTR_2_FOURCC(p) GAVL_PTR_2_32BE(p)

#define BGAV_WAVID_2_FOURCC(id) BGAV_MK_FOURCC(0x00, 0x00, (id>>8), (id&0xff))

#define BGAV_FOURCC_2_WAVID(f) (f & 0xffff)

/* utils.c */

void bgav_dump_fourcc(uint32_t fourcc);
int bgav_check_fourcc(uint32_t fourcc, const uint32_t * fourccs);

char * bgav_sprintf(const char * format,...)   __attribute__ ((format (printf, 1, 2)));

// char * bgav_strncat(char * old, const char * start, const char * end);

void bgav_dprintf(const char * format, ...) __attribute__ ((format (printf, 1, 2)));
void bgav_diprintf(int indent, const char * format, ...)
  __attribute__ ((format (printf, 2, 3)));

int bgav_url_split(const char * url,
                   char ** protocol,
                   char ** user,
                   char ** password,
                   char ** hostname,
                   int * port,
                   char ** path);

// char ** bgav_stringbreak(const char * str, char sep);
// void bgav_stringbreak_free(char ** str);

int bgav_slurp_file(const char * location,
                    bgav_packet_t * p,
                    const bgav_options_t * opt);

char * bgav_search_file_write(const bgav_options_t * opt,
                              const char * directory, const char * file);

char * bgav_search_file_read(const bgav_options_t * opt,
                             const char * directory, const char * file);

int bgav_match_regexp(const char * str, const char * regexp);

// char * bgav_escape_string(char * old_string, const char * escape_chars);

uint32_t bgav_compression_id_2_fourcc(gavl_codec_id_t id);



/* Check if file exist and is readable */

int bgav_check_file_read(const char * filename);



/* Read a single line from a filedescriptor */

int bgav_read_line_fd(const bgav_options_t * opt, int fd,
                      char ** ret, int * ret_alloc, int milliseconds);

int bgav_read_data_fd(const bgav_options_t * opt, int fd,
                      uint8_t * ret, int size, int milliseconds);

const char * bgav_coding_type_to_string(int type);

uint32_t * bgav_get_vobsub_palette(const char * str);


/* tcp.c */

int bgav_tcp_connect(const bgav_options_t * opt,
                     const char * host, int port);

int bgav_tcp_send(const bgav_options_t * opt,
                  int fd, uint8_t * data, int len);

struct addrinfo * bgav_hostbyname(const bgav_options_t * opt,
                                  const char * hostname,
                                  int port, int socktype, int flags);

/* udp.c */
int bgav_udp_open(const bgav_options_t * opt, int port);
int bgav_udp_read(int fd, uint8_t * data, int len);

int bgav_udp_write(const bgav_options_t * opt,
                   int fd, uint8_t * data, int len,
                   struct addrinfo * addr);

/* Charset utilities (charset.c) */

#define BGAV_UTF8 "UTF-8" // iconf string for UTF-8

bgav_charset_converter_t *
bgav_charset_converter_create(const char * in_charset,
                              const char * out_charset);

void bgav_charset_converter_destroy(bgav_charset_converter_t *);

char * bgav_convert_string(bgav_charset_converter_t *,
                           const char * in_string, int in_len,
                           uint32_t * out_len);

int bgav_convert_string_realloc(bgav_charset_converter_t * cnv,
                                const char * str, int len,
                                gavl_buffer_t * out);

/* subtitleconverter.c */
/* Subtitle converter (converts character sets and removes \r */


bgav_subtitle_converter_t * bgav_subtitle_converter_create(const char * charset);
void bgav_subtitle_converter_destroy(bgav_subtitle_converter_t* cnv);
gavl_packet_source_t * bgav_subtitle_converter_connect(bgav_subtitle_converter_t * cnv, gavl_packet_source_t * src);


/* audio.c */

int bgav_set_audio_compression_info(bgav_stream_t * s);
void bgav_audio_dump(bgav_stream_t * s);
int bgav_audio_init(bgav_stream_t * s);

int bgav_audio_start(bgav_stream_t * s);
void bgav_audio_stop(bgav_stream_t * s);

void bgav_stream_set_sbr(bgav_stream_t * s);

/* Resynchronize the stream to the next point
 * where decoding can start again.
 * After calling this, the out_time *must* be valid
 * Called AFTER seeking
 */

void bgav_audio_resync(bgav_stream_t * stream);

/* Skip to a point in the stream, return 0 on EOF */

int bgav_audio_skipto(bgav_stream_t * stream, int64_t * t, int scale);

/* video.c */

extern const uint32_t bgav_dv_fourccs[];
extern const uint32_t bgav_png_fourccs[];

void bgav_set_video_frame_from_packet(const bgav_packet_t * p,
                                      gavl_video_frame_t * f);

int bgav_set_video_compression_info(bgav_stream_t * s);
void bgav_video_dump(bgav_stream_t * s);
int bgav_video_init(bgav_stream_t * s);

int bgav_video_start(bgav_stream_t * s);
void bgav_video_stop(bgav_stream_t * s);

/* Resynchronize the stream to the next point
 * where decoding can start again.
 * After calling this, the out_time *must* be valid
 * Called AFTER seeking
 */

void bgav_video_resync(bgav_stream_t * s);
// void bgav_video_clear(bgav_stream_t * s);

int bgav_video_skipto(bgav_stream_t * stream, int64_t * t, int scale);

void bgav_video_set_still(bgav_stream_t * stream);


/* subtitle.c */

void bgav_subtitle_seek(bgav_demuxer_context_t * ctx, int64_t time, int scale);


void bgav_subtitle_dump(bgav_stream_t * s);

int bgav_text_start(bgav_stream_t * s);
int bgav_text_init(bgav_stream_t * s);

int bgav_overlay_start(bgav_stream_t * s);
int bgav_overlay_init(bgav_stream_t * s);

void bgav_subtitle_stop(bgav_stream_t * s);

void bgav_subtitle_resync(bgav_stream_t * stream);

int bgav_subtitle_skipto(bgav_stream_t * stream, int64_t * t, int scale);

int bgav_set_overlay_compression_info(bgav_stream_t * s);


extern const uint32_t bgav_dvdsub_fourccs[];

/* codecs.c */

void bgav_codecs_init(bgav_options_t * opt);

bgav_audio_decoder_t * bgav_find_audio_decoder(uint32_t fourcc);
bgav_video_decoder_t * bgav_find_video_decoder(uint32_t fourcc);

void bgav_audio_decoder_register(bgav_audio_decoder_t * dec);
void bgav_video_decoder_register(bgav_video_decoder_t * dec);

/* base64.c */

int bgav_base64encode(const unsigned char *input, int input_length,
                      unsigned char *output, int output_length);

int bgav_base64decode(const char *input,
                      int input_length,
                      unsigned char *output, int output_length);

/* device.c */

/*
 *  Append device info to an existing array and return the new array.
 *  arr can be NULL
 */

bgav_device_info_t * bgav_device_info_append(bgav_device_info_t * arr,
                                             const char * device,
                                             const char * name);

/* For debugging only */

void bgav_device_info_dump(bgav_device_info_t * arr);

/* languages.c */
#if 0
const char * gavl_language_get_iso639_2_b_from_code(const char * code);

GAVL_PUBLIC
const char * gavl_language_get_iso639_2_b_from_label(const char * label);

GAVL_PUBLIC
const char * gavl_language_get_label_from_code(const char * label);
#endif

#define bgav_lang_from_name(name) gavl_language_get_iso639_2_b_from_label(name)

#define bgav_lang_from_twocc(twocc) gavl_language_get_iso639_2_b_from_code(twocc)

#define bgav_lang_name(lang) gavl_language_get_label_from_code(lang)

void bgav_correct_language(char * lang);

#if 0
/* subreader.c */

struct bgav_subtitle_reader_context_s
  {
  bgav_input_context_t * input;
  const bgav_subtitle_reader_t * reader;
  char * charset;

  int stream;
  
  bgav_stream_t * s;
  
  char * info; /* Derived from filename difference */
  char * filename; /* Name of the subtitle file */
  
  int64_t time_offset;

  /* Timestamps are scaled with out_pts = pts * scale_num / scale_den */
  int scale_num;
  int scale_den;

  gavl_buffer_t line_buf;
  
  /* Some formats have a header... */
  int64_t data_start;
  
  /* bgav_subtitle_reader_open returns a chained list */
  bgav_subtitle_reader_context_t * next;

  /* Private data */
  void * priv;
  
  gavl_packet_source_t * psrc;
  
  int64_t seek_time;
  };

struct bgav_subtitle_reader_s
  {
  gavl_stream_type_t type;
  char * extensions;
  char * name;

  /* Probe for the subtitle format, return the number of streams */
  int (*probe)(char * line, bgav_input_context_t * ctx);

  int (*setup_stream)(bgav_stream_t*);
  
  int (*init)(bgav_stream_t*);
  void (*close)(bgav_stream_t*);
  void (*seek)(bgav_stream_t*,int64_t time, int scale);
  
  gavl_source_status_t (*read_packet)(bgav_stream_t*, bgav_packet_t * p);
  };

bgav_subtitle_reader_context_t *
bgav_subtitle_reader_open(bgav_input_context_t * input_ctx);

int bgav_subtitle_reader_start(bgav_stream_t *);

void bgav_subtitle_reader_stop(bgav_stream_t *);
void bgav_subtitle_reader_destroy(bgav_stream_t *);

void bgav_subtitle_reader_seek(bgav_stream_t *,
                               int64_t time, int scale);

/* Packet source functions */

gavl_source_status_t
bgav_subtitle_reader_read_packet(void * subreader,
                                 bgav_packet_t ** p);

gavl_source_status_t
bgav_subtitle_reader_peek_packet(void * subreader,
                                 bgav_packet_t ** p, int force);
#endif

/* log.c */

#if 0
#define GAVL_LOG_INFO    GAVL_LOG_INFO
#define GAVL_LOG_WARNING GAVL_LOG_WARNING
#define GAVL_LOG_ERROR   GAVL_LOG_ERROR
#define GAVL_LOG_DEBUG   GAVL_LOG_DEBUG

#define bgav_log(opt, level, domain, ...) gavl_log(level, domain, __VA_ARGS__)
#endif

/* bytebuffer.c */

#if 0
typedef struct
  {
  uint8_t * buffer;
  int size;
  int alloc;
  } bgav_bytebuffer_t;
#endif

void bgav_bytebuffer_append_packet(gavl_buffer_t * b, bgav_packet_t * p, int padding);

// void bgav_bytebuffer_append_data(gavl_buffer_t * b, uint8_t * data, int len, int padding);
int bgav_bytebuffer_append_read(gavl_buffer_t * b, bgav_input_context_t * input,
                                int len, int padding);


// int bgav_bytebuffer_append_read(bgav_bytebuffer_t * b, bgav_input_context_t * input,
//                                int len, int padding);

// void bgav_bytebuffer_remove(bgav_bytebuffer_t * b, int bytes);
// void bgav_bytebuffer_free(bgav_bytebuffer_t * b);
// void bgav_bytebuffer_flush(bgav_bytebuffer_t * b);

/* sampleseek.c */
int bgav_set_sample_accurate(bgav_t * b);

void bgav_check_sample_accurate(bgav_t * b);

int64_t bgav_video_stream_keyframe_after(bgav_stream_t * s, int64_t time);
int64_t bgav_video_stream_keyframe_before(bgav_stream_t * s, int64_t time);

/* Translation specific stuff */

void bgav_translation_init();

/* For dynamic strings */
#define TRD(s) dgettext(PACKAGE, (s))

/* For static strings */
#define TRS(s) (s)


#if 0
/* keyframetable.c */

/*
 *  A keyframe table is always associated with 
 *  either a file index or a superindex.
 */

struct bgav_keyframe_table_s
  {
  int num_entries;
  struct
    {
    int pos;
    int64_t pts;
    } * entries;
  };

bgav_keyframe_table_t * bgav_keyframe_table_create_fi(bgav_file_index_t * fi);
bgav_keyframe_table_t * bgav_keyframe_table_create_si(bgav_superindex_t * si,
                                                      bgav_stream_t * s);

void bgav_keyframe_table_destroy(bgav_keyframe_table_t *);
/* Returns the index position */
int bgav_keyframe_table_seek(bgav_keyframe_table_t *,
                             int64_t  seek_pts,
                             int64_t * kf_pts);

#endif
/* formattracker.c */


#if 0
bgav_video_format_tracker_t *
bgav_video_format_tracker_create(bgav_stream_t * s);

void bgav_video_format_tracker_destroy(bgav_video_format_tracker_t *);
#endif



/* parse_dca.c */
#ifdef HAVE_DCA
void bgav_dca_flags_2_channel_setup(int flags, gavl_audio_format_t * format);
#endif

/* videoparser.c */
int bgav_video_is_divx4(uint32_t fourcc);

/* Global locking around avcodec_[open|close]()
   Defined in video_ffmpeg.c, used from audio_ffmpeg.c as well
*/

void bgav_ffmpeg_lock();
void bgav_ffmpeg_unlock();


#if __GNUC__ >= 3

#define BGAV_UNLIKELY(exp) __builtin_expect((exp),0)
#define BGAV_LIKELY(exp)   __builtin_expect((exp),1)

#else

#define BGAV_UNLIKELY(exp) exp
#define BGAV_LIKELY(exp)   exp

#endif

#endif // BGAV_AVDEDEC_PRIVATE_H_INCLUDED

