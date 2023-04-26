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

// #define DUMP_SUPERINDEX
#include <avdec_private.h>
#include <parser.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_DOMAIN "demuxer"

extern const bgav_demuxer_t bgav_demuxer_asf;
extern const bgav_demuxer_t bgav_demuxer_avi;
extern const bgav_demuxer_t bgav_demuxer_rmff;
extern const bgav_demuxer_t bgav_demuxer_quicktime;
extern const bgav_demuxer_t bgav_demuxer_vivo;
extern const bgav_demuxer_t bgav_demuxer_fli;
extern const bgav_demuxer_t bgav_demuxer_flv;

extern const bgav_demuxer_t bgav_demuxer_ape;
extern const bgav_demuxer_t bgav_demuxer_wavpack;
extern const bgav_demuxer_t bgav_demuxer_tta;
extern const bgav_demuxer_t bgav_demuxer_voc;
extern const bgav_demuxer_t bgav_demuxer_wav;
extern const bgav_demuxer_t bgav_demuxer_au;
extern const bgav_demuxer_t bgav_demuxer_ircam;
extern const bgav_demuxer_t bgav_demuxer_sphere;
extern const bgav_demuxer_t bgav_demuxer_gsm;
extern const bgav_demuxer_t bgav_demuxer_8svx;
extern const bgav_demuxer_t bgav_demuxer_aiff;
extern const bgav_demuxer_t bgav_demuxer_ra;
extern const bgav_demuxer_t bgav_demuxer_mpegaudio;
extern const bgav_demuxer_t bgav_demuxer_mpegvideo;
extern const bgav_demuxer_t bgav_demuxer_mpegps;
//extern const bgav_demuxer_t bgav_demuxer_mpegts;
extern const bgav_demuxer_t bgav_demuxer_mpegts2;
extern const bgav_demuxer_t bgav_demuxer_mxf;
extern const bgav_demuxer_t bgav_demuxer_flac;
extern const bgav_demuxer_t bgav_demuxer_adts;
extern const bgav_demuxer_t bgav_demuxer_adif;
extern const bgav_demuxer_t bgav_demuxer_nsv;
extern const bgav_demuxer_t bgav_demuxer_4xm;
extern const bgav_demuxer_t bgav_demuxer_dsicin;
extern const bgav_demuxer_t bgav_demuxer_smaf;
extern const bgav_demuxer_t bgav_demuxer_psxstr;
extern const bgav_demuxer_t bgav_demuxer_tiertex;
extern const bgav_demuxer_t bgav_demuxer_smacker;
extern const bgav_demuxer_t bgav_demuxer_roq;
extern const bgav_demuxer_t bgav_demuxer_shorten;
extern const bgav_demuxer_t bgav_demuxer_daud;
extern const bgav_demuxer_t bgav_demuxer_nuv;
extern const bgav_demuxer_t bgav_demuxer_sol;
extern const bgav_demuxer_t bgav_demuxer_gif;
extern const bgav_demuxer_t bgav_demuxer_smjpeg;
extern const bgav_demuxer_t bgav_demuxer_vqa;
extern const bgav_demuxer_t bgav_demuxer_vmd;
extern const bgav_demuxer_t bgav_demuxer_avs;
extern const bgav_demuxer_t bgav_demuxer_wve;
extern const bgav_demuxer_t bgav_demuxer_mtv;
extern const bgav_demuxer_t bgav_demuxer_gxf;
extern const bgav_demuxer_t bgav_demuxer_dxa;
extern const bgav_demuxer_t bgav_demuxer_thp;
extern const bgav_demuxer_t bgav_demuxer_r3d;
extern const bgav_demuxer_t bgav_demuxer_matroska;
extern const bgav_demuxer_t bgav_demuxer_y4m;
extern const bgav_demuxer_t bgav_demuxer_rawaudio;
extern const bgav_demuxer_t bgav_demuxer_image;
extern const bgav_demuxer_t bgav_demuxer_cue;
extern const bgav_demuxer_t bgav_demuxer_vtt;
extern const bgav_demuxer_t bgav_demuxer_srt;

#ifdef HAVE_VORBIS
extern const bgav_demuxer_t bgav_demuxer_ogg2;
#endif

extern const bgav_demuxer_t bgav_demuxer_dv;

#ifdef HAVE_LIBA52
extern const bgav_demuxer_t bgav_demuxer_a52;
#endif

#ifdef HAVE_MUSEPACK
extern const bgav_demuxer_t bgav_demuxer_mpc;
#endif


#ifdef HAVE_LIBAVFORMAT
extern const bgav_demuxer_t bgav_demuxer_ffmpeg;
#endif

extern const bgav_demuxer_t bgav_demuxer_p2xml;

typedef struct
  {
  const bgav_demuxer_t * demuxer;
  char * format_name;
  } demuxer_t;

static const demuxer_t demuxers[] =
  {
    { &bgav_demuxer_vtt,       "WEBVTT" },
    { &bgav_demuxer_asf,       "ASF/WMV/WMA" },
    { &bgav_demuxer_adif,      "ADIF" },
    { &bgav_demuxer_avi,       "AVI" },
    { &bgav_demuxer_rmff,      "Real Media" },
    { &bgav_demuxer_ra,        "Real Audio" },
    { &bgav_demuxer_quicktime, "Quicktime/mp4/m4a" },
    { &bgav_demuxer_ape,       "APE"               },
    { &bgav_demuxer_wav,       "WAV" },
    { &bgav_demuxer_au,        "Sun AU" },
    { &bgav_demuxer_aiff,      "AIFF(C)" },
    { &bgav_demuxer_flac,      "FLAC" },
    { &bgav_demuxer_vivo,      "Vivo" },
    { &bgav_demuxer_mpegvideo, "Elementary video" },
    { &bgav_demuxer_fli,       "FLI/FLC Animation" },
    { &bgav_demuxer_flv,       "Flash video (FLV)" },
    { &bgav_demuxer_nsv,       "NullSoft Video" },
    { &bgav_demuxer_wavpack,   "Wavpack" },
    { &bgav_demuxer_tta,       "True Audio" },
    { &bgav_demuxer_voc,       "Creative voice" },
    { &bgav_demuxer_4xm,       "4xm" },
    { &bgav_demuxer_dsicin,    "Delphine Software CIN" },
    { &bgav_demuxer_8svx,      "Amiga IFF" },
    { &bgav_demuxer_smaf,      "SMAF Ringtone" },
    { &bgav_demuxer_psxstr,    "Sony Playstation (PSX) STR" },
    { &bgav_demuxer_tiertex,   "Tiertex SEQ" },
    { &bgav_demuxer_smacker,   "Smacker" },
    { &bgav_demuxer_roq,       "ID Roq" },
    { &bgav_demuxer_shorten,   "Shorten" },
    { &bgav_demuxer_nuv,       "NuppelVideo/MythTV" },
    { &bgav_demuxer_sol,       "Sierra SOL" },
    { &bgav_demuxer_gif,       "GIF" },
    { &bgav_demuxer_smjpeg,    "SMJPEG" },
    { &bgav_demuxer_vqa,       "Westwood VQA" },
    { &bgav_demuxer_avs,       "AVS" },
    { &bgav_demuxer_wve,       "Electronicarts WVE" },
    { &bgav_demuxer_mtv,       "MTV" },
    { &bgav_demuxer_gxf,       "GXF" },
    { &bgav_demuxer_dxa,       "DXA" },
    { &bgav_demuxer_thp,       "THP" },
    { &bgav_demuxer_r3d,       "R3D" },
    { &bgav_demuxer_cue,       "CUE" },
    { &bgav_demuxer_matroska,  "Matroska" },
#ifdef HAVE_VORBIS
    { &bgav_demuxer_ogg2, "Ogg Bitstream" },
#endif
#ifdef HAVE_LIBA52
    { &bgav_demuxer_a52, "A52 Bitstream" },
#endif
#ifdef HAVE_MUSEPACK
    { &bgav_demuxer_mpc, "Musepack" },
#endif
    { &bgav_demuxer_y4m, "yuv4mpeg" },
    { &bgav_demuxer_dv, "DV" },
    { &bgav_demuxer_mxf, "MXF" },
    { &bgav_demuxer_sphere, "nist Sphere"},
    { &bgav_demuxer_ircam, "IRCAM" },
    { &bgav_demuxer_gsm, "raw gsm" },
    { &bgav_demuxer_daud, "D-Cinema audio" },
    { &bgav_demuxer_vmd,  "Sierra VMD" },
    { &bgav_demuxer_image, "Image" },
    { &bgav_demuxer_p2xml, "P2 xml" },
    { &bgav_demuxer_rawaudio, "Raw audio" },
  };

static const demuxer_t sync_demuxers[] =
  {
    { &bgav_demuxer_srt,       "SubRip subtitle" },
    { &bgav_demuxer_mpegts2,   "MPEG-2 transport stream" },
    //    { &bgav_demuxer_mpegts,    "MPEG-2 transport stream" },
    { &bgav_demuxer_mpegaudio, "MPEG Audio" },
    { &bgav_demuxer_adts,      "ADTS" },
    { &bgav_demuxer_mpegps,    "MPEG System" },
  };

static struct
  {
  const bgav_demuxer_t * demuxer;
  char * mimetype;
  }
mimetypes[] =
  {
    { &bgav_demuxer_mpegaudio, "audio/mpeg" },
    { &bgav_demuxer_adts,      "audio/aacp" },
    { &bgav_demuxer_adts,      "audio/aac" },
    { &bgav_demuxer_vtt,       "text/vtt" },
    { &bgav_demuxer_mpegps,    "video/MP1S" },
    { &bgav_demuxer_mpegps,    "video/MP2P" },
  };

static const int num_demuxers = sizeof(demuxers)/sizeof(demuxers[0]);
static const int num_sync_demuxers = sizeof(sync_demuxers)/sizeof(sync_demuxers[0]);

static const int num_mimetypes = sizeof(mimetypes)/sizeof(mimetypes[0]);

// int bgav_demuxer_next_packet(bgav_demuxer_context_t * demuxer);


#define SYNC_BYTES (32*1024)



const bgav_demuxer_t * bgav_demuxer_probe(bgav_input_context_t * input)
  {
  int i;
  int bytes_skipped;
  uint8_t skip;
  const char * mimetype = NULL;
#ifdef HAVE_LIBAVFORMAT
  if(input->opt->prefer_ffmpeg_demuxers)
    {
    if(bgav_demuxer_ffmpeg.probe(input))
      return &bgav_demuxer_ffmpeg;
    }
#endif

  //  fprintf(stderr, "bgav_demuxer_probe\n");
  //  gavl_dictionary_dump(&input->m, 2);
  
  if(gavl_metadata_get_src(&input->m, GAVL_META_SRC, 0, &mimetype, NULL) && mimetype)
    {
    for(i = 0; i < num_mimetypes; i++)
      {
      if(!strcmp(mimetypes[i].mimetype, mimetype))
        {
        gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN,
                 "Got demuxer for mimetype %s", mimetype);
        return mimetypes[i].demuxer;
        }
      }
    }

  for(i = 0; i < num_demuxers; i++)
    {
    if(demuxers[i].demuxer->probe(input))
      {
      gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN,
               "Detected %s format", demuxers[i].format_name);
      return demuxers[i].demuxer;
      }
    }
  
  for(i = 0; i < num_sync_demuxers; i++)
    {
    if(sync_demuxers[i].demuxer->probe(input))
      {
      gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN,
               "Detected %s format",
               sync_demuxers[i].format_name);
      return sync_demuxers[i].demuxer;
      }
    }
  
  /* Try again with skipping initial bytes */

  bytes_skipped = 0;

  while(bytes_skipped < SYNC_BYTES)
    {
    bytes_skipped++;
    if(!bgav_input_read_data(input, &skip, 1))
      return NULL;

    for(i = 0; i < num_sync_demuxers; i++)
      {
      if(sync_demuxers[i].demuxer->probe(input))
        {
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
                 "Detected %s format after skipping %d bytes (position: %"PRId64")",
                 sync_demuxers[i].format_name, bytes_skipped, input->position);
        return sync_demuxers[i].demuxer;
        }
      }
    
    }
  
#ifdef HAVE_LIBAVFORMAT
  if(!input->opt->prefer_ffmpeg_demuxers && (input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    {
    bgav_input_seek(input, 0, SEEK_SET);
    if(bgav_demuxer_ffmpeg.probe(input))
      return &bgav_demuxer_ffmpeg;
    }
#endif
  
  return NULL;
  }

bgav_demuxer_context_t *
bgav_demuxer_create(bgav_t * b, const bgav_demuxer_t * demuxer, bgav_input_context_t * input)
  {
  bgav_demuxer_context_t * ret;
  
  ret = calloc(1, sizeof(*ret));
  ret->opt = &b->opt;
  ret->b = b;
  ret->demuxer = demuxer;

  if(input)
    ret->input = input;
  else
    ret->input = b->input;
  
  return ret;
  }

#define FREE(p) if(p){free(p);p=NULL;}

void bgav_demuxer_destroy(bgav_demuxer_context_t * ctx)
  {
  if(ctx->demuxer->close)
    ctx->demuxer->close(ctx);
  if(ctx->tt)
    bgav_track_table_unref(ctx->tt);

  if(ctx->si)
    bgav_superindex_destroy(ctx->si);
  free(ctx);
  }

/* Check and kick out streams, for which a header exists but no
   packets */

static void init_superindex(bgav_demuxer_context_t * ctx)
  {
  int i;

  i = 0;

  while(i < ctx->tt->cur->num_streams)
    {
    if(ctx->tt->cur->streams[i].last_index_position < 0)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "Removing stream %d (no packets found)", i+1);
      bgav_track_remove_stream(ctx->tt->cur, i);
      }
    else
      {
      bgav_superindex_set_durations(ctx->si, &ctx->tt->cur->streams[i]);
      bgav_superindex_set_stream_stats(ctx->si, &ctx->tt->cur->streams[i]);
      i++;
      }
    }
#if 0  
  while(i < ctx->tt->cur->num_audio_streams)
    {
    if(ctx->tt->cur->audio_streams[i].last_index_position < 0)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "Removing audio stream %d (no packets found)", i+1);
      bgav_track_remove_audio_stream(ctx->tt->cur, i);
      }
    else
      {
      bgav_superindex_set_durations(ctx->si, &ctx->tt->cur->audio_streams[i]);
      bgav_superindex_set_stream_stats(ctx->si, &ctx->tt->cur->audio_streams[i]);
      i++;
      }
    }

  i = 0;
  while(i < ctx->tt->cur->num_video_streams)
    {
    if(ctx->tt->cur->video_streams[i].last_index_position < 0)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "Removing video stream %d (no packets found)", i+1);
      bgav_track_remove_video_stream(ctx->tt->cur, i);
      }
    else
      {
      bgav_superindex_set_durations(ctx->si, &ctx->tt->cur->video_streams[i]);
      bgav_superindex_set_coding_types(ctx->si, &ctx->tt->cur->video_streams[i]);
      bgav_superindex_set_stream_stats(ctx->si, &ctx->tt->cur->video_streams[i]);
      i++;
      }
    }

  i = 0;
  while(i < ctx->tt->cur->num_text_streams)
    {
    if(ctx->tt->cur->text_streams[i].last_index_position < 0)
      bgav_track_remove_text_stream(ctx->tt->cur, i);
    else
      {
      bgav_superindex_set_durations(ctx->si, &ctx->tt->cur->text_streams[i]);
      bgav_superindex_set_stream_stats(ctx->si, &ctx->tt->cur->text_streams[i]);
      i++;
      }
    }

  i = 0;
  while(i < ctx->tt->cur->num_overlay_streams)
    {
    if(ctx->tt->cur->overlay_streams[i].last_index_position < 0)
      bgav_track_remove_overlay_stream(ctx->tt->cur, i);
    else
      {
      bgav_superindex_set_durations(ctx->si, &ctx->tt->cur->overlay_streams[i]);
      bgav_superindex_set_stream_stats(ctx->si, &ctx->tt->cur->overlay_streams[i]);
      i++;
      }
    }
#endif
  }

static void check_interleave(bgav_demuxer_context_t * ctx)
  {
  int i;

  
  /* If sample accurate decoding was requested, use non-interleaved mode */
  if((ctx->opt->sample_accurate == 1) || (ctx->flags & BGAV_DEMUXER_BUILD_INDEX))
    {
    ctx->demux_mode = DEMUX_MODE_SI_NI;
    }
  /* One stream always means non-interleaved */
  else if(ctx->tt->cur->num_streams <= 1)
    {
    ctx->demux_mode = DEMUX_MODE_SI_NI;
    }
  else
    {
    ctx->demux_mode = DEMUX_MODE_SI_I;
    
    if((ctx->tt->cur->streams[0].last_index_position < ctx->tt->cur->streams[1].first_index_position) ||
       (ctx->tt->cur->streams[1].last_index_position < ctx->tt->cur->streams[0].first_index_position))
      {
      ctx->demux_mode = DEMUX_MODE_SI_NI;
      }
    }
  
  /* Adjust index positions for the streams */
  for(i = 0; i < ctx->tt->cur->num_streams; i++)
    ctx->tt->cur->streams[i].index_position = ctx->tt->cur->streams[i].first_index_position;
  }

int bgav_demuxer_start(bgav_demuxer_context_t * ctx)
  {
  if(!ctx->demuxer || !ctx->demuxer->open(ctx))
    return 0;
  
  if(ctx->si)
    {
    if(!(ctx->flags & BGAV_DEMUXER_SI_PRIVATE_FUNCS))
      {
      init_superindex(ctx);
      check_interleave(ctx);

      if((ctx->demux_mode == DEMUX_MODE_SI_NI) &&
         !(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Non interleaved file from non seekable source");
        return 0;
        }
      }
    //#ifdef DUMP_SUPERINDEX
    if(ctx->opt->dump_indices)
      bgav_superindex_dump(ctx->si);
    // #endif
    }
  return 1;
  }

void bgav_demuxer_stop(bgav_demuxer_context_t * ctx)
  {
  ctx->demuxer->close(ctx);
  ctx->priv = NULL;
    
  /* Reset global variables */
  ctx->flags &= ~BGAV_DEMUXER_SI_SEEKING;
  
  if(ctx->si)
    {
    bgav_superindex_destroy(ctx->si);
    ctx->si = NULL;
    }
  
  }


gavl_source_status_t bgav_demuxer_next_packet_interleaved(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * stream;
  bgav_packet_t * p;
  
  if(ctx->si->current_position >= ctx->si->num_entries)
    {
    return GAVL_SOURCE_EOF;
    }
  
  if(ctx->input->position >=
     ctx->si->entries[ctx->si->num_entries - 1].offset + 
     ctx->si->entries[ctx->si->num_entries - 1].size)
    {
    return  GAVL_SOURCE_EOF;
    }
  stream =
    bgav_track_find_stream(ctx,
                           ctx->si->entries[ctx->si->current_position].stream_id);
  
  if(!stream) /* Skip unused stream */
    {
    //  fprintf(stderr, "Skipping unused stream\n");
    //    bgav_input_skip_dump(ctx->input,
    //                         ctx->si->entries[ctx->si->current_position].size);
    
#if 0
    fprintf(stderr, "Skip unused %d\n",
            ctx->si->entries[ctx->si->current_position].stream_id);
#endif
    ctx->si->current_position++;
    return  GAVL_SOURCE_OK;
    }
#if 0
  if(stream->type == GAVF_STREAM_TEXT)
    {
    fprintf(stderr, "Got subtitle packet\n");
    }
#endif
  if((ctx->flags & BGAV_DEMUXER_SI_SEEKING) &&
     (stream->index_position > ctx->si->current_position))
    {
    ctx->si->current_position++;
    return GAVL_SOURCE_OK;
    }
  
  p = bgav_stream_get_packet_write(stream);
  bgav_packet_alloc(p, ctx->si->entries[ctx->si->current_position].size);
  p->buf.len = ctx->si->entries[ctx->si->current_position].size;
  p->flags = ctx->si->entries[ctx->si->current_position].flags;

  if(stream->flags & STREAM_DTS_ONLY)
    p->dts = ctx->si->entries[ctx->si->current_position].pts;
  else
    p->pts = ctx->si->entries[ctx->si->current_position].pts;
  
  p->duration = ctx->si->entries[ctx->si->current_position].duration;
  p->position = ctx->si->current_position;

  /* Skip until this packet */
  if(ctx->si->entries[ctx->si->current_position].offset > ctx->input->position)
    {
    bgav_input_skip(ctx->input,
                    ctx->si->entries[ctx->si->current_position].offset - ctx->input->position);
    }
  
  if(bgav_input_read_data(ctx->input, p->buf.buf, p->buf.len) < p->buf.len)
    return GAVL_SOURCE_EOF;
  
  if(stream->process_packet)
    stream->process_packet(stream, p);
  
  bgav_stream_done_packet_write(stream, p);
  
  ctx->si->current_position++;
  return GAVL_SOURCE_OK;
  }

static int next_packet_noninterleaved(bgav_demuxer_context_t * ctx)
  {
  bgav_packet_t * p;
  bgav_stream_t * s = NULL;

  s = ctx->request_stream;
  
  if(s->index_position > s->last_index_position)
    return 0;
  
  /* If the file is truely noninterleaved, this isn't neccessary, but who knows? */
  while(ctx->si->entries[s->index_position].stream_id != s->stream_id)
    {
    s->index_position++;
    }

  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    {
    bgav_input_seek(ctx->input, ctx->si->entries[s->index_position].offset, SEEK_SET);
    }
  else if(ctx->si->entries[s->index_position].offset > ctx->input->position)
    {
    bgav_input_skip(ctx->input,
                    ctx->si->entries[s->index_position].offset - ctx->input->position);
    }
  
  p = bgav_stream_get_packet_write(s);
  p->buf.len = ctx->si->entries[s->index_position].size;
  bgav_packet_alloc(p, p->buf.len);
  
  p->pts = ctx->si->entries[s->index_position].pts;
  p->duration = ctx->si->entries[s->index_position].duration;

  p->flags = ctx->si->entries[s->index_position].flags;
  p->position = s->index_position;
  
  if(bgav_input_read_data(ctx->input, p->buf.buf, p->buf.len) < p->buf.len)
    return 0;

  if(s->process_packet)
    s->process_packet(s, p);
  
  bgav_stream_done_packet_write(s, p);
  
  s->index_position++;
  return 1;
  
  }

gavl_source_status_t bgav_demuxer_next_packet(bgav_demuxer_context_t * demuxer)
  {
  gavl_source_status_t ret = GAVL_SOURCE_EOF;
  int i;
  
  /* Send state */
  if((demuxer->b->flags & (BGAV_FLAG_STATE_SENT|BGAV_FLAG_IS_RUNNING)) ==
     (BGAV_FLAG_IS_RUNNING))
    {
    /* Broadcast state */
    bgav_send_state(demuxer->b);
    demuxer->b->flags |= BGAV_FLAG_STATE_SENT;
    }
  
  //   fprintf(stderr, "bgav_demuxer_next_packet\n");
  switch(demuxer->demux_mode)
    {
    case DEMUX_MODE_SI_I:
      if(bgav_track_eof_d(demuxer->tt->cur))
        return 0;
      
      ret = bgav_demuxer_next_packet_interleaved(demuxer);
      if(!ret)
        bgav_track_set_eof_d(demuxer->tt->cur);
      
      break;
    case DEMUX_MODE_SI_NI:
      if(demuxer->request_stream->flags & STREAM_EOF_D)
        return 0;
      ret = next_packet_noninterleaved(demuxer);
      if(!ret)
        demuxer->request_stream->flags |= STREAM_EOF_D;
      break;
    case DEMUX_MODE_STREAM:
      ret = demuxer->demuxer->next_packet(demuxer);
      
      if(ret == GAVL_SOURCE_EOF)
        {
        /* Some demuxers have packets stored in the streams,
           we flush them here */
        
        for(i = 0; i < demuxer->tt->cur->num_streams; i++)
          {
          if(demuxer->tt->cur->streams[i].packet)
            {
            bgav_stream_done_packet_write(&demuxer->tt->cur->streams[i],
                                          demuxer->tt->cur->streams[i].packet);
            demuxer->tt->cur->streams[i].packet = NULL;
            ret = 1;
            }
          }
        bgav_track_set_eof_d(demuxer->tt->cur);
        }
      break;
    }
  
  return ret;
  }

void bgav_formats_dump()
  {
  int i;
  bgav_dprintf("<h2>Formats</h2>\n<ul>");
  for(i = 0; i < num_demuxers; i++)
    bgav_dprintf("<li>%s\n", demuxers[i].format_name);
  for(i = 0; i < num_sync_demuxers; i++)
    bgav_dprintf("<li>%s\n", sync_demuxers[i].format_name);
  bgav_dprintf("</ul>\n");
  }

static void parse_start(bgav_demuxer_context_t * ctx, int type_mask, int dur)
  {
  int j;
  bgav_track_clear(ctx->tt->cur);
  
  for(j = 0; j < ctx->tt->cur->num_streams; j++)
    {
    if(!(ctx->tt->cur->streams[j].type & type_mask))
      continue;
    ctx->tt->cur->streams[j].action = BGAV_STREAM_PARSE;

    if(dur)
      {
      ctx->tt->cur->streams[j].psink_parse =
        gavl_packet_sink_create(NULL,
                                bgav_stream_put_packet_get_duration,
                                &ctx->tt->cur->streams[j]);
      
      gavl_packet_buffer_set_calc_frame_durations(ctx->tt->cur->streams[j].pbuffer, 1);
      }
    else
      {
      ctx->tt->cur->streams[j].psink_parse =
        gavl_packet_sink_create(NULL,
                                bgav_stream_put_packet_parse,
                                &ctx->tt->cur->streams[j]);

      gavl_packet_buffer_set_mark_last(ctx->tt->cur->streams[j].pbuffer, 1);
      gavl_packet_buffer_set_calc_frame_durations(ctx->tt->cur->streams[j].pbuffer, 1);
      }
    }
  }

static void parse_end(bgav_demuxer_context_t * ctx, int type_mask)
  {
  int j;
  bgav_track_clear(ctx->tt->cur);
  for(j = 0; j < ctx->tt->cur->num_streams; j++)
    {
    if(!(ctx->tt->cur->streams[j].type & type_mask))
      continue;
    ctx->tt->cur->streams[j].action = BGAV_STREAM_MUTE;
    gavl_packet_buffer_clear(ctx->tt->cur->streams[j].pbuffer);
    if(ctx->tt->cur->streams[j].psink_parse)
      {
      gavl_packet_sink_destroy(ctx->tt->cur->streams[j].psink_parse);
      ctx->tt->cur->streams[j].psink_parse = NULL;
      }

    gavl_packet_buffer_set_mark_last(ctx->tt->cur->streams[j].pbuffer, 0);
    gavl_packet_buffer_set_calc_frame_durations(ctx->tt->cur->streams[j].pbuffer, 0);
    
    }
  bgav_input_seek(ctx->input, ctx->tt->cur->data_start, SEEK_SET);
  }

static int parse_packet(bgav_demuxer_context_t * ctx)
  {
  int i;
  gavl_packet_t * p = NULL;

  if(bgav_demuxer_next_packet(ctx) != GAVL_SOURCE_OK)
    return 0;
    
  for(i = 0; i < ctx->tt->cur->num_streams; i++)
    {
    if(!ctx->tt->cur->streams[i].psink_parse)
      continue;
    
    while(1)
      {
      p = NULL;
      if(gavl_packet_source_read_packet(gavl_packet_buffer_get_source(ctx->tt->cur->streams[i].pbuffer), &p) != GAVL_SOURCE_OK)
        break;
      gavl_packet_sink_put_packet(ctx->tt->cur->streams[i].psink_parse, p);
      }
    }
  return 1;
  }

int bgav_demuxer_get_duration(bgav_demuxer_context_t * ctx)
  {
  int i;
  int done = 0;
  int64_t position;
  int64_t end_position;
  int type_mask = GAVL_STREAM_AUDIO | GAVL_STREAM_VIDEO;

  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Getting stream duration");
  
  parse_start(ctx, type_mask, 1);

  if(ctx->tt->cur->data_end > 0)
    end_position = ctx->tt->cur->data_end;
  else
    end_position = ctx->input->total_bytes;

  position = end_position;
  
  while(!done)
    {
    position -= 1024*1024; // Start 1 MB from track end
    
    if(position < ctx->tt->cur->data_start)
      position = ctx->tt->cur->data_start;

    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Trying position: %"PRId64, position);
    
    bgav_input_seek(ctx->input, position, SEEK_SET);
    if(!ctx->demuxer->post_seek_resync(ctx))
      return 0;
    
    while(parse_packet(ctx))
      ;
    
    /* Check if we are done */

    done = 1;

    for(i = 0; i < ctx->tt->cur->num_streams; i++)
      {
      if(!(ctx->tt->cur->streams[i].type & type_mask))
        continue;
      if(ctx->tt->cur->streams[i].stats.pts_end == GAVL_TIME_UNDEFINED)
        {
        done = 0;
        break;
        }
      }
    
    if(!done)
      {
      bgav_track_clear(ctx->tt->cur);
      
      /* Reached beginning */
      if(position == ctx->tt->cur->data_start)
        {
        parse_end(ctx, type_mask);
        return 0;
        }
      }
    }
  
  parse_end(ctx, type_mask);
  return 1;
  }

void bgav_demuxer_set_clock_time(bgav_demuxer_context_t * ctx,
                                 int64_t pts, int scale, gavl_time_t clock_time)
  {
  gavl_time_t offset;
  
  if((ctx->flags & BGAV_DEMUXER_HAS_CLOCK_TIME) ||
     !ctx->tt ||
     !ctx->tt->cur)
    return;

  /* clock_time = pts_time - gavl_time_unscale(scale, pts) + clock_time */

  //  fprintf(stderr, "bgav_demuxer_set_clock_time: %"PRId64" %d %"PRId64"\n",
  //          pts, scale, clock_time);
  
  offset = clock_time - gavl_time_unscale(scale, pts);
  
  //  gavl_track_set_clock_time_map(ctx->tt->cur->info, pts, scale, clock_time);
  gavl_track_set_pts_to_clock_time(ctx->tt->cur->info, offset);
  
  ctx->flags |= BGAV_DEMUXER_HAS_CLOCK_TIME;
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got PTS to Clock time offset: %"PRId64, offset);
  
  // fprintf(stderr, "Have clock time\n");
  
  }

#if 0
void bgav_demuxer_parse_track(bgav_demuxer_context_t * ctx)
  {
  int type_mask = GAVL_STREAM_AUDIO | GAVL_STREAM_VIDEO | GAVL_STREAM_TEXT | GAVL_STREAM_OVERLAY;

  parse_start(ctx, type_mask, 0);
  
  bgav_input_seek(ctx->input, ctx->tt->cur->data_start, SEEK_SET);
  ctx->demuxer->post_seek_resync(ctx);
  
  while(parse_packet(ctx))
    ;
  
  parse_end(ctx, type_mask);
  
  }
#endif

#define RAW_BUFFER_LEN 1024

gavl_source_status_t bgav_demuxer_next_packet_raw(bgav_demuxer_context_t * ctx)
  {
  int ret;
  bgav_packet_t * p;
  bgav_stream_t * s;

  if(!(s = bgav_track_find_stream(ctx, BGAV_DEMUXER_STREAM_ID_RAW)))
    return GAVL_SOURCE_EOF;
  
  p = bgav_stream_get_packet_write(s);
  
  bgav_packet_alloc(p, RAW_BUFFER_LEN);
  p->position = ctx->input->position;
  p->buf.len = bgav_input_read_data(ctx->input, p->buf.buf, RAW_BUFFER_LEN);
  
  ret = (p->buf.len > 0) ? GAVL_SOURCE_OK : GAVL_SOURCE_EOF;
  bgav_stream_done_packet_write(s, p);
  return ret;
  }
