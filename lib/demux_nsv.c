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
#include <ctype.h>

#include <avdec_private.h>

#define AUDIO_ID 0
#define VIDEO_ID 1

#define LOG_DOMAIN "nsv"

/*
 *  Straight forward implementation of the NSV spec from
 *  http://ultravox.aol.com/
 */

#define NSV_FILE_HEADER BGAV_MK_FOURCC('N','S','V','f')
#define NSV_SYNC_HEADER BGAV_MK_FOURCC('N','S','V','s')

static int next_packet_nsv(bgav_demuxer_context_t * ctx);


typedef struct
  {
  uint32_t header_size;
  uint32_t file_size;
  uint32_t file_len; /* Milliseconds */
  uint32_t metadata_len;
  uint32_t toc_alloc;
  uint32_t toc_size;

  struct
    {
    char * title;
    char * url;
    char * creator;
    char * aspect;
    char * framerate;
    } metadata;

  struct
    {
    uint32_t * offsets;
    uint32_t * frames; /* Only present for Version 2 TOCs */
    } toc;
  
  } nsv_file_header_t;

static uint8_t * parse_metadata(uint8_t * buf,
                                char ** name,
                                char ** value)
  {
  char delim;
  char * start, *end;
  while(isspace(*buf) && (*buf != '\0'))
    buf++;
  if(*buf == '\0')
    return NULL;
  start = (char*)buf;
  end = strchr(start, '=');
  if(!end)
    return NULL;
  *name = gavl_strndup(start, end);
  start = end;
  start++; /* Start now points to the delimiter */
  delim = *start;
  start++;
  end = strchr(start, delim);
  if(!end)
    return NULL;
  *value = gavl_strndup(start, end);
  end++;
  start = end;
  start++;
  return (uint8_t*)start;
  }

static int nsv_file_header_read(bgav_input_context_t * ctx,
                                nsv_file_header_t * ret)
  {
  int i;
  uint8_t * buf, *pos;
  char * meta_name, *meta_value;
  
  bgav_input_skip(ctx, 4); /* NSVf */

  if(!bgav_input_read_32_le(ctx, &ret->header_size) ||
     !bgav_input_read_32_le(ctx, &ret->file_size) ||
     !bgav_input_read_32_le(ctx, &ret->file_len) || /* Milliseconds */
     !bgav_input_read_32_le(ctx, &ret->metadata_len) ||
     !bgav_input_read_32_le(ctx, &ret->toc_alloc) ||
     !bgav_input_read_32_le(ctx, &ret->toc_size))
    return 0;

  /* Allocate buffer */

  buf = malloc(ret->metadata_len+1 > ret->toc_alloc*4 ?
               ret->metadata_len+1 : ret->toc_alloc*4);
  
  /* Read metadata */
  if(bgav_input_read_data(ctx, buf, ret->metadata_len) < ret->metadata_len)
    return 0;
  buf[ret->metadata_len] = '\0';
  
  pos = buf;
  while(pos && (pos - buf < ret->metadata_len))
    {
    meta_name  = NULL;
    meta_value = NULL;
    pos = parse_metadata(pos, &meta_name, &meta_value);

    if(!pos)
      {
      if(meta_name) free(meta_name);
      if(meta_value) free(meta_value);
      break;
      }
    
    if(!strcasecmp(meta_name, "Title"))
      ret->metadata.title = meta_value;
    else if(!strcasecmp(meta_name, "URL"))
      ret->metadata.url = meta_value;
    else if(!strcasecmp(meta_name, "Creator"))
      ret->metadata.creator = meta_value;
    else if(!strcasecmp(meta_name, "Aspect"))
      ret->metadata.aspect = meta_value;
    else if(!strcasecmp(meta_name, "Framerate"))
      ret->metadata.framerate = meta_value;
    else
      free(meta_value);
    free(meta_name);
    }

  /* Read TOC */

  if(ret->toc_alloc)
    {
    if(bgav_input_read_data(ctx, buf, ret->toc_alloc*4) < ret->toc_alloc*4)
      return 0;

    /* Read TOC version 1 */
    ret->toc.offsets = malloc(ret->toc_size * sizeof(*ret->toc.offsets));
    pos = buf;

    for(i = 0; i < ret->toc_size; i++)
      {
      ret->toc.offsets[i] = GAVL_PTR_2_32LE(pos); pos+=4;
      }
    /* Read TOC version 2 */
    if((ret->toc_alloc > ret->toc_size * 2) &&
       (pos[0] == 'T') &&
       (pos[1] == 'O') &&
       (pos[2] == 'C') &&
       (pos[3] == '2'))
      {
      pos+=4;
      ret->toc.frames = malloc(ret->toc_size * sizeof(*ret->toc.frames));
      for(i = 0; i < ret->toc_size; i++)
        {
        ret->toc.frames[i] = GAVL_PTR_2_32LE(pos); pos+=4;
        }
      }
    }
  
  return 1;
  }

static void nsv_file_header_free(nsv_file_header_t * h)
  {
  if(h->metadata.title)     free(h->metadata.title);
  if(h->metadata.url)       free(h->metadata.url);
  if(h->metadata.creator)   free(h->metadata.creator);
  if(h->metadata.aspect)    free(h->metadata.aspect);
  if(h->metadata.framerate) free(h->metadata.framerate);

  if(h->toc.offsets)        free(h->toc.offsets);
  if(h->toc.frames)         free(h->toc.frames);
  }

static void nsv_file_header_dump(nsv_file_header_t * h)
  {
  int i;
  bgav_dprintf( "file_header\n");

  bgav_dprintf( "  header_size:  %d\n", h->header_size);
  bgav_dprintf( "  file_size:    %d\n", h->file_size);
  bgav_dprintf( "  file_len:     %d\n", h->file_len); /* Milliseconds */
  bgav_dprintf( "  metadata_len: %d\n", h->metadata_len);
  bgav_dprintf( "  toc_alloc:    %d\n", h->toc_alloc);
  bgav_dprintf( "  toc_size:     %d\n", h->toc_size);
  bgav_dprintf( "  title:        %s\n",
          (h->metadata.title ? h->metadata.title : "[not set]"));
  bgav_dprintf( "  url:          %s\n",
          (h->metadata.url ? h->metadata.url : "[not set]"));
  bgav_dprintf( "  creator:      %s\n",
          (h->metadata.creator ? h->metadata.creator : "[not set]"));
  bgav_dprintf( "  aspect:       %s\n",
          (h->metadata.aspect ? h->metadata.aspect : "[not set]"));
  bgav_dprintf( "  framerate:    %s\n",
          (h->metadata.framerate ? h->metadata.framerate : "[not set]"));

  if(h->toc_size)
    {
    if(h->toc.frames)
      {
      bgav_dprintf( "  TOC version 2 (%d entries)\n", h->toc_size);
      for(i = 0; i < h->toc_size; i++)
        bgav_dprintf( "    frame: %d, offset: %d\n",
                h->toc.frames[i], h->toc.offsets[i]);
      }
    else
      {
      bgav_dprintf( "  TOC version 1 (%d entries)\n", h->toc_size);
      for(i = 0; i < h->toc_size; i++)
        bgav_dprintf( "    offset: %d\n", h->toc.offsets[i]);
      }
    }
  else
    bgav_dprintf( "  No TOC\n");
  }

typedef struct
  {
  uint32_t vidfmt;
  uint32_t audfmt;
  uint16_t width;
  uint16_t height;
  uint8_t  framerate;
  int16_t  syncoffs;
  } nsv_sync_header_t;

static int nsv_sync_header_read(bgav_input_context_t * ctx,
                                nsv_sync_header_t * ret)
  {
  bgav_input_skip(ctx, 4); /* NSVs */

  if(!bgav_input_read_fourcc(ctx, &ret->vidfmt) ||
     !bgav_input_read_fourcc(ctx, &ret->audfmt) ||
     !bgav_input_read_16_le(ctx, &ret->width) ||
     !bgav_input_read_16_le(ctx, &ret->height) ||
     !bgav_input_read_8(ctx, &ret->framerate) ||
     !bgav_input_read_16_le(ctx, (uint16_t*)(&ret->syncoffs)))
    return 0;
  return 1;
  }

static void nsv_sync_header_dump(nsv_sync_header_t * h)
  {
  bgav_dprintf( "sync_header\n");
  bgav_dprintf( "  vidfmt: ");
  bgav_dump_fourcc(h->vidfmt);
  bgav_dprintf( "\n");

  bgav_dprintf( "  audfmt: ");
  bgav_dump_fourcc(h->audfmt);
  bgav_dprintf( "\n");

  bgav_dprintf( "  width:         %d\n", h->width);
  bgav_dprintf( "  height:        %d\n", h->height);
  bgav_dprintf( "  framerate_idx: %d\n", h->framerate);
  bgav_dprintf( "  syncoffs:      %d\n", h->syncoffs);
  }

typedef struct
  {
  nsv_file_header_t fh;
  int has_fh;
  
  int payload_follows; /* Header already read, payload follows */

  int is_pcm;
  int need_pcm_format;
  } nsv_priv_t;

static int probe_nsv(bgav_input_context_t * input)
  {
  uint32_t fourcc;
  char * pos;
  const char * mimetype; 
  /* Check for video/nsv */

  if(gavl_dictionary_get_src(&input->m, GAVL_META_SRC, 0, &mimetype, NULL) && mimetype &&
     !strcmp(mimetype, "video/nsv"))
    return 1;

  /* Probing a stream without any usable headers at the
     beginning isn't save enough so we check for the extension */
    
  if(input->filename)
    {
    pos = strrchr(input->filename, '.');
    if(pos && !strcasecmp(pos, ".nsv"))
      return 1;
    }
  
  if(!bgav_input_get_fourcc(input, &fourcc))
    return 0;

  if((fourcc != NSV_FILE_HEADER) &&
     (fourcc != NSV_SYNC_HEADER))
    return 0;

  return 1;
  }

static void simplify_rational(uint32_t * num, uint32_t * den)
  {
  int i = 2;

  while(i <= *den)
    {
    if(!(*num % i) && !(*den % i))
      {
      *num /= i;
      *den /= i;
      }
    else
      i++;
    }
  }

static void calc_framerate(int code, uint32_t * num, uint32_t * den)
  {
  int t, s_num, s_den;
  if(!(code & 0x80))
    {
    *num = code;
    *den = 1;
    return;
    }
  t = (code & 0x7f) >> 2;
  if(t < 16)
    {
    s_num = 1;
    s_den = t+1;
    }
  else
    {
    s_num = t - 15;
    s_den = 1;
    }

  if(code & 1)
    {
    s_num *= 1000;
    s_den *= 1001;
    }

  if((code & 3) == 3)
    {
    *num = s_num * 24;
    *den = s_den;
    }
  else if((code & 3) == 2)
    {
    *num = s_num * 25;
    *den = s_den;
    }
  else
    {
    *num = s_num * 30;
    *den = s_den;
    }
  simplify_rational(num, den);
  }

static int open_nsv(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;
  nsv_priv_t * p;

  nsv_sync_header_t sh;
  uint32_t fourcc;
  int done = 0;
  bgav_input_context_t * input_save = NULL;
  
  //  test_framerate();
  
  p = calloc(1, sizeof(*p));
  ctx->priv = p;
  
  while(!done)
    {
    /* Find the first chunk we can handle */
    while(1)
      {
      if(!bgav_input_get_fourcc(ctx->input, &fourcc))
        return 0;
      
      if((fourcc == NSV_FILE_HEADER) || (fourcc == NSV_SYNC_HEADER))
        break;
      bgav_input_skip(ctx->input, 1);
      }

    if(fourcc == NSV_FILE_HEADER)
      {
      if(!nsv_file_header_read(ctx->input, &p->fh))
        return 0;
      else
        {
        p->has_fh = 1;
        if(ctx->opt->dump_headers)
          nsv_file_header_dump(&p->fh);
        }
      }
    else if(fourcc == NSV_SYNC_HEADER)
      {
      if(!nsv_sync_header_read(ctx->input, &sh))
        return 0;
      if(ctx->opt->dump_headers)
        nsv_sync_header_dump(&sh);
      done = 1;
      }
    }
  ctx->tt = bgav_track_table_create(1);

  /* Set up streams */

  if(sh.vidfmt != BGAV_MK_FOURCC('N','O','N','E'))
    {
    s = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
    s->flags |= STREAM_NO_DURATIONS;
    s->fourcc = sh.vidfmt;
    
    s->data.video.format->image_width  = sh.width;
    s->data.video.format->image_height = sh.height;
    
    s->data.video.format->frame_width =
      s->data.video.format->image_width;
    s->data.video.format->frame_height =
      s->data.video.format->image_height;
    
    s->data.video.format->pixel_width  = 1;
    s->data.video.format->pixel_height = 1;
    s->stream_id = VIDEO_ID;

    /* Calculate framerate */

    calc_framerate(sh.framerate,
                   &s->data.video.format->timescale,
                   &s->data.video.format->frame_duration);
    /* Get depth for RGB3 */
    //    if(sh.vidfmt == BGAV_MK_FOURCC('R','G','B','3'))
    s->data.video.depth = 24;
    
    }
  
  if(sh.audfmt != BGAV_MK_FOURCC('N','O','N','E'))
    {
    s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
    s->fourcc = sh.audfmt;

    /* Probably also ('A','A','C',' ')? */
    if(s->fourcc == BGAV_MK_FOURCC('A','A','C','P'))
      {
      s->flags |= STREAM_PARSE_FULL;
      }
    
    s->stream_id = AUDIO_ID;
    if(sh.audfmt == BGAV_MK_FOURCC('P','C','M',' '))
      {
      p->is_pcm = 1;
      p->need_pcm_format = 1;
      }
    }

  /* Handle File header */

  if(p->has_fh)
    {
    /* Duration */
    if(p->fh.file_len != 0xFFFFFFFF)
      gavl_track_set_duration(ctx->tt->cur->info,
                              gavl_time_unscale(1000, p->fh.file_len));
    
    /* Metadata */
    if(p->fh.metadata.title)
      gavl_dictionary_set_string(ctx->tt->cur->metadata,
                        GAVL_META_TITLE,
                        p->fh.metadata.title);
    if(p->fh.metadata.url)
      gavl_dictionary_set_string(ctx->tt->cur->metadata,
                        GAVL_META_RELURL,
                        p->fh.metadata.url);
    if(p->fh.metadata.creator)
      gavl_dictionary_set_string(ctx->tt->cur->metadata,
                        GAVL_META_AUTHOR,
                        p->fh.metadata.creator);
    
    /* Decide whether we can seek */
    if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
      {
      if(p->fh.toc.offsets)
        ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
      else
        {
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                 "Seeking with version 2 TOC not support due to lack of sample files.\nContact the authors to solve this");
        }
      }

    }
  
  p->payload_follows = 1;

  ctx->data_start = ctx->input->position;
  ctx->flags |= BGAV_DEMUXER_HAS_DATA_START;

  bgav_track_set_format(ctx->tt->cur, "NSV", "video/nsv");

  if(p->need_pcm_format)
    {
    if(!(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
      {
      input_save = ctx->input;
      ctx->input = bgav_input_open_as_buffer(ctx->input);
      }
    while(p->need_pcm_format)
      {
      if(!next_packet_nsv(ctx)) /* This lets us get the format for PCM audio */
        return 0;
      }

    if(input_save)
      {
      bgav_input_close(ctx->input);
      bgav_input_destroy(ctx->input);
      ctx->input = input_save;
      }
    else
      bgav_input_seek(ctx->input, ctx->data_start, SEEK_SET);
    }
  
  return 1;
  }

static int get_pcm_format(bgav_demuxer_context_t * ctx, bgav_stream_t * s)
  {
  uint8_t  tmp_8;
  uint16_t tmp_16;
  /* Bits */
  if(!bgav_input_read_8(ctx->input, &tmp_8))
    return 0;
  s->data.audio.bits_per_sample = tmp_8;

  /* Channels */
  if(!bgav_input_read_8(ctx->input, &tmp_8))
    return 0;
  s->data.audio.format->num_channels = tmp_8;
  
  /* Samplerate */
  if(!bgav_input_read_16_le(ctx->input, &tmp_16))
    return 0;
  s->data.audio.format->samplerate = tmp_16;
  s->data.audio.block_align = (s->data.audio.bits_per_sample * s->data.audio.format->num_channels) / 8;

#if 1 /* What's that???? */
  s->data.audio.bits_per_sample = 8;
  s->data.audio.format->num_channels = 1;
  s->data.audio.format->samplerate /= 4;
#endif
  return 1;
  }

static int next_packet_nsv(bgav_demuxer_context_t * ctx)
  {
  //  uint8_t test_data[32];
  int i;
  bgav_packet_t * p;
  bgav_stream_t * s;
  nsv_priv_t * priv;
  uint8_t num_aux;
  uint16_t tmp_16;
  uint32_t aux_plus_video_len;
  uint32_t video_len;
  uint16_t audio_len;

  uint16_t aux_chunk_len;
  uint32_t aux_chunk_type;
  int skipped_bytes;
  
  nsv_sync_header_t sh;
  uint32_t fourcc;
  int have_sync_header = 0;

  priv = ctx->priv;

  //  fprintf(stderr, "next_packet_nsv\n");
  
  if(!priv->payload_follows)
    {
    /* Read header */

    if(!bgav_input_get_16_le(ctx->input, &tmp_16))
      return 0;

    if(tmp_16 != 0xbeef)
      {
      skipped_bytes = 0;
      while(1)
        {
        if(!bgav_input_get_fourcc(ctx->input, &fourcc))
          return 0;
        
        if(fourcc == NSV_SYNC_HEADER)
          break;
        bgav_input_skip(ctx->input, 1);
        skipped_bytes++;
        }
      if(!nsv_sync_header_read(ctx->input, &sh))
        return 0;
      if(ctx->opt->dump_headers)
        nsv_sync_header_dump(&sh);
      have_sync_header = 1;
      }
    else
      bgav_input_skip(ctx->input, 2); /* Skip beef */
    }
  else
    have_sync_header = 1;
  

  //  bgav_input_get_data(ctx->input, test_data, 32);
  //  gavl_hexdump(test_data, 32, 16);
  /* Parse payload */

  //  bgav_input_get_24_be(ctx->input, &dummy);
  //  fprintf(stderr, "24 bit: %06x\n", dummy);
  
  if(!bgav_input_read_8(ctx->input, &num_aux))
    return 0;

  if(!bgav_input_read_16_le(ctx->input, &tmp_16))
    return 0;
  aux_plus_video_len = tmp_16;
  
  if(!bgav_input_read_16_le(ctx->input, &audio_len))
    return 0;

  
  aux_plus_video_len = (aux_plus_video_len << 4) | (num_aux >> 4);
  num_aux &= 0x0f;
  video_len = aux_plus_video_len;

  //  fprintf(stderr, "Num AUX packets: %d\n", num_aux);
  
  /* Skip aux packets */
  for(i = 0; i < num_aux; i++)
    {
    if(!bgav_input_read_16_le(ctx->input, &aux_chunk_len) ||
       !bgav_input_read_fourcc(ctx->input, &aux_chunk_type))
      return 0;

    bgav_input_skip(ctx->input, aux_chunk_len);

    video_len -= (aux_chunk_len+6);
    }

  /* Video data */

  //  fprintf(stderr, "Video len: %d\n", video_len);
  
  if(video_len)
    {
    if(priv->need_pcm_format)
      s = bgav_track_get_video_stream( ctx->tt->cur, 0);
    else
      s = bgav_track_find_stream(ctx, VIDEO_ID);
    if(s && !priv->need_pcm_format)
      {
      p = bgav_stream_get_packet_write(s);
      bgav_packet_alloc(p, video_len);
      if(bgav_input_read_data(ctx->input, p->buf.buf, video_len) < video_len)
        return 0;
      p->buf.len = video_len;
      p->pts =
        s->in_position * s->data.video.format->frame_duration;
      
      if(s->fourcc == BGAV_MK_FOURCC('V','P','6','1'))
        {
        if(p->buf.buf[1] == 0x36)
          PACKET_SET_KEYFRAME(p);
        }
      else if(s->fourcc == BGAV_MK_FOURCC('V','P','6','2'))
        {
        if(p->buf.buf[1] == 0x46)
          PACKET_SET_KEYFRAME(p);
        }
      else
        {
        if(have_sync_header)
          PACKET_SET_KEYFRAME(p);
        }
      
      bgav_stream_done_packet_write(s, p);
      }
    else
      bgav_input_skip(ctx->input, video_len);
    }

  if(audio_len)
    {
    if(priv->need_pcm_format)
      s = bgav_track_get_audio_stream( ctx->tt->cur, 0);
    else
      s = bgav_track_find_stream(ctx, AUDIO_ID);
    if(s)
      {
      /* Special treatment for PCM */
      if(priv->is_pcm)
        {
        if(priv->need_pcm_format)
          {
          if(!get_pcm_format(ctx, s))
            return 0;
          priv->need_pcm_format = 0;
          }
        else
          bgav_input_skip(ctx->input, 4);
        audio_len -= 4;
        }
      if(audio_len && !priv->need_pcm_format)
        {
        p = bgav_stream_get_packet_write(s);
        bgav_packet_alloc(p, audio_len);
        if(bgav_input_read_data(ctx->input, p->buf.buf, audio_len) < audio_len)
          return 0;
        p->buf.len = audio_len;
        bgav_stream_done_packet_write(s, p);
        }
      }
    else
      bgav_input_skip(ctx->input, audio_len);
    }
  priv->payload_follows = 0;
  return 1;
  }

static void seek_nsv(bgav_demuxer_context_t * ctx, int64_t time, int scale)
  {
  int64_t frame_position;
  uint32_t index_position;
  int64_t file_position;
  nsv_priv_t * priv;
  gavl_time_t sync_time = GAVL_TIME_UNDEFINED;

  bgav_stream_t * vs, *as;
  nsv_sync_header_t sh;
  uint32_t fourcc;
  gavl_time_t duration = gavl_track_get_duration(ctx->tt->cur->info);
  
  priv = ctx->priv;
  
  if(!priv->fh.toc.frames) /* TOC version 1 */
    {
    index_position =
      (uint32_t)((double)gavl_time_unscale(scale, time) / (double)duration *
                 priv->fh.toc_size + 0.5);
    if(index_position >= priv->fh.toc_size)
      index_position = priv->fh.toc_size - 1;
    else if(index_position < 0)
      index_position = 0;
    sync_time = time;
    }
  else  /* TOC version 2 */
    {
    return;
    }
  file_position =
    priv->fh.toc.offsets[index_position] + priv->fh.header_size;
  bgav_input_seek(ctx->input, file_position, SEEK_SET);

  /* Now, resync and update stream times */

  while(1)
    {
    if(!bgav_input_get_fourcc(ctx->input, &fourcc))
      return;
    
    if(fourcc == NSV_SYNC_HEADER)
      break;
    bgav_input_skip(ctx->input, 1);
    }
  
  if(!nsv_sync_header_read(ctx->input, &sh))
    return;

  /* We consider the video time to be exact and calculate the audio
     time from the sync offset */

  vs = bgav_track_find_stream(ctx, VIDEO_ID);
  as = bgav_track_find_stream(ctx, AUDIO_ID);

  if(vs)
    {
    frame_position = gavl_time_rescale(scale, vs->data.video.format->timescale,
                                       sync_time);
    frame_position /= vs->data.video.format->frame_duration;
    STREAM_SET_SYNC(vs, frame_position * vs->data.video.format->frame_duration);
    vs->in_position = frame_position;
    }
  if(as)
    {
    STREAM_SET_SYNC(as,
                    gavl_time_rescale(scale, as->data.audio.format->samplerate, sync_time)+
                    gavl_time_rescale(1000, as->data.audio.format->samplerate, sh.syncoffs));
    }
  }

static int select_track_nsv(bgav_demuxer_context_t * ctx, int track)
  {
  nsv_priv_t * priv;
  priv = ctx->priv;

  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    priv->payload_follows = 1;
  return 1;
  }

static void close_nsv(bgav_demuxer_context_t * ctx)
  {
  nsv_priv_t * priv;
  priv = ctx->priv;
  nsv_file_header_free(&priv->fh);
  }

const bgav_demuxer_t bgav_demuxer_nsv =
  {
    .probe =        probe_nsv,
    .open =         open_nsv,
    .select_track = select_track_nsv,
    .next_packet =  next_packet_nsv,
    .seek =         seek_nsv,
    .close =        close_nsv
  };

