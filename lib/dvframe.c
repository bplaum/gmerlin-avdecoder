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

/*
 *  Handler for DV frames
 *
 *  it will be used to parse raw DV streams as well as
 *  DV frames from AVI files.
 *
 *  It detects most interesting parameters from the DV
 *  data and can extract audio.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <avdec_private.h>
#include <dvframe.h>
// #include <libdv/dv.h>

/*
 *  This used to be implemented with libdv, later replaced by a port from
 *  the ffmpeg DV demuxer/decoder.
 */

typedef struct AVRational{
    int num; ///< numerator
    int den; ///< denominator
} AVRational;

typedef struct DVprofile
  {
  int              dsf;                 /* value of the dsf in the DV header */
  int              video_stype;         /* stype for VAUX source pack */
  int              frame_size;          /* total size of one frame in bytes */
  int              difseg_size;         /* number of DIF segments per DIF channel */
  int              n_difchan;           /* number of DIF channels per frame */
  int              frame_rate;
  int              frame_rate_base;
  int              ltc_divisor;         /* FPS from the LTS standpoint */
  int              height;              /* picture height in pixels */
  int              width;               /* picture width in pixels */
  AVRational       sar[2];              /* sample aspect ratios for 4:3 and 16:9 */
  //  const uint16_t  *video_place;         /* positions of all DV macro blocks */
  gavl_pixelformat_t pix_fmt;             /* picture pixel format */
  int              bpm;                 /* blocks per macroblock */
  // const uint8_t   *block_sizes;         /* AC block sizes, in bits */
  int              audio_stride;        /* size of audio_shuffle table */
  int              audio_min_samples[3];/* min ammount of audio samples */
  /* for 48Khz, 44.1Khz and 32Khz */
  int              audio_samples_dist[5];/* how many samples are supposed to be */
                                         /* in each frame in a 5 frames window */
  const uint16_t (*audio_shuffle)[9];  /* PCM shuffling table */
  } DVprofile;

static const uint16_t dv_audio_shuffle525[10][9] = {
  {  0, 30, 60, 20, 50, 80, 10, 40, 70 }, /* 1st channel */
  {  6, 36, 66, 26, 56, 86, 16, 46, 76 },
  { 12, 42, 72,  2, 32, 62, 22, 52, 82 },
  { 18, 48, 78,  8, 38, 68, 28, 58, 88 },
  { 24, 54, 84, 14, 44, 74,  4, 34, 64 },

  {  1, 31, 61, 21, 51, 81, 11, 41, 71 }, /* 2nd channel */
  {  7, 37, 67, 27, 57, 87, 17, 47, 77 },
  { 13, 43, 73,  3, 33, 63, 23, 53, 83 },
  { 19, 49, 79,  9, 39, 69, 29, 59, 89 },
  { 25, 55, 85, 15, 45, 75,  5, 35, 65 },
};

static const uint16_t dv_audio_shuffle625[12][9] = {
  {   0,  36,  72,  26,  62,  98,  16,  52,  88}, /* 1st channel */
  {   6,  42,  78,  32,  68, 104,  22,  58,  94},
  {  12,  48,  84,   2,  38,  74,  28,  64, 100},
  {  18,  54,  90,   8,  44,  80,  34,  70, 106},
  {  24,  60,  96,  14,  50,  86,   4,  40,  76},
  {  30,  66, 102,  20,  56,  92,  10,  46,  82},

  {   1,  37,  73,  27,  63,  99,  17,  53,  89}, /* 2nd channel */
  {   7,  43,  79,  33,  69, 105,  23,  59,  95},
  {  13,  49,  85,   3,  39,  75,  29,  65, 101},
  {  19,  55,  91,   9,  45,  81,  35,  71, 107},
  {  25,  61,  97,  15,  51,  87,   5,  41,  77},
  {  31,  67, 103,  21,  57,  93,  11,  47,  83},
};

static const DVprofile dv_profiles[] = {
  /* DV NTSC SD */
  { .dsf = 0,
    .video_stype = 0x0,
    .frame_size = 120000,        /* IEC 61834, SMPTE-314M - 525/60 (NTSC) */
    .difseg_size = 10,
    .n_difchan = 1,
    .frame_rate = 30000,
    .ltc_divisor = 30,
    .frame_rate_base = 1001,
    .height = 480,
    .width = 720,
    .sar = {{10, 11}, {40, 33}},
    //      .video_place = dv_place_411,
    .pix_fmt = GAVL_YUV_411_P,
    .bpm = 6,
    //      .block_sizes = block_sizes_dv2550,
    .audio_stride = 90,
    .audio_min_samples = { 1580, 1452, 1053 }, /* for 48, 44.1 and 32Khz */
    .audio_samples_dist = { 1600, 1602, 1602, 1602, 1602 }, /* per SMPTE-314M */
    .audio_shuffle = dv_audio_shuffle525,
  },
  /* DV PAL SD */
  { .dsf = 1,
    .video_stype = 0x0,
    .frame_size = 144000,        /* IEC 61834 - 625/50 (PAL) */
    .difseg_size = 12,
    .n_difchan = 1,
    .frame_rate = 25,
    .frame_rate_base = 1,
    .ltc_divisor = 25,
    .height = 576,
    .width = 720,
    .sar = {{59, 54}, {118, 81}},
    //      .video_place = dv_place_420,
    .pix_fmt = GAVL_YUV_420_P,
    .bpm = 6,
    //      .block_sizes = block_sizes_dv2550,
    .audio_stride = 108,
    .audio_min_samples = { 1896, 1742, 1264 }, /* for 48, 44.1 and 32Khz */
    .audio_samples_dist = { 1920, 1920, 1920, 1920, 1920 },
    .audio_shuffle = dv_audio_shuffle625,
  },
  /* DVCPRO PAL SD */
  { .dsf = 1,
    .video_stype = 0x0,
    .frame_size = 144000,        /* SMPTE-314M - 625/50 (PAL) */
    .difseg_size = 12,
    .n_difchan = 1,
    .frame_rate = 25,
    .frame_rate_base = 1,
    .ltc_divisor = 25,
    .height = 576,
    .width = 720,
    .sar = {{59, 54}, {118, 81}},
    //      .video_place = dv_place_411P,
    .pix_fmt = GAVL_YUV_411_P,
    .bpm = 6,
    //      .block_sizes = block_sizes_dv2550,
    .audio_stride = 108,
    .audio_min_samples = { 1896, 1742, 1264 }, /* for 48, 44.1 and 32Khz */
    .audio_samples_dist = { 1920, 1920, 1920, 1920, 1920 },
    .audio_shuffle = dv_audio_shuffle625,
  },
  /* DVCPRO50 NTSC */
  { .dsf = 0,
    .video_stype = 0x4,
    .frame_size = 240000,        /* SMPTE-314M - 525/60 (NTSC) 50 Mbps */
    .difseg_size = 10,           /* also known as "DVCPRO50" */
    .n_difchan = 2,
    .frame_rate = 30000,
    .ltc_divisor = 30,
    .frame_rate_base = 1001,
    .height = 480,
    .width = 720,
    .sar = {{10, 11}, {40, 33}},
    //      .video_place = dv_place_422_525,
    .pix_fmt = GAVL_YUV_422_P,
    .bpm = 6,
    //      .block_sizes = block_sizes_dv2550,
    .audio_stride = 90,
    .audio_min_samples = { 1580, 1452, 1053 }, /* for 48, 44.1 and 32Khz */
    .audio_samples_dist = { 1600, 1602, 1602, 1602, 1602 }, /* per SMPTE-314M */
    .audio_shuffle = dv_audio_shuffle525,
  },
  /* DVCPRO50 PAL */
  { .dsf = 1,
    .video_stype = 0x4,
    .frame_size = 288000,        /* SMPTE-314M - 625/50 (PAL) 50 Mbps */
    .difseg_size = 12,           /* also known as "DVCPRO50" */
    .n_difchan = 2,
    .frame_rate = 25,
    .frame_rate_base = 1,
    .ltc_divisor = 25,
    .height = 576,
    .width = 720,
    .sar = {{59, 54}, {118, 81}},
    //      .video_place = dv_place_422_625,
    .pix_fmt = GAVL_YUV_422_P,
    .bpm = 6,
    //      .block_sizes = block_sizes_dv2550,
    .audio_stride = 108,
    .audio_min_samples = { 1896, 1742, 1264 }, /* for 48, 44.1 and 32Khz */
    .audio_samples_dist = { 1920, 1920, 1920, 1920, 1920 },
    .audio_shuffle = dv_audio_shuffle625,
  },
  /* DVCPRO HD */
  { .dsf = 0,
    .video_stype = 0x14,
    .frame_size = 480000,        /* SMPTE-370M - 1080i60 100 Mbps */
    .difseg_size = 10,           /* also known as "DVCPRO HD" */
    .n_difchan = 4,
    .frame_rate = 30000,
    .ltc_divisor = 30,
    .frame_rate_base = 1001,
    .height = 1080,
    .width = 1280,
    .sar = {{1, 1}, {1, 1}},
    //      .video_place = dv_place_1080i60,
    .pix_fmt = GAVL_YUV_422_P,
    .bpm = 8,
    //      .block_sizes = block_sizes_dv100,
    .audio_stride = 90,
    .audio_min_samples = { 1580, 1452, 1053 }, /* for 48, 44.1 and 32Khz */
    .audio_samples_dist = { 1600, 1602, 1602, 1602, 1602 }, /* per SMPTE-314M */
    .audio_shuffle = dv_audio_shuffle525,
  },
  /* DVCPRO HD */
  { .dsf = 1,
    .video_stype = 0x14,
    .frame_size = 576000,        /* SMPTE-370M - 1080i50 100 Mbps */
    .difseg_size = 12,           /* also known as "DVCPRO HD" */
    .n_difchan = 4,
    .frame_rate = 25,
    .frame_rate_base = 1,
    .ltc_divisor = 25,
    .height = 1080,
    .width = 1440,
    .sar = {{1, 1}, {1, 1}},
    //      .video_place = dv_place_1080i50,
    .pix_fmt = GAVL_YUV_422_P,
    .bpm = 8,
    //      .block_sizes = block_sizes_dv100,
    .audio_stride = 108,
    .audio_min_samples = { 1896, 1742, 1264 }, /* for 48, 44.1 and 32Khz */
    .audio_samples_dist = { 1920, 1920, 1920, 1920, 1920 },
    .audio_shuffle = dv_audio_shuffle625,
  },
  /* DVCPRO HD */
  { .dsf = 0,
    .video_stype = 0x18,
    .frame_size = 240000,        /* SMPTE-370M - 720p60 100 Mbps */
    .difseg_size = 10,           /* also known as "DVCPRO HD" */
    .n_difchan = 2,
    .frame_rate = 60000,
    .ltc_divisor = 60,
    .frame_rate_base = 1001,
    .height = 720,
    .width = 960,
    .sar = {{1, 1}, {1, 1}},
    //      .video_place = dv_place_720p60,
    .pix_fmt = GAVL_YUV_422_P,
    .bpm = 8,
    //      .block_sizes = block_sizes_dv100,
    .audio_stride = 90,
    .audio_min_samples = { 1580, 1452, 1053 }, /* for 48, 44.1 and 32Khz */
    .audio_samples_dist = { 1600, 1602, 1602, 1602, 1602 }, /* per SMPTE-314M */
    .audio_shuffle = dv_audio_shuffle525,
  },
  /* DVCPRO HD */
  { .dsf = 1,
    .video_stype = 0x18,
    .frame_size = 288000,        /* SMPTE-370M - 720p50 100 Mbps */
    .difseg_size = 12,           /* also known as "DVCPRO HD" */
    .n_difchan = 2,
    .frame_rate = 50,
    .ltc_divisor = 50,
    .frame_rate_base = 1,
    .height = 720,
    .width = 960,
    .sar = {{1, 1}, {1, 1}},
    //      .video_place = dv_place_720p50,
    .pix_fmt = GAVL_YUV_422_P,
    .bpm = 8,
    //      .block_sizes = block_sizes_dv100,
    .audio_stride = 90,
    .audio_min_samples = { 1580, 1452, 1053 }, /* for 48, 44.1 and 32Khz */
    .audio_samples_dist = { 1600, 1602, 1602, 1602, 1602 }, /* per SMPTE-314M */
    .audio_shuffle = dv_audio_shuffle525,
  }
};

static const DVprofile* dv_frame_profile(const uint8_t* frame)
{
   int i;

   int dsf = (frame[3] & 0x80) >> 7;

   int stype = frame[80*5 + 48 + 3] & 0x1f;

   /* 576i50 25Mbps 4:1:1 is a special case */
   if (dsf == 1 && stype == 0 && frame[5] & 0x07) {
       return &dv_profiles[2];
   }

   for (i=0; i<sizeof(dv_profiles)/sizeof(DVprofile); i++)
       if (dsf == dv_profiles[i].dsf && stype == dv_profiles[i].video_stype)
           return &dv_profiles[i];

   return NULL;
}

enum dv_pack_type {
     dv_header525     = 0x3f, /* see dv_write_pack for important details on */
     dv_header625     = 0xbf, /* these two packs */
     dv_timecode      = 0x13,
     dv_audio_source  = 0x50,
     dv_audio_control = 0x51,
     dv_audio_recdate = 0x52,
     dv_audio_rectime = 0x53,
     dv_video_source  = 0x60,
     dv_video_control = 0x61,
     dv_video_recdate = 0x62,
     dv_video_rectime = 0x63,
     dv_unknown_pack  = 0xff,
};

/*
 * This is the dumbest implementation of all -- it simply looks at
 * a fixed offset and if pack isn't there -- fails. We might want
 * to have a fallback mechanism for complete search of missing packs.
 */
static const uint8_t* dv_extract_pack(uint8_t* frame, enum dv_pack_type t)
{
    int offs;

    switch (t) {
    case dv_audio_source:
          offs = (80*6 + 80*16*3 + 3);
          break;
    case dv_audio_control:
          offs = (80*6 + 80*16*4 + 3);
          break;
    case dv_video_control:
          offs = (80*5 + 48 + 5);
          break;
    default:
          return NULL;
    }

    return (frame[offs] == t ? &frame[offs] : NULL);
}


struct bgav_dv_dec_s
  {
  uint8_t * buffer;

  const DVprofile* profile;
  
  gavl_audio_format_t audio_format;
  gavl_video_format_t video_format;
  
  int64_t frame_counter;
  int64_t sample_counter;
  int ach;

  gavl_audio_frame_t * audio_frame;
  
  };


bgav_dv_dec_t * bgav_dv_dec_create()
  {
  bgav_dv_dec_t * ret;
  ret = calloc(1, sizeof(*ret));
  return ret;
  }

void bgav_dv_dec_destroy(bgav_dv_dec_t * d)
  {
  if(d->audio_frame)
    {
    gavl_audio_frame_null(d->audio_frame);
    gavl_audio_frame_destroy(d->audio_frame);
    }
  free(d);
  }

/* Sets the header for parsing. data must be 6 bytes long */

void bgav_dv_dec_set_header(bgav_dv_dec_t * d, uint8_t * data)
  {
  d->profile = dv_frame_profile(data);
  }

int bgav_dv_dec_get_frame_size(bgav_dv_dec_t * d)
  {
  return d->profile->frame_size;
  }

/* Sets the frame for parsing. data must be frame_size bytes long */

void bgav_dv_dec_set_frame(bgav_dv_dec_t * d, uint8_t * data)
  {
  d->buffer = data;
  //  d->profile =   
  }

/* Set up audio and video streams */

static const int dv_audio_frequency[3] = {
    48000, 44100, 32000,
};

void bgav_dv_dec_init_audio(bgav_dv_dec_t * d, bgav_stream_t * s)
  {
  const uint8_t* as_pack;
  int freq, stype, quant;
  
  as_pack = dv_extract_pack(d->buffer, dv_audio_source);
  if (!as_pack || !d->profile)
    {    /* No audio ? */
    return;
    }

  // smpls = as_pack[1] & 0x3f; /* samples in this frame - min. samples */
  freq = (as_pack[4] >> 3) & 0x07; /* 0 - 48KHz, 1 - 44,1kHz, 2 - 32 kHz */
  stype = (as_pack[3] & 0x1f); /* 0 - 2CH, 2 - 4CH */
  quant = as_pack[4] & 0x07; /* 0 - 16bit linear, 1 - 12bit nonlinear */

  /* note: ach counts PAIRS of channels (i.e. stereo channels) */
  if(stype == 3)
    d->ach = 4;
  else
    d->ach = (stype == 2 || (quant && (freq == 2))) ? 2 : 1;
  
  s->data.audio.format->samplerate = dv_audio_frequency[freq];
  
  s->data.audio.format->num_channels = d->ach * 2;
  //  s->data.audio.format->num_channels = dv_is_4ch(d->dv) ? 4 : 2;
  s->data.audio.format->sample_format =  GAVL_SAMPLE_S16;

  if(d->ach == 1)
    s->data.audio.format->interleave_mode =  GAVL_INTERLEAVE_ALL;
  else
    s->data.audio.format->interleave_mode =  GAVL_INTERLEAVE_2;
  
  s->data.audio.format->samples_per_frame = d->profile->audio_min_samples[freq] + 0x3f;
  
  gavl_set_channel_setup(s->data.audio.format);
  s->fourcc = BGAV_MK_FOURCC('g', 'a', 'v', 'l');
  
  gavl_audio_format_copy(&d->audio_format, s->data.audio.format);
  }

void bgav_dv_dec_get_pixel_aspect(bgav_dv_dec_t * d, uint32_t * pixel_width, uint32_t * pixel_height)
  {
  const uint8_t* vsc_pack;
  int apt, is16_9;
  /* finding out SAR is a little bit messy */
  vsc_pack = dv_extract_pack(d->buffer, dv_video_control);
  apt = d->buffer[4] & 0x07;
  is16_9 = (vsc_pack && ((vsc_pack[2] & 0x07) == 0x02 ||
                         (!apt && (vsc_pack[2] & 0x07) == 0x07)));
  *pixel_width  = d->profile->sar[is16_9].num;
  *pixel_height = d->profile->sar[is16_9].den;
  }

void bgav_dv_dec_get_timecode_format(bgav_dv_dec_t * d,
                                     gavl_timecode_format_t * tf,
                                     const bgav_options_t * opt)
  {
  gavl_timecode_t dummy;
  if(d->profile)
    {
    if(!opt->dv_datetime &&
       !bgav_dv_dec_get_timecode(d, &dummy))
      return;
    
    tf->int_framerate = d->profile->ltc_divisor;
    if(d->profile->frame_rate_base == 1001)
      tf->flags = GAVL_TIMECODE_DROP_FRAME;
    else
      tf->flags = 0;
    }
  
  }

void bgav_dv_dec_get_video_format(bgav_dv_dec_t * d, gavl_video_format_t * fmt)
  {
  bgav_dv_dec_get_pixel_aspect(d, &fmt->pixel_width,
                               &fmt->pixel_height);

  bgav_dv_dec_get_image_size(d, &fmt->image_width,
                             &fmt->image_height);

  fmt->pixelformat = bgav_dv_dec_get_pixelformat(d);
  
  fmt->frame_width  = fmt->image_width;
  fmt->frame_height = fmt->image_height;
  
  if(!fmt->timescale)
    {
    fmt->frame_duration = d->profile->frame_rate_base;
    fmt->timescale      = d->profile->frame_rate;
    }
  }

void bgav_dv_dec_init_video(bgav_dv_dec_t * d, bgav_stream_t * s)
  {
  s->fourcc = BGAV_MK_FOURCC('d', 'v', 'c', ' ');
  bgav_dv_dec_get_video_format(d, s->data.video.format);
  gavl_video_format_copy(&d->video_format, s->data.video.format);
  }

void bgav_dv_dec_set_frame_counter(bgav_dv_dec_t * d, int64_t frames)
  {
  d->frame_counter = frames;
  }

void bgav_dv_dec_set_sample_counter(bgav_dv_dec_t * d, int64_t samples)
  {
  d->sample_counter = samples;
  }

/* Extract audio and video packets suitable for the decoders */

static inline uint16_t dv_audio_12to16(uint16_t sample)
  {
  uint16_t shift, result;
  
  sample = (sample < 0x800) ? sample : sample | 0xf000;
  shift = (sample & 0xf00) >> 8;
  
  if (shift < 0x2 || shift > 0xd)
    {
    result = sample;
    }
  else if (shift < 0x8)
    {
    shift--;
    result = (sample - (256 * shift)) << shift;
    }
  else
    {
    shift = 0xe - shift;
    result = ((sample + ((256 * shift) + 1)) << shift) - 1;
    }
  return result;
  }

static int dv_extract_audio(uint8_t* frame, uint8_t* ppcm[4],
                            const DVprofile *sys)
{
    int size, chan, i, j, d, of, smpls, freq, quant, half_ch;
    uint16_t lc, rc;
    const uint8_t* as_pack;
    uint8_t *pcm, ipcm;

    as_pack = dv_extract_pack(frame, dv_audio_source);
    if (!as_pack)    /* No audio ? */
        return 0;

    smpls = as_pack[1] & 0x3f; /* samples in this frame - min. samples */
    freq = (as_pack[4] >> 3) & 0x07; /* 0 - 48KHz, 1 - 44,1kHz, 2 - 32 kHz */
    quant = as_pack[4] & 0x07; /* 0 - 16bit linear, 1 - 12bit nonlinear */

    if (quant > 1)
        return -1; /* Unsupported quantization */

    size = (sys->audio_min_samples[freq] + smpls) * 4; /* 2ch, 2bytes */
    half_ch = sys->difseg_size/2;

    /* We work with 720p frames split in half, thus even frames have channels 0,1 and odd 2,3 */
    ipcm = (sys->height == 720 && ((frame[1]>>2)&0x3) == 0)?2:0;
    pcm = ppcm[ipcm++];

    /* for each DIF channel */
    for (chan = 0; chan < sys->n_difchan; chan++) {
        /* for each DIF segment */
        for (i = 0; i < sys->difseg_size; i++) {
            frame += 6 * 80; /* skip DIF segment header */
            if (quant == 1 && i == half_ch) {
                /* next stereo channel (12bit mode only) */
                pcm = ppcm[ipcm++];
                if (!pcm)
                    break;
            }

            /* for each AV sequence */
            for (j = 0; j < 9; j++) {
                for (d = 8; d < 80; d += 2) {
                    if (quant == 0) {  /* 16bit quantization */
                        of = sys->audio_shuffle[i][j] + (d - 8)/2 * sys->audio_stride;
                        if (of*2 >= size)
                            continue;

                        pcm[of*2] = frame[d+1]; // FIXME: may be we have to admit
                        pcm[of*2+1] = frame[d]; //        that DV is a big endian PCM
                        if (pcm[of*2+1] == 0x80 && pcm[of*2] == 0x00)
                            pcm[of*2+1] = 0;
                    } else {           /* 12bit quantization */
                        lc = ((uint16_t)frame[d] << 4) |
                             ((uint16_t)frame[d+2] >> 4);
                        rc = ((uint16_t)frame[d+1] << 4) |
                             ((uint16_t)frame[d+2] & 0x0f);
                        lc = (lc == 0x800 ? 0 : dv_audio_12to16(lc));
                        rc = (rc == 0x800 ? 0 : dv_audio_12to16(rc));

                        of = sys->audio_shuffle[i%half_ch][j] + (d - 8)/3 * sys->audio_stride;
                        if (of*2 >= size)
                            continue;

                        pcm[of*2] = lc & 0xff; // FIXME: may be we have to admit
                        pcm[of*2+1] = lc >> 8; //        that DV is a big endian PCM
                        of = sys->audio_shuffle[i%half_ch+half_ch][j] +
                            (d - 8)/3 * sys->audio_stride;
                        pcm[of*2] = rc & 0xff; // FIXME: may be we have to admit
                        pcm[of*2+1] = rc >> 8; //        that DV is a big endian PCM
                        ++d;
                    }
                }

                frame += 16 * 80; /* 15 Video DIFs + 1 Audio DIF */
            }
        }

        /* next stereo channel (50Mbps and 100Mbps only) */
        pcm = ppcm[ipcm++];
        if (!pcm)
            break;
    }

    return size / 4;
}


int bgav_dv_dec_get_audio_packet(bgav_dv_dec_t * d, bgav_packet_t * p)
  {
  int samples, i;

  uint8_t * pcm[4];
  
  if(p)
    {
    int buf_len = gavl_audio_format_buffer_size(&d->audio_format);
    
    bgav_packet_alloc(p, buf_len);
    
    if(!d->audio_frame)
      d->audio_frame = gavl_audio_frame_create(NULL);

    gavl_audio_frame_from_data(d->audio_frame,
                               &d->audio_format,
                               p->buf.buf, p->buf.len);
    
    for(i = 0; i < d->ach; i++)
      pcm[i] = d->audio_frame->channels.u_8[i*2];
    for(i = d->ach; i < 4; i++)
      pcm[i] = NULL;
    
    samples = dv_extract_audio(d->buffer,
                               pcm, d->profile);
    PACKET_SET_KEYFRAME(p);
    p->pts = d->sample_counter;
    p->duration                   = samples;
    d->sample_counter             += samples;
    }
  return 1;
  }

void bgav_dv_dec_get_video_packet(bgav_dv_dec_t * d, bgav_packet_t * p)
  {
  if(p)
    {
    PACKET_SET_KEYFRAME(p);
    
    if(p->pts == GAVL_TIME_UNDEFINED)
      {
      p->pts = d->video_format.frame_duration * d->frame_counter;
      p->duration = d->video_format.frame_duration;
      }
    
    bgav_packet_alloc(p, d->profile->frame_size);
    memcpy(p->buf.buf, d->buffer, d->profile->frame_size);
    p->buf.len = d->profile->frame_size;
    d->frame_counter++;
    }
  }

static int GetSSYBPack(bgav_dv_dec_t * d,
                       int packNum, uint8_t * pack)
  {
  int i, j, k;

  /* process all DIF sequences */
  
  for(i = 0; i < d->profile->difseg_size; i++)
    {
    /* there are two DIF blocks in the subcode section */
    
    for (j = 0; j < 2; j++)
      {
      /* each block has 6 packets */
      
      for(k = 0; k < 6; k++)
        {
        /* calculate address: 150 DIF blocks per sequence, 80 bytes
           per DIF block, subcode blocks start at block 1, block and
           packet have 3 bytes header, packet is 8 bytes long
           (including header) */
        const uint8_t *s =
          &d->buffer[ i * 150 * 80 + 1 * 80 + j * 80 + 3 + k * 8 + 3 ];
        // printf("ssyb %d: %2.2x %2.2x %2.2x %2.2x %2.2x\n",
        // j * 6 + k, s[0], s[1], s[2], s[3], s[4]);
        if ( s[ 0 ] == packNum )
          {
          pack[ 0 ] = s[ 0 ];
          pack[ 1 ] = s[ 1 ];
          pack[ 2 ] = s[ 2 ];
          pack[ 3 ] = s[ 3 ];
          pack[ 4 ] = s[ 4 ];
          return 1;
          }
        }
      }
    }
  return 0;
  }

int bgav_dv_dec_get_date(bgav_dv_dec_t * d,
                         int * year, int * month, int * day)
  {
  uint8_t pack[5];
  if(!GetSSYBPack(d, dv_video_recdate, pack))
    return 0;
  if(year)
    {
    *year = ( pack[4] & 0xf ) + 10 * ( ( pack[4] >> 4 ) & 0xf );
    if(*year < 25 )
      *year += 2000;
    else
      *year += 1900;
    }
  if(month)
    {
    *month = (pack[3]&0xf) + 10 * ( (pack[3] >> 4 ) & 0x1 );
    }
  if(day)
    {
    *day = ( pack[2] & 0xf ) + 10 * ( ( pack[2] >> 4 ) & 0x3 );
    }
  return 1;
  }

int bgav_dv_dec_get_time(bgav_dv_dec_t * d,
                         int * hour, int * minute, int * second)
  {
  uint8_t pack[5];
  if(!GetSSYBPack(d, dv_video_rectime, pack))
    return 0;
  if(hour)
    {
    *hour = ( pack[4] & 0xf ) + 10 * ( ( pack[4] >> 4 ) & 0x3 );
    }
  if(minute)
    {
    *minute = ( pack[3] & 0xf ) + 10 * ( ( pack[3] >> 4 ) & 0x7 );
    }
  if(second)
    {
    *second = ( pack[2] & 0xf ) + 10 * ( ( pack[2] >> 4 ) & 0x7 );
    }
  return 1;
  }

int bgav_dv_dec_get_timecode(bgav_dv_dec_t * d,
                             gavl_timecode_t * tc)
  {
  int frame, minute, second, hour;
  uint8_t pack[5];
  if(!GetSSYBPack(d, dv_timecode, pack))
    return 0;

  frame  = ( pack[1] & 0xf ) + 10 * ( ( pack[1] >> 4 ) & 0x3 );
  second = ( pack[2] & 0xf ) + 10 * ( ( pack[2] >> 4 ) & 0x7 );
  minute = ( pack[3] & 0xf ) + 10 * ( ( pack[3] >> 4 ) & 0x7 );
  hour   = ( pack[4] & 0xf ) + 10 * ( ( pack[4] >> 4 ) & 0x3 );

  gavl_timecode_from_hmsf(tc,
                          hour,
                          minute,
                          second,
                          frame);
  return 1;
  }

void bgav_dv_dec_get_image_size(bgav_dv_dec_t * d, uint32_t * width,
                                uint32_t * height)
  {
  *width  = d->profile->width;
  *height = d->profile->height;
  }

gavl_pixelformat_t bgav_dv_dec_get_pixelformat(bgav_dv_dec_t * d)
  {
  return d->profile->pix_fmt;
  }
    
