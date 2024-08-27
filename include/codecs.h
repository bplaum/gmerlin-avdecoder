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



#ifndef BGAV_CODECS_H_INCLUDED
#define BGAV_CODECS_H_INCLUDED

#ifdef HAVE_AVCODEC
void bgav_init_audio_decoders_ffmpeg(bgav_options_t * opt);
void bgav_init_video_decoders_ffmpeg(bgav_options_t * opt);
#endif
#ifdef HAVE_VORBIS
void bgav_init_audio_decoders_vorbis();
#endif

#ifdef HAVE_OPUS
void bgav_init_audio_decoders_opus();
#endif


#ifdef HAVE_LIBA52
void bgav_init_audio_decoders_a52();
#endif

#ifdef HAVE_DCA
void bgav_init_audio_decoders_dca();
#endif

#ifdef HAVE_MAD
void bgav_init_audio_decoders_mad();
#endif

#ifdef HAVE_LIBPNG
void bgav_init_video_decoders_png();
#endif

#ifdef HAVE_OPENJPEG
void bgav_init_video_decoders_openjpeg();
#endif

#ifdef HAVE_LIBTIFF
void bgav_init_video_decoders_tiff();
#endif

#ifdef HAVE_THEORADEC
void bgav_init_video_decoders_theora();
#endif

#ifdef HAVE_SCHROEDINGER
void bgav_init_video_decoders_schroedinger();
#endif

#ifdef HAVE_SPEEX
void bgav_init_audio_decoders_speex();
#endif

#ifdef HAVE_FLAC
void bgav_init_audio_decoders_flac();
#endif

#ifdef HAVE_V4L2
void bgav_init_video_decoders_v4l2();
#endif

/* The following are always supported */

void bgav_init_audio_decoders_gavl();
void bgav_init_audio_decoders_pcm();

#ifdef HAVE_LIBGSM
void bgav_init_audio_decoders_gsm();
#endif

void bgav_init_video_decoders_aviraw();
void bgav_init_video_decoders_qtraw();
void bgav_init_video_decoders_yuv();
void bgav_init_video_decoders_y4m();
void bgav_init_video_decoders_tga();
void bgav_init_video_decoders_rtjpeg();
void bgav_init_video_decoders_gavl();
void bgav_init_video_decoders_dvdsub();

void bgav_init_audio_decoders_gavf();
void bgav_init_video_decoders_gavf();

#endif // BGAV_CODECS_H_INCLUDED

