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
#include <string.h>

#include <config.h>

#define MAX_PAGE_BYTES 65307
#define MIN_HEADER_BYTES 27

#include <avdec_private.h>
#include <ogg_header.h>
#include <vorbis_comment.h>

#include <gavl/log.h>
#define LOG_DOMAIN "demux_ogg2"

static uint32_t detect_codec(const gavl_buffer_t * buf);
static int post_seek_resync_ogg(bgav_demuxer_context_t * ctx);

/* Fourcc definitions.
   Each private stream data also has a fourcc, which lets us
   detect OGM streams independently of the actual fourcc used here */

#define FOURCC_VORBIS    BGAV_MK_FOURCC('V','B','I','S') /* MUST match BGAV_VORBIS
                                                            from audio_vorbis.c */
#define FOURCC_THEORA    BGAV_MK_FOURCC('T','H','R','A')
#define FOURCC_FLAC      BGAV_MK_FOURCC('F','L','A','C')
#define FOURCC_FLAC_NEW  BGAV_MK_FOURCC('F','L','C','N')
#define FOURCC_OPUS      BGAV_MK_FOURCC('O','P','U','S')

#define FOURCC_SPEEX     BGAV_MK_FOURCC('S','P','E','X')
#define FOURCC_OGM_VIDEO BGAV_MK_FOURCC('O','G','M','V')
#define FOURCC_DIRAC     BGAV_MK_FOURCC('B','B','C','D')

#define FOURCC_OGM_TEXT  BGAV_MK_FOURCC('T','E','X','T')

#define FLAG_UNSYNC    (1<<0)
#define FLAG_DO_RESYNC (1<<1)

typedef struct
  {
  int64_t prev_granulepos;
  int header_packets_read;
  int header_packets_needed;
  uint32_t fourcc;
  gavl_dictionary_t m;

  int flags;
  
  }  ogg_stream_t;


static int
stream_append_header_packet(bgav_stream_t * s, const gavl_buffer_t * buf);

static void cleanup_stream_ogg(bgav_stream_t * s)
  {
  ogg_stream_t * p = s->priv;
  gavl_dictionary_free(&p->m);
  free(s->priv);
  s->priv = NULL;
  }


static ogg_stream_t * init_stream_common(bgav_stream_t * s,
                                         uint32_t fourcc, int header_packets_needed)
  {
  ogg_stream_t * stream_priv = calloc(1, sizeof(*stream_priv));

  s->priv = stream_priv;
  s->cleanup = cleanup_stream_ogg;
  s->fourcc = fourcc;
  stream_priv->fourcc = fourcc;
  stream_priv->header_packets_needed = header_packets_needed;
  return stream_priv;
  }

static int add_stream(bgav_demuxer_context_t * ctx, bgav_track_t * t, const bgav_ogg_page_t * page)
  {
  int i;
  uint32_t fourcc;
  int len, result;
  gavl_buffer_t buf;
  bgav_stream_t * s = NULL;
  int num_packets;
  
  gavl_buffer_init(&buf);

  num_packets = bgav_ogg_page_num_packets(page);
  
  len = bgav_ogg_page_get_packet_size(page, 0);
  if(!len)
    return 0;
  
  gavl_buffer_alloc(&buf, len);
  result = bgav_input_read_data(ctx->input, buf.buf, len);
  if(result < len)
    return 0;
  buf.len = result;
        
  fourcc = detect_codec(&buf);  

  if(!fourcc)
    {
    gavl_dprintf("Unknown header packet:\n");
    gavl_hexdump(buf.buf, buf.len, 16);
    return 0;
    }

  switch(fourcc)
    {
    case FOURCC_VORBIS:
      s = bgav_track_add_audio_stream(t, ctx->opt);
      init_stream_common(s, fourcc, 3);
      break;
    case FOURCC_FLAC:
      s = bgav_track_add_audio_stream(t, ctx->opt);
      /* The identification header counts as a header packet */
      init_stream_common(s, fourcc, 2);
      break;
    case FOURCC_FLAC_NEW:
      s = bgav_track_add_audio_stream(t, ctx->opt);
      init_stream_common(s, fourcc, GAVL_PTR_2_16BE(buf.buf+7)+1);
      break;
    case FOURCC_OPUS:
      s = bgav_track_add_audio_stream(t, ctx->opt);
      init_stream_common(s, fourcc, 2);
      break;
    case FOURCC_SPEEX:
      s = bgav_track_add_audio_stream(t, ctx->opt);
      init_stream_common(s, fourcc, 2);
      break;
    case FOURCC_THEORA:
      s = bgav_track_add_video_stream(t, ctx->opt);
      init_stream_common(s, fourcc, 3);
      break;
    case FOURCC_DIRAC:
      s = bgav_track_add_video_stream(t, ctx->opt);
      init_stream_common(s, fourcc, 1);
      break;
#if 0
    case FOURCC_OGM_VIDEO:
      s = bgav_track_add_video_stream(t, ctx->opt);
      init_stream_common(s, fourcc);
      break;
    case FOURCC_OGM_TEXT:
      s = bgav_track_add_text_stream(t, ctx->opt, NULL);
      init_stream_common(s, fourcc);
      break;
#endif
    }

  if(s)
    {
    stream_append_header_packet(s, &buf);
    s->stream_id = page->serialno;
    }
  
  for(i = 1; i < num_packets; i++)
    {
    len = bgav_ogg_page_get_packet_size(page, i);
    gavl_buffer_alloc(&buf, len);
    result = bgav_input_read_data(ctx->input, buf.buf, len);
    if(result < len)
      return 0;
    buf.len = len;

    if(s)
      stream_append_header_packet(s, &buf);
    }
  
  gavl_buffer_free(&buf);
  return 1;
  }


static int init_track(bgav_demuxer_context_t * ctx, bgav_track_t * t)
  {
  int i;
  int num_packets = 0;
  bgav_stream_t * s;
  int done = 0;
  ogg_stream_t * sp;
  bgav_ogg_page_t page;
  
  while(!done)
    {
    if(!bgav_ogg_page_read_header(ctx->input, &page))
      return GAVL_SOURCE_EOF;
    
    /* Got new stream */
    if(!(s = bgav_track_find_stream_all(t, page.serialno)))
      add_stream(ctx, t, &page);
    else // Another header -> append to stream
      {
      int len;
      int result;
      gavl_buffer_t buf;
      gavl_buffer_init(&buf);

      num_packets = bgav_ogg_page_num_packets(&page);
      
      for(i = 0; i < num_packets; i++)
        {
        len = bgav_ogg_page_get_packet_size(&page, i);
        gavl_buffer_alloc(&buf, len);
        result = bgav_input_read_data(ctx->input, buf.buf, len);
        if(result < len)
          return 0;
        buf.len = result;
        stream_append_header_packet(s, &buf);
        }
      }
    
    /*
     *  "officially" we can detect header pages by their zero granulepos,
     *   but in the wild that's not always correct.
     */

    done = 1;
    for(i = 0; i < t->num_streams; i++)
      {
      s = &t->streams[i];
      sp = s->priv;
      if(sp->header_packets_read < sp->header_packets_needed)
        {
        done = 0;
        break;
        }
      }
    
    }
  
  t->data_start = ctx->input->position;
  
  for(i = 0; i < t->num_streams; i++)
    {
    /* Merge metadata */
    sp =  t->streams[i].priv;
    gavl_dictionary_merge2(t->metadata, &sp->m);
    
    /* Set parse flags */
    switch(t->streams[i].fourcc)
      {
      case FOURCC_VORBIS:
      case FOURCC_FLAC:
      case FOURCC_FLAC_NEW:
      case FOURCC_OPUS:
      case FOURCC_SPEEX:
        t->streams[i].flags |= STREAM_DEMUXER_SETS_PTS_END;
        /* Fall through */
      case FOURCC_THEORA:
        bgav_stream_set_parse_frame(&t->streams[i]);
        break;
      case FOURCC_DIRAC:
      case FOURCC_OGM_VIDEO:
      case FOURCC_OGM_TEXT:
        break;
      }
    }
  return 1;
  }

static void init_chained(bgav_demuxer_context_t * ctx, int last_page_serialno)
  {
  /* We go through the file (skipping all pages) and detect the track boundaries */
  int64_t pos;
  bgav_ogg_page_t ph;
  bgav_track_t * t = NULL;
  
  memset(&ph, 0, sizeof(ph));
  pos = ctx->input->position;
  
  while(1)
    {
    if(!bgav_ogg_page_read_header(ctx->input, &ph))
      break;
    if(ph.header_type_flags & BGAV_OGG_HEADER_TYPE_BOS)
      {
      if(t)
        t->data_end = ph.position;
      else
        ctx->tt->tracks[0]->data_end = ph.position;
      
      t = bgav_track_table_append_track(ctx->tt);
      bgav_input_seek(ctx->input, ph.position, SEEK_SET);
      init_track(ctx, t);

      //      fprintf(stderr, "Got track\n");
      //      gavl_dictionary_dump(t->info, 2);
      
      /*
       * Return early if the serialno of the last page of the file
       * is already contained in the track
       *
       */
      
      if(bgav_track_find_stream_all(t, last_page_serialno))
        {
        t->data_end = ctx->input->total_bytes;
        break;
        }
      }
    else
      bgav_ogg_page_skip(ctx->input, &ph);
    }
  bgav_input_seek(ctx->input, pos, SEEK_SET);
  }

static int open_ogg(bgav_demuxer_context_t * ctx)
  {
  ctx->tt = bgav_track_table_create(1);

  if(!init_track(ctx, ctx->tt->cur))
    return 0;

  /* Default. Will be overwritten by init_chained() */
  if(ctx->input->total_bytes > 0)
    ctx->tt->cur->data_end = ctx->input->total_bytes;
  
  /* Check for chained stream, initialize chained stream */
  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    {
    int64_t position = ctx->input->position;
    
        
    bgav_input_seek(ctx->input, -MAX_PAGE_BYTES, SEEK_END);
    if(post_seek_resync_ogg(ctx))
      {
      bgav_ogg_page_t ph;
      memset(&ph, 0, sizeof(ph));
      
      /* Get the last page header */
      while(ctx->input->position < ctx->input->total_bytes)
        {
        if(!bgav_ogg_page_read_header(ctx->input, &ph))
          break;
        bgav_ogg_page_skip(ctx->input, &ph);
        }
      if(!bgav_track_find_stream_all(ctx->tt->cur, ph.serialno))
        {
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Detected chained stream");
        bgav_input_seek(ctx->input, position, SEEK_SET);
        init_chained(ctx, ph.serialno);
        }
      }
    bgav_input_seek(ctx->input, position, SEEK_SET);
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;

    }
  ctx->flags |= BGAV_DEMUXER_GET_DURATION;
  
  return 1;
  }

static gavl_source_status_t next_packet_ogg(bgav_demuxer_context_t * ctx)
  {
  int num_packets;
  int i;
  int len, result;
  bgav_stream_t * s;
  ogg_stream_t * stream_priv;
  bgav_ogg_page_t page;
  /* We process one entire page */
  
  //  fprintf(stderr, "next_packet_ogg ");

  if(ctx->input->position >= ctx->tt->cur->data_end)
    return GAVL_SOURCE_EOF;
  
  if(!bgav_ogg_probe(ctx->input))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Lost sync at pos %"PRId64, ctx->input->position);
    bgav_input_skip_dump(ctx->input, 16);
    return GAVL_SOURCE_EOF;
    }
  
  if(!bgav_ogg_page_read_header(ctx->input, &page))
    return GAVL_SOURCE_EOF;

  if(page.header_type_flags & BGAV_OGG_HEADER_TYPE_BOS)
    {
    /* Track change */
    return GAVL_SOURCE_EOF;
    }
  
  //  fprintf(stderr, "Continued: %d\n",
  //          !!(page.header_type_flags & BGAV_OGG_HEADER_TYPE_CONTINUED));
  
  /* Whole page is consumed in this function */
  
  s = bgav_track_find_stream(ctx, page.serialno);

  if(!s)
    {
    bgav_ogg_page_skip(ctx->input, &page);
    return GAVL_SOURCE_OK;
    }
#if 0
  if(s->type == GAVL_STREAM_MSG)
    {
    fprintf(stderr, "Buuuug\n");
    }
#endif

  if((s->action == BGAV_STREAM_PARSE) &&
     (s->type == GAVL_STREAM_AUDIO) &&
     (s->stats.pts_end < page.granulepos))
    s->stats.pts_end = page.granulepos;
  
  num_packets = bgav_ogg_page_num_packets(&page);
  
  stream_priv = s->priv;

  /* Finish resync from before */
  if(stream_priv->flags & FLAG_DO_RESYNC)
    {
    /*
     * Discard last packet from previous page if a new
     * packet starts here. This ensures that we
     * start with a valid pes_pts after seeking
     */
    if(!(page.header_type_flags & BGAV_OGG_HEADER_TYPE_CONTINUED) &&
       s->packet)
      {
      fprintf(stderr, "Discarding packet from last page: %d bytes\n", s->packet->buf.len);
      
      gavl_buffer_reset(&s->packet->buf);
      s->packet->position = page.position;

      }

    stream_priv->flags &= ~FLAG_DO_RESYNC;
    }
  else
    {
    /* Send last packet from previous page */
    if(!(page.header_type_flags & BGAV_OGG_HEADER_TYPE_CONTINUED))
      {
      if(s->packet)
        {
        bgav_stream_done_packet_write(s, s->packet);
        s->packet = NULL;
        }
      }
    }
      
  /* Resync after seeking */
  if(stream_priv->flags & FLAG_UNSYNC)
    {
    
    /*
     *   0: header page
     *  -1: No packet finished on this page
     */
    if(page.granulepos < 1)
      {
      bgav_ogg_page_skip(ctx->input, &page);
      return GAVL_SOURCE_OK;
      }

    fprintf(stderr, "Resync stream %d, prev_granulepos: %"PRId64"\n",
            s->stream_id, page.granulepos);
    
    stream_priv->prev_granulepos = page.granulepos;
    
    /* There are two cases to distinguish:

     * The last packet of this page is continued on the next page.
       In this case, granulepos is the PTS for the last packet beginning on this page

     * The last packet of this page ends on this page. In this case, granulepos is the
       PTS of the first packet on the next page.

       The actual case can only be identified according to the BGAV_OGG_HEADER_TYPE_CONTINUED
       flag of the *next* page.
       
    */
    stream_priv->flags &= ~FLAG_UNSYNC;
    stream_priv->flags |= FLAG_DO_RESYNC;
    }
  
  /* Should never happen */
  if(!page.num_page_segments)
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Page has zero segments");
    return GAVL_SOURCE_OK;
    }
  
  for(i = 0; i < num_packets; i++)
    {
    len = bgav_ogg_page_get_packet_size(&page, i);

    /* Skip partial packet */
    if(!i && (page.header_type_flags & BGAV_OGG_HEADER_TYPE_CONTINUED) &&
       !s->packet)
      {
      bgav_input_skip(ctx->input, len);
      continue;
      }
    
    /*
     *  When resyncing, we skip all packets except the last one.
     *  If it's continued on the next page, the granulepos will be the pes_pts
     *  of that packet
     */
    if(stream_priv->flags & FLAG_DO_RESYNC)
      {
      if(i < num_packets - 1)
        {
        bgav_input_skip(ctx->input, len);
        continue;
        }
      }
    
    //    fprintf(stderr, "got packet segment %d\n", len);

    if(!s->packet)
      {
      s->packet = bgav_stream_get_packet_write(s);
      s->packet->position = page.position;
      }

    /* The first packet, which is released with data from the
       current page has the granulepos of the *last* page as
       timestamp */
    
    if((stream_priv->prev_granulepos != GAVL_TIME_UNDEFINED) &&
       (s->packet->pes_pts == GAVL_TIME_UNDEFINED))
      {
      s->packet->pes_pts = stream_priv->prev_granulepos;
      stream_priv->prev_granulepos = GAVL_TIME_UNDEFINED;
      }

    //    if(stream_priv->flags & FLAG_DO_RESYNC)
    //      fprintf(stderr, "Blupp\n");
    
    gavl_buffer_alloc(&s->packet->buf, s->packet->buf.len + len);
    result = bgav_input_read_data(ctx->input,
                                  s->packet->buf.buf + s->packet->buf.len, len);
    
    if(result < len)
      return 0;
    s->packet->buf.len += result;

    if(i < num_packets - 1)
      {
      bgav_stream_done_packet_write(s, s->packet);
      s->packet = NULL;
      }
    /* End of stream */
    else if((page.header_type_flags & BGAV_OGG_HEADER_TYPE_EOS) &&
            (i == num_packets - 1))
      {
      s->packet->flags |= GAVL_PACKET_LAST;
      bgav_stream_done_packet_write(s, s->packet);
      s->packet = NULL;
      }
    }
  
  /* Remember granulepos */

  if(!(stream_priv->flags & FLAG_DO_RESYNC))
    stream_priv->prev_granulepos = page.granulepos;

#if 0  
  if(!len && !num_packets)
    {
    fprintf(stderr, "Got zero segments\n");
    }
#endif
  return GAVL_SOURCE_OK;
  }

static void sync_streams(bgav_demuxer_context_t * ctx, int64_t t)
  {
  int i;
  
  /* Clear the prev_granulepos */
  for(i = 0; i < ctx->tt->cur->num_streams; i++)
    {
    ogg_stream_t * sp;

    if(ctx->tt->cur->streams[i].type == GAVL_STREAM_MSG)
      continue;
    
    sp = ctx->tt->cur->streams[i].priv;
    sp->prev_granulepos = t;
    if(t == GAVL_TIME_UNDEFINED)
      sp->flags |= FLAG_UNSYNC;
    else if(!t)
      sp->flags &= ~FLAG_UNSYNC;
    }
  
  }

static int post_seek_resync_ogg(bgav_demuxer_context_t * ctx)
  {
  int i;
  int64_t position;
  bgav_ogg_page_t ph;

  sync_streams(ctx, GAVL_TIME_UNDEFINED);
  
  /* We don't bother calculating CRC. Instead we check for the "OggS" pattern where we expect
     it */
  
  for(i = 0; i < MAX_PAGE_BYTES; i++)
    {
    if(ctx->input->position >= ctx->input->total_bytes - MIN_HEADER_BYTES)
      return 0;
    
    if(!bgav_ogg_probe(ctx->input))
      {
      bgav_input_skip(ctx->input, 1);
      continue;
      }
    position = ctx->input->position;
    
    /* Check for next page */
    if(!bgav_ogg_page_read_header(ctx->input, &ph))
      return 0;

    bgav_ogg_page_skip(ctx->input, &ph);
    
    if(ctx->input->position == ctx->input->total_bytes)
      {
      bgav_input_seek(ctx->input, position, SEEK_SET);
      return 1;
      }
       
    if(bgav_ogg_probe(ctx->input))
      {
      bgav_input_seek(ctx->input, position, SEEK_SET);
      return 1;
      }
    else
      bgav_input_seek(ctx->input, position+1, SEEK_SET);
    }
  return 0;
  }

#if 0
static void close_ogg(bgav_demuxer_context_t * ctx)
  {
  ogg_t * priv = ctx->priv;
  free(priv);
  }
#endif

static int select_track_ogg(bgav_demuxer_context_t * ctx, int track)
  {
  //  ogg_t * priv = ctx->priv;

  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    sync_streams(ctx, 0);
  else
    sync_streams(ctx, GAVL_TIME_UNDEFINED);
  return 1;
  }

const bgav_demuxer_t bgav_demuxer_ogg2 =
  {
    .probe =        bgav_ogg_probe,
    .open =         open_ogg,
    .next_packet =  next_packet_ogg,
    .post_seek_resync =  post_seek_resync_ogg,
    // .seek =         seek_ogg,
    .select_track = select_track_ogg
  };

/* Codec specific struff goes here */


/* Get the fourcc from the identification packet */
static uint32_t detect_codec(const gavl_buffer_t * buf)
  {
  if((buf->len > 7) &&
     (buf->buf[0] == 0x01) &&
     gavl_string_starts_with((char*)(buf->buf+1), "vorbis"))
    return FOURCC_VORBIS;

  if((buf->len >= 19) &&
     gavl_string_starts_with((char*)(buf->buf), "OpusHead"))
    return FOURCC_OPUS;
  
  else if((buf->len > 7) &&
          (buf->buf[0] == 0x80) &&
          gavl_string_starts_with((char*)(buf->buf+1), "theora"))
    return FOURCC_THEORA;

  else if((buf->len == 4) &&
          gavl_string_starts_with((char*)(buf->buf), "fLaC"))
    return FOURCC_FLAC;

  else if((buf->len > 5) &&
          (buf->buf[0] == 0x7F) &&
          gavl_string_starts_with((char*)(buf->buf+1), "FLAC"))
    return FOURCC_FLAC_NEW;
  
  else if((buf->len >= 80) &&
          gavl_string_starts_with((char*)(buf->buf), "Speex"))
    return FOURCC_SPEEX;

  else if((buf->len >= 9) &&
          (buf->buf[0] == 0x01) &&
          gavl_string_starts_with((char*)(buf->buf+1), "video"))
    return FOURCC_OGM_VIDEO;

  else if((buf->len >= 9) &&
          (buf->buf[0] == 0x01) &&
          gavl_string_starts_with((char*)(buf->buf+1), "text"))
    return FOURCC_OGM_TEXT;
  else if((buf->len >= 4) &&
          gavl_string_starts_with((char*)(buf->buf), "BBCD"))
    return FOURCC_DIRAC;
  
  return 0;
  }

static void parse_vorbis_comment(bgav_stream_t * s, uint8_t * data,
                                 int len)
  {
  const char * language;
  const char * field;
  ogg_stream_t * stream_priv;
  bgav_vorbis_comment_t vc;
  bgav_input_context_t * input_mem;
  input_mem = bgav_input_open_memory(data, len);

  memset(&vc, 0, sizeof(vc));

  stream_priv = s->priv;
  
  if(!bgav_vorbis_comment_read(&vc, input_mem))
    return;

  gavl_dictionary_reset(&stream_priv->m);
  
  bgav_vorbis_comment_2_metadata(&vc, &stream_priv->m);

  field = bgav_vorbis_comment_get_field(&vc, "LANGUAGE", 0);
  if(field)
    {
    language = bgav_lang_from_name(field);
    if(language)
      gavl_dictionary_set_string(s->m, GAVL_META_LANGUAGE, language);
    }
  
  gavl_dictionary_set_string(s->m, GAVL_META_SOFTWARE, vc.vendor);
  bgav_vorbis_comment_free(&vc);
  bgav_input_destroy(input_mem);
  
  //  fprintf(stderr, "Got vorbis comment:\n");
  //  gavl_dictionary_dump(&stream_priv->m, 2);
  
  }

static int stream_append_header_packet(bgav_stream_t * s, const gavl_buffer_t * buf)
  {
  ogg_stream_t * p = s->priv;
  
  p->header_packets_read++;
  
  switch(p->fourcc)
    {
    case FOURCC_VORBIS: 
    case FOURCC_THEORA:
      if(p->header_packets_read == 2)
        parse_vorbis_comment(s, buf->buf + 7, buf->len - 7);
      gavl_append_xiph_header(&s->ci->codec_header, buf->buf, buf->len);
      break;
    case FOURCC_FLAC:
      {
      //      fprintf(stderr, "Got flac header: ");
      //      gavl_hexdump(buf->buf, buf->len > 16 ? 16 : buf->len, 16);

      /* Skip identification header */
      if((buf->len == 4) && !memcmp(buf->buf, "fLaC", 4))
        return 0;
      
      switch(buf->buf[0] & 0x7f)
        {
        case 0: /* STREAMINFO, this is the only info we'll tell to the flac decoder */
          if(s->ci->codec_header.buf)
            return 0;
          
          gavl_buffer_alloc(&s->ci->codec_header, buf->len + 4);
          s->ci->codec_header.buf[0] = 'f';
          s->ci->codec_header.buf[1] = 'L';
          s->ci->codec_header.buf[2] = 'a';
          s->ci->codec_header.buf[3] = 'C';
          s->ci->codec_header.len = 4;
          gavl_buffer_append(&s->ci->codec_header, buf);
          
          /* We tell the decoder, that this is the last metadata packet */
          s->ci->codec_header.buf[4] |= 0x80;

          // fprintf(stderr, "Got flac header\n");
          // gavl_hexdump(s->ci->codec_header.buf, s->ci->codec_header.len, 16);
          
          break;
        case 1:
          parse_vorbis_comment(s, buf->buf + 4, buf->len - 4);
          break;
        default:
          fprintf(stderr, "Unknown flac header: ");
          gavl_hexdump(buf->buf, buf->len, 16);
        }
      if(!(buf->buf[0] & 0x80))
        p->header_packets_needed++;
      }
      break;
    case FOURCC_FLAC_NEW:
      if(p->header_packets_read == 1)
        bgav_stream_set_extradata(s, buf->buf + 9, buf->len - 9);
      
      /* We tell the decoder, that this is the last metadata packet */
      s->ci->codec_header.buf[4] |= 0x80;
      
      break;
    case FOURCC_OPUS:
      switch(p->header_packets_read)
        {
        case 1:
          bgav_stream_set_extradata(s, buf->buf, buf->len);
          break;
        case 2:
          /* Vorbis comment */
          if(gavl_string_starts_with((char*)buf->buf, "OpusTags"))
            {
            parse_vorbis_comment(s, buf->buf + 8, buf->len - 8);
            }
          break;
        }
      break;
    case FOURCC_SPEEX:
      switch(p->header_packets_read)
        {
        case 1:
          bgav_stream_set_extradata(s, buf->buf, buf->len);
          break;
        case 2:
          /* Vorbis comment */
          parse_vorbis_comment(s, buf->buf + 8, buf->len - 8);
          break;
        }
      
      break;
    case FOURCC_OGM_VIDEO:
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Unsupported video format (OGM), please report");
      return 0;
      break;
    case FOURCC_DIRAC:
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Unsupported video format (Dirac), please report");
      return 0;
      break;
    case FOURCC_OGM_TEXT:
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Unsupported subtitle format (OGM Text), please report");
      return 0;
      break;
    }
  return 1;
  }

