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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <avdec_private.h>

#include <rmff.h>
#define LOG_DOMAIN "demux_rm"


#define PN_SAVE_ENABLED 0x0001
#define PN_PERFECT_PLAY_ENABLED 0x0002
#define PN_LIVE_BROADCAST 0x0004

#define PN_KEYFRAME_FLAG 0x0002

typedef struct
  {
  bgav_rmff_header_t * header;
  
  //  uint32_t data_start;
  //  uint32_t data_size;
  int do_seek;
  uint32_t next_packet;

  uint32_t first_timestamp;
  int need_first_timestamp;
  
  int is_multirate;
  } rm_private_t;

/* Frame info for a video frame */
typedef struct
  {
  int pts;
  int pict_type;
  } frame_info_t;

static uint32_t seek_indx(bgav_rmff_indx_t * indx, uint32_t millisecs,
                          uint32_t * position, uint32_t * start_packet,
                          uint32_t * end_packet)
  {
  uint32_t ret;
  ret = indx->num_indices - 1;
  while(ret)
    {
    if(indx->records[ret].timestamp < millisecs)
      break;
    ret--;
    }
  if(*position > indx->records[ret].offset)
    *position = indx->records[ret].offset;
  if(*start_packet > indx->records[ret].packet_count_for_this_packet)
    *start_packet = indx->records[ret].packet_count_for_this_packet;
  if(*end_packet < indx->records[ret].packet_count_for_this_packet)
    *end_packet = indx->records[ret].packet_count_for_this_packet;
    
  
  return ret;
  }

static void cleanup_stream_rm(bgav_stream_t * s)
  {
  if(s->priv) free(s->priv);
  }

/* Get position for multirate files */

static int get_multirate_offsets(bgav_demuxer_context_t * ctx,
                                 bgav_rmff_stream_t * stream, uint32_t * data_start,
                                 uint32_t * data_end);

typedef struct
  {
  bgav_rmff_stream_t * stream;
  uint32_t index_record;
  uint32_t data_start, data_end, data_pos;
  } rm_stream_t;

typedef struct
  {
  rm_stream_t com;
  //  uint32_t kf_pts;
  //  uint32_t kf_base;
  uint32_t sub_id;
  
  void (*parse_frame_info)(uint8_t * data, int len, frame_info_t * info, uint32_t sub_id);
  
#if 0  
  void (*set_keyframe)(bgav_stream_t * s,
                       bgav_packet_t * p,
                       uint8_t * data, int len);
#endif
  int num_slices;
  int pic_num;
  int64_t pts_offset;
  int last_pts;
  } rm_video_stream_t;


#if 0
static void set_keyframe_rv2(bgav_stream_t * s,
                             bgav_packet_t * p,
                             uint8_t * data, int len)
  {
  //  rm_video_stream_t * sp = s->priv;

  if(p->flags)
    return;
  
  switch(data[0] >> 6)
    {
    case 0:
    case 1:
      PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_I);
      PACKET_SET_KEYFRAME(p);
      break;
    case 2:
      PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_P);
      break;
    case 3:
      PACKET_SET_CODING_TYPE(p, BGAV_CODING_TYPE_B);
      break;
    }

  //  fprintf(stderr, "RV20 type: %02x, %c %08x\n", data[0],
  //          PACKET_GET_CODING_TYPE(p), sp->sub_id);
  
  }
#endif

#if 0
static void dump_frame_info(frame_info_t * info)
  {
  bgav_dprintf("  Frame info: type: %c, PTS: %d\n",
               info->pict_type, info->pts);
  }
#endif

#define SKIP_BITS(n) buffer<<=n
#define SHOW_BITS(n) ((buffer)>>(32-(n)))

static void parse_frame_info_rv10(uint8_t * data, int len, frame_info_t * info, uint32_t sub_id)
  {
  uint32_t buffer = GAVL_PTR_2_32BE(data);
  info->pts = -1;

  SKIP_BITS(1);

  info->pict_type= SHOW_BITS(1);

  if(info->pict_type == 1)
    info->pict_type = BGAV_CODING_TYPE_P;
  else
    info->pict_type = BGAV_CODING_TYPE_I;
  }

static void parse_frame_info_rv20(uint8_t * data, int len, frame_info_t * info, uint32_t sub_id)
  {
  uint32_t buffer = GAVL_PTR_2_32BE(data);
  if(sub_id == 0x30202002 || sub_id == 0x30203002)
    {
    SKIP_BITS(3);
    }

  info->pict_type= SHOW_BITS(2);

  switch(info->pict_type)
    {
    case 0:
    case 1:
      info->pict_type = BGAV_CODING_TYPE_I;
      break;
    case 2:
      info->pict_type = BGAV_CODING_TYPE_P;
      break;
    case 3:
      info->pict_type = BGAV_CODING_TYPE_B;
      break;
    }
  
  info->pts = -1;
  }

static void parse_frame_info_rv30(uint8_t * data, int len, frame_info_t * info, uint32_t sub_id)
  {
  uint32_t buffer = GAVL_PTR_2_32BE(data);

  SKIP_BITS(3);
  info->pict_type= SHOW_BITS(2);
  SKIP_BITS(2 + 7);
  info->pts = SHOW_BITS(13);

  switch(info->pict_type)
    {
    case 0:
    case 1:
      info->pict_type = BGAV_CODING_TYPE_I;
      break;
    case 2:
      info->pict_type = BGAV_CODING_TYPE_P;
      break;
    case 3:
      info->pict_type = BGAV_CODING_TYPE_B;
      break;
    }
  
  }

static void parse_frame_info_rv40(uint8_t * data, int len, frame_info_t * info, uint32_t sub_id)
  {
  uint32_t buffer = GAVL_PTR_2_32BE(data);

  SKIP_BITS(1);
  info->pict_type= SHOW_BITS(2);
  SKIP_BITS(2 + 7 + 3);
  info->pts = SHOW_BITS(13);

  switch(info->pict_type)
    {
    case 0:
    case 1:
      info->pict_type = BGAV_CODING_TYPE_I;
      break;
    case 2:
      info->pict_type = BGAV_CODING_TYPE_P;
      break;
    case 3:
      info->pict_type = BGAV_CODING_TYPE_B;
      break;
    }
  }


/* Audio and video stream specific stuff */

typedef struct
  {
  rm_stream_t com;
  
  uint32_t coded_framesize;
  uint16_t sub_packet_h;
  //  uint16_t frame_size;
  uint16_t sub_packet_size;
  int sub_packet_cnt;
  uint16_t audiopk_size;
    
  //  uint8_t * extradata;
  //  int bytes_to_read;
  
  uint8_t * audio_buf;
  } rm_audio_stream_t;
#if 0
static void dump_audio(rm_audio_stream_t*s)
  {
  bgav_dprintf( "Audio stream:\n");
  bgav_dprintf( "coded_framesize: %d\n", s->coded_framesize);
  bgav_dprintf( "sub_packet_h      %d\n", s->sub_packet_h);
  bgav_dprintf( "audiopk_size      %d\n", s->audiopk_size);
  bgav_dprintf( "sub_packet_size   %d\n", s->sub_packet_size);
  //  bgav_dprintf( "Bytes to read     %d\n", s->bytes_to_read);
  
  //  uint8_t * extradata;

  }
#endif
static void init_audio_stream(bgav_demuxer_context_t * ctx,
                              bgav_rmff_stream_t * stream,
                              uint8_t * data)
  {
  uint16_t codecdata_length;
  uint16_t samplesize;
  uint16_t frame_size;
  int version;
#if 0
  int flavor;
#endif
  int coded_framesize;
  int sub_packet_size;
  int sub_packet_h;

  bgav_stream_t * bg_as;
  rm_audio_stream_t * rm_as;
  uint8_t desc_len;
  bgav_track_t * track = ctx->tt->cur;

  rm_private_t * priv;
  priv = ctx->priv;

    
  bg_as = bgav_track_add_audio_stream(track, ctx->opt);
  bg_as->cleanup = cleanup_stream_rm;
  /* Set container bitrate */
  bg_as->container_bitrate = stream->mdpr.avg_bit_rate;

  rm_as = calloc(1, sizeof(*rm_as));

  bg_as->priv = rm_as;

  /* Skip initial header */
  data += 4;
  
  version = GAVL_PTR_2_16BE(data);data+=2;

  if(version == 3)
    {
    int len;
    data += 16;
    len = *data; data += len+1; // Name
    len = *data; data += len+1; // Author
    len = *data; data += len+1; // Copyright
    data += 2;
    len = *data;
    data++;
    bg_as->fourcc = BGAV_PTR_2_FOURCC(data);
    bg_as->data.audio.format->num_channels = 1;
    bg_as->data.audio.bits_per_sample = 16;
    bg_as->data.audio.format->samplerate = 8000;
    }
  else
    {
    data += 2; // 00 00
    data += 4; /* .ra4 or .ra5 */
    data += 4; // ???
    data += 2; /* version (4 or 5) */
    data += 4; // header size == 0x4E
    bg_as->subformat = GAVL_PTR_2_16BE(data);data+=2;/* codec flavor id */
    coded_framesize = GAVL_PTR_2_32BE(data);data+=4;/* needed by codec */
    data += 4; // big number
    data += 4; // bigger number
    data += 4; // 2 || -''-
    // data += 2; // 0x10
    sub_packet_h = GAVL_PTR_2_16BE(data);data+=2;
    //		    data += 2; /* coded frame size */
    frame_size = GAVL_PTR_2_16BE(data);data+=2;
    sub_packet_size = GAVL_PTR_2_16BE(data);data+=2;
    data += 2; // 0
      
    if (version == 5)
      data += 6; //0,srate,0
  
    bg_as->data.audio.format->samplerate = GAVL_PTR_2_16BE(data);data+=2;
    data += 2;  // 0
    samplesize = GAVL_PTR_2_16BE(data);data += 2;
    samplesize /= 8;
    bg_as->data.audio.bits_per_sample = samplesize * 8;
    bg_as->data.audio.format->num_channels = GAVL_PTR_2_16BE(data);data+=2;
    if (version == 5)
      {
      data += 4;  // "genr"
      bg_as->fourcc = BGAV_PTR_2_FOURCC(data);data += 4;
      }
    else
      {		
      /* Desc #1 */
      desc_len = *data;
      data += desc_len+1;
      /* Desc #2 */
      desc_len = *data;
      data++;
      bg_as->fourcc = BGAV_PTR_2_FOURCC(data);
      data += desc_len;
      }
  
    switch(bg_as->fourcc)
      {
      case BGAV_MK_FOURCC('d', 'n', 'e', 't'):
        /* AC3 byteswapped */
        break;
      case BGAV_MK_FOURCC('1', '4', '_', '4'):
        bg_as->data.audio.block_align = 0x14;
        break;
      case BGAV_MK_FOURCC('2', '8', '_', '8'):
        bg_as->data.audio.block_align = coded_framesize;

        rm_as->sub_packet_size = sub_packet_size;
        rm_as->sub_packet_h = sub_packet_h;
        rm_as->coded_framesize = coded_framesize;
        rm_as->audiopk_size = frame_size;
        break;
      case BGAV_MK_FOURCC('s', 'i', 'p', 'r'):
      case BGAV_MK_FOURCC('a', 't', 'r', 'c'):
      case BGAV_MK_FOURCC('c', 'o', 'o', 'k'):
        /* Get extradata for codec */
        data += 3;
        if(version == 5)
          data++;

        codecdata_length = GAVL_PTR_2_32BE(data);data+=4;

        bgav_stream_set_extradata(bg_as, data, codecdata_length);
        
        if((bg_as->fourcc == BGAV_MK_FOURCC('a', 't', 'r', 'c')) ||
           (bg_as->fourcc == BGAV_MK_FOURCC('c', 'o', 'o', 'k')))
          bg_as->data.audio.block_align = sub_packet_size;
        else
          bg_as->data.audio.block_align = coded_framesize;
      
        rm_as->sub_packet_size = sub_packet_size;
        rm_as->sub_packet_h = sub_packet_h;
        rm_as->coded_framesize = coded_framesize;
        rm_as->audiopk_size = frame_size;
        break;
      case BGAV_MK_FOURCC('r', 'a', 'a', 'c'):
      case BGAV_MK_FOURCC('r', 'a', 'c', 'p'):
        data += 3;
        if(version == 5)
          data++;

        codecdata_length = GAVL_PTR_2_32BE(data);data+=4;

        if(codecdata_length>=1)
          bgav_stream_set_extradata(bg_as, data+1, codecdata_length-1);
        break;
      default:
      
        break;
      }

    if(rm_as->sub_packet_h && rm_as->audiopk_size)
      rm_as->audio_buf = malloc(rm_as->sub_packet_h * rm_as->audiopk_size);
    } // Version > 3

  //  dump_audio(rm_as);

  bg_as->stream_id = stream->mdpr.stream_number;
  bg_as->timescale = 1000;
    
  rm_as->com.stream = stream;

  /* Init multirate */

  if(priv->is_multirate)
    {
    if(!get_multirate_offsets(ctx, stream, &rm_as->com.data_start,
                              &rm_as->com.data_end))
      {
      }
    else
      {
      rm_as->com.data_pos = rm_as->com.data_start;
      }
    
    }
  
  }

static void init_audio_stream_mp3(bgav_demuxer_context_t * ctx, bgav_rmff_stream_t * stream)
  {
  bgav_stream_t * bg_as;
  bgav_track_t * track = ctx->tt->cur;
  rm_audio_stream_t * rm_as;
  rm_as = calloc(1, sizeof(*rm_as));

  
  bg_as = bgav_track_add_audio_stream(track, ctx->opt);
  bg_as->cleanup = cleanup_stream_rm;

  /* Set container bitrate */
  bg_as->container_bitrate = stream->mdpr.avg_bit_rate;

  /* RTP-packed mp3 */
  bg_as->fourcc = BGAV_MK_FOURCC('r','m','p','3');
  bg_as->stream_id = stream->mdpr.stream_number;
  bg_as->timescale = 1000;

  bg_as->priv = rm_as;
  
  rm_as->com.stream = stream;


  }


static void init_video_stream(bgav_demuxer_context_t * ctx,
                              bgav_rmff_stream_t * stream,
                              uint8_t * data_start, int len)
  {
  bgav_stream_t * bg_vs;
  rm_video_stream_t * rm_vs;
  
  bgav_track_t * track = ctx->tt->cur;

  uint8_t * data = data_start;

  rm_private_t * priv = ctx->priv;
  
  bg_vs = bgav_track_add_video_stream(track, ctx->opt);
  bg_vs->cleanup = cleanup_stream_rm;

  bg_vs->flags |= STREAM_NO_DURATIONS;
  
  rm_vs = calloc(1, sizeof(*rm_vs));

  /* Set container bitrate */
  bg_vs->container_bitrate = stream->mdpr.avg_bit_rate;
  
  bg_vs->priv = rm_vs;

  data += 4; // VIDO

  bg_vs->fourcc = BGAV_PTR_2_FOURCC(data);data+=4;

  /* emulate BITMAPINFOHEADER */

  bg_vs->data.video.format->frame_width = GAVL_PTR_2_16BE(data);data+=2;
  bg_vs->data.video.format->frame_height = GAVL_PTR_2_16BE(data);data+=2;

  bg_vs->data.video.format->image_width = bg_vs->data.video.format->frame_width;
  bg_vs->data.video.format->image_height = bg_vs->data.video.format->frame_height;

  bg_vs->data.video.format->pixel_width = 1;
  bg_vs->data.video.format->pixel_height = 1;
    
  // bg_vs->data.video.format->framerate_num = GAVL_PTR_2_16BE(data);data+=2;
  //  bg_vs->data.video.format->framerate_den = 1;
  // we probably won't even care about fps
  //  if (bg_vs->data.video.format->framerate_num<=0) bg_vs->data.video.format->framerate_num=24; 

  bg_vs->data.video.format->timescale = 1000;
  bg_vs->data.video.format->framerate_mode = GAVL_FRAMERATE_VARIABLE;
  
  data += 2; // fps =  GAVL_PTR_2_16BE(data); data+=2; /* fps */
  
  data += 4; /* Unknown */
  data += 2; // fps2 =  GAVL_PTR_2_16BE(data); data+=2; /* fps2 */
  data += 2; /* Unknown */

  //  fprintf(stderr, "FPS: %d, FPS2: %d\n", fps, fps2);
  
  /* Read extradata */

  bgav_stream_set_extradata(bg_vs, data, len - (data - data_start));
  
  switch(bg_vs->fourcc)
    {
    case BGAV_MK_FOURCC('R','V','1','0'):
      rm_vs->parse_frame_info = parse_frame_info_rv10;
      rm_vs->sub_id = GAVL_PTR_2_32BE(bg_vs->ci->global_header+4);
      
      break;
    case BGAV_MK_FOURCC('R','V','2','0'):
      rm_vs->parse_frame_info = parse_frame_info_rv20;
      rm_vs->sub_id = GAVL_PTR_2_32BE(bg_vs->ci->global_header+4);

      if(rm_vs->sub_id == 0x30202002
         || rm_vs->sub_id == 0x30203002
         || (rm_vs->sub_id >= 0x20200002 && rm_vs->sub_id < 0x20300000))
        bg_vs->ci->flags |= GAVL_COMPRESSION_HAS_B_FRAMES;
      break;
    case BGAV_MK_FOURCC('R','V','3','0'):
      rm_vs->parse_frame_info = parse_frame_info_rv30;
      bg_vs->ci->flags |= GAVL_COMPRESSION_HAS_B_FRAMES;
      break;
    case BGAV_MK_FOURCC('R','V','4','0'):
      rm_vs->parse_frame_info = parse_frame_info_rv40;
      bg_vs->ci->flags |= GAVL_COMPRESSION_HAS_B_FRAMES;
      break;
    }
  
#if 0
  
  //		    if(sh->format==0x30335652 || sh->format==0x30325652 )
  if(1)
    {
    tmp=GAVL_PTR_2_16BE(data);data+=2;
    //    if(tmp>0){
    //    bg_vs->data.video.format->framerate_num = tmp;
    //    }
    } else {
    int fps=GAVL_PTR_2_16BE(data);data+=2;
    printf("realvid: ignoring FPS = %d\n",fps);
    }

  data += 2; // Unknown
  
  // read codec sub-format (to make difference between low and high rate codec)
  //  ((unsigned int*)(sh->bih+1))[0]=GAVL_PTR_2_32BE(data);data+=4;
  ((uint32_t*)(bg_vs->ci->global_header))[0] = GAVL_PTR_2_32BE(data);data+=4;
  
  /* h263 hack */
  tmp = GAVL_PTR_2_32BE(data);data+=4;
  ((uint32_t*)(bg_vs->ci->global_header))[1] = tmp;
  
  rm_vs->sub_id = tmp;
  
  switch(tmp)
    {
    case 0x10000000:
      /* sub id: 0 */
      /* codec id: rv10 */
      break;
    case 0x10003000:
    case 0x10003001:
      /* sub id: 3 */
      /* codec id: rv10 */
      bg_vs->fourcc = BGAV_MK_FOURCC('R', 'V', '1', '3');
      break;
    case 0x20001000:
    case 0x20100001:
    case 0x20200002:
      /* codec id: rv20 */
      rm_vs->set_keyframe = set_keyframe_rv2;
      break;
    case 0x30202002:
      /* codec id: rv30 */
      break;
    case 0x40000000:
    case 0x40002000:
      /* codec id: rv40 */
      break;
    default:
      /* codec id: none */
      break;
    }

  version = ((bg_vs->fourcc & 0x000000FF) - '0') +
    (((bg_vs->fourcc & 0x0000FF00) >> 8)- '0') * 10;

  if((version < 30) && (tmp>=0x20200002))
    {
    int cnt = len - (data - _data);

    if(cnt < 2)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "Video extradata too short: %d", cnt);
      }
    else
      {
      int ii;
      if(cnt > 6)
        {
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                  "Video extradata too long: %d", cnt);
        cnt = 6;
        }
      //      memcpy(bg_vs->ci->global_header + 8, data, cnt);
      for (ii = 0; ii < cnt; ii++)
        ((unsigned char*)bg_vs->ci->global_header)[8+ii]=
          (unsigned short)data[ii];
      }
    }
#endif
  
  bg_vs->stream_id = stream->mdpr.stream_number;
  rm_vs->com.stream = stream;
  
  if(priv->is_multirate)
    {
    if(!get_multirate_offsets(ctx, stream, &rm_vs->com.data_start,
                              &rm_vs->com.data_end))
      {
      }
    else
      {
      rm_vs->com.data_pos = rm_vs->com.data_start;
      }
    }

  }


static int get_multirate_offsets(bgav_demuxer_context_t * ctx,
                                 bgav_rmff_stream_t * stream, uint32_t * data_start,
                                 uint32_t * data_end)
  {
  bgav_rmff_chunk_t chunk;
  bgav_rmff_data_header_t data_header;
  
  uint32_t fourcc;
  int64_t old_pos;
  rm_private_t * priv;
  int i, j;
  *data_start = 0;

  old_pos = ctx->input->position;
    
  priv = ctx->priv;
  
  /* Look in each logical stream description for the stream number */
  for(i = 0; i < priv->header->num_streams; i++)
    {
    if(priv->header->streams[i].mdpr.is_logical_stream)
      {
      for(j = 0; j < priv->header->streams[i].mdpr.logical_stream.num_physical_streams; j++)
        {
        if(priv->header->streams[i].mdpr.logical_stream.physical_stream_numbers[j] ==
           stream->mdpr.stream_number)
          {
          *data_start = priv->header->streams[i].mdpr.logical_stream.data_offsets[j];
          break;
          }
        }
      }
    }
  if(*data_start) /* Got it */
    {
    bgav_input_seek(ctx->input, *data_start, SEEK_SET);
    }
  else
    {
    if(stream->has_indx) /* Try index */
      {
      /* 18 is the size of a version 0 data chunk header */
      *data_start = stream->indx.records[0].offset - 18;
      if(*data_start < 0)
        return 0;

      bgav_input_seek(ctx->input, *data_start, SEEK_SET);
      while(1)
        {
        if(!bgav_input_get_fourcc(ctx->input, &fourcc))
          return 0;

        if(fourcc == BGAV_MK_FOURCC('D', 'A', 'T', 'A'))
          break;
        else
          {
          (*data_start)--;
          if(*data_start < 0)
            return 0;
          bgav_input_seek(ctx->input, *data_start, SEEK_SET);
          }
        }
      }
    else
      return 0;
    }

  /* Try to read chunk */
  if(!bgav_rmff_chunk_header_read(&chunk, ctx->input))
    return 0;
  if(chunk.id != BGAV_MK_FOURCC('D', 'A', 'T', 'A'))
    return 0;
  if(!bgav_rmff_data_header_read(ctx->input, &data_header))
    return 0;

  *data_start = ctx->input->position;
  *data_end = *data_start + chunk.size - (ctx->input->position - chunk.start_position);
    
  if(stream->has_indx)
    *data_start = stream->indx.records[0].offset;
  
  bgav_input_seek(ctx->input, old_pos, SEEK_SET);
  
  return 1;
  }



int bgav_demux_rm_open_with_header(bgav_demuxer_context_t * ctx,
                                   bgav_rmff_header_t * h)
  {
  rm_private_t * priv;
  uint8_t * pos;
  int i, j;
  bgav_rmff_mdpr_t * mdpr;
  uint32_t header = 0;
  bgav_track_t * track;
  
  bgav_charset_converter_t * cnv;

  //  bgav_rmff_header_dump(h);
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  priv->header = h;
  
  /* Create track */
  ctx->tt = bgav_track_table_create(1);
  track = ctx->tt->cur;
  
  /* Create streams */

  for(i = 0; i < h->num_streams; i++)
    {
    mdpr = &h->streams[i].mdpr;

    if(mdpr->is_logical_stream)
      {
      if(!(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Cannot play multirate real from non seekable source");
        return 0;
        }
      else
        {
        priv->is_multirate = 1;
        gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN,
                 "Detected multirate real");
        }
      continue;
      }

    if(mdpr->type_specific_len)
      {
      j = mdpr->type_specific_len - 4;
      pos = mdpr->type_specific_data;
      while(j)
        {
        header = BGAV_PTR_2_FOURCC(pos);
        if((header == BGAV_MK_FOURCC('.', 'r', 'a', 0xfd)) ||
           (header == BGAV_MK_FOURCC('V', 'I', 'D', 'O')))
          break;
        pos++;
        j--;
        }
      if(header == BGAV_MK_FOURCC('.', 'r', 'a', 0xfd))
        {
        init_audio_stream(ctx, &h->streams[i], pos);
        }
      else if(header == BGAV_MK_FOURCC('V', 'I', 'D', 'O'))
        {
        init_video_stream(ctx, &h->streams[i], pos,
                          mdpr->type_specific_len - (int)(pos - mdpr->type_specific_data));
        }
      }
    else if(!strcmp(mdpr->mime_type, "audio/X-MP3-draft-00"))
      {
      init_audio_stream_mp3(ctx, &h->streams[i]);
      }
    
    }
  
  /* Update global fields */

  if(h->prop.duration)
    gavl_track_set_duration(track->info, h->prop.duration * (GAVL_TIME_SCALE / 1000));
  
  priv->need_first_timestamp = 1;
  
  if(ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE)
    {
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
    for(i = 0; i < track->num_audio_streams; i++)
      {
      bgav_stream_t * s;
      s = bgav_track_get_audio_stream(ctx->tt->cur, i);
      
      if(!((rm_stream_t*)(s->priv))->stream->has_indx)
        ctx->flags &= ~BGAV_DEMUXER_CAN_SEEK;
      }
    for(i = 0; i < track->num_video_streams; i++)
      {
      bgav_stream_t * s;
      s = bgav_track_get_video_stream(ctx->tt->cur, i);
      if(!((rm_stream_t*)(s->priv))->stream->has_indx)
        ctx->flags &= ~BGAV_DEMUXER_CAN_SEEK;
      }
    }

  bgav_track_set_format(ctx->tt->cur, "RM", "application/vnd.rn-realmedia");
  
  /* Handle metadata */

  cnv = bgav_charset_converter_create(ctx->opt, "ISO-8859-1", BGAV_UTF8);

  if(priv->header->cont.title_len)
    gavl_dictionary_set_string_nocopy(track->metadata,
                            GAVL_META_TITLE,
                            bgav_convert_string(cnv,
                                                priv->header->cont.title,
                                                priv->header->cont.title_len,
                                                NULL));
  if(priv->header->cont.author_len)
    gavl_dictionary_set_string_nocopy(track->metadata,
                            GAVL_META_AUTHOR,
                            bgav_convert_string(cnv,
                                                priv->header->cont.author,
                                                priv->header->cont.author_len,
                                                NULL));
  
  if(priv->header->cont.copyright_len)
    gavl_dictionary_set_string_nocopy(track->metadata,
                            GAVL_META_COPYRIGHT,
                            bgav_convert_string(cnv,
                                                priv->header->cont.copyright,
                                                priv->header->cont.copyright_len,
                                                NULL));
  
  if(priv->header->cont.comment_len)
    gavl_dictionary_set_string_nocopy(track->metadata,
                            GAVL_META_COMMENT,
                            bgav_convert_string(cnv,
                                                priv->header->cont.comment,
                                                priv->header->cont.comment_len,
                                                NULL));
  bgav_charset_converter_destroy(cnv);

  ctx->data_start = ctx->input->position;
  ctx->flags |= BGAV_DEMUXER_HAS_DATA_START;
  
  return 1;
  }

static int open_rmff(bgav_demuxer_context_t * ctx)
  {
  bgav_rmff_header_t * h =
    bgav_rmff_header_read(ctx->input);
    
  if(!h)
    return 0;
  
  return bgav_demux_rm_open_with_header(ctx, h);
  }
  

static int probe_rmff(bgav_input_context_t * input)
  {
  uint32_t header;

  if(!bgav_input_get_fourcc(input, &header))
    return 0;
  
  if(header == BGAV_MK_FOURCC('.', 'R', 'M', 'F'))
    {
    return 1;
    }
  return 0;
  }

#define READ_8(b) if(!bgav_input_read_8(ctx->input,&b))return 0;len--
#define READ_16(b) if(!bgav_input_read_16_be(ctx->input,&b))return 0;len-=2

typedef struct dp_hdr_s {
    uint32_t chunks;    // number of chunks
    uint32_t timestamp; // timestamp from packet header
    uint32_t len;       // length of actual data
    uint32_t chunktab;  // offset to chunk offset array
} dp_hdr_t;


#if 0

#define SKIP_BITS(n) buffer<<=n
#define SHOW_BITS(n) ((buffer)>>(32-(n)))

static int64_t
fix_timestamp(bgav_stream_t * stream, uint8_t * s, uint32_t timestamp, bgav_packet_t * p)
  {
  uint32_t buffer= (s[0]<<24) + (s[1]<<16) + (s[2]<<8) + s[3];
  int kf=timestamp;
  int pict_type;
  int orig_kf;
  rm_video_stream_t * priv = stream->priv;
#if 1
  if(stream->fourcc==BGAV_MK_FOURCC('R','V','3','0') ||
     stream->fourcc==BGAV_MK_FOURCC('R','V','4','0'))
    {
    if(stream->fourcc==BGAV_MK_FOURCC('R','V','3','0'))
      {
      SKIP_BITS(3);
      pict_type= SHOW_BITS(2);
      SKIP_BITS(2 + 7);
      }
    else
      {
      SKIP_BITS(1);
      pict_type= SHOW_BITS(2);
      SKIP_BITS(2 + 7 + 3);
      }
    orig_kf=kf= SHOW_BITS(13);  //    kf= 2*SHOW_BITS(12);
    //    if(pict_type==0)
    if(pict_type<=1)
      {
      // I frame, sync timestamps:
      priv->kf_base=timestamp-kf;
      kf=timestamp;
      PACKET_SET_KEYFRAME(p);
      }
    else
      {
      // P/B frame, merge timestamps:
      int tmp=timestamp-priv->kf_base;
      kf|=tmp & (~0x1fff);        // combine with packet timestamp
      if(kf<tmp-4096) kf+=8192; else // workaround wrap-around problems
      if(kf>tmp+4096) kf-=8192;
      kf+=priv->kf_base;
      }
    if(pict_type != 3)
      { // P || I  frame -> swap timestamps
      int tmp=kf;
      kf=priv->kf_pts;
      priv->kf_pts=tmp;
      //      if(kf<=tmp) kf=0;
      }
    //    if(verbose>1) printf("\nTS: %08X -> %08X (%04X) %d %02X %02X %02X %02X %5d\n",timestamp,kf,orig_kf,pict_type,s[0],s[1],s[2],s[3],kf-(int)(1000.0*priv->v_pts));
    }
#endif
  return (int64_t)kf;
  }

#endif

#define PAYLOAD_LENGTH(HEADER) ((HEADER)->length - (((HEADER)->object_version == 0) ? 12 : 13))

#define IS_KEYFRAME(HEADER) \
  (((HEADER)->object_version == 0) ? ((HEADER)->flags & 0x02) : ((HEADER)->asm_flags & 0x02))


static int get_num(bgav_input_context_t * ctx, int * len, int * ret)

  {
  uint16_t n, n1;

  if(!bgav_input_read_16_be(ctx,&n))
    return 0;
  
  (*len) -= 2;
  
  n &= 0x7FFF;
  if(n >= 0x4000)
    {
    *ret = n - 0x4000;
    }
  else
    {
    if(!bgav_input_read_16_be(ctx,&n1))
      return 0;
    
    (*len) -= 2;
    
    *ret = (n << 16) | n1;
    }
  return 1;
  }

static void set_vpacket_flags(bgav_stream_t * s, int len,
                              bgav_rmff_packet_header_t * h,
                              int chunk_type, int pos)
  {
  frame_info_t fi;
  rm_video_stream_t * sp = s->priv;

  memset(&fi, 0, sizeof(fi));
  sp->parse_frame_info(s->packet->buf.buf + 9, len, &fi, sp->sub_id);
  //  dump_frame_info(&fi);
  
  /* Set picture type and keyframe flag */
  PACKET_SET_CODING_TYPE(s->packet, fi.pict_type);
  
  if(fi.pict_type == BGAV_CODING_TYPE_I)
    PACKET_SET_KEYFRAME(s->packet);

  /* Set pts */

  if(fi.pts >= 0)
    {
    if(fi.pict_type == BGAV_CODING_TYPE_I)
      {
      s->packet->pts = h->timestamp;
      sp->pts_offset = (int64_t)h->timestamp - (int64_t)fi.pts;
      }
    else
      {
      s->packet->pts = sp->pts_offset + fi.pts;

      if(s->packet->pts - sp->last_pts < -4096)
        {
        s->packet->pts         += 8192;
        sp->pts_offset += 8192;
        }
      else if(s->packet->pts - sp->last_pts > 4096)
        s->packet->pts         -= 8192;
      }
    sp->last_pts = s->packet->pts;
    }
  else
    {
    if(chunk_type == 3)
      s->packet->pts = pos;
    else
      s->packet->pts = h->timestamp;
    }
  }

static int process_video_chunk(bgav_demuxer_context_t * ctx,
                               bgav_rmff_packet_header_t * h,
                               bgav_stream_t * s)
  {
  uint8_t hdr, seq;
  int type;
  int len = PAYLOAD_LENGTH(h);
  int len2 = 0, pos = 0;
  uint8_t pic_num = 0;
  rm_video_stream_t * sp = s->priv;
  int num_chunks = 0;
  
  //  bgav_rmff_packet_header_dump(h);
  
  while(len >= 1)
    {
  
    READ_8(hdr);

    /*
     *  Type:
     *  0 Whole slice in one packet
     *  1 Whole frame in one packet
     *  2 Slice as part of packet
     *  3 Frame as part of a packet
     */
  
    type = hdr >> 6;

    //    fprintf(stderr, "  Type: %d ", type);
  
    if(type != 3) // not frame as a part of packet
      {
      READ_8(seq);
      //      fprintf(stderr, "Seq: %d ", seq);
    
      }
    if(type != 1) // not whole frame
      {
      if(!get_num(ctx->input, &len, &len2) ||
         !get_num(ctx->input, &len, &pos))
        return 0;
      READ_8(pic_num);
      //      fprintf(stderr, "len2: %d, pos: %d, pic_num: %d", len2, pos, pic_num);
      }
    //    fprintf(stderr, "\n");
    
    if(len <= 0)
      return 0;

    if(type & 1) // Frame (not slice)
      {
      int packet_size, frame_size;
      
      if(s->packet)
        bgav_stream_done_packet_write(s, s->packet);
      s->packet = bgav_stream_get_packet_write(s);

      if(!num_chunks)
        s->packet->pts = h->timestamp;
      
      if(type == 3)
        frame_size = len2;
      else
        frame_size = len;
      
      /* 1 byte slice count + 8 bytes per slice */
      packet_size = frame_size + 1 + 8;
    
      bgav_packet_alloc(s->packet, packet_size);

      if(bgav_input_read_data(ctx->input, s->packet->buf.buf + 9, frame_size) < frame_size)
        return 0;
      
      s->packet->buf.buf[0] = 0;
      GAVL_32LE_2_PTR(1, s->packet->buf.buf+1);
      GAVL_32LE_2_PTR(0, s->packet->buf.buf+5);
      s->packet->buf.len = packet_size;
      
      len -= frame_size;

      set_vpacket_flags(s, frame_size, h, type, pos);
      }
    else // Slices
      {
      int bytes_to_read = len;
      
      if((type == 2) && (bytes_to_read > pos))
        bytes_to_read = pos;
      
      if(sp->pic_num != pic_num) /* New picture started */
        {
        if(s->packet)
          bgav_stream_done_packet_write(s, s->packet);
        s->packet = bgav_stream_get_packet_write(s);

        if(!num_chunks)
          s->packet->pts = h->timestamp;
        
        bgav_packet_alloc(s->packet, bytes_to_read + 9);

        s->packet->buf.buf[0] = 0;
        GAVL_32LE_2_PTR(1, s->packet->buf.buf+1);
        GAVL_32LE_2_PTR(0, s->packet->buf.buf+5);
        
        if(bgav_input_read_data(ctx->input, s->packet->buf.buf + 9, bytes_to_read) < bytes_to_read)
          return 0;
        
        s->packet->buf.len = bytes_to_read + 9;
        sp->num_slices = 1;

        // fprintf(stderr, "slice: %d (%d bytes)\n", sp->num_slices, bytes_to_read);
        sp->pic_num = pic_num;
        
        set_vpacket_flags(s, bytes_to_read, h, type, pos);
        }
      else /* Append slice */
        {
        bgav_packet_alloc(s->packet, s->packet->buf.len + bytes_to_read + 8);

        /* Move payload up by 8 bytes */
        memmove(s->packet->buf.buf + 1 + (sp->num_slices+1) * 8,
                s->packet->buf.buf + 1 + sp->num_slices * 8,
                s->packet->buf.len - (1 + sp->num_slices * 8));

        GAVL_32LE_2_PTR(1, s->packet->buf.buf + 1 + 8 * sp->num_slices);
        GAVL_32LE_2_PTR(s->packet->buf.len - (1 + sp->num_slices * 8),
                        s->packet->buf.buf + 1 + 8 * sp->num_slices + 4);
        s->packet->buf.len += 8;

        if(bgav_input_read_data(ctx->input,
                                s->packet->buf.buf + s->packet->buf.len, bytes_to_read) < bytes_to_read)
          return 0;
        
        sp->num_slices++;
        s->packet->buf.buf[0] += 1;
        s->packet->buf.len += bytes_to_read;
        
        // fprintf(stderr, "slice: %d (%d bytes)\n", sp->num_slices, bytes_to_read);
        }
      len -= bytes_to_read;
      
      // s->packet_seq = seq;

      
      }
    num_chunks++;
    }
  
  return 1; 
  
  }

static const unsigned char sipr_swaps[38][2]={
    {0,63},{1,22},{2,44},{3,90},{5,81},{7,31},{8,86},{9,58},{10,36},{12,68},
    {13,39},{14,73},{15,53},{16,69},{17,57},{19,88},{20,34},{21,71},{24,46},
    {25,94},{26,54},{28,75},{29,50},{32,70},{33,92},{35,74},{38,85},{40,56},
    {42,87},{43,65},{45,59},{48,79},{49,93},{51,89},{55,95},{61,76},{67,83},
    {77,80} };

static int process_audio_chunk(bgav_demuxer_context_t * ctx,
                               bgav_rmff_packet_header_t * h,
                               bgav_stream_t * stream)
  {
  bgav_packet_t * p;
  //  int bytes_to_read;
  int packet_size;
  rm_audio_stream_t * as;
  int x, sps, cfs, sph, spc, w;
  uint8_t swp;

  uint16_t aac_packet_lengths[16];
  uint8_t num_aac_packets;
  
  as = stream->priv;
  
  packet_size = PAYLOAD_LENGTH(h);

  if((stream->fourcc == BGAV_MK_FOURCC('2', '8', '_', '8')) ||
     (stream->fourcc == BGAV_MK_FOURCC('c', 'o', 'o', 'k')) ||
     (stream->fourcc == BGAV_MK_FOURCC('a', 't', 'r', 'c')) ||
     (stream->fourcc == BGAV_MK_FOURCC('s', 'i', 'p', 'r')))
    {
    sps = as->sub_packet_size;
    sph = as->sub_packet_h;
    cfs = as->coded_framesize;
    w   = as->audiopk_size;
    spc = as->sub_packet_cnt;

    if(IS_KEYFRAME(h))
      as->sub_packet_cnt = 0;
    
    switch(stream->fourcc)
      {
      case BGAV_MK_FOURCC('2', '8', '_', '8'):
        for (x = 0; x < sph / 2; x++)
          bgav_input_read_data(ctx->input, as->audio_buf + x * 2 * w + spc * cfs, cfs);
        break;
      case BGAV_MK_FOURCC('c', 'o', 'o', 'k'):
      case BGAV_MK_FOURCC('a', 't', 'r', 'c'):
        for (x = 0; x < w / sps; x++)
          bgav_input_read_data(ctx->input,
                               as->audio_buf + sps * (sph * x + ((sph + 1) / 2) * (spc & 1) +
                                                      (spc >> 1)),
                               sps);
        break;
      case BGAV_MK_FOURCC( 's', 'i', 'p', 'r'):
        bgav_input_read_data(ctx->input, as->audio_buf + spc * w, w);
        if (spc == sph - 1)
          {
          int n;
          int bs = sph * w * 2 / 96;  // nibbles per subpacket
          // Perform reordering
          for(n=0; n < 38; n++)
            {
            int j;
            int i = bs * sipr_swaps[n][0];
            int o = bs * sipr_swaps[n][1];
            // swap nibbles of block 'i' with 'o'      TODO: optimize
            for(j = 0;j < bs; j++)
              {
              int x = (i & 1) ? (as->audio_buf[i >> 1] >> 4) : (as->audio_buf[i >> 1] & 0x0F);
              int y = (o & 1) ? (as->audio_buf[o >> 1] >> 4) : (as->audio_buf[o >> 1] & 0x0F);
              if(o & 1)
                as->audio_buf[o >> 1] = (as->audio_buf[o >> 1] & 0x0F) | (x << 4);
              else
                as->audio_buf[o >> 1] = (as->audio_buf[o >> 1] & 0xF0) | x;
              if(i & 1)
              as->audio_buf[i >> 1] = (as->audio_buf[i >> 1] & 0x0F) | (y << 4);
              else
                as->audio_buf[i >> 1] = (as->audio_buf[i >> 1] & 0xF0) | y;
              ++i; ++o;
              }
            }
          }
        break;
      }
    /* Release all packets */
    
    if(++(as->sub_packet_cnt) >= sph)
      {
      int apk_usize = stream->data.audio.block_align;
      as->sub_packet_cnt = 0;
    
      for (x = 0; x < sph*w/apk_usize; x++)
        {
        p = bgav_stream_get_packet_write(stream);
        bgav_packet_alloc(p, apk_usize);
        p->buf.len = apk_usize;
        memcpy(p->buf.buf, as->audio_buf + x * apk_usize, apk_usize);
        if(!x)
          PACKET_SET_KEYFRAME(p);
        bgav_stream_done_packet_write(stream, p);
        }
      }
    }
  /* Byte swapped AC3 */
  else if(stream->fourcc == BGAV_MK_FOURCC('d', 'n', 'e', 't')) 
    {
    p = bgav_stream_get_packet_write(stream);
    bgav_packet_alloc(p, packet_size);
    if(bgav_input_read_data(ctx->input, p->buf.buf, packet_size) < packet_size)
      return 0;

    for(x = 0; x < packet_size; x += 2)
      {
      swp = p->buf.buf[x];
      p->buf.buf[x] = p->buf.buf[x+1];
      p->buf.buf[x+1] = swp;
      }
    
    p->buf.len = packet_size;
    bgav_stream_done_packet_write(stream, p);
    }
  else if((stream->fourcc == BGAV_MK_FOURCC('r', 'a', 'a', 'c')) ||
          (stream->fourcc == BGAV_MK_FOURCC('r', 'a', 'c', 'p')))
    {
    bgav_input_skip(ctx->input, 1);
    if(!bgav_input_read_data(ctx->input, &num_aac_packets, 1))
      return 0;
    num_aac_packets >>= 4;
    for(x = 0; x < num_aac_packets; x++)
      {
      if(!bgav_input_read_16_be(ctx->input, &aac_packet_lengths[x]))
        return 0;
      }
    for(x = 0; x < num_aac_packets; x++)
      {
      p = bgav_stream_get_packet_write(stream);
      bgav_packet_alloc(p, packet_size);
      
      bgav_input_read_data(ctx->input, p->buf.buf, aac_packet_lengths[x]);
      p->buf.len = aac_packet_lengths[x];
      bgav_stream_done_packet_write(stream, p);
      }
    }
  else /* No reordering needed */
    {
    p = bgav_stream_get_packet_write(stream);
    bgav_packet_alloc(p, packet_size);
    
    bgav_input_read_data(ctx->input, p->buf.buf, packet_size);
    p->buf.len = packet_size;
    bgav_stream_done_packet_write(stream, p);

    
    }
  
  return 1;
  }

static gavl_source_status_t next_packet_rmff(bgav_demuxer_context_t * ctx)
  {
  //  bgav_packet_t * p;
  bgav_stream_t * stream = NULL;
  rm_private_t * rm;
  bgav_rmff_packet_header_t h;
  rm_stream_t * rs = NULL;
  int result = 0;
  uint32_t stream_pos = 0;
  int i;
  
  rm = ctx->priv;

  if(!rm->is_multirate)
    {
    if(rm->header->data_size && (ctx->input->position + 10 >=
                                 rm->header->data_start + rm->header->data_size))
      {
      return GAVL_SOURCE_EOF;
      }
    if(!bgav_rmff_packet_header_read(ctx->input, &h))
      {
      return GAVL_SOURCE_EOF;
      }
  
    if(rm->need_first_timestamp)
      {
      rm->first_timestamp = h.timestamp;
      rm->need_first_timestamp = 0;
      }
    if(h.timestamp > rm->first_timestamp)
      h.timestamp -= rm->first_timestamp;
    else
      h.timestamp = 0;

    stream = bgav_track_find_stream(ctx, h.stream_number);

    if(!stream) /* Skip unknown stuff */
      {
      bgav_input_skip(ctx->input, PAYLOAD_LENGTH(&h));
      return GAVL_SOURCE_OK;
      }
    }
  else /* multirate */
    {
    if(ctx->request_stream)
      {
      stream = ctx->request_stream;

      rs = stream->priv;

      if(rs->data_pos >= rs->data_end)
        return GAVL_SOURCE_EOF;
      else
        stream_pos = rs->data_pos;
      }
    else
      {
      /* Find the first stream with empty packetbuffer */
      for(i = 0; i < ctx->tt->cur->num_audio_streams; i++)
        {
        stream = bgav_track_get_audio_stream(ctx->tt->cur, i);
        rs = stream->priv;
        
        if((stream->action == BGAV_STREAM_MUTE) ||
           !bgav_packet_buffer_is_empty(stream->packet_buffer) ||
           rs->data_pos >= rs->data_end)
          {
          stream = NULL;
          }
        else
          {
          stream_pos = rs->data_pos;
          break;
          }
        }
      if(!stream)
        {
        for(i = 0; i < ctx->tt->cur->num_video_streams; i++)
          {
          stream = bgav_track_get_video_stream(ctx->tt->cur, i);
          rs = stream->priv;
          
          if((stream->action == BGAV_STREAM_MUTE) ||
             !bgav_packet_buffer_is_empty(stream->packet_buffer) ||
             rs->data_pos >= rs->data_end)
            {
            stream = NULL;
            }
          else
            {
            stream_pos = rs->data_pos;
            break;
            }
          }
        }
      }
    
    /* If we have no more stream, eof is reached */
    if(!stream)
      return GAVL_SOURCE_EOF;

    /* Seek to the place we've been before */
    bgav_input_seek(ctx->input, stream_pos, SEEK_SET);

    while(1)
      {
      if(!bgav_rmff_packet_header_read(ctx->input, &h))
        {
        return GAVL_SOURCE_EOF;
        }
      //      bgav_rmff_packet_header_dump(&h);
      if(stream->stream_id == h.stream_number)
        break;
      else if(rs->data_start == rs->data_pos)
        { 
        /* If this is the first packet in the stream and
           the stream id of the first packet is different,
           this means, that these packets are meant for us */
        stream->stream_id = h.stream_number;
        break;
        }
      else
        {
        bgav_input_skip(ctx->input, PAYLOAD_LENGTH(&h));
        }
      }
    }
  
  if(stream->type == GAVL_STREAM_VIDEO)
    {
    rs = stream->priv;
    if(rm->do_seek)
      {
      if(rs->stream->indx.records[rs->index_record].packet_count_for_this_packet > rm->next_packet)
        {
        bgav_input_skip(ctx->input, PAYLOAD_LENGTH(&h));
        result = 1;
        }
      else
        result = process_video_chunk(ctx, &h, stream);
      }
    else
      result = process_video_chunk(ctx, &h, stream);
    rs->data_pos = ctx->input->position;
    }
  else if(stream->type == GAVL_STREAM_AUDIO)
    {
    rs = stream->priv;
    if(rm->do_seek)
      {
      if(rs->stream->indx.records[rs->index_record].packet_count_for_this_packet > rm->next_packet)
        {
        bgav_input_skip(ctx->input, PAYLOAD_LENGTH(&h));
        result = 1;
        }
      else
        result = process_audio_chunk(ctx, &h, stream);
      }
    else
      result = process_audio_chunk(ctx, &h, stream);
    rs->data_pos = ctx->input->position;
    }
  rm->next_packet++;
  return result ? GAVL_SOURCE_OK : GAVL_SOURCE_EOF;
  }

static void seek_rmff(bgav_demuxer_context_t * ctx, int64_t time, int scale)
  {
  uint32_t i;
  bgav_stream_t * stream;
  rm_stream_t * rs;
  rm_video_stream_t * vs;
  rm_private_t * rm;
  bgav_track_t * track;
  uint32_t real_time; /* We'll never be more accurate than milliseconds */
  uint32_t position = ~0x0;

  uint32_t start_packet = ~0x00;
  uint32_t end_packet =    0x00;
  
  
  rm = ctx->priv;
  track = ctx->tt->cur;
  
  real_time = gavl_time_rescale(scale, 1000, time);
  
  /* First step: Seek the index records for each stream */

  /* Seek the pointers for the index records for each stream */
  /* We also calculate the file position where we will start again */
  
  for(i = 0; i < track->num_video_streams; i++)
    {
    stream = bgav_track_get_video_stream(track, i);
    vs = stream->priv;
    vs->com.index_record = seek_indx(&vs->com.stream->indx, real_time,
                                 &position, &start_packet, &end_packet);
    STREAM_SET_SYNC(stream, vs->com.stream->indx.records[vs->com.index_record].timestamp);
    // vs->kf_pts = vs->com.stream->indx.records[vs->com.index_record].timestamp;
    vs->com.data_pos = vs->com.stream->indx.records[vs->com.index_record].offset;
    vs->pic_num = -1;
    }
  for(i = 0; i < track->num_audio_streams; i++)
    {
    stream = bgav_track_get_audio_stream(track, i);
    rs = stream->priv;
    rs->index_record = seek_indx(&rs->stream->indx, real_time,
                                 &position, &start_packet, &end_packet);
    STREAM_SET_SYNC(stream, rs->stream->indx.records[rs->index_record].timestamp);
    rs->data_pos = rs->stream->indx.records[rs->index_record].offset;
    }
  
  /* Seek to the position */

  if(!rm->is_multirate)
    {
    bgav_input_seek(ctx->input, position, SEEK_SET);

    /* Skip unused packets */
    
    rm->do_seek = 1;
    rm->next_packet = start_packet;
    while(rm->next_packet < end_packet)
      next_packet_rmff(ctx);
    rm->do_seek = 0;
    }
  }


static void close_rmff(bgav_demuxer_context_t * ctx)
  {
  rm_private_t * priv;
  priv = ctx->priv;
  
  if(priv)
    {
    if(priv->header)
      bgav_rmff_header_destroy(priv->header);
    free(priv);
    }
  }

static int select_track_rmff(bgav_demuxer_context_t * ctx, int t)
  {
  bgav_stream_t * s;
  rm_audio_stream_t * as;
  rm_video_stream_t * vs;
  rm_private_t * priv;
  bgav_track_t * track;
  int i;
  priv = ctx->priv;
  
  priv->next_packet = 0;
  
  track = ctx->tt->cur;

#if 1
  
  for(i = 0; i < track->num_audio_streams; i++)
    {
    s = bgav_track_get_audio_stream(track, i);
    as = s->priv;
    if(as)
      {
      as->sub_packet_cnt = 0;
      as->com.data_pos = as->com.data_start;
      }
    }
  for(i = 0; i < track->num_video_streams; i++)
    {
    s = bgav_track_get_video_stream(track, i);
    vs = s->priv;
    if(vs)
      {
      //      vs->kf_pts = 0;
      //      vs->kf_base = 0;
      vs->com.data_pos = vs->com.data_start;
      vs->pic_num = -1;
      }
    }
#endif
  return 1;
  }


const bgav_demuxer_t bgav_demuxer_rmff =
  {
    .probe =        probe_rmff,
    .open =         open_rmff,
    .select_track = select_track_rmff,
    .next_packet =  next_packet_rmff,
    .seek =         seek_rmff,
    .close =        close_rmff
  };
