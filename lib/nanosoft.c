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

static uint32_t swap_endian(uint32_t val)
  {
  return ((val & 0x000000FF) << 24) |
    ((val & 0x0000FF00) << 8) |
    ((val & 0x00FF0000) >> 8) |
    ((val & 0xFF000000) >> 24);
  }

/* Speaker configurations for WAVEFORMATEXTENSIBLE */

#define SPEAKER_FRONT_LEFT 	        0x1
#define SPEAKER_FRONT_RIGHT 	        0x2
#define SPEAKER_FRONT_CENTER 	        0x4
#define SPEAKER_LOW_FREQUENCY 	        0x8
#define SPEAKER_BACK_LEFT 	        0x10
#define SPEAKER_BACK_RIGHT 	        0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER 	0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER 	0x80
#define SPEAKER_BACK_CENTER 	        0x100
#define SPEAKER_SIDE_LEFT 	        0x200
#define SPEAKER_SIDE_RIGHT 	        0x400
#define SPEAKER_TOP_CENTER 	        0x800
#define SPEAKER_TOP_FRONT_LEFT 	        0x1000
#define SPEAKER_TOP_FRONT_CENTER 	0x2000
#define SPEAKER_TOP_FRONT_RIGHT 	0x4000
#define SPEAKER_TOP_BACK_LEFT 	        0x8000
#define SPEAKER_TOP_BACK_CENTER 	0x10000
#define SPEAKER_TOP_BACK_RIGHT 	        0x20000

#if 0
    GAVL_CHID_FRONT_CENTER,       /*!< For mono                                  */
    GAVL_CHID_FRONT_LEFT,         /*!< Front left                                */
    GAVL_CHID_FRONT_RIGHT,        /*!< Front right                               */
    GAVL_CHID_FRONT_CENTER_LEFT,  /*!< Left of Center                            */
    GAVL_CHID_FRONT_CENTER_RIGHT, /*!< Right of Center                           */
    GAVL_CHID_REAR_LEFT,          /*!< Rear left                                 */
    GAVL_CHID_REAR_RIGHT,         /*!< Rear right                                */
    GAVL_CHID_REAR_CENTER,        /*!< Rear Center                               */
    GAVL_CHID_SIDE_LEFT,          /*!< Side left                                 */
    GAVL_CHID_SIDE_RIGHT,         /*!< Side right                                */
    GAVL_CHID_LFE,                /*!< Subwoofer                                 */
    GAVL_CHID_AUX,                /*!< Additional channel (can be more than one) */
#endif


static const struct
  {
  int flag;
  gavl_channel_id_t id;
  }
channel_flags[] =
  {
    { SPEAKER_FRONT_LEFT,            GAVL_CHID_FRONT_LEFT },
    { SPEAKER_FRONT_RIGHT,           GAVL_CHID_FRONT_RIGHT },
    { SPEAKER_FRONT_CENTER,          GAVL_CHID_FRONT_CENTER },
    { SPEAKER_LOW_FREQUENCY,         GAVL_CHID_LFE },
    { SPEAKER_BACK_LEFT,             GAVL_CHID_REAR_LEFT },
    { SPEAKER_BACK_RIGHT,            GAVL_CHID_REAR_RIGHT },
    { SPEAKER_FRONT_LEFT_OF_CENTER,  GAVL_CHID_FRONT_CENTER_LEFT },
    { SPEAKER_FRONT_RIGHT_OF_CENTER, GAVL_CHID_FRONT_CENTER_RIGHT },
    { SPEAKER_BACK_CENTER,           GAVL_CHID_REAR_CENTER },
    { SPEAKER_SIDE_LEFT,             GAVL_CHID_SIDE_LEFT },
    { SPEAKER_SIDE_RIGHT,            GAVL_CHID_SIDE_RIGHT },
    { SPEAKER_TOP_CENTER,            GAVL_CHID_AUX },
    { SPEAKER_TOP_FRONT_LEFT,        GAVL_CHID_AUX }, 
    { SPEAKER_TOP_FRONT_CENTER,      GAVL_CHID_AUX },
    { SPEAKER_TOP_FRONT_RIGHT,       GAVL_CHID_AUX },
    { SPEAKER_TOP_BACK_LEFT,         GAVL_CHID_AUX },
    { SPEAKER_TOP_BACK_CENTER,       GAVL_CHID_AUX },
    { SPEAKER_TOP_BACK_RIGHT,        GAVL_CHID_AUX },
  };


static void channel_mask_2_format(uint32_t channel_mask, gavl_audio_format_t * format)
  {
  int i;
  int index = 0;
  for(i = 0; i < sizeof(channel_flags)/sizeof(channel_flags[0]); i++)
    {
    if(channel_mask & channel_flags[i].flag)
      {
      format->channel_locations[index] = channel_flags[i].id;
      index++;
      }
    }
  }

void bgav_WAVEFORMAT_read(bgav_WAVEFORMAT_t * ret, uint8_t * data, int len)
  {
  uint8_t * ptr;
  memset(ret, 0, sizeof(*ret));

  if(len < 12)
    return;

  ptr = data;
  ret->type = BGAV_WAVEFORMAT_WAVEFORMAT;
  ret->f.WAVEFORMAT.wFormatTag      = GAVL_PTR_2_16LE(ptr);ptr+=2;
  ret->f.WAVEFORMAT.nChannels       = GAVL_PTR_2_16LE(ptr);ptr+=2;
  ret->f.WAVEFORMAT.nSamplesPerSec  = GAVL_PTR_2_32LE(ptr);ptr+=4;
  ret->f.WAVEFORMAT.nAvgBytesPerSec = GAVL_PTR_2_32LE(ptr);ptr+=4;
  /* size of a data sample */
  ret->f.WAVEFORMAT.nBlockAlign     = GAVL_PTR_2_16LE(ptr);ptr+=2;

  if(len >= 16)
    {
    ret->type = BGAV_WAVEFORMAT_PCMWAVEFORMAT;
    ret->f.PCMWAVEFORMAT.wBitsPerSample     = GAVL_PTR_2_16LE(ptr);ptr+=2;
    }
  if(len >= 18)
    {
    ret->type = BGAV_WAVEFORMAT_WAVEFORMATEX;
    ret->f.WAVEFORMATEX.cbSize = GAVL_PTR_2_16LE(ptr);ptr+=2;
    if(ret->f.WAVEFORMATEX.cbSize)
      {
      if(ret->f.WAVEFORMAT.wFormatTag == 0xfffe && (ret->f.WAVEFORMATEX.cbSize >= 22))
        {
        ret->type = BGAV_WAVEFORMAT_WAVEFORMATEXTENSIBLE;
        ret->f.WAVEFORMATEXTENSIBLE.Samples.wValidBitsPerSample = GAVL_PTR_2_16LE(ptr);ptr+=2;
        ret->f.WAVEFORMATEXTENSIBLE.dwChannelMask = GAVL_PTR_2_32LE(ptr);ptr+=4;
        bgav_GUID_from_ptr(&ret->f.WAVEFORMATEXTENSIBLE.SubFormat, ptr);ptr+=16;

        if((ret->f.WAVEFORMATEX.cbSize > 22) && (len >= (int)(ptr - data) + ret->f.WAVEFORMATEX.cbSize - 22))
          {
          ret->f.WAVEFORMATEX.ext_data = malloc(ret->f.WAVEFORMATEX.cbSize - 22);
          ret->f.WAVEFORMATEX.ext_size = ret->f.WAVEFORMATEX.cbSize - 22;
          memcpy(ret->f.WAVEFORMATEX.ext_data, ptr, ret->f.WAVEFORMATEX.cbSize - 22);
          ptr += (ret->f.WAVEFORMATEX.cbSize - 22);
          }
        
        }
      else
        {
        if(ret->f.WAVEFORMATEX.cbSize && (len >= (int)(ptr - data) + ret->f.WAVEFORMATEX.cbSize))
          {
          ret->f.WAVEFORMATEX.ext_data = malloc(ret->f.WAVEFORMATEX.cbSize);
          ret->f.WAVEFORMATEX.ext_size = ret->f.WAVEFORMATEX.cbSize;
          memcpy(ret->f.WAVEFORMATEX.ext_data, ptr, ret->f.WAVEFORMATEX.cbSize);
          ptr += ret->f.WAVEFORMATEX.cbSize;
          }
        }

      }
    }
  }

static const struct
  {
  uint32_t fourcc;
  bgav_GUID_t guid;
  }
wav_guids[] = 
  {
    {
      BGAV_MK_FOURCC('V','O','R','B'),
      { 0x8a0566ac, 0x42b3, 0x4ad9, { 0xac, 0xa3, 0x93, 0xb9, 0x6, 0xdd, 0xf9, 0x8a }}
    },
    {
      BGAV_MK_FOURCC('V','O','R','B'),
      { 0x6ba47966, 0x3f83, 0x4178, { 0x96, 0x65, 0x00, 0xf0, 0xbf, 0x62, 0x92, 0xe5 } }
    }
  };

static uint32_t guid_2_fourcc(bgav_GUID_t * g)
  {
  int i;
  /* Generic check */

  if(!(g->v1 & 0xffff0000) && 
     (g->v2 == 0x0000) &&
     (g->v3 == 0x0010) &&
     (g->v4[0] == 0x80) &&
     (g->v4[1] == 0x00) &&
     (g->v4[2] == 0x00) &&
     (g->v4[3] == 0xaa) &&
     (g->v4[4] == 0x00) &&
     (g->v4[5] == 0x38) &&
     (g->v4[6] == 0x9b) &&
     (g->v4[7] == 0x71))
    {
    return BGAV_WAVID_2_FOURCC(g->v1);
    }
  for(i = 0; i < sizeof(wav_guids)/sizeof(wav_guids[0]); i++)
    {
    if(bgav_GUID_equal(g, &wav_guids[i].guid))
      return wav_guids[i].fourcc;
    }
  return 0;
  }

void bgav_WAVEFORMAT_get_format(bgav_WAVEFORMAT_t * wf,
                                bgav_stream_t * s)
  {
  //  bgav_WAVEFORMAT_dump(wf);
  
  s->fourcc                         = BGAV_WAVID_2_FOURCC(wf->f.WAVEFORMAT.wFormatTag);
  s->data.audio.format->num_channels = wf->f.WAVEFORMAT.nChannels;
  s->data.audio.format->samplerate   = wf->f.WAVEFORMAT.nSamplesPerSec;
  s->codec_bitrate                  = wf->f.WAVEFORMAT.nAvgBytesPerSec * 8;
  s->container_bitrate              = wf->f.WAVEFORMAT.nAvgBytesPerSec * 8;
  s->data.audio.block_align         = wf->f.WAVEFORMAT.nBlockAlign;
  
  s->timescale = s->data.audio.format->samplerate;

  switch(wf->type)
    {
    case BGAV_WAVEFORMAT_WAVEFORMAT:
      s->data.audio.bits_per_sample     = 8;
      gavl_set_channel_setup(s->data.audio.format);
      break;
    case BGAV_WAVEFORMAT_PCMWAVEFORMAT:
      s->data.audio.bits_per_sample     = wf->f.PCMWAVEFORMAT.wBitsPerSample;
      gavl_set_channel_setup(s->data.audio.format);
      break;
    case BGAV_WAVEFORMAT_WAVEFORMATEX:
      s->data.audio.bits_per_sample     = wf->f.PCMWAVEFORMAT.wBitsPerSample;

      if(wf->f.WAVEFORMATEX.ext_size)
        {
        gavl_buffer_append_data(&s->ci->codec_header, wf->f.WAVEFORMATEX.ext_data,
                                wf->f.WAVEFORMATEX.ext_size);
        }
      
      gavl_set_channel_setup(s->data.audio.format);
      break;
    case BGAV_WAVEFORMAT_WAVEFORMATEXTENSIBLE:
      s->data.audio.bits_per_sample     = wf->f.PCMWAVEFORMAT.wBitsPerSample;
      s->fourcc = guid_2_fourcc(&wf->f.WAVEFORMATEXTENSIBLE.SubFormat);
      channel_mask_2_format(wf->f.WAVEFORMATEXTENSIBLE.dwChannelMask,
                            s->data.audio.format);

      if(wf->f.WAVEFORMATEX.ext_size)
        {
        gavl_buffer_append_data(&s->ci->codec_header, wf->f.WAVEFORMATEX.ext_data,
                                wf->f.WAVEFORMATEX.ext_size);
        }


      break;
    }
  }

void bgav_WAVEFORMAT_set_format(bgav_WAVEFORMAT_t * wf,
                                  bgav_stream_t * s)
  {
  memset(wf, 0, sizeof(*wf));
  wf->type = BGAV_WAVEFORMAT_WAVEFORMATEX;
  wf->f.WAVEFORMAT.nChannels         = s->data.audio.format->num_channels;
  wf->f.WAVEFORMAT.nSamplesPerSec    = s->data.audio.format->samplerate;
  wf->f.WAVEFORMAT.nAvgBytesPerSec   = s->codec_bitrate / 8;
  wf->f.WAVEFORMAT.nBlockAlign       = s->data.audio.block_align;
  wf->f.WAVEFORMAT.wFormatTag        = BGAV_FOURCC_2_WAVID(s->fourcc);
  wf->f.PCMWAVEFORMAT.wBitsPerSample = s->data.audio.bits_per_sample;
  wf->f.WAVEFORMATEX.cbSize          = 0;
  }


void bgav_WAVEFORMAT_dump(bgav_WAVEFORMAT_t * ret)
  {
  switch(ret->type)
    {
    case BGAV_WAVEFORMAT_WAVEFORMAT:
      bgav_dprintf( "WAVEFORMAT\n");
      break;
    case BGAV_WAVEFORMAT_PCMWAVEFORMAT:
      bgav_dprintf( "PCMWAVEFORMAT\n");
      break;
    case BGAV_WAVEFORMAT_WAVEFORMATEX:
      bgav_dprintf( "WAVEFORMATEX\n");
      break;
    case BGAV_WAVEFORMAT_WAVEFORMATEXTENSIBLE:
      bgav_dprintf( "WAVEFORMATEXTENSIBLE\n");
      break;
    }
  bgav_dprintf( "  wFormatTag:      %04x\n", ret->f.WAVEFORMAT.wFormatTag);
  bgav_dprintf( "  nChannels:       %d\n",   ret->f.WAVEFORMAT.nChannels);
  bgav_dprintf( "  nSamplesPerSec:  %d\n",   ret->f.WAVEFORMAT.nSamplesPerSec);
  bgav_dprintf( "  nAvgBytesPerSec: %d\n",   ret->f.WAVEFORMAT.nAvgBytesPerSec);
  bgav_dprintf( "  nBlockAlign:     %d\n",   ret->f.WAVEFORMAT.nBlockAlign);

  switch(ret->type)
    {
    case BGAV_WAVEFORMAT_WAVEFORMAT:
      break;
    case BGAV_WAVEFORMAT_PCMWAVEFORMAT:
      bgav_dprintf( "  wBitsPerSample:  %d\n",   ret->f.PCMWAVEFORMAT.wBitsPerSample);
      break;
    case BGAV_WAVEFORMAT_WAVEFORMATEX:
      bgav_dprintf( "  wBitsPerSample:  %d\n",   ret->f.PCMWAVEFORMAT.wBitsPerSample);
      bgav_dprintf( "  cbSize:          %d\n",   ret->f.WAVEFORMATEX.cbSize);
      if(ret->f.WAVEFORMATEX.ext_size)
        {
        bgav_dprintf( "Extradata %d bytes, hexdump follows\n", ret->f.WAVEFORMATEX.ext_size);
        gavl_hexdump(ret->f.WAVEFORMATEX.ext_data, ret->f.WAVEFORMATEX.ext_size, 16);
        }
      break;
    case BGAV_WAVEFORMAT_WAVEFORMATEXTENSIBLE:
      bgav_dprintf( "  wBitsPerSample:  %d\n",   ret->f.PCMWAVEFORMAT.wBitsPerSample);
      bgav_dprintf( "  cbSize:          %d\n",   ret->f.WAVEFORMATEX.cbSize);
      
      bgav_dprintf( "  wValidBitsPerSample: %d\n", ret->f.WAVEFORMATEXTENSIBLE.Samples.wValidBitsPerSample);
      bgav_dprintf( "  dwChannelMask:       %08x\n", ret->f.WAVEFORMATEXTENSIBLE.dwChannelMask);
      bgav_dprintf( "  SubFormat:           ");
      bgav_GUID_dump(&ret->f.WAVEFORMATEXTENSIBLE.SubFormat);
      if(ret->f.WAVEFORMATEX.ext_size)
        {
        bgav_dprintf( "Extradata %d bytes, hexdump follows\n", ret->f.WAVEFORMATEX.ext_size);
        gavl_hexdump(ret->f.WAVEFORMATEX.ext_data, ret->f.WAVEFORMATEX.ext_size, 16);
        }
      break;
    }
  }

void bgav_WAVEFORMAT_free(bgav_WAVEFORMAT_t * wf)
  {
  if(wf->f.WAVEFORMATEX.ext_data)
    free(wf->f.WAVEFORMATEX.ext_data);
  }


/* BITMAPINFOHEADER */

void bgav_BITMAPINFOHEADER_read(bgav_BITMAPINFOHEADER_t * ret, uint8_t ** data)
  {
  ret->biSize          = GAVL_PTR_2_32LE((*data));
  ret->biWidth         = GAVL_PTR_2_32LE((*data)+4);
  ret->biHeight        = GAVL_PTR_2_32LE((*data)+8);
  ret->biPlanes        = GAVL_PTR_2_16LE((*data)+12);
  ret->biBitCount      = GAVL_PTR_2_16LE((*data)+14);
  ret->biCompression   = GAVL_PTR_2_32LE((*data)+16);
  ret->biSizeImage     = GAVL_PTR_2_32LE((*data)+20);
  ret->biXPelsPerMeter = GAVL_PTR_2_32LE((*data)+24);
  ret->biYPelsPerMeter = GAVL_PTR_2_32LE((*data)+28);
  ret->biClrUsed       = GAVL_PTR_2_32LE((*data)+32);
  ret->biClrImportant  = GAVL_PTR_2_32LE((*data)+36);
  *data += 40;
  }

uint32_t bgav_BITMAPINFOHEADER_get_fourcc(bgav_BITMAPINFOHEADER_t * bh)
  {
  return swap_endian(bh->biCompression);
  }


void bgav_BITMAPINFOHEADER_get_format(bgav_BITMAPINFOHEADER_t * bh,
                                      bgav_stream_t * s)
  {
  s->data.video.format->frame_width  = bh->biWidth;
  s->data.video.format->frame_height = bh->biHeight;

  s->data.video.format->image_width  = bh->biWidth;
  s->data.video.format->image_height = bh->biHeight;

  s->data.video.format->pixel_width  = 1;
  s->data.video.format->pixel_height = 1;
  s->data.video.depth               = bh->biBitCount;
  s->data.video.image_size          = bh->biSizeImage;
  s->data.video.planes              = bh->biPlanes;
  
  s->fourcc = bgav_BITMAPINFOHEADER_get_fourcc(bh);
  
  /* Introduce a fourcc for RGB */
  
  if(!s->fourcc)
    s->fourcc = BGAV_MK_FOURCC('R', 'G', 'B', ' ');
  }

void bgav_BITMAPINFOHEADER_set_format(bgav_BITMAPINFOHEADER_t * bh,
                                      bgav_stream_t * s)
  {
  memset(bh, 0, sizeof(*bh));
  bh->biWidth = s->data.video.format->image_width;
  bh->biHeight = s->data.video.format->image_height;
  bh->biCompression = swap_endian(s->fourcc);
  bh->biBitCount    = s->data.video.depth;
  //  if(bh->biBitCount > 24)
  //    bh->biBitCount = 24;
  bh->biSizeImage   = s->data.video.image_size;
  bh->biPlanes      = s->data.video.planes;
  bh->biSize        = 40;
  if(!bh->biPlanes)
    bh->biPlanes = 1;
  }


void bgav_BITMAPINFOHEADER_dump(bgav_BITMAPINFOHEADER_t * ret)
  {
  uint32_t fourcc_be;
  bgav_dprintf( "BITMAPINFOHEADER:\n");
  bgav_dprintf( "  biSize: %d\n", ret->biSize); /* sizeof(BITMAPINFOHEADER) */
  bgav_dprintf( "  biWidth: %d\n", ret->biWidth);
  bgav_dprintf( "  biHeight: %d\n", ret->biHeight);
  bgav_dprintf( "  biPlanes: %d\n", ret->biPlanes);
  bgav_dprintf( "  biBitCount: %d\n", ret->biBitCount);
  fourcc_be = swap_endian(ret->biCompression);
  bgav_dprintf( "  biCompression: ");
  bgav_dump_fourcc(fourcc_be);
  bgav_dprintf( "\n");
  bgav_dprintf( "  biSizeImage: %d\n", ret->biSizeImage);
  bgav_dprintf( "  biXPelsPerMeter: %d\n", ret->biXPelsPerMeter);
  bgav_dprintf( "  biYPelsPerMeter: %d\n", ret->biXPelsPerMeter);
  bgav_dprintf( "  biClrUsed: %d\n", ret->biClrUsed);
  bgav_dprintf( "  biClrImportant: %d\n", ret->biClrImportant);
  }

/* RIFF INFO chunk */

int bgav_RIFFINFO_probe(bgav_input_context_t * input)
  {
  uint8_t test_data[12];

  if(bgav_input_get_data(input, test_data, 12) < 12)
    return 0;

  if((test_data[0] == 'L') &&
     (test_data[1] == 'I') &&
     (test_data[2] == 'S') &&
     (test_data[3] == 'T') &&
     (test_data[8] == 'I') &&
     (test_data[9] == 'N') &&
     (test_data[10] == 'F') &&
     (test_data[11] == 'O'))
    return 1;

  return 0;
  }

/* RS == read_string */

#define RS(tag) \
  if(!strncmp((char*)ptr, #tag, 4))             \
    { \
    ret->tag = gavl_strdup((char*)(ptr + 8));    \
    ptr += string_len + 8; \
    if(string_len % 2) \
      ptr++; \
    continue; \
    }

bgav_RIFFINFO_t * bgav_RIFFINFO_read_without_header(bgav_input_context_t * input, int size)
  {
  int string_len;
  bgav_RIFFINFO_t * ret;
  uint8_t *buf, * ptr, *end_ptr;
  buf = malloc(size);
  ptr = buf;
  
  if(bgav_input_read_data(input, ptr, size) < size)
    {
    free(buf);
    return NULL;
    }
  end_ptr = ptr + size;

  ret = calloc(1, sizeof(*ret));

  while(ptr < end_ptr)
    {
    string_len = GAVL_PTR_2_32LE(ptr + 4);

    //    gavl_hexdump(ptr, 8, 8);
    
    RS(IARL);
    RS(IART);
    RS(ICMS);
    RS(ICMT);
    RS(ICOP);
    RS(ICRD);
    RS(ICRP);
    RS(IDIM);
    RS(IDPI);
    RS(IENG);
    RS(IGNR);
    RS(IKEY);
    RS(ILGT);
    RS(IMED);
    RS(INAM);
    RS(IPLT);
    RS(IPRD);
    RS(ISBJ);
    RS(ISFT);
    RS(ISHP);
    RS(ISRC);
    RS(ISRF);
    RS(ITCH);
    
    ptr += string_len + 8;
    if(string_len % 2)
      ptr++;
    }
  free(buf);
  return ret;
  }

#undef RS

bgav_RIFFINFO_t * bgav_RIFFINFO_read(bgav_input_context_t * input)
  {
  uint32_t size;

  /*
   *  Read 12 byte header. We assume that it's already verified with
   *  bgav_RIFFINFO_probe
   */

  bgav_input_skip(input, 4);
  bgav_input_read_32_le(input, &size);
  bgav_input_skip(input, 4);

  size -= 4; /* INFO */

  return bgav_RIFFINFO_read_without_header(input, size);
  }


/* DS == dump_string */

#define DS(tag) if(info->tag) bgav_dprintf( "  %s: %s\n", #tag, info->tag)

void bgav_RIFFINFO_dump(bgav_RIFFINFO_t * info)
  {
  bgav_dprintf( "INFO\n");

  DS(IARL);
  DS(IART);
  DS(ICMS);
  DS(ICMT);
  DS(ICOP);
  DS(ICRD);
  DS(ICRP);
  DS(IDIM);
  DS(IDPI);
  DS(IENG);
  DS(IGNR);
  DS(IKEY);
  DS(ILGT);
  DS(IMED);
  DS(INAM);
  DS(IPLT);
  DS(IPRD);
  DS(ISBJ);
  DS(ISFT);
  DS(ISHP);
  DS(ISRC);
  DS(ISRF);
  DS(ITCH);
  
  }

#undef DS

/* FS = free_string */

#define FS(tag) if(info->tag) { free(info->tag); info->tag = NULL; }

void bgav_RIFFINFO_destroy(bgav_RIFFINFO_t * info)
  {

  FS(IARL);
  FS(IART);
  FS(ICMS);
  FS(ICMT);
  FS(ICOP);
  FS(ICRD);
  FS(ICRP);
  FS(IDIM);
  FS(IDPI);
  FS(IENG);
  FS(IGNR);
  FS(IKEY);
  FS(ILGT);
  FS(IMED);
  FS(INAM);
  FS(IPLT);
  FS(IPRD);
  FS(ISBJ);
  FS(ISFT);
  FS(ISHP);
  FS(ISRC);
  FS(ISRF);
  FS(ITCH);

  free(info);
  }
#undef FS

/* CS == copy_string */

#define CS(meta, tag) \
  if(!gavl_dictionary_get_string(m, meta) && info->tag)               \
    gavl_dictionary_set_string(m, meta, info->tag);

void bgav_RIFFINFO_get_metadata(bgav_RIFFINFO_t * info, bgav_metadata_t * m)
  {
  CS(GAVL_META_ARTIST,    IART);
  CS(GAVL_META_TITLE,     INAM);
  CS(GAVL_META_COMMENT,   ICMT);
  CS(GAVL_META_COPYRIGHT, ICOP);
  CS(GAVL_META_GENRE,     IGNR);
  CS(GAVL_META_DATE,      ICRD);
  CS(GAVL_META_SOFTWARE,  ISFT);
  CS(GAVL_META_CREATOR,   IENG);
  
  
  }

#undef CS

/* GUID */

void bgav_GUID_dump(bgav_GUID_t * g)
  {
  bgav_dprintf(
          "%08x-%04x-%04x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n",
          g->v1, g->v2, g->v3, g->v4[0], g->v4[1], g->v4[2], g->v4[3],
          g->v4[4], g->v4[5], g->v4[6], g->v4[7]);
  }

int bgav_GUID_equal(const bgav_GUID_t * g1, const bgav_GUID_t * g2)
  {
  return (g1->v1 == g2->v1) &&
    (g1->v2 == g2->v2) &&
    (g1->v3 == g2->v3) &&
    !memcmp(g1->v4, g2->v4, 8);
  }

int bgav_GUID_read(bgav_GUID_t * ret, bgav_input_context_t * input)
  {
  return bgav_input_read_32_le(input, &ret->v1) &&
    bgav_input_read_16_le(input, &ret->v2) &&
    bgav_input_read_16_le(input, &ret->v3) &&
    (bgav_input_read_data(input, ret->v4, 8) == 8);
  }

void bgav_GUID_from_ptr(bgav_GUID_t * ret, uint8_t * ptr)
  {
  ret->v1 = GAVL_PTR_2_32LE(ptr);
  ret->v2 = GAVL_PTR_2_16LE(ptr+4);
  ret->v3 = GAVL_PTR_2_16LE(ptr+6);
  memcpy(ret->v4, ptr + 8, 8);
  }

int bgav_GUID_get(bgav_GUID_t * ret, bgav_input_context_t * input)
  {
  uint8_t data[16];

  if(bgav_input_get_data(input, data, 16) < 16)
    return 0;
  bgav_GUID_from_ptr(ret, data);
  return 1;
  }
