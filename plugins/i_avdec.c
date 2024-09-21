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
      .name =       "video_options",
      .long_name =  TRS("Video options"),
      .type =       BG_PARAMETER_SECTION
    },
    PARAM_VIDEO_GENERIC,
    {
      .name =       "network_options",
      .long_name =  TRS("Network options"),
      .type =       BG_PARAMETER_SECTION
    },
    {
      .name =        "connect_timeout",
      .long_name =   TRS("Connect timeout (milliseconds)"),
      .type =        BG_PARAMETER_INT,
      .val_default = GAVL_VALUE_INIT_INT(5000),
      .val_min =     GAVL_VALUE_INIT_INT(0),
      .val_max =     GAVL_VALUE_INIT_INT(2000000),
    },
    {
      .name =        "read_timeout",
      .long_name =   TRS("Read timeout (milliseconds)"),
      .type =        BG_PARAMETER_INT,
      .val_default = GAVL_VALUE_INIT_INT(5000),
      .val_min =     GAVL_VALUE_INIT_INT(0),
      .val_max =     GAVL_VALUE_INIT_INT(2000000),
    },
    {
      .name =        "network_bandwidth",
      .long_name =   TRS("Bandwidth"),
      .type =        BG_PARAMETER_STRINGLIST,
      .val_default = GAVL_VALUE_INIT_STRING("524300"),
      .multi_names = (char const *[]){ "14400",
                              "19200",
                              "28800",
                              "33600",
                              "34400",
                              "57600",
                              "115200",
                              "262200",
                              "393200",
                              "524300",
                              "1500000",
                              "10500000",
                              NULL },
      .multi_labels = (char const *[]){ TRS("14.4 Kbps (Modem)"),
                               TRS("19.2 Kbps (Modem)"),
                               TRS("28.8 Kbps (Modem)"),
                               TRS("33.6 Kbps (Modem)"),
                               TRS("34.4 Kbps (Modem)"),
                               TRS("57.6 Kbps (Modem)"),
                               TRS("115.2 Kbps (ISDN)"),
                               TRS("262.2 Kbps (Cable/DSL)"),
                               TRS("393.2 Kbps (Cable/DSL)"),
                               TRS("524.3 Kbps (Cable/DSL)"),
                               TRS("1.5 Mbps (T1)"),
                               TRS("10.5 Mbps (LAN)"),
                               NULL },
    },
    {
      .name =       "http_options",
      .long_name =  TRS("HTTP Options"),
      .type =       BG_PARAMETER_SECTION
    },
    {
      .name =        "http_shoutcast_metadata",
      .long_name =   TRS("Enable shoutcast title streaming"),
      .type =        BG_PARAMETER_CHECKBUTTON,
      .val_default = GAVL_VALUE_INIT_INT(1)
    },
    {
      .name =        "http_use_proxy",
      .long_name =   TRS("Use proxy"),
      .type =        BG_PARAMETER_CHECKBUTTON,
      .val_default = GAVL_VALUE_INIT_INT(0),
    },
    {
      .name =        "http_proxy_host",
      .long_name =   TRS("Proxy host"),
      .type =        BG_PARAMETER_STRING,
    },
    {
      .name =        "http_proxy_port",
      .long_name =   TRS("Proxy port"),
      .type =        BG_PARAMETER_INT,
      .val_min =     GAVL_VALUE_INIT_INT(1),
      .val_max =     GAVL_VALUE_INIT_INT(65535),
      .val_default = GAVL_VALUE_INIT_INT(80),
    },
    {
      .name =        "http_proxy_auth",
      .long_name =   TRS("Proxy needs authentication"),
      .type =        BG_PARAMETER_CHECKBUTTON,
      .val_default = GAVL_VALUE_INIT_INT(0),
    },
    {
      .name =        "http_proxy_user",
      .long_name =   TRS("Proxy username"),
      .type =        BG_PARAMETER_STRING,
      .val_default = GAVL_VALUE_INIT_STRING(NULL),
    },
    {
      .name =        "http_proxy_pass",
      .long_name =   TRS("Proxy password"),
      .type =        BG_PARAMETER_STRING_HIDDEN,
      .val_default = GAVL_VALUE_INIT_STRING(NULL),
    },
    {
      .name =       "rtsp_options",
      .long_name =  TRS("RTSP Options"),
      .type =       BG_PARAMETER_SECTION
    },
    {
      .name =        "rtp_try_tcp",
      .long_name =   TRS("Try RTP over TCP"),
      .type =        BG_PARAMETER_CHECKBUTTON,
      .val_default = GAVL_VALUE_INIT_INT(0),
      .help_string = TRS("Use this if your filewall blocks all UDP traffic. Not all servers support TCP"),
    },
    {
      .name =        "rtp_port_base",
      .long_name =   TRS("Port base for RTP"),
      .type =        BG_PARAMETER_INT,
      .val_min =     GAVL_VALUE_INIT_INT(0),
      .val_max =     GAVL_VALUE_INIT_INT(65530),
      .val_default = GAVL_VALUE_INIT_INT(0),
      .help_string = TRS("Port base for RTP over UDP. Values of 1024 or smaller enable random ports (recommended for RTSP aware firewalls). Values larger than 1024 define the base port. 2 consecutive ports are used for each A/V stream, these must be accessible through the firewall. Odd values are rounded to the next even value."),
    },
    {
      .name =       "ftp_options",
      .long_name =  TRS("FTP Options"),
      .type =       BG_PARAMETER_SECTION
    },
    {
      .name =        "ftp_anonymous",
      .long_name =   TRS("Login as anonymous"),
      .type =        BG_PARAMETER_CHECKBUTTON,
      .val_default = GAVL_VALUE_INIT_INT(1)
    },
    {
      .name =        "ftp_anonymous_password",
      .long_name =   TRS("Anonymous ftp password"),
      .type =        BG_PARAMETER_STRING,
      .val_default = GAVL_VALUE_INIT_STRING("gates@nanosoft.com")
    },
    {
      .name =       "subtitle_options",
      .long_name =  TRS("Subtitle Options"),
      .type =       BG_PARAMETER_SECTION
    },
    {
      .name =        "seek_subtitles",
      .long_name =   TRS("Seek external subtitles"),
      .type =        BG_PARAMETER_STRINGLIST,
      .val_default =  GAVL_VALUE_INIT_STRING("video"),
      .multi_names =  (char const *[]){ "never", "video", "always", NULL },
      .multi_labels = (char const *[]){ TRS("Never"),
                               TRS("For video files only"),
                               TRS("Always"),
                               NULL },
      .help_string =  TRS("If the input is a regular file, gmerlin_avdecoder can scan the\
 directory for matching subtitle files. For a file movie.mpg, possible\
 subtitle files are e.g. movie_english.srt, movie_german.srt. The\
 rule is, that the first part of the filename of the subtitle file\
 must be equal to the movie filename up to the extension.\
 Furthermore, the subtitle filename must have an extension supported by\
 any of the subtitle readers. Subtitle seeking can be disabled, enabled for video files or \
enabled for all files.")
    },
    {
      .name =        "default_subtitle_encoding",
      .long_name =   TRS("Default subtitle encoding"),
      .type =        BG_PARAMETER_STRING,
      .val_default = GAVL_VALUE_INIT_STRING("LATIN1"),
      .help_string = TRS("This sets the default encoding for text subtitles,\
when the original encoding is unknown. It must be a character set name\
recognized by iconv. Type 'iconv -l' at the commandline for a list of \
supported encodings."),
    },
    {
      .name =       "misc_options",
      .long_name =  TRS("Misc options"),
      .type =       BG_PARAMETER_SECTION,
    },
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
    {
      .name =        "cache_time",
      .long_name =   TRS("Cache time (milliseconds)"),
      .type =        BG_PARAMETER_INT,
      .val_default = GAVL_VALUE_INIT_INT(500),
      .help_string = TRS("If building an index takes longer than the specified time, it will be cached."),
    },
    {
      .name =        "cache_size",
      .long_name =   TRS("Cache size (Megabytes)"),
      .type =        BG_PARAMETER_INT,
      .val_default = GAVL_VALUE_INIT_INT(20),
      .help_string = TRS("Set the maximum total size of the cache directory."),
    },
    { /* End of parameters */ }
  };

static const bg_parameter_info_t * get_parameters_avdec(void * priv)
  {
  return parameters;
  }

/* TODO: Build these dynamically */

static char const * const protocols = "http https ftp rtsp smb mms pnm stdin hls hlss";

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
  "application/x-subrip ";

static const char * get_mimetypes(void * priv)
  {
  return mimetypes;
  }

static char const * const extensions = "avi asf asx wmv rm ra ram mov wav mp4 m4a 3gp qt au aiff aif mp3 mpg mpeg vob m3u pls ogg flac aac mpc spx vob wv tta gsm vp5 vp6 voc opus wma ac3 mp2 ogv";

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
      
    },
    .get_protocols = get_protocols,
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
    
    .skip_video =      bg_avdec_skip_video,

    .get_src = bg_avdec_get_src,
    
    /* Stop playback, close all decoders */
    .stop = NULL,
    .close = bg_avdec_close,
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
