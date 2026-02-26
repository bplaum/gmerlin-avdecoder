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



#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <config.h>

#include <gavl/metatags.h>

#include <gmerlin/translation.h>

#include <gmerlin/plugin.h>
#include <gmerlin/utils.h>
#include <avdec.h>

#include "avdec_common.h"


static int read_callback(void * priv, uint8_t * data, int len)
  {
  return gavl_io_read_data(priv, data, len);
  }

static int64_t seek_callback(void * priv, int64_t pos, int whence)
  {
  return gavl_io_seek(priv, pos, whence);
  }

static int open_io_avdec(void * priv, gavl_io_t * io)
  {
  bgav_options_t * opt;
  avdec_priv * avdec = priv;

  avdec->dec = bgav_create();
  opt = bgav_get_options(avdec->dec);

  bgav_options_copy(opt, avdec->opt);
  
  if(!bgav_open_callbacks(avdec->dec, read_callback, 
                          gavl_io_can_seek(io) ? seek_callback : 0, io, 
                          gavl_io_filename(io), gavl_io_mimetype(io),
                          gavl_io_total_bytes(io)))
    return 0;
  return 1;
  }

static int open_avdec(void * priv, const char * location)
  {
  bgav_options_t * opt;
  avdec_priv * avdec = priv;

  avdec->dec = bgav_create();
  opt = bgav_get_options(avdec->dec);

  bgav_options_copy(opt, avdec->opt);
  
  if(!bgav_open(avdec->dec, location))
    return 0;
  return 1;
  }


static const bg_parameter_info_t parameters[] =
  {
    {
      .name =       "audio_options",
      .long_name =  TRS("Audio options"),
      .type =       BG_PARAMETER_SECTION,
      .gettext_domain =    PACKAGE,
      .gettext_directory = LOCALE_DIR,
    },
    PARAM_DYNRANGE,
    {
      .name =        "sample_accuracy",
      .long_name =   TRS("Sample accurate"),
      .type =        BG_PARAMETER_STRINGLIST,
      .val_default = GAVL_VALUE_INIT_STRING("never"),
      .multi_names  = (char const *[]){ "never", "always", "when_necessary", NULL },
      .multi_labels = (char const *[]){ TRS("Never"),
                                        TRS("Always"),
                                        TRS("When necessary"),
                                        NULL },
      
      .help_string = TRS("Try sample accurate seeking. For most formats, this is not necessary, since normal seeking works fine. Some formats are only seekable in sample accurate mode. Choose \"When necessary\" to enable seeking for most formats with the smallest overhead."),
    },
    { /* End of parameters */ }
  };

static const bg_parameter_info_t * get_parameters_avdec(void * priv)
  {
  return parameters;
  }

/* TODO: Build these dynamically */

static char const * const protocols = "http https ftp rtsp smb mms pnm stdin hls hlss file";

static const char * get_protocols(void * priv)
  {
  return protocols;
  }

static char const * const mimetypes =
  "video/x-ms-asf "
  "audio/x-pn-realaudio-plugin "
  "video/x-pn-realvideo-plugin "
  "audio/x-pn-realaudio "
  "video/x-pn-realvideo "
  "audio/x-mpegurl "
  "audio/mpegurl "
  "audio/x-scpls "
  "audio/scpls "
  "audio/m3u "
  "audio/L8 "
  "audio/L16 "
  "audio/L16;rate=44100;channels=1 "
  "audio/L16;rate=44100;channels=2 "
  "audio/L16;rate=44100;channels=4 "
  "audio/L16;rate=44100;channels=6 "
  "audio/L16;rate=44100;channels=8 "
  "audio/L16;rate=48000;channels=1 "
  "audio/L16;rate=48000;channels=2 "
  "audio/L16;rate=11000;channels=1 "
  "audio/L16;rate=11000;channels=2 "
  "audio/L16;rate=11025;channels=1 "
  "audio/L16;rate=11025;channels=2 "
  "audio/L16;rate=22000;channels=1 "
  "audio/L16;rate=22000;channels=2 "
  "audio/L16;rate=22050;channels=1 "
  "audio/L16;rate=22050;channels=2 "
  "audio/L16;rate=8000;channels=1 "
  "audio/L16;rate=8000;channels=2 "
  "audio/L8;rate=11000;channels=1 "
  "audio/L8;rate=11000;channels=2 "
  "audio/L8;rate=11025;channels=1 "
  "audio/L8;rate=11025;channels=2 "
  "audio/L8;rate=22000;channels=1 "
  "audio/L8;rate=22000;channels=2 "
  "audio/L8;rate=22050;channels=1 "
  "audio/L8;rate=22050;channels=2 "
  "audio/L8;rate=44100;channels=1 "
  "audio/L8;rate=44100;channels=2 "
  "audio/L8;rate=48000;channels=1 "
  "audio/L8;rate=48000;channels=2 "
  "audio/L8;rate=8000;channels=1 "
  "audio/L8;rate=8000;channels=2 "
  "audio/x-ms-wma "
  "audio/mpeg "
  "audio/flac "
  "audio/x-flac "
  "audio/wav "
  "audio/x-wav "
  "audio/mp4 "
  "audio/m4a "
  "audio/x-m4a "
  "audio/m4b "
  "audio/x-m4b "
  "audio/x-aac "
  "audio/x-aacp "
  "audio/aac "
  "audio/aacp "
  "audio/ogg "
  "audio/opus "
  "audio/gavf "
  "audio/3gpp "
  "audio/3gpp2 "
  "audio/x-wavpack "
  "video/MP2P "
  "video/MP2T "
  "video/mp4 "
  "video/mpeg "
  "video/quicktime "
  "video/x-flv "
  "video/x-matroska "
  "video/x-mkv "
  "video/x-msvideo "
  "video/x-ms-wmv "
  "video/x-ms-asf "
  "video/avi "
  "video/x-avi "
  "video/x-xvid "
  "video/vnd.dlna.mpeg-tts "
  "video/webm "
  "video/3gpp "
  "video/3gpp2 "
  "application/x-cue "
  "application/x-subrip";

static const char * get_mimetypes(void * priv)
  {
  return mimetypes;
  }

static char const * const extensions = "avi asf asx wmv rm ra ram mov wav mp4 m4a 3gp qt au aiff aif mp3 mpg mpeg vob m3u pls ogg flac aac mpc spx vob wv tta gsm vp5 vp6 voc opus wma ac3 mp2 ogv webm mkv ts m1v m2v cue flv";

static const char * get_extensions(void * priv)
  {
  return extensions;
  }

const bg_input_plugin_t the_plugin =
  {
    .common =
    {
      BG_LOCALE,
      .name =           "i_avdec",
      .long_name =      TRS("AVDecoder plugin"),
      .description =    TRS("Plugin based on the Gmerlin avdecoder library. Supports most media formats. Playback is supported from files, URLs (with various protocols) and stdin."),
      .type =           BG_PLUGIN_INPUT,
      .flags =          0,
      .priority =       BG_PLUGIN_PRIORITY_MAX,
      .create =         bg_avdec_create,
      .destroy =        bg_avdec_destroy,
      .get_parameters = get_parameters_avdec,
      .set_parameter =  bg_avdec_set_parameter,
      .get_controllable = bg_avdec_get_controllable,
      .get_extensions = get_extensions,
      .get_protocols = get_protocols,
      
    },
    .get_mimetypes = get_mimetypes,
    /* Open file/device */
    .open = open_avdec,
    .open_io = open_io_avdec,
  /* For file and network plugins, this can be NULL */
    .get_media_info = bg_avdec_get_media_info,
    //    .get_edl  = bg_avdec_get_edl,

    .get_src = bg_avdec_get_src,

    /* Return track information */
    
    .get_frame_table =       bg_avdec_get_frame_table,
    
    .get_src = bg_avdec_get_src,
    
    /* Stop playback, close all decoders */
    .stop = NULL,
    .close = bg_avdec_close,
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
