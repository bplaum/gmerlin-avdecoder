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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <avdec_private.h>
#include <nanosoft.h>

#define AUDIO_ERROR_CONCEAL_NONE       0
#define AUDIO_ERROR_CONCEAL_INTERLEAVE 1

#define ASF_TIME_SCALE 1000

#define LOG_DOMAIN "asf"

/* Copied from avifile CVS */

/* base ASF objects */
static const bgav_GUID_t guid_header = 
  { 0x75b22630, 0x668e, 0x11cf,
    { 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c } };

static const bgav_GUID_t guid_data = 
  { 0x75b22636, 0x668e, 0x11cf,
    { 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c } };

#if 0
static const bgav_GUID_t guid_simple_index = 
  { 0x33000890, 0xe5b1, 0x11cf,
    { 0x89, 0xf4, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xcb } };
#endif

/* header ASF objects */
static const bgav_GUID_t guid_file_properties = 
  { 0x8cabdca1, 0xa947, 0x11cf,
    { 0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65 } };

static const bgav_GUID_t guid_stream_header = 
  { 0xb7dc0791, 0xa9b7, 0x11cf,
    { 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65 } };

static const bgav_GUID_t guid_stream_bitrate_properties =  /* (http://get.to/sdp) */
  { 0x7bf875ce, 0x468d, 0x11d1,
      { 0x8d, 0x82, 0x00, 0x60, 0x97, 0xc9, 0xa2, 0xb2 } };

static const bgav_GUID_t guid_comment_header =
  { 0x75b22633, 0x668e, 0x11cf,
    { 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c } };

/* stream properties object stream type */
static const bgav_GUID_t guid_audio_media = 
  { 0xf8699e40, 0x5b4d, 0x11cf,
    { 0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b } };

static const bgav_GUID_t guid_video_media = 
  { 0xbc19efc0, 0x5b4d, 0x11cf,
    { 0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b } };

/* GUID indicating that interleaved audio error concealment is present */
static const bgav_GUID_t guid_audio_conceal_interleave =
  { 0xbfc3cd50, 0x618f, 0x11cf,
    { 0x8b, 0xb2, 0x00, 0xaa, 0x00, 0xb4, 0xe2, 0x20 } };

#if 0
static const bgav_GUID_t guid_content_description = 
  { 0x75b22633, 0x668e, 0x11cf,
    { 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c } };


static const bgav_GUID_t guid_extended_content_encryption = 
  { 0x298ae614, 0x2622, 0x4c17,
    { 0xb9, 0x35, 0xda, 0xe0, 0x7e, 0xe9, 0x28, 0x9c } };


static const bgav_GUID_t guid_script_command = 
  { 0x1efb1a30, 0x0b62, 0x11d0,
    { 0xa3, 0x9b, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6 } };


static const bgav_GUID_t guid_marker = 
  { 0xf487cd01, 0xa951, 0x11cf,
    { 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65 } };


static const bgav_GUID_t guid_header_extension = 
  { 0x5fbf03b5, 0xa92e, 0x11cf,
    { 0x8e, 0xe3, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65 } };

static const bgav_GUID_t guid_bitrate_mutual_exclusion = 
  { 0xd6e229dc, 0x35da, 0x11d1,
    { 0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe } };


static const bgav_GUID_t guid_codec_list = 
  { 0x86d15240, 0x311d, 0x11d0,
    { 0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6 } };


static const bgav_GUID_t guid_extended_content_description = 
  { 0xd2d0a440, 0xe307, 0x11d2,
    { 0x97, 0xf0, 0x00, 0xa0, 0xc9, 0x5e, 0xa8, 0x50 } };


static const bgav_GUID_t guid_error_correction = 
  { 0x75b22635, 0x668e, 0x11cf,
    { 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c } };


static const bgav_GUID_t guid_padding = 
  { 0x1806d474, 0xcadf, 0x4509,
    { 0xa4, 0xba, 0x9a, 0xab, 0xcb, 0x96, 0xaa, 0xe8 } };

static const bgav_GUID_t guid_command_media = 
  { 0x59dacfc0, 0x59e6, 0x11d0,
    { 0xa3, 0xac, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6 } };



/* GUID indicating that audio error concealment is absent */
static const bgav_GUID_t guid_audio_conceal_none=
  { 0x49f1a440, 0x4ece, 0x11d0,
    { 0xa3, 0xac, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6 } };


/* stream properties object error correction */
static const bgav_GUID_t guid_no_error_correction = 
  { 0x20fb5700, 0x5b55, 0x11cf,
    { 0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b } };

static const bgav_GUID_t guid_audio_spread = 
  { 0xbfc3cd50, 0x618f, 0x11cf,
    { 0x8b, 0xb2, 0x00, 0xaa, 0x00, 0xb4, 0xe2, 0x20 } };


/* mutual exclusion object exlusion type */
static const bgav_GUID_t guid_mutex_bitrate = 
    { 0xd6e22a01, 0x35da, 0x11d1,
      { 0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe } };

static const bgav_GUID_t guid_mutex_unknown =  
  { 0xd6e22a02, 0x35da, 0x11d1,
    { 0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe } };


/* header extension */
static const bgav_GUID_t guid_reserved_1 = 
  { 0xabd3d211, 0xa9ba, 0x11cf,
    { 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65 } };

/* script command */
static const bgav_GUID_t guid_reserved_script_command = 
  { 0x4B1ACBE3, 0x100B, 0x11D0,
    { 0xA3, 0x9B, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6 } };

/* marker object */
static const bgav_GUID_t guid_reserved_marker = 
  { 0x4CFEDB20, 0x75F6, 0x11CF,
    { 0x9C, 0x0F, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xCB } };

/* various */
static const bgav_GUID_t guid_head2 = 
  { 0xabd3d211, 0xa9ba, 0x11cf,
    { 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65 } };

static const bgav_GUID_t guid_codec_comment1_header = 
  { 0x86d15241, 0x311d, 0x11d0,
    { 0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6 } };

static const bgav_GUID_t guid_asf_2_0_header = 
  { 0xd6e229d1, 0x35da, 0x11d1,
    { 0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe } };
#endif

/* ASF specific stream flags */

typedef struct
  {
  int error_concealment;
  uint8_t  descramble_h;
  uint16_t descramble_w;
  uint16_t descramble_b;
  uint16_t block_align_2;

  uint8_t * scramble_buffer;
  int scramble_buffer_size;
  
  } asf_audio_stream_t;
#if 0
static void dump_audio_stream(asf_audio_stream_t * as)
  {
  gavl_dprintf( "descramble_h: %d\n", as->descramble_h);
  gavl_dprintf( "descramble_w: %d\n", as->descramble_w);
  gavl_dprintf( "descramble_b: %d\n", as->descramble_b);
  }
#endif
static void asf_descramble(asf_audio_stream_t * as,
                    uint8_t * data,
                    int data_size)
  {
  uint8_t * dst;
  unsigned char *s2=data;
  int i=0,x,y;
  if(as->scramble_buffer_size < data_size)
    {
    as->scramble_buffer_size = data_size;
    as->scramble_buffer = realloc(as->scramble_buffer,
                                  as->scramble_buffer_size);
    }
  dst = as->scramble_buffer;
  
  
  while(data_size-i>=as->descramble_h*as->descramble_w*as->descramble_b)
    {
    for(x=0;x<as->descramble_w;x++)
      for(y=0;y<as->descramble_h;y++){
      memcpy(dst+i,s2+(y*as->descramble_w+x)*
             as->descramble_b,as->descramble_b);
      i+=as->descramble_b;
      }
    s2+=as->descramble_h*as->descramble_w*as->descramble_b;
    }
  //if(i<len) memcpy(dst+i,src+i,len-i);
  memcpy(data,dst,i);
  }

#if 0
typedef struct
  {
  int dummy;
  } asf_video_stream_t;
#endif
/* Private data */

typedef struct
  {
  bgav_GUID_t guid;		/* generated by client computer */
  uint64_t file_size;	/* in bytes */
                        /* invalid if broadcasting */
  uint64_t create_time; /* time of creation, in 100-nanosecond units */
                        /* since 1.1.1601 invalid if broadcasting */
  
  uint64_t packets_count; /* how many packets are there in the file */
                          /* invalid if broadcasting                */

  uint64_t play_time; /* play time, in 100-nanosecond units */
                      /* invalid if broadcasting            */

  uint64_t send_time; /* time to send file, in 100-nanosecond units */
                      /* invalid if broadcasting (could be ignored) */

  uint32_t preroll;  /* timestamp of the first packet, in milliseconds */
                     /* if nonzero - substract from time */

  uint32_t ignore;   /* preroll is 64bit - but let's just ignore it */

  uint32_t flags;    /* 0x01 - broadcast */
                     /* 0x02 - seekable  */
                     /* rest is reserved should be 0 */

  uint32_t min_pktsize;	/* size of a data packet   */
                        /* invalid if broadcasting */
  
  uint32_t max_pktsize;	/* shall be the same as for min_pktsize */
                        /* invalid if broadcasting              */

  uint32_t max_bitrate;	/* bandwith of stream in bps            */
                        /* should be the sum of bitrates of the */
                        /* individual media streams             */
  } asf_main_header_t;

static void cleanup_stream_asf(bgav_stream_t * s)
  {
  asf_audio_stream_t * as;
  if(s->type == GAVL_STREAM_AUDIO)
    {
    as = s->priv;
    
    if(as->scramble_buffer)
      free(as->scramble_buffer);
    if(s->priv)
      free(s->priv);
    }
  else
    {
    if(s->priv)
      free(s->priv);
    }
  }

typedef struct
  {
  uint64_t packets_read;
  asf_main_header_t hdr;
  uint8_t * packet_buffer;
  int64_t data_size;

  //  uint32_t first_timestamp;
  //  int need_first_timestamp;
  
  struct
    {
    int stream_id;
    uint32_t bitrate;
    } * stream_bitrates;
  int num_stream_bitrates;
  } asf_t;

static int probe_asf(bgav_input_context_t * input)
  {
  bgav_GUID_t guid;
  if(!bgav_GUID_get(&guid, input))
    return 0;

  if(bgav_GUID_equal(&guid, &guid_header))
    {
    return 1;
    }
  return 0;
  }

static int read_bitrate_properties(bgav_demuxer_context_t * ctx)
  {
  uint16_t i_tmp;
  int i;
  asf_t * asf;
  asf = ctx->priv;

  if(!bgav_input_read_16_le(ctx->input, &i_tmp))
    return 0;

  asf->num_stream_bitrates = i_tmp;

  if(!asf->num_stream_bitrates)
    {
    return 1;
    }
  asf->stream_bitrates     = calloc(asf->num_stream_bitrates,
                                    sizeof(*(asf->stream_bitrates)));

  for(i = 0; i < asf->num_stream_bitrates; i++)
    {
    if(!bgav_input_read_16_le(ctx->input, &i_tmp) ||
       !bgav_input_read_32_le(ctx->input, &asf->stream_bitrates[i].bitrate))
      return 0;
    asf->stream_bitrates[i].stream_id = i_tmp;
    
    }
  return 1;
  }

static void update_stream_bitrates(bgav_demuxer_context_t * ctx)
  {
  int i;
  bgav_stream_t * stream;
    
  asf_t * asf;
  asf = ctx->priv;

  for(i = 0; i < asf->num_stream_bitrates; i++)
    {
    stream = bgav_track_find_stream(ctx, asf->stream_bitrates[i].stream_id);
    if(stream)
      stream->container_bitrate = asf->stream_bitrates[i].bitrate;
    else
      {
      }
    }
  }


static int read_metadata(bgav_demuxer_context_t * ctx)
  {
  char * str = NULL;
  uint16_t len1, len2, len3, len4, len5;
  int str_len;
  bgav_charset_converter_t * cnv = NULL;
  
  if(!bgav_input_read_16_le(ctx->input, &len1) ||
     !bgav_input_read_16_le(ctx->input, &len2) ||
     !bgav_input_read_16_le(ctx->input, &len3) ||
     !bgav_input_read_16_le(ctx->input, &len4) ||
     !bgav_input_read_16_le(ctx->input, &len5))
    goto fail;

  cnv = bgav_charset_converter_create("UTF-16LE", BGAV_UTF8);
  
  str_len = len1;
  if(str_len < len2)
    str_len = len2;
  if(str_len < len3)
    str_len = len3;
  if(str_len < len4)
    str_len = len4;
  if(str_len < len5)
    str_len = len5;

  str = malloc(str_len);
    
  /* Title */
  
  if(len1)
    {
    if(bgav_input_read_data(ctx->input, (uint8_t*)str, len1) < len1)
      goto fail;
    gavl_dictionary_set_string_nocopy(ctx->tt->cur->metadata,
                            GAVL_META_TITLE, bgav_convert_string(cnv, str, len1,
                                                                 NULL));
    }

  /* Author */
  
  if(len2)
    {
    if(bgav_input_read_data(ctx->input, (uint8_t*)str, len2) < len2)
      goto fail;
    gavl_dictionary_set_string_nocopy(ctx->tt->cur->metadata,
                            GAVL_META_AUTHOR, bgav_convert_string(cnv, str, len2,
                                                                  NULL));
    }

  /* Copyright */
  
  if(len3)
    {
    if(bgav_input_read_data(ctx->input, (uint8_t*)str, len3) < len3)
      goto fail;
    gavl_dictionary_set_string_nocopy(ctx->tt->cur->metadata,
                            GAVL_META_COPYRIGHT, bgav_convert_string(cnv, str, len3,
                                                                     NULL));
    }

  /* Comment */
  
  if(len4)
    {
    if(bgav_input_read_data(ctx->input, (uint8_t*)str, len4) < len4)
      goto fail;
    gavl_dictionary_set_string_nocopy(ctx->tt->cur->metadata,
                            GAVL_META_COMMENT, bgav_convert_string(cnv, str, len4,
                                                                   NULL));
    }

  /* Unknown stuff */

  if(len5)
    bgav_input_skip(ctx->input, len5);
  
  free(str);
  bgav_charset_converter_destroy(cnv);
  return 1;

  fail:
  if(cnv)
    bgav_charset_converter_destroy(cnv);
  if(str)
    free(str);
  return 0;
  }


static int open_asf(bgav_demuxer_context_t * ctx)
  {
  int64_t chunk_start_pos;
  
  bgav_BITMAPINFOHEADER_t bh;
  bgav_WAVEFORMAT_t     wf;
  uint8_t * buf = NULL;
  uint8_t * pos;
  uint16_t stream_number;
  int buf_size = 0;
  bgav_GUID_t guid, guid1;
  uint64_t size;
  uint32_t type_specific_size;
  uint32_t stream_specific_size;
  int error_concealment;
  asf_t * asf;
  asf_audio_stream_t  * asf_as;
  //  asf_video_stream_t  * asf_vs;
  bgav_stream_t * bgav_as;
  bgav_stream_t * bgav_vs;

  /* Create track */
  ctx->tt = bgav_track_table_create(1);
  
  if(!bgav_GUID_read(&guid, ctx->input))
    return 0;
  
  /* Read header */
  
  if(!bgav_GUID_equal(&guid, &guid_header))
    return 0;

  bgav_input_skip(ctx->input, 8); /*  Chunk length UINT64          */
  bgav_input_skip(ctx->input, 4); /*  Number of subchunks UINT32   */
  bgav_input_skip(ctx->input, 2); /*  Unknown                      */

  /* Now the file header should come */
  asf = calloc(1, sizeof(*asf));

  ctx->priv = asf;
  while(1) /* Read all GUIDs until the data comes */
    {
    chunk_start_pos = ctx->input->position;
    if(!bgav_GUID_read(&guid, ctx->input) ||
       !bgav_input_read_64_le(ctx->input, &size))
      goto fail;
    
    /* Overall file properties */
    
    if(bgav_GUID_equal(&guid, &guid_file_properties))
      {
      if(!bgav_GUID_read(&asf->hdr.guid, ctx->input) ||
         !bgav_input_read_64_le(ctx->input, &asf->hdr.file_size) ||
         !bgav_input_read_64_le(ctx->input, &asf->hdr.create_time) ||
         !bgav_input_read_64_le(ctx->input, &asf->hdr.packets_count) ||
         !bgav_input_read_64_le(ctx->input, &asf->hdr.play_time) ||
         !bgav_input_read_64_le(ctx->input, &asf->hdr.send_time) ||
         !bgav_input_read_32_le(ctx->input, &asf->hdr.preroll) ||
         !bgav_input_read_32_le(ctx->input, &asf->hdr.ignore) ||
         !bgav_input_read_32_le(ctx->input, &asf->hdr.flags) ||
         !bgav_input_read_32_le(ctx->input, &asf->hdr.min_pktsize) ||
         !bgav_input_read_32_le(ctx->input, &asf->hdr.max_pktsize) ||
         !bgav_input_read_32_le(ctx->input, &asf->hdr.max_bitrate))
        goto fail;

      if(asf->hdr.send_time)
        {
        gavl_track_set_duration(ctx->tt->cur->info,
                                (asf->hdr.send_time - asf->hdr.preroll) 
                                / (10000000/GAVL_TIME_SCALE));
        }
      }
    /* Stream properties */
    else if(bgav_GUID_equal(&guid, &guid_stream_header))
      {
      /* Stream type */
      if(!bgav_GUID_read(&guid, ctx->input))
        goto fail;

      /* Error concealment type */
      if(!bgav_GUID_read(&guid1, ctx->input))
        goto fail;

      error_concealment = AUDIO_ERROR_CONCEAL_NONE;
      
      //      if(bgav_GUID_equal(&guid1, &guid_audio_conceal_none))
      //        error_concealment = AUDIO_ERROR_CONCEAL_NONE;
      if(bgav_GUID_equal(&guid1, &guid_audio_conceal_interleave))
        error_concealment = AUDIO_ERROR_CONCEAL_INTERLEAVE;
      
      bgav_input_skip(ctx->input, 8);  /* Unknown */
        
      if(!bgav_input_read_32_le(ctx->input, &type_specific_size) ||
         !bgav_input_read_32_le(ctx->input, &stream_specific_size) ||
         !bgav_input_read_16_le(ctx->input, &stream_number))
        goto fail;

      bgav_input_skip(ctx->input, 4);  /* Unknown */
      
      /* Found audio stream */
      if(bgav_GUID_equal(&guid, &guid_audio_media))
        {
        
        bgav_as = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
        bgav_as->cleanup = cleanup_stream_asf;
        bgav_as->stream_id = stream_number;

        asf_as  = calloc(1, sizeof(*asf_as));
       
        bgav_as->priv = asf_as;
        asf_as->error_concealment = error_concealment;

        /* Type specific data */
        
        if(buf_size < type_specific_size)
          {
          buf_size = type_specific_size;
          buf = realloc(buf, buf_size);
          }
        if(bgav_input_read_data(ctx->input, buf, type_specific_size) <
           type_specific_size)
          goto fail;
        bgav_WAVEFORMAT_read(&wf, buf, type_specific_size);
        bgav_WAVEFORMAT_dump(&wf);
        bgav_WAVEFORMAT_get_format(&wf, bgav_as);
        bgav_WAVEFORMAT_free(&wf);
        
        /* Stream specific data */

        if(buf_size < stream_specific_size)
          {
          buf_size = stream_specific_size;
          buf = realloc(buf, buf_size);
          }
        if(bgav_input_read_data(ctx->input, buf, stream_specific_size) <
           stream_specific_size)
          goto fail;
        if(asf_as->error_concealment == AUDIO_ERROR_CONCEAL_INTERLEAVE)
          {
          pos = buf;
          
          asf_as->descramble_h = *pos;
          pos++;
          asf_as->descramble_w = GAVL_PTR_2_16LE(pos);
          pos+=2;
          asf_as->descramble_b = GAVL_PTR_2_16LE(pos);
          pos+=2;
          asf_as->descramble_w /= asf_as->descramble_b;
          
          asf_as->block_align_2 = GAVL_PTR_2_16LE(pos);
          pos+=2;
          }
        else
          {
          asf_as->descramble_b = 1;
          asf_as->descramble_h = 1;
          asf_as->descramble_w = 1;
          }
        bgav_as->timescale = 1000;
        //        dump_audio_stream(asf_as);
        }
      
      /* Found video stream */
      else if(bgav_GUID_equal(&guid, &guid_video_media))
        {
        bgav_vs = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
        bgav_vs->cleanup = cleanup_stream_asf;
        bgav_vs->stream_id = stream_number;

        bgav_vs->flags |= STREAM_NO_DURATIONS;
        
        //        asf_vs  = calloc(1, sizeof(*asf_vs));
        
        //        bgav_vs->priv = asf_vs;

        if(buf_size < type_specific_size)
          {
          buf_size = type_specific_size;
          buf = realloc(buf, buf_size);
          }
        if(bgav_input_read_data(ctx->input, buf, type_specific_size) <
           type_specific_size)
          goto fail;
        pos = buf;

        pos += 4;  /* Picture width UINT32 4 */
        pos += 4;  /* Picture height UINT32 4 */
        pos ++;    /* Unknown	UINT8	1 */
        pos += 2; /* Size of Bitmapinfoheader */
        
        //        bgav_input_read_32_le(ctx->input, &size1);

        bgav_BITMAPINFOHEADER_read(&bh, &pos);
        //        bgav_BITMAPINFOHEADER_dump(&bh);
        bgav_BITMAPINFOHEADER_get_format(&bh, bgav_vs);
        /* Fill in the remeaining values */
        
        bgav_vs->data.video.format->timescale = 1000;
        bgav_vs->data.video.format->framerate_mode = GAVL_FRAMERATE_VARIABLE;
        
        //        gavl_video_format_dump(&bgav_vs->format);
        if(pos - buf < type_specific_size)
          bgav_stream_set_extradata(bgav_vs, pos, type_specific_size - (pos - buf));
        
        /* Stream specific (not known for now) */

        if(stream_specific_size)
          {
          bgav_input_skip(ctx->input, stream_specific_size);
          }
        
        }
      }
    /* Metadata */
    else if(bgav_GUID_equal(&guid_comment_header, &guid))
      {
      if(!read_metadata(ctx))
        return 0;
      }
    else if(bgav_GUID_equal(&guid_stream_bitrate_properties, &guid))
      {
      if(!read_bitrate_properties(ctx))
        return 0;
      }
    else if(bgav_GUID_equal(&guid_data, &guid))
      {
      asf->data_size = size;
      break;
      }

    /* Skip unused junk */
    if(ctx->input->position - chunk_start_pos < size)
      {
      bgav_input_skip(ctx->input,
                      size - (ctx->input->position - chunk_start_pos));
      
      }
    //    if(size & 1)
    //      {
    //      bgav_input_skip(ctx->input, 1);
    //      }
    
    }

  /* Skip 26 bytes */
  
  bgav_input_skip(ctx->input, 16); /* Unknown GUID */
  bgav_input_skip(ctx->input, 8);  /* Number of packets	UINT64 */
  bgav_input_skip(ctx->input, 1);  /* Unknown UINT8  */
  bgav_input_skip(ctx->input, 1);  /* Unknown UINT8 */

  /* Set the packet size */
  
  ctx->packet_size = asf->hdr.max_pktsize;
  asf->packet_buffer = malloc(ctx->packet_size);
  
  ctx->tt->cur->data_start = ctx->input->position;

  /* Update stream bitrates */

  if(asf->num_stream_bitrates)
    {
    update_stream_bitrates(ctx);
    }
      
  if(buf)
    free(buf);
  
  if((ctx->input->flags & BGAV_INPUT_CAN_SEEK_BYTE) && asf->hdr.packets_count)
    ctx->flags |= BGAV_DEMUXER_CAN_SEEK;
  
  bgav_track_set_format(ctx->tt->cur, "ASF", "application/x-mplayer2");
  
  return 1;
  fail:
  if(buf)
    free(buf);
  return 0;
  }

/* Packet info */

typedef struct
  {
  uint8_t flags;
  uint8_t segtype;
  int plen;      // packet size
  int sequence;  // sequence
  int padding;   // padding size (padding)
  uint32_t time;
  int16_t duration;
  
  int segsizetype;
  int segs;
  } asf_packet_header_t;
#if 0
static void dump_packet_header(asf_packet_header_t * h)
  {
  gavl_dprintf( "asf packet header\n");
  gavl_dprintf( "  packet_flags:    0x%02x\n", h->flags);
  gavl_dprintf( "  packet_property: 0x%02x\n", h->segtype);
  gavl_dprintf( "  packet_length:   %d\n",     h->plen);
  gavl_dprintf( "  sequence:        %d\n",     h->sequence);
  gavl_dprintf( "  padsize:         %d\n",     h->padding);
  gavl_dprintf( "  timestamp:       %d\n",     h->time);
  gavl_dprintf( "  duration:        %d\n",     h->duration);
  gavl_dprintf( "  segsizetype:     0x%02x\n", h->segsizetype);
  gavl_dprintf( "  segments:        %d\n",     h->segs);
  }
#endif
/* Returns the number of bytes used or -1 */

static int read_packet_header(bgav_demuxer_context_t * ctx, asf_t * asf,
                              asf_packet_header_t * ret,
                              uint8_t * data)
  {
  uint8_t * data_ptr;

  ret->time        = 0;
  ret->duration    = 0;
  ret->segs        = 1;
  ret->segsizetype = 0x80;
  
  data_ptr = data + (1 + (*data & 15));
  
  ret->flags = *(data_ptr++);
  ret->segtype      = *(data_ptr++);

  // Read packet size (plen):
  switch((ret->flags>>5)&3)
    {
    case 3: ret->plen=GAVL_PTR_2_32LE(data_ptr);data_ptr+=4;break; // dword
    case 2: ret->plen=GAVL_PTR_2_16LE(data_ptr);data_ptr+=2;break; // word
    case 1: ret->plen=data_ptr[0];data_ptr++;break;                // byte
    default: ret->plen=0;
      //plen==0 is handled later
      //mp_msg(MSGT_DEMUX,MSGL_V,"Invalid plen type! assuming plen=0\n");
    }
  
  // Read sequence:
  switch((ret->flags>>1)&3)
    {
    case 3: ret->sequence=GAVL_PTR_2_32LE(data_ptr);data_ptr+=4;break;// dword
    case 2: ret->sequence=GAVL_PTR_2_16LE(data_ptr);data_ptr+=2;break;// word
    case 1: ret->sequence=data_ptr[0];data_ptr++;break;            // byte
    default: ret->sequence=0;
    }
  
  // Read padding size (padding):
  switch((ret->flags>>3)&3)
    {
    case 3: ret->padding=GAVL_PTR_2_32LE(data_ptr);data_ptr+=4;break;// dword
    case 2: ret->padding=GAVL_PTR_2_16LE(data_ptr);data_ptr+=2;break;// word
    case 1: ret->padding=data_ptr[0];data_ptr++;break;             // byte
    default: ret->padding=0;
    }
  
  if(((ret->flags>>5)&3)!=0)
    {
    // Explicit (absoulte) packet size
    //     if(ret->plen>asf->packet_size) "Warning! plen>packetsize!"
    }
  else
    {
    // Padding (relative) size
    ret->plen=ctx->packet_size-ret->padding;
    }
  
  // Read time & duration:
  ret->time = GAVL_PTR_2_32LE(data_ptr);     data_ptr+=4;
  ret->duration = GAVL_PTR_2_16LE(data_ptr); data_ptr+=2;
  
  // Read payload .flags =
  if(ret->flags&1)
    {
    // multiple sub-packets
    ret->segsizetype=data_ptr[0]>>6;
    ret->segs=data_ptr[0] & 0x3F;
    ++data_ptr;
    }
  //  dump_packet_header(ret);
  return data_ptr - data;
  }
  
typedef struct
  {
  unsigned char streamno;
  unsigned int seq;
  unsigned int x;   // offset or timestamp
  unsigned int rlen;
  //
  int len;
  unsigned int time2;
  int keyframe;
  } asf_segment_header_t;
#if 0
static void dump_segment_header(asf_segment_header_t*h)
  {
  gavl_dprintf( "Segment header:\n");
  gavl_dprintf( "  Stream number: %d\n", h->streamno);
  gavl_dprintf( "  Seq:           %d\n", h->seq);
  gavl_dprintf( "  x:             %d\n", h->x);   // offset or timestamp
  gavl_dprintf( "  rlen:          %d\n", h->rlen);
  gavl_dprintf( "  len:           %d\n", h->len);
  gavl_dprintf( "  time2:         %d\n", h->time2);
  gavl_dprintf( "  keyframe:      %d\n", h->keyframe);
  }
#endif
static int read_segment_header(const bgav_options_t * opt,
                               asf_t * asf,
                               asf_packet_header_t * pkt_hdr,
                               asf_segment_header_t * ret,
                               uint8_t * data)
  {
  uint8_t * data_ptr;

  ret->time2=0;
  ret->keyframe=0;

  data_ptr = data;
  
  ret->streamno = (*data_ptr) & 0x7f;
  ret->keyframe = !!((*data_ptr) & 0x80);

  data_ptr++;

  // Read media object number (seq):
  switch((pkt_hdr->segtype>>4)&3)
    {
    case 3: ret->seq=GAVL_PTR_2_32LE(data_ptr);data_ptr+=4;break; // dword
    case 2: ret->seq=GAVL_PTR_2_16LE(data_ptr);data_ptr+=2;break; // word
    case 1: ret->seq=data_ptr[0];data_ptr++;break;         // byte
    default: ret->seq=0;
    }
  
  // Read offset or timestamp:
  switch((pkt_hdr->segtype>>2)&3)
    {
    case 3: ret->x=GAVL_PTR_2_32LE(data_ptr);data_ptr+=4;break; // dword
    case 2: ret->x=GAVL_PTR_2_16LE(data_ptr);data_ptr+=2;break; // word
    case 1: ret->x=data_ptr[0];data_ptr++;break;         // byte
    default: ret->x=0;
    }
  
  // Read replic.data len:
  switch((pkt_hdr->segtype)&3)
    {
    case 3: ret->rlen=GAVL_PTR_2_32LE(data_ptr);data_ptr+=4;break; // dword
    case 2: ret->rlen=GAVL_PTR_2_16LE(data_ptr);data_ptr+=2;break; // word
    case 1: ret->rlen=data_ptr[0];data_ptr++;break;              // byte
    default: ret->rlen=0;
    }

  switch(ret->rlen)
    {
    case 0x01: // 1 = special, means grouping
      //printf("grouping: %02X  \n",p[0]);
      ++data_ptr; // skip PTS delta
      break;
    default:
      if(ret->rlen>=8)
        {
        data_ptr+=4;       // skip object size
        ret->time2=GAVL_PTR_2_32LE(data_ptr); // read PTS
        data_ptr+=ret->rlen-4;
        }
      else
        {
        if(ret->rlen)
          gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                   "unknown segment type (rlen): 0x%02X", ret->rlen);
        ret->time2=0; // unknown
        data_ptr+=ret->rlen;
        //        return -1;
        }
    }
  if(pkt_hdr->flags&1)
    {
    // multiple segments
    switch(pkt_hdr->segsizetype)
      {
      case 3: ret->len=GAVL_PTR_2_32LE(data_ptr);data_ptr+=4;break;  // dword
      case 2: ret->len=GAVL_PTR_2_16LE(data_ptr);data_ptr+=2;break;  // word
      case 1: ret->len=data_ptr[0];data_ptr++;break;           // byte
      default: ret->len=pkt_hdr->plen-(data_ptr-asf->packet_buffer); // ???
      }
    }
  else
    {
    // single segment
    ret->len=pkt_hdr->plen-(data_ptr-asf->packet_buffer);
    }
  return (data_ptr - data);
  }

static void add_packet(bgav_demuxer_context_t * ctx,
                       uint8_t * data,
                       int len,
                       int id,
                       int seq,
                       unsigned long time,
                       unsigned short dur,
                       int offs,
                       int keyframe)
  {
  bgav_stream_t * s;
  asf_audio_stream_t * as;
  //  asf_t * asf = ctx->priv;

  s = bgav_track_find_stream(ctx, id);
  
  if(!s)
    return;

  if(!s->packet && (offs > 0))
    return;
  
  if(s->packet)
    {
    if(s->packet_seq != seq)
      {
      if(s->type == GAVL_STREAM_AUDIO)
        {
        as = s->priv;
        if((as->descramble_w > 1) && (as->descramble_h > 1))
          asf_descramble(as,
                         s->packet->buf.buf,
                         s->packet->buf.len);
        }
#if 0
      if(s->type == GAVL_STREAM_AUDIO)
        {
        fprintf(stderr, "Got audio packet: ");
        bgav_packet_dump(s->packet);
        }
      else
        {
        fprintf(stderr, "Got video packet: ");
        bgav_packet_dump(s->packet);
        }
#endif
      bgav_stream_done_packet_write(s, s->packet);
      s->packet = NULL;
      }
    else
      {
      // append data to it!
      if((s->packet->buf.len != offs) &&
         (offs != -1))
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "data_size %d, Offset: %d",
                s->packet->buf.len, offs);

      gavl_packet_alloc(s->packet,
                        s->packet->buf.len + len);
      memcpy(s->packet->buf.buf + s->packet->buf.len, data, len);
      s->packet->buf.len += len;
      return;
      }
    }
  
  s->packet = bgav_stream_get_packet_write(s);
  gavl_packet_alloc(s->packet, len);
  

  if(s->type == GAVL_STREAM_AUDIO)
    s->packet->pes_pts = time;
  else
    s->packet->pts = time;
  
  // s->packet->timestamp -= ((gavl_time_t)(asf->hdr.preroll) * GAVL_TIME_SCALE) / 1000;
  
  if(keyframe)
    PACKET_SET_KEYFRAME(s->packet);
  s->packet_seq = seq;
  memcpy(s->packet->buf.buf, data, len);
  s->packet->buf.len = len;
  }

static gavl_source_status_t next_packet_asf(bgav_demuxer_context_t * ctx)
  {
  int i, len2, result;
  asf_t * asf;
  uint8_t * data_ptr;
  asf_packet_header_t pkt_hdr;
  asf_segment_header_t seg_hdr;
  asf = ctx->priv;

  //  if(ctx->input->position >= asf->data_start + asf->data_size)
  //    return 0;

  if(asf->hdr.packets_count && (asf->packets_read >= asf->hdr.packets_count))
    return GAVL_SOURCE_EOF;
  
  if(bgav_input_read_data(ctx->input, asf->packet_buffer,
                          ctx->packet_size) < ctx->packet_size)
    return GAVL_SOURCE_EOF;
  
  data_ptr = asf->packet_buffer +
    read_packet_header(ctx, asf, &pkt_hdr, asf->packet_buffer);

  for(i = 0; i < pkt_hdr.segs; i++)
    {
    result = read_segment_header(ctx->opt, asf, &pkt_hdr, &seg_hdr, data_ptr);
    if(result < 0)
      {
      return GAVL_SOURCE_EOF;
      }
    data_ptr += result;
        
    switch(seg_hdr.rlen)
      {
      case 0x01:
        while(seg_hdr.len > 0)
          {
          len2 = *data_ptr;
          data_ptr++;
          add_packet(ctx,
                     data_ptr,          /* Data     */
                     len2,              /* Len      */
                     seg_hdr.streamno,  /* ID       */
                     seg_hdr.seq,       /* Sequence */
                     seg_hdr.x,         /* Time     */
                     pkt_hdr.duration,  /* Duration */ 
                     -1,                /* Offset   */
                     seg_hdr.keyframe); /* Keyframe */
          data_ptr += len2;
          seg_hdr.len-=len2+1;
          seg_hdr.seq++;
          }
        break;
      default:
        {
        add_packet(ctx,
                   data_ptr,          /* Data     */
                   seg_hdr.len,       /* Len      */
                   seg_hdr.streamno,  /* ID       */
                   seg_hdr.seq,       /* Sequence */
                   seg_hdr.time2,     /* Time     */
                   pkt_hdr.duration,  /* Duration */
                   seg_hdr.x,         /* Offset   */
                   seg_hdr.keyframe); /* Keyframe */
        data_ptr += seg_hdr.len;
        }
      }
        
    }
  return GAVL_SOURCE_OK;
  }

#if 0
static void seek_asf(bgav_demuxer_context_t * ctx, int64_t time, int scale)
  {
  int64_t filepos;
  asf_t * asf = ctx->priv;
  /* Reset all the streams */
  
  asf->packets_read =
    (int64_t)((double)asf->hdr.packets_count *
              (gavl_time_to_seconds(gavl_time_unscale(scale, time))/
               gavl_time_to_seconds(gavl_track_get_duration(ctx->tt->cur->info))));
  
  filepos = ctx->tt->cur->data_start +
    ctx->packet_size * asf->packets_read;
  
  bgav_input_seek(ctx->input, filepos, SEEK_SET);
  
  }
#endif

static int post_seek_resync_asf(bgav_demuxer_context_t * ctx)
  {
  int rest;
  asf_t * asf = ctx->priv;
  int64_t pos = ctx->input->position;
  
  if(pos < ctx->tt->cur->data_start)
    pos = ctx->tt->cur->data_start;

  if((rest = (pos - ctx->tt->cur->data_start) % ctx->packet_size))
    pos -= rest;
  
  asf->packets_read = (pos - ctx->tt->cur->data_start) / ctx->packet_size;
  
  if(pos != ctx->input->position)
    bgav_input_seek(ctx->input, pos, SEEK_SET);
  return 1;
  }

static void close_asf(bgav_demuxer_context_t * ctx)
  {
  asf_t * asf = ctx->priv;

  if(asf->packet_buffer)
    free(asf->packet_buffer);

  if(asf->stream_bitrates)
    free(asf->stream_bitrates);
  
  free(ctx->priv);
  }

static int select_track_asf(bgav_demuxer_context_t * ctx, int track)
  {
  asf_t * asf = ctx->priv;
  asf->packets_read = 0;
  return 1;
  }


const bgav_demuxer_t bgav_demuxer_asf =
  {
    .probe            =  probe_asf,
    .open             =  open_asf,
    .select_track     =  select_track_asf,
    .next_packet      =  next_packet_asf,
    //    .seek             =  seek_asf,
    .post_seek_resync =  post_seek_resync_asf,
    .close            =  close_asf
  };
