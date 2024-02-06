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


#include <avdec_private.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LOG_DOMAIN "4xm"

// #define ID_RIFF BGAV_MK_FOURCC('R','I','F','F')
// #define ID_4XMV BGAV_MK_FOURCC('4','X','M','V')

#define ID_LIST BGAV_MK_FOURCC('L','I','S','T')
#define ID_HEAD BGAV_MK_FOURCC('H','E','A','D')
#define ID_HNFO BGAV_MK_FOURCC('H','N','F','O')
#define ID_info BGAV_MK_FOURCC('i','n','f','o')
#define ID_std_ BGAV_MK_FOURCC('s','t','d','_')
#define ID_TRK_ BGAV_MK_FOURCC('T','R','K','_')
#define ID_MOVI BGAV_MK_FOURCC('M','O','V','I')

#define ID_vtrk BGAV_MK_FOURCC('v','t','r','k')
#define ID_strk BGAV_MK_FOURCC('s','t','r','k')

#define ID_VTRK BGAV_MK_FOURCC('V','T','R','K')
#define ID_STRK BGAV_MK_FOURCC('S','T','R','K')

#define ID_ifrm BGAV_MK_FOURCC('i','f','r','m')
#define ID_pfrm BGAV_MK_FOURCC('p','f','r','m')
#define ID_cfrm BGAV_MK_FOURCC('c','f','r','m')
#define ID_snd_ BGAV_MK_FOURCC('s','n','d','_')

#define AUDIO_STREAM_OFFSET 10

typedef struct
  {
  int64_t video_pts;
  uint32_t movi_end;
  
  uint8_t video_tag[4];
  } fourxm_priv_t;

typedef struct
  {
  uint32_t fourcc;
  uint32_t size;
  uint32_t type;   /* Only valid if fourcc == LIST */
  int64_t end_pos;
  } fourxm_chunk_t;

static int read_chunk_header(bgav_input_context_t * input,
                              fourxm_chunk_t * ch)
  {
  if(input->position & 1)
    bgav_input_skip(input, 1);
  if(!bgav_input_read_fourcc(input, &ch->fourcc) ||
     !bgav_input_read_32_le(input, &ch->size))
    return 0;

  ch->end_pos = input->position + ch->size;
  
  if(ch->fourcc == ID_LIST)
    {
    if(!bgav_input_read_fourcc(input, &ch->type))
      return 0;
    }
  else
    ch->type = 0;
  
  return 1;
  }

static void skip_chunk(bgav_input_context_t * input,
                       fourxm_chunk_t * ch)
  {
  int32_t bytes_to_skip = ch->end_pos - input->position;
  if(bytes_to_skip > 0)
    bgav_input_skip(input, bytes_to_skip);
  }

#if 0
static void dump_chunk_header(fourxm_chunk_t * ch)
  {
  gavl_dprintf("4xm chunk header\n");
  gavl_dprintf("  fourcc: ");
  bgav_dump_fourcc(ch->fourcc);
  gavl_dprintf("\n");

  gavl_dprintf("  size:   %d\n", ch->size);
  if(ch->fourcc == ID_LIST)
    {
    gavl_dprintf("  type:   ");
    bgav_dump_fourcc(ch->type);
    gavl_dprintf("\n");
    }
  }
#endif

static int probe_4xm(bgav_input_context_t * input)
  {
  uint8_t data[12];
  if(bgav_input_get_data(input, data, 12) < 12)
    return 0;

  if((data[0]  == 'R') &&
     (data[1]  == 'I') &&
     (data[2]  == 'F') &&
     (data[3]  == 'F') &&
     (data[8]  == '4') &&
     (data[9]  == 'X') &&
     (data[10] == 'M') &&
     (data[11] == 'V'))
    return 1;
  return 0;
  }

static int setup_audio_stream(bgav_demuxer_context_t * ctx,
                              fourxm_chunk_t * parent)
  {
  fourxm_chunk_t chunk;
  bgav_stream_t * s;
  uint32_t tmp_32;
  
  while(ctx->input->position < parent->end_pos)
    {
    if(!read_chunk_header(ctx->input, &chunk))
      return 0;

    switch(chunk.fourcc)
      {
      case ID_strk:
        s = bgav_track_add_audio_stream(ctx->tt->cur,
                                        ctx->opt);
        // bytes 8-11   track number
        if(!bgav_input_read_32_le(ctx->input, &tmp_32))
          return 0;
        s->stream_id = AUDIO_STREAM_OFFSET + tmp_32;

        // bytes 12-15  audio .type = 0 = PCM, 1 = 4X IMA ADPCM
        if(!bgav_input_read_32_le(ctx->input, &tmp_32))
          return 0;

        switch(tmp_32)
          {
          case 0: // PCM
            s->fourcc = BGAV_WAVID_2_FOURCC(0x0001);
            break;
          case 1:
            s->fourcc = BGAV_MK_FOURCC('4','X','M','A');
            break;
          }
        // bytes 16-35  unknown
        bgav_input_skip(ctx->input, 35 - 16 + 1);

        // bytes 36-39  number of audio channels
        if(!bgav_input_read_32_le(ctx->input, &tmp_32))
          return 0;
        s->data.audio.format->num_channels = tmp_32;

        // bytes 40-43  audio sample rate
        if(!bgav_input_read_32_le(ctx->input, &tmp_32))
          return 0;
        s->data.audio.format->samplerate = tmp_32;
       
        // bytes 44-47  audio sample resolution (8 or 16 bits)
        if(!bgav_input_read_32_le(ctx->input, &tmp_32))
          return 0;

        s->data.audio.bits_per_sample = tmp_32;
        break;
      default:
        skip_chunk(ctx->input, &chunk);
      }
    
    }
  return 1;
  }

static int setup_video_stream(bgav_demuxer_context_t * ctx,
                              fourxm_chunk_t * parent,
                              float framerate)
  {
  uint32_t width, height;
  fourxm_chunk_t chunk;
  bgav_stream_t * s;
  fourxm_priv_t * priv = ctx->priv;
  
  while(ctx->input->position < parent->end_pos)
    {
    if(!read_chunk_header(ctx->input, &chunk))
      return 0;
    
    switch(chunk.fourcc)
      {
      case ID_vtrk:
        s = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);

        bgav_input_skip(ctx->input, 8); // 
        
        if(bgav_input_read_data(ctx->input, priv->video_tag, 4) < 4)
          return 0;

        bgav_stream_set_extradata(s, priv->video_tag, 4);
        bgav_input_skip(ctx->input, 16); // bytes 8-35   unknown

        if(!bgav_input_read_32_le(ctx->input, &width) ||
           !bgav_input_read_32_le(ctx->input, &height))
          return 0;

        s->data.video.format->image_width = width;
        s->data.video.format->frame_width = width;

        s->data.video.format->image_height = height;
        s->data.video.format->frame_height = height;

        s->data.video.format->pixel_width  = 1;
        s->data.video.format->pixel_height = 1;

        s->data.video.format->timescale      = 1000000;
        s->data.video.format->frame_duration = 1000000.0/framerate;
        
        /* Will be increased before the first frame is read */
        priv->video_pts = -s->data.video.format->frame_duration;
        s->fourcc = BGAV_MK_FOURCC('4','X','M','V');

        //        skip_chunk(ctx->input, &chunk);
#if 1
        bgav_input_skip(ctx->input, 4); // bytes 44-47  video width (again?)
        bgav_input_skip(ctx->input, 4); // bytes 48-51  video height (again?)
        bgav_input_skip(ctx->input, 75 - 52 + 1); // bytes 52-75  unknown
#endif
        break;
      default:
        skip_chunk(ctx->input, &chunk);
      }

    
    }
  return 1;
  }


static int open_4xm(bgav_demuxer_context_t * ctx)
  {
  int done = 0;
  fourxm_chunk_t list_chunk;
  fourxm_chunk_t list_subchunk;
  fourxm_chunk_t chunk;
  fourxm_priv_t * priv;
  float framerate = 25.0; // Dummy value

  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
  
  /* Header is already probed */
  bgav_input_skip(ctx->input, 12);

  ctx->tt = bgav_track_table_create(1);
  
  while(!done)
    {
    if(!read_chunk_header(ctx->input, &list_chunk))
      {
      return 0;
      }
    //    dump_chunk_header(&list_chunk);

    switch(list_chunk.type)
      {
      case ID_HEAD:
        while(ctx->input->position < list_chunk.end_pos)
          {
          if(!read_chunk_header(ctx->input, &list_subchunk))
            return 0;
          
          //          dump_chunk_header(&list_subchunk);

          switch(list_subchunk.type)
            {
            case ID_HNFO:
              while(ctx->input->position < list_subchunk.end_pos)
                {
                if(!read_chunk_header(ctx->input, &chunk))
                  return 0;
                //                dump_chunk_header(&chunk);

                switch(chunk.fourcc)
                  {
                  case ID_info:
                    {
                    char * tmp_string = calloc(chunk.size+1, 1);
                    if(bgav_input_read_data(ctx->input,
                                            (uint8_t*)tmp_string,
                                            chunk.size) < chunk.size)
                      {
                      free(tmp_string);
                      return 0;
                      }
                    gavl_dictionary_set_string_nocopy(ctx->tt->cur->metadata,
                                            GAVL_META_COMMENT, tmp_string);
                    }
                    break;
                  case ID_std_:
                    bgav_input_skip(ctx->input, 4); // data rate
                    if(!bgav_input_read_float_32_le(ctx->input, &framerate))
                      return 0;
                    break;
                  default:
                    skip_chunk(ctx->input, &chunk);
                    break;
                  }
                }
              break;
            case ID_TRK_:
              while(ctx->input->position < list_subchunk.end_pos)
                {
                if(!read_chunk_header(ctx->input, &chunk))
                  return 0;
                //                dump_chunk_header(&chunk);

                switch(chunk.type)
                  {
                  case ID_VTRK:
                    if(!setup_video_stream(ctx, &chunk, framerate))
                      return 0;
                    break;
                  case ID_STRK:
                    if(!setup_audio_stream(ctx, &chunk))
                      return 0;
                    break;
                  default:
                    skip_chunk(ctx->input, &chunk);
                    break;
                  }
                }
              skip_chunk(ctx->input, &list_subchunk);
              break;
            default:
              skip_chunk(ctx->input, &list_subchunk);
              break;
            }
          }
        break;
      case ID_MOVI:
        priv->movi_end = list_chunk.end_pos;
        done = 1;
        break;
      default:
        skip_chunk(ctx->input, &list_chunk);
        break;
      }
    }
  ctx->tt->cur->data_start = ctx->input->position;
  
  
  bgav_track_set_format(ctx->tt->cur, "4XM", NULL);
  return 1;
  }

static int select_track_4xm(bgav_demuxer_context_t * ctx, int track)
  {
  fourxm_priv_t * priv;
  bgav_stream_t * s = bgav_track_get_video_stream(ctx->tt->cur, 0);
  priv = ctx->priv;
  priv->video_pts = - s->data.video.format->frame_duration;
  return 1;
  }

static gavl_source_status_t next_packet_4xm(bgav_demuxer_context_t * ctx)
  {
  uint8_t header[8];
  uint32_t fourcc;
  int done = 0;
  uint32_t size;
  bgav_stream_t * s;
  bgav_packet_t * p;
  uint32_t stream_id;
  fourxm_priv_t * priv;

  priv = ctx->priv;
  
  while(!done)
    {
    // FIXME: Is this needed/correct?
    if(ctx->input->position & 1)
      bgav_input_skip(ctx->input, 1);

    if(ctx->input->position >= priv->movi_end)
      return GAVL_SOURCE_EOF; // EOF
    
    if(bgav_input_read_data(ctx->input, header, 8) < 8)
      return GAVL_SOURCE_EOF;

    
    fourcc = BGAV_PTR_2_FOURCC(&header[0]);
    switch(fourcc)
      {
      case ID_LIST:
        bgav_input_skip(ctx->input, 4); // FRAM
        s = bgav_track_get_video_stream(ctx->tt-> cur, 0);
        priv->video_pts += s->data.video.format->frame_duration;
        break;
      case ID_ifrm:
      case ID_pfrm:
      case ID_cfrm:
        size = GAVL_PTR_2_32LE(&header[4]);
        s = bgav_track_find_stream(ctx, 0);

        
        if(!s)
          {
          done = 1;
          bgav_input_skip(ctx->input, size);
          break;
          }
        
        p = bgav_stream_get_packet_write(s);
        bgav_packet_alloc(p, size + 8);
        memcpy(p->buf.buf, header, 8);
        p->buf.len = 8 + bgav_input_read_data(ctx->input, p->buf.buf+8, size);

        if(p->buf.len < size + 8)
          return GAVL_SOURCE_EOF;
                
        p->pts = priv->video_pts;
        
        bgav_stream_done_packet_write(s, p);


        done = 1;
        break;
      case ID_snd_:
        size = GAVL_PTR_2_32LE(&header[4]);
       
        if(!bgav_input_read_32_le(ctx->input, &stream_id))
          return GAVL_SOURCE_EOF;
        bgav_input_skip(ctx->input, 4); // out_size
        s = bgav_track_find_stream(ctx,
                                   stream_id + AUDIO_STREAM_OFFSET);

        if(!s)
          {
          bgav_input_skip(ctx->input, size-8);
          done = 1;
          break;
          }
        
        p = bgav_stream_get_packet_write(s);
        bgav_packet_alloc(p, size - 8);
        p->buf.len = bgav_input_read_data(ctx->input, p->buf.buf, size-8);

        if(p->buf.len < size-8)
          return GAVL_SOURCE_EOF;
        bgav_stream_done_packet_write(s, p);
        done = 1;
        break;
      default:
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                 "Unknown Chunk %c%c%c%c",
                 (fourcc & 0xFF000000) >> 24,
                 (fourcc & 0x00FF0000) >> 16,
                 (fourcc & 0x0000FF00) >> 8,
                 (fourcc & 0x000000FF));
        /* Exit here to prevent crashes with some broken files */
        return GAVL_SOURCE_EOF;
      }
    }
  
  return GAVL_SOURCE_OK;
  }

static void close_4xm(bgav_demuxer_context_t * ctx)
  {
  fourxm_priv_t * priv;
  priv = ctx->priv;
  free(priv);
  }

const bgav_demuxer_t bgav_demuxer_4xm =
  {
    .probe =        probe_4xm,
    .open =         open_4xm,
    .select_track = select_track_4xm,
    .next_packet =  next_packet_4xm,
    .close =        close_4xm
  };
