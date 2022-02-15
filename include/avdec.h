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

/* Public entry points */

/**
 * @file avdec.h
 * external api header.
 */

#include <gavl/gavl.h>
#include <gavl/compression.h>
#include <gavl/metadata.h>
#include <gavl/chapterlist.h>
#include <gavl/connectors.h>
#include <gavl/edl.h>
#include <gavl/msg.h>

#include "bgavdefs.h" // This is ugly, but works

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup decoding Decoding of multimedia streams
 *
 *  Setting up a decoder is a multi-step process. It might be a bit complicated,
 *  but it provides support for many different multimedia sources through a single
 *  API.
 *
 *  First you need to create a decoder instance (see \ref decoder). Then you might want to
 *  change some options and set callbacks (see \ref options). The opening can happen with
 *  different functions (see \ref opening). If you also open URLs, it can happen, that
 *  you opened a redirector (see \ref redirector).
 *
 *  If you have no redirector, the decoder can have an arbitrary number of tracks. You
 *  can query various informations about the tracks and must select the track you want
 *  (see \ref track).
 *
 *  Once a track is opened, you must set up the streams (See \ref streams). To tell the decoder,
 *  that you are done, call \ref bgav_start. This will start all codecs. Once the codecs are initialized,
 *  you can get informations about the streams (see \ref stream_info).
 *  Then you are ready to decode media frames (see \ref decode).
 *  In many streams you can also seek  (see \ref seeking).
 *  
 */ 

/** \defgroup decoder Creating and destroying a decoder
 *  \ingroup decoding
 */

/*! \ingroup decoder
 * \brief Opaque decoder structure
 *
 * You don't want to know, what's inside here
 */

typedef struct bgav_s bgav_t;

/*! \ingroup decoder
 * \brief Create a decoder instance
 * \returns A newly allocated decoder instance
 */

BGAV_PUBLIC
bgav_t * bgav_create();

/** \defgroup options Configuration of a decoder
 *  \ingroup decoding
 *
 *  These functions are used to configure a decoder and to set callback functions.
 *  After you created a \ref bgav_t object, get it's options
 *  with \ref bgav_get_options. All options are returned in an option container
 *  (\ref bgav_options_t), and the bgav_options_set_* functions allow you
 *  to change single options. 
 *  As an alternative, you can create an independent option container with
 *  \ref bgav_options_create, change it as you like and use \ref bgav_options_copy to
 *  copy them to the container you got with \ref bgav_get_options.
 *
 *  All options become valid with (and should not be changed after) the
 *  call to one of the bgav_open* functions.
 */

/** \defgroup opening Opening a media stream
 *  \ingroup decoding
 *  \brief Various methods of opening a media stream
 */

/** \defgroup metadata Metadata
 */

/** \ingroup metadata
 * \brief Opaque metadata container
 *
 * This structure is returned by \ref bgav_get_metadata
 */

typedef gavl_dictionary_t bgav_metadata_t;

/** \ingroup metadata
 * \brief Get the author (or composer) of the track
 * \param metadata Metadata container
 * \returns The author of the track in UTF-8 or NULL
 */

BGAV_PUBLIC
const char * bgav_metadata_get_author(const bgav_metadata_t*metadata);

/** \ingroup metadata
 * \brief Get the title of the track
 * \param metadata Metadata container
 * \returns The title of the track in UTF-8 or NULL
 */

BGAV_PUBLIC
const char * bgav_metadata_get_title(const bgav_metadata_t * metadata);

/** \ingroup metadata
 * \brief Get an additional comment of the track
 * \param metadata Metadata container
 * \returns The comment in UTF-8 or NULL
 */

BGAV_PUBLIC
const char * bgav_metadata_get_comment(const bgav_metadata_t * metadata);

/** \ingroup metadata
 * \brief Get the copyright notice of the track
 * \param metadata Metadata container
 * \returns The copyright notice in UTF-8 or NULL
 */

BGAV_PUBLIC
const char * bgav_metadata_get_copyright(const bgav_metadata_t * metadata);

/** \ingroup metadata
 * \brief Get the album this track comes from
 * \param metadata Metadata container
 * \returns The album in UTF-8 or NULL
 */

BGAV_PUBLIC
const char * bgav_metadata_get_album(const bgav_metadata_t * metadata);

/** \ingroup metadata
 * \brief Get the artist (or performer) of this track
 * \param metadata Metadata container
 * \returns The artist in UTF-8 or NULL
 */

BGAV_PUBLIC
const char * bgav_metadata_get_artist(const bgav_metadata_t * metadata);

/** \ingroup metadata
 * \brief Get the album artist of this track
 * \param metadata Metadata container
 * \returns The album artist in UTF-8 or NULL
 */

BGAV_PUBLIC
const char * bgav_metadata_get_albumartist(const bgav_metadata_t * metadata);

/** \ingroup metadata
 * \brief Get the genre this track belongs to
 * \param metadata Metadata container
 * \returns The genre in UTF-8 or NULL
 */

BGAV_PUBLIC
const char * bgav_metadata_get_genre(const bgav_metadata_t * metadata);

/** \ingroup metadata
 * \brief Get the date of the recording
 * \param metadata Metadata container
 * \returns The date in UTF-8 or NULL
 */

BGAV_PUBLIC
const char * bgav_metadata_get_date(const bgav_metadata_t * metadata);

/** \ingroup metadata
 * \brief Get the track index
 * \param metadata Metadata container
 * \returns The track index or 0
 */

BGAV_PUBLIC
int bgav_metadata_get_track(const bgav_metadata_t * metadata);

/***************************************************
 * Housekeeping Functions
 ***************************************************/

/***************************************************
 * Set parameters
 ***************************************************/


/** \ingroup options
 * \brief Opaque option container
 */

typedef struct bgav_options_s bgav_options_t;

/** \ingroup options
 * \brief Get the options of a decoder instance
 * \returns Options
 *
 * Use this to get the options container. You can use the bgav_options_set_* functions
 * to change the options. Options will become valid when you call one of the
 * bgav_open*() functions.
 */

BGAV_PUBLIC
bgav_options_t * bgav_get_options(bgav_t * bgav);

/** \ingroup options
 *  \brief Create an options container
 *  \returns A newly allocated option container
 *
 * This function returns a newly allocated option container
 * filled with default values. Use this, if you need to store
 * decoder options independently from decoder instances.
 * To destroy the container again, use \ref bgav_options_destroy.
 *
 * To pass the options to a decoder structure, use \ref bgav_get_options
 * and \ref bgav_options_copy.
 *
 */

BGAV_PUBLIC
bgav_options_t * bgav_options_create();

/** \ingroup options
 *  \brief Destroy option cotainer.
 *  \param opt option cotainer
 *
 * Use the only to destroy options, you created with \ref bgav_options_create.
 * Options returned by \ref bgav_get_options are owned by the \ref bgav_t instance,
 * and must not be freed by you.
 */

BGAV_PUBLIC
void bgav_options_destroy(bgav_options_t * opt);

/** \ingroup options
 *  \brief Copy options
 *  \param dst Destination
 *  \param src Source
 */

BGAV_PUBLIC
void bgav_options_copy(bgav_options_t * dst, const bgav_options_t * src);

/** \ingroup options
 *  \brief Set connect timeout
 *  \param opt Option container
 *  \param timeout Timeout in milliseconds.
 *
 *  This timeout will be used until the connection is ready to
 *  receive media data.
 */

BGAV_PUBLIC
void bgav_options_set_connect_timeout(bgav_options_t * opt, int timeout);

/** \ingroup options
 *  \brief Set read timeout
 *  \param opt Option container
 *  \param timeout Timeout in milliseconds.
 *
 *  This timeout will be used during streaming to detect server/connection
 *  failures.
 */

BGAV_PUBLIC
void bgav_options_set_read_timeout(bgav_options_t * opt, int timeout);

/** \ingroup options
 *  \brief Set RTP port pase
 *  \param opt Option container
 *  \param p Port base
 *
 *  Values below and including 1024 enable random
 *  ports. Random ports should be used if you have no or an
 *  RTSP aware firewall. Values above 1024 enable a fixed base port.
 *  For usual videos (one audio- one videostream) you need 4 consecutive
 *  open ports starting with the base port. If your firewall blocks all UDP
 *  traffic, use \ref bgav_options_set_rtp_try_tcp.
 */


BGAV_PUBLIC
void bgav_options_set_rtp_port_base(bgav_options_t * opt, int p);

/** \ingroup options
 *  \brief Try TCP before UDP
 *  \param opt Option container
 *  \param enable 1 to try TCP, 0 else
 *
 *  Use this if your firewall blocks all UDP traffic. Not all servers,
 *  however, support TCP fallback.
 */

BGAV_PUBLIC
void bgav_options_set_rtp_try_tcp(bgav_options_t * opt, int enable);

/** \ingroup options
 *  \brief Set network bandwidth
 *  \param opt Option container
 *  \param bandwidth Bandwidth of your internet connection (in bits per second)
 *
 *  Some network protocols allow
 *  to select among multiple streams according to the speed of the internet connection.
 */

BGAV_PUBLIC
void bgav_options_set_network_bandwidth(bgav_options_t * opt, int bandwidth);


/* HTTP Options */

/** \ingroup options
 *  \brief Set proxy usage
 *  \param opt Option container
 *  \param enable Set 1 of you use a http proxy, 0 else.
 *
 *  Set usage of a http proxy. If you enable proxies, you must specify the proxy
 *  host with \ref bgav_options_set_http_proxy_host and the proxy port with
 *  \ref bgav_options_set_http_proxy_port.
 */

BGAV_PUBLIC
void bgav_options_set_http_use_proxy(bgav_options_t* opt, int enable);

/** \ingroup options
 *  \brief Set proxy host
 *  \param opt Option container
 *  \param host Hostname of the proxy.
 *
 *  Note that you must enable proxies with \ref bgav_options_set_http_use_proxy before
 *  it's actually used.
 */

BGAV_PUBLIC
void bgav_options_set_http_proxy_host(bgav_options_t* opt, const char * host);

/** \ingroup options
 *  \brief Set proxy port
 *  \param opt Option container
 *  \param port Port of the proxy.
 *
 *  Note that you must enable proxies with \ref bgav_options_set_http_use_proxy before
 *  it's actually used.
 */

BGAV_PUBLIC
void bgav_options_set_http_proxy_port(bgav_options_t* opt, int port);

/** \ingroup options
 *  \brief Enable or disable proxy authentication.
 *  \param opt Option container
 *  \param enable Set 1 if your http proxy needs authentication, 0 else.
 *
 *  If you enable http proxy authentication, you must specify the username
 *  host with \ref bgav_options_set_http_proxy_user and the proxy password with
 *  \ref bgav_options_set_http_proxy_pass.
 */

BGAV_PUBLIC
void bgav_options_set_http_proxy_auth(bgav_options_t* opt, int enable);

/** \ingroup options
 *  \brief Set proxy username
 *  \param opt Option container
 *  \param user The username for the proxy.
 *
 *  Note that you must enable proxy authentication with \ref bgav_options_set_http_proxy_auth.
 */

BGAV_PUBLIC
void bgav_options_set_http_proxy_user(bgav_options_t* opt, const char * user);

/** \ingroup options
 *  \brief Set proxy password
 *  \param opt Option container
 *  \param pass The password for the proxy.
 *
 *  Note that you must enable proxy authentication with \ref bgav_options_set_http_proxy_auth.
 */

BGAV_PUBLIC
void bgav_options_set_http_proxy_pass(bgav_options_t* opt, const char * pass);

/** \ingroup options
 *  \brief Enable or disable shoutcast metadata streaming
 *  \param opt Option container
 *  \param enable Set to 1 if the decoder should try to get shoutcast metadata, 0 else
 *
 * Normally it's ok to enable shoutcast metadata streaming even if we connect to non-shoutcast servers.
 */

BGAV_PUBLIC
void bgav_options_set_http_shoutcast_metadata(bgav_options_t* opt, int enable);

/* Set FTP options */

/** \ingroup options
 *  \brief Enable or disable anonymous ftp login
 *  \param opt Option container
 *  \param enable Set to 1 if the decoder should try log anonymously into ftp servers, 0 else
 */

BGAV_PUBLIC
void bgav_options_set_ftp_anonymous(bgav_options_t* opt, int enable);

/** \ingroup options
 *  \brief Set anonymous password
 *  \param opt Option container
 *  \param pass
 *
 *  Note that you must enable anonymous ftp login with \ref bgav_options_set_ftp_anonymous.
 */

BGAV_PUBLIC
void bgav_options_set_ftp_anonymous_password(bgav_options_t* opt, const char* pass);

/** \ingroup options
 *  \brief Set default subtitle encoding
 *  \param opt Option container
 *  \param encoding Encoding
 *
 *  This sets the default encoding for text subtitles, when the right
 *  encoding is unknown. It must be a character set name recognized by
 *  iconv. Type "iconv -l" at the commandline for a list of supported
 *  encodings).
 */

BGAV_PUBLIC
void bgav_options_set_default_subtitle_encoding(bgav_options_t* opt,
                                                const char* encoding);

/** \ingroup options
 *  \brief Enable dynamic range control
 *  \param opt Option container
 *  \param audio_dynrange 1 for enabling dynamic range control.
 *
 *  This enables dynamic range control for codecs, which supports it.
 *  By default dynamic range control is enabled. Use this function to switch it
 *  off for the case, that you have a better dynamic range control in your processing pipe.
 */

BGAV_PUBLIC
void bgav_options_set_audio_dynrange(bgav_options_t* opt,
                                     int audio_dynrange);


/** \ingroup options
 *  \brief Try to be sample accurate
 *  \param opt Option container
 *  \param enable Specifies how sample accurate mode should be enabled (see below)
 *
 *  If enable is 0, sample accuracy will never be enabled. If enable is 1,
 *  sample accuracy will always (whenever possible) be enabled. If enable is 2,
 *  sample accuracy will only be enabled, if the file would not be seekable otherwise.
 *
 *  When switching to sample accurate mode, other demultiplexing
 *  methods are enabled, which might slow things down a bit.
 */

BGAV_PUBLIC
void bgav_options_set_sample_accurate(bgav_options_t*opt, int enable);

/** \ingroup options
 *  \brief Set the index creation time for caching
 *  \param opt Option container
 *  \param t Time (in milliseconds) needed for creating the index
 *
 *  This option controls, which indices are cached, based on the
 *  time needed for creating the index. Indices, whose creation takes
 *  longer than the specified time will be cached. If you set this to
 *  zero, all indices are cached.
 */

BGAV_PUBLIC
void bgav_options_set_cache_time(bgav_options_t*opt, int t);

/** \ingroup options
 *  \brief Set the maximum total size of the index cache
 *  \param opt Option container
 *  \param s Maximum size (in megabytes) of the whole cache directory
 *
 *  If a new index is created and the size becomes larger than
 *  the maximum size, older indices will be deleted. Zero means infinite.
 */

BGAV_PUBLIC
void bgav_options_set_cache_size(bgav_options_t*opt, int s);

/** \ingroup options
 *  \brief Enable external subtitle files
 *  \param opt Option container
 *  \param seek_subtitles If 1, subtitle files will be seeked for video files. If 2, subtitles
        will be seeked for all files.
 *
 *  If the input is a regular file, gmerlin_avdecoder can scan the
 *  directory for matching subtitle files. For a file movie.mpg, possible
 *  subtitle files are e.g. movie_english.srt, movie_german.srt. The
 *  rule is, that the first part of the filename of the subtitle file
 *  must be equal to the movie filename up to the extension (hiere: .mpg).
 *  Furthermore, the subtitle filename must have an extension supported by
 *  any of the subtitle readers.
 */

BGAV_PUBLIC
void bgav_options_set_seek_subtitles(bgav_options_t* opt,
                                    int seek_subtitles);

/** \ingroup options
 *  \brief Set postprocessing level
 *  \param opt Option container
 *  \param pp_level Value between 0 (no postprocessing) and 6 (maximum postprocessing)
 *
 *  Warning: This function is depracated, use \ref bgav_options_set_postprocessing_level
 *  instead.
 */

BGAV_PUBLIC
void bgav_options_set_pp_level(bgav_options_t* opt,
                               int pp_level);

/** \ingroup options
 *  \brief Set postprocessing level
 *  \param opt Option container
 *  \param pp_level Value between 0.0 (no postprocessing) and 1.0 (maximum postprocessing)
 *
 *  Since 1.0.2
 */

BGAV_PUBLIC
void bgav_options_set_postprocessing_level(bgav_options_t* opt,
                                           float pp_level);

/** \ingroup options
 *  \brief Set number of threads
 *  \param opt Option container
 *  \param threads Number of threads to use
 *
 *   Not all codecs support this
 */

BGAV_PUBLIC
void bgav_options_set_threads(bgav_options_t * opt, int threads);

  
/** \ingroup options
 *  \brief Set DVB channels file
 *  \param opt Option container
 *  \param file Name of the channel configurations file
 *
 *  The channels file must have the format of the dvb-utils
 *  programs (like szap, tzap). If you don't set this file,
 *  several locations like $HOME/.tzap/channels.conf will be
 *  searched.
 */

BGAV_PUBLIC
void bgav_options_set_dvb_channels_file(bgav_options_t* opt,
                                        const char * file);

/** \ingroup options
 *  \brief Preference of ffmpeg demultiplexers
 *  \param opt Option container
 *  \param prefer 1 to prefer ffmpeg demultiplexers, 0 else
 *
 *  Usually, native demultiplexers are prefered over the
 *  ffmpeg ones. This function allows you to chnage that.
 *  If gmerlin_avdecoder was compiled without libavformat support,
 *  this function is meaningless.
 */

BGAV_PUBLIC
void bgav_options_set_prefer_ffmpeg_demuxers(bgav_options_t* opt,
                                             int prefer);

/** \ingroup options
 *  \brief Exports the date and time as timecode field of DV streams
 *  \param opt Option container
 *  \param datetime 1 to export date and time as timecodes, 0 else
 */

BGAV_PUBLIC
void bgav_options_set_dv_datetime(bgav_options_t* opt,
                                  int datetime);

/** \ingroup options
 *  \brief Shrink factor
 *  \param opt Option container
 *  \param factor Exponent of the shrink factor
 *
 *  This enables downscaling of images while decoding.
 *  Currently only supported for JPEG-2000.
 */

BGAV_PUBLIC
void bgav_options_set_shrink(bgav_options_t* opt,
                             int factor);

/** \ingroup options
 *  \brief VDPAU acceleration
 *  \param opt Option container
 *  \param vdpau 1 to enable vdpau, 0 else
 *
 *  Since 1.0.2
 *
 *  This is a noop now since VDPAU support was removed. Might get enabled
 *  again in the future.
 */

BGAV_PUBLIC
void bgav_options_set_vdpau(bgav_options_t* opt,
                            int vdpau);

/** \ingroup options
 *  \brief VAAPI acceleration
 *  \param opt Option container
 *  \param vaapi 1 to enable vaapi, 0 else
 *
 *  Since 1.0.2
 *
 */

BGAV_PUBLIC
void bgav_options_set_vaapi(bgav_options_t* opt,
                            int vaapi);

/** \ingroup options
 *  \brief Dump file headers
 *  \param opt Option container
 *  \param enable 1 to dump file headers, 0 else
 *
 *  Since 1.1.1
 */

BGAV_PUBLIC
void bgav_options_set_dump_headers(bgav_options_t* opt,
                                   int enable);

/** \ingroup options
 *  \brief Dump file indices
 *  \param opt Option container
 *  \param enable 1 to dump file indices, 0 else
 *
 *  Since 1.1.1
 */

BGAV_PUBLIC
void bgav_options_set_dump_indices(bgav_options_t* opt,
                                   int enable);

  
/** \ingroup options
 *  \brief Dump packets
 *  \param opt Option container
 *  \param enable 1 to dump packets, 0 else
 *
 *  Since 1.1.1
 */

BGAV_PUBLIC
void bgav_options_set_dump_packets(bgav_options_t* opt,
                                   int enable);


/** \ingroup options
 *  \brief Enumeration for log levels
 *
 * These will be called from within log callbacks
 */
 

/* Set callbacks */

/** \ingroup options
 *  \brief Function to be called if the metadata change
 *  \param data The data you passed to \ref bgav_options_set_metadata_change_callback.
 *  \param metadata The new metadata of the track
 *
 *  This function will be called whenever the metadata of the track changed. Such changes
 *  can have different reasons, the most obvious one is a song change in a webradio stream.
 */


typedef void (*bgav_metadata_change_callback)(void*data, const bgav_metadata_t * metadata);

/** \ingroup options
 *  \brief Set the callback for metadata change events
 *  \param opt Option container
 *  \param callback The callback
 *  \param data Some data you want to get passed to the callback
 */

BGAV_PUBLIC void
bgav_options_set_metadata_change_callback(bgav_options_t* opt,
                                          bgav_metadata_change_callback callback,
                                          void * data);

/** \ingroup options
 *  \brief Function to be called if the input module is buffering data
 *  \param data The data you passed to \ref bgav_options_set_buffer_callback.
 *  \param percentage The percentage done so far (0.0 .. 1.0)
 *
 *  This function will be called when the input module fills a buffer.
 *  It is called multiple times with increasing percentage.
 */

typedef void (*bgav_buffer_callback)(void*data, float percentage);

/** \ingroup options
 *  \brief Set the callback for buffering notification
 *  \param opt Option container
 *  \param callback The callback
 *  \param data Some data you want to get passed to the callback
 */
  
BGAV_PUBLIC void
bgav_options_set_buffer_callback(bgav_options_t* opt,
                                 bgav_buffer_callback callback,
                                 void * data);

/** \ingroup options
 *  \brief Function to be called if the input module needs authentication data
 *  \param data The data you passed to \ref bgav_options_set_user_pass_callback.
 *  \param resource A string describing the resource (e.g. the hostname of the server)
 *  \param username Returns the username.
 *  \param password Returns the password.
 *  \returns 1 of the username and password are valid, 0 if the user cancelled the authentication.
 *
 *  This is called during one of the bgav_open* calls.
 */

typedef int (*bgav_user_pass_callback)(void*data, const char * resource,
                                       char ** username, char ** password);
  
/** \ingroup options
 *  \brief Set the callback for user authentication
 *  \param opt Option container
 *  \param callback The callback
 *  \param data Some data you want to get passed to the callback
 *
 *  Note that there is no default authentication built into the library.
 *  Therefore you MUST set a callback, otherwise urls which require the user to
 *  enter authentication data, cannot be openend.
 */

BGAV_PUBLIC void
bgav_options_set_user_pass_callback(bgav_options_t* opt,
                                    bgav_user_pass_callback callback,
                                    void * data);

/** \ingroup options
 *  \brief Function to be called if a change of the aspect ratio was detected
 *  \param data The data you passed to \ref bgav_options_set_aspect_callback.
 *  \param stream Index of the video stream (starts with 0)
 *  \param pixel_width New pixel width
 *  \param pixel_height New pixel height
 *
 *  This is called during \ref bgav_read_video.
 */

typedef void (*bgav_aspect_callback)(void*data, int stream,
                                     int pixel_width, int pixel_height);


/** \ingroup options
 *  \brief Set aspect ratio change callback
 *  \param opt Option container
 *  \param callback The callback
 *  \param data Some data you want to get passed to the callback
 */

BGAV_PUBLIC void
bgav_options_set_aspect_callback(bgav_options_t* opt,
                                 bgav_aspect_callback callback,
                                 void * data);

/** \ingroup options
 *  \brief Function to be called periodically while an index is built
 *  \param data The data you passed to \ref bgav_options_set_index_callback.
 *  \param perc Percentage completed to far
 *
 */

typedef void (*bgav_index_callback)(void*data, float percentage);

/** \ingroup options
 *  \brief Set index build callback
 *  \param opt Option container
 *  \param callback The callback
 *  \param data Some data you want to get passed to the callback
 */

BGAV_PUBLIC void
bgav_options_set_index_callback(bgav_options_t* opt,
                                bgav_index_callback callback,
                                void * data);

BGAV_PUBLIC void
bgav_set_msg_callback_by_id(bgav_t* b,
                            int id,
                            gavl_handle_msg_func callback,
                            void * data);

  
  
/* Device description */

/** \defgroup devices Device description
 *  \brief Device description
 *
 *  The input modules, which access special hardware devices (e.g. CD drives),
 *  have autoscanning functionality built in. You can call one of the bgav_find_devices_* functions
 *  to get a list of supported devices (see \ref bgav_device_info_t). The created list must be
 *  freed with \ref bgav_device_info_destroy when it's no longer used.
 */

/** \ingroup devices
 *  \brief Info structure for a device.
 *
 * This contains information about a device: The device node name (which can be passed to
 * the bgav_open_* function) and, if available, a human readable description. The last element
 * in the array has the device string set to NULL and terminates the array.
 */

typedef struct
  {
  char * device; /*!< Can be passed to the bgav_open_*() function */
  char * name;   /*!< Device name, might be NULL */
  } bgav_device_info_t;

/* Scan for devices */

/** \ingroup devices
 *  \brief Scan for VCD capable devices
 *  \returns A \ref bgav_device_info_t array or NULL
 *
 *  Free the returned array with \ref bgav_device_info_destroy
 */

BGAV_PUBLIC
bgav_device_info_t * bgav_find_devices_vcd();

/** \ingroup devices
 *  \brief Test if a device is VCD capable
 *  \param device The device node name
 *  \param name Returns a human readable decription in a newly allocated string or NULL
 *  \returns 1 if the device can play VCDs, 0 else.
*/

BGAV_PUBLIC
int bgav_check_device_vcd(const char * device, char ** name);

/** \ingroup devices
 *  \brief Scan for DVD capable devices
 *  \returns A \ref bgav_device_info_t array or NULL
 *
 *  Free the returned array with \ref bgav_device_info_destroy
 */

BGAV_PUBLIC
bgav_device_info_t * bgav_find_devices_dvd();

/** \ingroup devices
 *  \brief Test if a device is DVD capable
 *  \param device The device node name
 *  \param name Returns a human readable decription in a newly allocated string or NULL
 *  \returns 1 if the device can play DVDs, 0 else.
*/

BGAV_PUBLIC
int bgav_check_device_dvd(const char * device, char ** name);

/** \ingroup devices
 *  \brief Scan for DVB capable devices
 *  \returns A \ref bgav_device_info_t array or NULL
 *
 *  Free the returned array with \ref bgav_device_info_destroy
 */

BGAV_PUBLIC
bgav_device_info_t * bgav_find_devices_dvb();

/** \ingroup devices
 *  \brief Test if a device is DVB capable
 *  \param device The directory (e.g. /dev/dvb/adaptor0)
 *  \param name Returns a human readable decription in a newly allocated string or NULL
 *  \returns 1 if the device is ready to receive DVB streams, 0 else.
*/

BGAV_PUBLIC
int bgav_check_device_dvb(const char * device, char ** name);

/** \ingroup devices
 *  \brief Destroy a device info array
 *  \param arr A device info returned by \ref bgav_find_devices_dvd, \ref bgav_find_devices_dvb or \ref bgav_find_devices_dvd
 */


BGAV_PUBLIC
void bgav_device_info_destroy(bgav_device_info_t * arr);

/** \ingroup devices
 *  \brief Eject a disc
 *  \param device Device name
 *  \return 1 if the disc could be ejected, 0 else
 */

BGAV_PUBLIC
int bgav_eject_disc(const char * device);

/** \ingroup devices
 *  \brief Get the name of a disc
 *  \param bgav A decoder instance
 *  \return The name of the disc, or NULL if it's not known or irrelevant
 */

BGAV_PUBLIC
const char * bgav_get_disc_name(bgav_t * bgav);

/******************************************************
 * Open
 ******************************************************/

/* Open a file or URL, return 1 on success */

/** \ingroup opening
 *  \brief Open a file or URL
 *  \param bgav A decoder instance
 *  \param location The URL or path to open
 *  \returns 1 if the location was successfully openend, 0 else.
 */

BGAV_PUBLIC
int bgav_open(bgav_t * bgav, const char * location);

/** \ingroup opening
 *  \brief Open a VCD device
 *  \param bgav A decoder instance
 *  \param location The device node
 *  \returns 1 if the VCD device was successfully openend, 0 else.
 */

BGAV_PUBLIC
int bgav_open_vcd(bgav_t * bgav, const char * location);

/** \ingroup opening
 *  \brief Open a DVD device
 *  \param bgav A decoder instance
 *  \param location The device node
 *  \returns 1 if the DVD device was successfully openend, 0 else.
 */

BGAV_PUBLIC
int bgav_open_dvd(bgav_t * bgav, const char * location);

/** \ingroup opening
 *  \brief Open a DVB device
 *  \param bgav A decoder instance
 *  \param location The device directory
 *  \returns 1 if the DVB device was successfully openend, 0 else.
 *
 *  This function will search your system for channel configuration files,
 *  which are created by other tools. The channels are then available as
 *  normal tracks.
 */

BGAV_PUBLIC
int bgav_open_dvb(bgav_t * bgav, const char * location);

/** \ingroup opening
 *  \brief Open a decoder with callbacks
 *  \param bgav A decoder instance
 *  \param read_callback Callback for reading data
 *  \param seek_callback Callback for seeking
 *  \param priv Private argument for the callbacks
 *  \param filename The filename of the input or NULL if this info is not known.
 *  \param mimetype The mimetype of the input or NULL if this info is not known.
 *  \param total_bytes File size in bytes or 0 if this info is not known.
 *  \returns 1 on success, 0 else.
 */

BGAV_PUBLIC
int bgav_open_callbacks(bgav_t * bgav,
                        int (*read_callback)(void * priv, uint8_t * data, int len),
                        int64_t (*seek_callback)(void * priv, int64_t pos, int whence),
                        void * priv,
                        const char * filename, const char * mimetype, int64_t total_bytes);

  



/* Close and destroy everything */

/** \ingroup decoder
 *  \brief Close a decoder and free all associated memory
 *  \param bgav A decoder instance
 */

BGAV_PUBLIC
void bgav_close(bgav_t * bgav);

/** \ingroup decoder
 *  \brief Get an EDL from an open decoder
 *  \param bgav A decoder instance
 *  \returns The edl or NULL
 */

BGAV_PUBLIC
const gavl_dictionary_t * bgav_get_edl(bgav_t * bgav);

/***************************************************
 * Check for redirecting: You MUST check if you opened
 * a redirector, because reading data from redirectors
 * crashes
 * After you read the URLs, close the bgav_t object
 * and open a new one with one of the URLs.
 ***************************************************/

/** \defgroup redirector Redirector support
 *  \ingroup decoding
 *  \brief Redirector support
 *
 *  After opening the decoder with \ref bgav_open, you must check if the
 *  opened stream is actually a redirector with \ref bgav_is_redirector.
 *  If this returns 1, get the number of urls with \ref bgav_redirector_get_num_urls.
 *  Then, for each URL, get the address with \ref bgav_redirector_get_url and the
 *  title of the URL with \ref bgav_redirector_get_name.
 *  After you have this info, close and destroy the decoder with \ref bgav_close and
 *  open a newly created decoder with one of the URLs you got.
 */

/** \ingroup redirector
 *  \brief Query if the decoder opened a redirector
 *  \param bgav A decoder instance
 *  \returns 1 if the decoder opened a redirector, 0 else
 *
 *  This function MUST be called after \ref bgav_open to query if we
 *  openend a redirector.
 */

#if 0  
 

/** \ingroup redirector
 *  \brief Get the number of URLs found in the redirector
 *  \param bgav A decoder instance
 *  \returns The number of URLs.
 */

BGAV_PUBLIC
int bgav_redirector_get_num_urls(bgav_t * bgav);

/** \ingroup redirector
 *  \brief Get the address of an URL
 *  \param bgav A decoder instance
 *  \param index Index of the url (starting with 0)
 *  \returns The URL (can be passed to a subsequent \ref bgav_open)
 */

BGAV_PUBLIC
const char * bgav_redirector_get_url(bgav_t * bgav, int index);

/** \ingroup redirector
 *  \brief Get the address of an URL
 *  \param bgav A decoder instance
 *  \param index Index of the url (starting with 0)
 *  \returns The name of the stream or NULL if this information is not present.
 */

BGAV_PUBLIC
const char * bgav_redirector_get_name(bgav_t * bgav, int index);

/** \ingroup redirector
 *  \brief Get the metadata for an URL
 *  \param bgav A decoder instance
 *  \param index Index of the url (starting with 0)
 *  \returns The metadata assiciated with this URL
 */

BGAV_PUBLIC
const gavl_dictionary_t * bgav_redirector_get_metadata(bgav_t * b, int index);

#endif
  
/***************************************************
 * Get information about the file
 ***************************************************/

/** \defgroup track Query and select tracks
 *  \ingroup decoding
 *
 * Each opened decoder can have multiple tracks. You must select the track you wish to decode before
 * you can do anything else.
 */

/** \ingroup track
 *  \brief Get the number of tracks
 *  \param bgav A decoder instance
 *  \returns The number of tracks.
 */

BGAV_PUBLIC
int bgav_num_tracks(bgav_t * bgav);

/** \ingroup track
 *  \brief Get a technical description of the format
 *  \param bgav A decoder instance
 *  \returns Description
 */

BGAV_PUBLIC
const char * bgav_get_description(bgav_t * bgav);

/** \ingroup track
 *  \brief Get the duration of a track
 *  \param bgav A decoder instance
 *  \param track Track index (starting with 0)
 *  \returns The duration of a track or \ref GAVL_TIME_UNDEFINED if the duration is not known.
 */

BGAV_PUBLIC
gavl_time_t bgav_get_duration(bgav_t * bgav, int track);

/* Query stream numbers */

/** \ingroup track
 *  \brief Get the number of audio streams of a track
 *  \param bgav A decoder instance
 *  \param track Track index (starting with 0)
 *  \returns The number of audio streams
 */

BGAV_PUBLIC
int bgav_num_audio_streams(bgav_t * bgav, int track);

/** \ingroup track
 *  \brief Get the number of video streams of a track
 *  \param bgav A decoder instance
 *  \param track Track index (starting with 0)
 *  \returns The number of video streams
 */

BGAV_PUBLIC
int bgav_num_video_streams(bgav_t * bgav, int track);

/** \ingroup track
 *  \brief Get the number of subtitle streams of a track
 *  \param bgav A decoder instance
 *  \param track Track index (starting with 0)
 *  \returns The number of subtitle streams
 *
 *  Since version 1.3.0 the 2 categories of subtitles
 *  are referred to as text and overlay streams.
 *  The legacy functions still work as long as you use them
 *  consistently.
 */

BGAV_PUBLIC
int bgav_num_subtitle_streams(bgav_t * bgav, int track);

/** \ingroup track
 *  \brief Get the number of text streams of a track
 *  \param bgav A decoder instance
 *  \param track Track index (starting with 0)
 *  \returns The number of text subtitle streams
 */

BGAV_PUBLIC
int bgav_num_text_streams(bgav_t * bgav, int track);

/** \ingroup track
 *  \brief Get the number of overlay streams of a track
 *  \param bgav A decoder instance
 *  \param track Track index (starting with 0)
 *  \returns The number of overlay subtitle streams
 */

BGAV_PUBLIC
int bgav_num_overlay_streams(bgav_t * bgav, int track);

/** \ingroup track
 *  \brief Get the name a track
 *  \param bgav A decoder instance
 *  \param track Track index (starting with 0)
 *  \returns The track name if present or NULL.
 */

BGAV_PUBLIC
const char * bgav_get_track_name(bgav_t * bgav, int track);

/** \ingroup track
 *  \brief Get metadata for a track.
 *  \param bgav A decoder instance
 *  \param track Track index (starts with 0)
 *  \returns A metadata container (see \ref metadata)
 */

BGAV_PUBLIC
const bgav_metadata_t * bgav_get_metadata(bgav_t * bgav,int track);

/** \ingroup track
 *  \brief Get media information
 *  \param bgav A decoder instance
 *  \param 
 *  \returns A dictionary containing the media information
 *
 *  This replaces all bgav_get_* functions. It returns a dictionary,
 *  which contains the tracks which, in turn, contain the A/V streams
 *
 *  The API for extracing the infos is in gavl/trackinfo.h
 *  The returned dictionary can be passed to gavl_get_num_tracks and
 *  gavl_get_track.
 */

BGAV_PUBLIC
gavl_dictionary_t * bgav_get_media_info(bgav_t * bgav);

  
/** \ingroup track
 *  \brief Select a track
 *  \param bgav A decoder instance
 *  \param track Track index (starts with 0)
 *  \returns 0 if there was no such track, 1 else
 *
 *  Select the track. All subsequent function calls will refer to the track you
 *  selected.
 */


BGAV_PUBLIC
int bgav_select_track(bgav_t * bgav, int track);

/** \ingroup track
 *  \brief Get the number of chapters
 *  \param bgav A decoder instance
 *  \param track Track index (starts with 0)
 *  \param timescale Returns the timescale of the seekpoints
 *  \returns The number of chapters or 0 if the format doesn't support chapters
 *
 *  Chapters are simply named seekpoints. Use
 *  \ref bgav_get_chapter_time and \ref bgav_get_chapter_name
 *  to query the chapters.
 */

BGAV_PUBLIC
int bgav_get_num_chapters(bgav_t * bgav, int track, int * timescale);

/** \ingroup track
 *  \brief Get the name of a chapter
 *  \param bgav A decoder instance
 *  \param track Track index (starts with 0)
 *  \param chapter Chapter index (starts with 0)
 *  \returns The name of the chapter or NULL
 */

BGAV_PUBLIC const char *
bgav_get_chapter_name(bgav_t * bgav, int track, int chapter);

/** \ingroup track
 *  \brief Get the name of a chapter
 *  \param bgav A decoder instance
 *  \param track Track index (starts with 0)
 *  \param chapter Chapter index (starts with 0)
 *  \returns The time of the chapter
 */

BGAV_PUBLIC
int64_t bgav_get_chapter_time(bgav_t * bgav, int track, int chapter);

/** \ingroup track
 *  \brief Get the chapter list
 *  \param bgav A decoder instance
 *  \param track Track index (starts with 0)
 *  \returns A chapter list (or NULL)
 *
 *  Use this as a replacement for \ref bgav_get_num_chapters ,
 *  \ref bgav_get_chapter_name and \ref bgav_get_chapter_time
 */

BGAV_PUBLIC
const gavl_chapter_list_t * bgav_get_chapter_list(bgav_t * bgav, int track);

  
/** \defgroup streams Query and select streams
 * \ingroup decoding
 *
 * The numbers of streams are already known before selecting a track (see \ref bgav_num_audio_streams and
 * \ref bgav_num_video_streams. Note that by default, all streams are muted, which means that you cannot
 * skip the stream selection in your application.
 */

/** \ingroup streams
 *  \brief Get the language of an audio stream
 *  \param bgav A decoder instance
 *  \param stream Audio stream index (starting with 0)
 *  \returns A language string.
 */

BGAV_PUBLIC
const char * bgav_get_audio_language(bgav_t * bgav, int stream);

/** \ingroup streams
 *  \brief Get the bitrate of an audio stream
 *  \param bgav A decoder instance
 *  \param stream Audio stream index (starting with 0)
 *  \returns Bitrate in bits / sec. 0 means VBR or unknown
 */

BGAV_PUBLIC
int bgav_get_audio_bitrate(bgav_t * bgav, int stream);

/** \ingroup streams
 *  \brief Get the metadata of an audio stream of the current track
 *  \param bgav A decoder instance
 *  \param stream Audio stream index (starting with 0)
 *  \returns The metadata for the stream
 */

BGAV_PUBLIC
const bgav_metadata_t *
bgav_get_audio_metadata(bgav_t * bgav, int stream);

/** \ingroup streams
 *  \brief Get the metadata of an audio stream
 *  \param bgav A decoder instance
 *  \param t Track index (starting with 0)
 *  \param s Audio stream index (starting with 0)
 *  \returns The metadata for the stream
 */

BGAV_PUBLIC
const bgav_metadata_t * bgav_get_audio_metadata_t(bgav_t * b, int t, int s);
  
/** \ingroup streams
 *  \brief Get the language of a subtitle stream
 *  \param bgav A decoder instance
 *  \param stream Subtitle stream index (starting with 0)
 *  \returns A language string.
 */

BGAV_PUBLIC
const char * bgav_get_subtitle_language(bgav_t * bgav, int stream);

/** \ingroup streams
 *  \brief Get the metadata of a subtitle stream
 *  \param bgav A decoder instance
 *  \param stream Subtitle stream index (starting with 0)
 *  \returns The metadata for the stream
 */

BGAV_PUBLIC
const bgav_metadata_t *
bgav_get_subtitle_metadata(bgav_t * bgav, int stream);

/** \ingroup streams
 *  \brief Get the metadata of a text stream of the current track
 *  \param bgav A decoder instance
 *  \param stream Subtitle stream index (starting with 0)
 *  \returns The metadata for the stream
 */

BGAV_PUBLIC
const bgav_metadata_t *
bgav_get_text_metadata(bgav_t * b, int stream);

/** \ingroup streams
 *  \brief Get the metadata of a text stream
 *  \param bgav A decoder instance
 *  \param track Track index (starting with 0)
 *  \param stream Subtitle stream index (starting with 0)
 *  \returns The metadata for the stream
 */

BGAV_PUBLIC
const bgav_metadata_t *
bgav_get_text_metadata_t(bgav_t * b, int track, int stream);
  
/** \ingroup streams
 *  \brief Get the metadata of an overlay stream of the current track
 *  \param bgav A decoder instance
 *  \param stream Subtitle stream index (starting with 0)
 *  \returns The metadata for the stream
 */

BGAV_PUBLIC
const bgav_metadata_t *
bgav_get_overlay_metadata(bgav_t * b, int stream);

/** \ingroup streams
 *  \brief Get the metadata of an overlay stream
 *  \param bgav A decoder instance
 *  \param track Track index (starting with 0)
 *  \param stream Subtitle stream index (starting with 0)
 *  \returns The metadata for the stream
 */
  
BGAV_PUBLIC
const bgav_metadata_t *
bgav_get_overlay_metadata_t(bgav_t * b, int track, int stream);
  
  
/** \ingroup streams
 *  \brief Get the metadata of a video stream of the current track
 *  \param bgav A decoder instance
 *  \param stream Video stream index (starting with 0)
 *  \returns The metadata for the stream
 */

BGAV_PUBLIC
const bgav_metadata_t *
bgav_get_video_metadata(bgav_t * bgav, int stream);

/** \ingroup streams
 *  \brief Get the metadata of a video stream
 *  \param bgav A decoder instance
 *  \param t Track index (starting with 0)
 *  \param stream Video stream index (starting with 0)
 *  \returns The metadata for the stream
 */

BGAV_PUBLIC
const bgav_metadata_t *
bgav_get_video_metadata_t(bgav_t * b, int t, int s);
  
  
/** \ingroup streams
 *  \brief Stream action
 *
 *  This is used to tell the decoder, what to do with the stream.
 *  Only supported actions right now are mute (default) and decode.
 */

typedef enum
  {
    BGAV_STREAM_MUTE    = 0,  /*!< Stream is switched off */
    BGAV_STREAM_DECODE  = 1, /*!< Stream is switched on and will be decoded */
    BGAV_STREAM_PARSE   = 2, /*!< Used internally when building indices */
    BGAV_STREAM_READRAW = 3 /*!< Read compressed packets from the stream */
  }
bgav_stream_action_t;

/** \defgroup readraw Read compressed media packets
 *  \ingroup decoding
 *
 *  This API layer allows you to read compressed media packets from the
 *  stream bypassing the codecs. You can read some streams of a file
 *  as compressed packets while decoding others. You can however not
 *  mix read calls for compressed packets and decompressed frames in a
 *  single stream. Reading compressed packets is supported only for a
 *  subset of codecs, which are defined by the \ref gavl_codec_id_t.
 *  If your application supports compressed packets, you can first get the
 *  compression info with \ref bgav_get_audio_compression_info or
 *  \ref bgav_get_video_compression_info. If you support this compression,
 *  set the stream mode to \ref BGAV_STREAM_READRAW. Before reading you must
 *  call \ref bgav_start. Packets are then read with \ref bgav_read_audio_packet
 *  and \ref bgav_read_video_packet.
 */

  
/** \ingroup readraw
 *  \brief Get audio compression info
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \param info Returns the compression info
 *  \returns 1 if a compression info was returned, 0 else
 *
 *  This function must be called after \ref bgav_select_track. Before
 *  selecting the track the compression info might not be complete.
 *  Free the returned compression info with \ref gavl_compression_info_free.
 */

BGAV_PUBLIC
int bgav_get_audio_compression_info(bgav_t * bgav, int stream,
                                    gavl_compression_info_t * info);

/** \ingroup readraw
 *  \brief Get video compression info
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \param info Returns the compression info
 *  \returns 1 if a compression info was returned, 0 else
 *
 *  This function must be called after \ref bgav_select_track. Before
 *  selecting the track the compression info might not be complete.
 *  Free the returned compression info with \ref gavl_compression_info_free.
 */

BGAV_PUBLIC
int bgav_get_video_compression_info(bgav_t * bgav, int stream,
                                    gavl_compression_info_t * info);

/** \ingroup readraw
 *  \brief Get overlay compression info
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \param info Returns the compression info
 *  \returns 1 if a compression info was returned, 0 else
 *
 *  This function must be called after \ref bgav_select_track. Before
 *  selecting the track the compression info might not be complete.
 *  Free the returned compression info with \ref gavl_compression_info_free.
 */

BGAV_PUBLIC
int bgav_get_overlay_compression_info(bgav_t * bgav, int stream,
                                      gavl_compression_info_t * info);

  
/** \ingroup readraw
 *  \brief Read compressed audio packet
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \param p Returns the packet
 *  \returns 1 if a packet was read, 0 else
 *
 *  You can pass the same packet multiple times to a read fuction.
 *  Use \ref gavl_packet_free when it's no longer used.
 */
  
BGAV_PUBLIC
int bgav_read_audio_packet(bgav_t * bgav, int stream, gavl_packet_t * p);

/** \ingroup readraw
 *  \brief Get a packet source for an audio stream
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns The packet source
 *
 *  Use this as an alternative for \ref bgav_read_audio_packet
 */

BGAV_PUBLIC
gavl_packet_source_t *
bgav_get_audio_packet_source(bgav_t * bgav, int stream);

  
/** \ingroup readraw
 *  \brief Read compressed video packet
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \param p Returns the packet
 *  \returns 1 if a packet was read, 0 else
 *
 *  You can pass the same packet multiple times to a read fuction.
 *  Use \ref gavl_packet_free when it's no longer used.
 */

BGAV_PUBLIC
int bgav_read_video_packet(bgav_t * bgav, int stream, gavl_packet_t * p);

/** \ingroup readraw
 *  \brief Get a packet source for a video stream
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns The packet source
 *
 *  Use this as an alternative for \ref bgav_read_video_packet
 */

BGAV_PUBLIC
gavl_packet_source_t *
bgav_get_video_packet_source(bgav_t * bgav, int stream);

/** \ingroup readraw
 *  \brief Get a packet source for a text subtitle stream
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns The packet source
 *
 *  Use this as an alternative for \ref bgav_read_subtitle_text
 */

BGAV_PUBLIC
gavl_packet_source_t *
bgav_get_text_packet_source(bgav_t * bgav, int stream);

/** \ingroup readraw
 *  \brief Get a packet source for an overlay stream
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns The packet source
 */

BGAV_PUBLIC
gavl_packet_source_t *
bgav_get_overlay_packet_source(bgav_t * bgav, int stream);

  
/** \ingroup streams
 * \brief Select mode for an audio stream
 * \param bgav A decoder instance
 * \param stream Stream index (starting with 0)
 * \param action The stream action.
 *
 * Note that the default stream action is BGAV_STREAM_MUTE, which means that
 * all streams are switched off by default.
 */

BGAV_PUBLIC
int bgav_set_audio_stream(bgav_t * bgav, int stream, bgav_stream_action_t action);

/** \ingroup streams
 * \brief Select mode for a video stream
 * \param bgav A decoder instance
 * \param stream Stream index (starting with 0)
 * \param action The stream action.
 *
 * Note that the default stream action is BGAV_STREAM_MUTE, which means that
 * all streams are switched off by default.
 */

BGAV_PUBLIC
int bgav_set_video_stream(bgav_t * bgav, int stream, bgav_stream_action_t action);

/** \ingroup streams
 * \brief Select mode for a subtitle stream
 * \param bgav A decoder instance
 * \param stream Stream index (starting with 0)
 * \param action The stream action.
 *
 * Note that the default stream action is BGAV_STREAM_MUTE, which means that
 * all streams are switched off by default.
 */

BGAV_PUBLIC
int bgav_set_subtitle_stream(bgav_t * bgav, int stream,
                             bgav_stream_action_t action);

/** \ingroup streams
 * \brief Select mode for a text stream
 * \param bgav A decoder instance
 * \param stream Stream index (starting with 0)
 * \param action The stream action.
 *
 * Note that the default stream action is BGAV_STREAM_MUTE, which means that
 * all streams are switched off by default.
 */

BGAV_PUBLIC
int bgav_set_text_stream(bgav_t * bgav, int stream,
                         bgav_stream_action_t action);

/** \ingroup streams
 * \brief Select mode for an overlay stream
 * \param bgav A decoder instance
 * \param stream Stream index (starting with 0)
 * \param action The stream action.
 *
 * Note that the default stream action is BGAV_STREAM_MUTE, which means that
 * all streams are switched off by default.
 */

BGAV_PUBLIC
int bgav_set_overlay_stream(bgav_t * bgav, int stream,
                            bgav_stream_action_t action);

/***************************************************
 * Stream handling functions
 ***************************************************/

/*
 *  You MUST these, if you want to decode anything.
 *  After bgav_open(), all streams are switched off by
 *  default
 */


/** \defgroup start Start codecs
 *  \ingroup decoding
 */

/**  \ingroup start
 *  \brief Start all codecs.
 *  \param bgav A decoder instance
 *  \returns 1 if everything went well, 0 if an error occurred.
 *
 *  Call this function, after you called \ref bgav_set_audio_stream
 *  and/or \ref bgav_set_video_stream.
 */

BGAV_PUBLIC
int bgav_start(bgav_t * bgav);

BGAV_PUBLIC
int bgav_pause(bgav_t * bgav);

BGAV_PUBLIC
int bgav_resume(bgav_t * bgav);

  
/** \defgroup stream_info Information about the streams
    \ingroup decoding
 */

/** \ingroup stream_info
 *  \brief Get the format of an audio stream of the current track
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns The format
 *
 *  Note, that you can trust the return value of this function only, if you enabled
 *  the stream (see \ref bgav_set_audio_stream) and started the decoders
 *  (see \ref bgav_start).
 */

BGAV_PUBLIC
const gavl_audio_format_t * bgav_get_audio_format(bgav_t * bgav, int stream);

/** \ingroup stream_info
 *  \brief Get the format of an audio stream
 *  \param bgav A decoder instance
 *  \param t Track index (starting with 0)
 *  \param s Stream index (starting with 0)
 *  \returns The format
 *
 *  Note, that you can trust the return value of this function only, if you enabled
 *  the stream (see \ref bgav_set_audio_stream) and started the decoders
 *  (see \ref bgav_start).
 */


 BGAV_PUBLIC
 const gavl_audio_format_t * bgav_get_audio_format_t(bgav_t *  bgav, int track, int stream);
  
/** \ingroup stream_info
 *  \brief Get the format of a video stream
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns The format
 *
 *  Note, that you can trust the return value of this function only, if
 *  you enabled the stream (see \ref bgav_set_video_stream) and started
 *  the decoders (see \ref bgav_start).
 *
 *  Special care has to be taken, if the video stream consists of (a)
 *  still image(s). This is the case, when the framerate_mode member
 *  of the format is \ref GAVL_FRAMERATE_STILL. See
 *  \ref bgav_video_has_still.
 */

BGAV_PUBLIC
const gavl_video_format_t * bgav_get_video_format(bgav_t * bgav, int stream);

/** \ingroup stream_info
 *  \brief Get the format of a video stream
 *  \param bgav A decoder instance
 *  \param t Track index (starting with 0)
 *  \param stream Stream index (starting with 0)
 *  \returns The format
 *
 *  Note, that you can trust the return value of this function only, if
 *  you enabled the stream (see \ref bgav_set_video_stream) and started
 *  the decoders (see \ref bgav_start).
 *
 *  Special care has to be taken, if the video stream consists of (a)
 *  still image(s). This is the case, when the framerate_mode member
 *  of the format is \ref GAVL_FRAMERATE_STILL. See
 *  \ref bgav_video_has_still.
 */

BGAV_PUBLIC
const gavl_video_format_t * bgav_get_video_format_t(bgav_t * bgav, int t, int stream);
  
/** \ingroup stream_info
 *  \brief Get the format of an overlay stream
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns The format
 *
 *  Note, that you can trust the return value of this function only, if
 *  you enabled the stream (see \ref bgav_set_video_stream) and started
 *  the decoders (see \ref bgav_start).
 *
 */

BGAV_PUBLIC
const gavl_video_format_t * bgav_get_overlay_format(bgav_t * bgav, int stream);

/** \ingroup stream_info
 *  \brief Get the timescale for a text stream
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns The timescale
 *
 *  Note, that you can trust the return value of this function only, if
 *  you enabled the stream (see \ref bgav_set_video_stream) and started
 *  the decoders (see \ref bgav_start).
 */

BGAV_PUBLIC
int bgav_get_text_timescale(bgav_t * bgav, int stream);

  
/** \ingroup stream_info
 *  \brief Get the frame table of a video stream
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns The frame table or NULL
 *
 *  Note, that you can trust the return value of this function only, if
 *  you enabled the stream (see \ref bgav_set_video_stream) and started
 *  the decoders (see \ref bgav_start).
 *
 *  If you want to make sure that the frame table is available for
 *  as many files as possible, use sample accurate mode (see
 *  \ref bgav_options_set_sample_accurate)
 *  The returned table (if non-null) must be freed by the caller with
 *  \ref gavl_frame_table_destroy.
 *
 *  Since 1.0.2
 */
  
BGAV_PUBLIC
gavl_frame_table_t * bgav_get_frame_table(bgav_t * bgav, int stream);

  
/** \ingroup stream_info
 *  \brief Get the video format of a subtitle stream
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns The format
 *
 *  Note, that you can trust the return value of this function only, if you enabled
 *  the stream (see \ref bgav_set_subtitle_stream) and started the decoders
 *  (see \ref bgav_start). For overlay subtitles, this is the video format
 *  of the decoded overlays.
 *  For text subtitles, it's the format of the associated video stream.
 *  The timescale member is always the timescale of the subtitles (not the
 *  video frames).
 */


  
BGAV_PUBLIC const gavl_video_format_t *
bgav_get_subtitle_format(bgav_t * bgav, int stream);

/** \ingroup stream_info
 *  \brief Check if a subtitle is text or graphics based
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns 1 for text subtitles, 0 for graphic subtitles
 *
 *  If this function returns 1, you must use \ref bgav_read_subtitle_text
 *  to decode subtitles, else use \ref bgav_read_subtitle_overlay
 */

BGAV_PUBLIC
int bgav_subtitle_is_text(bgav_t * bgav, int stream);

/** \ingroup stream_info
 *  \brief Get the description of an audio stream
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns A technical decription of the stream 
 *
 *  Note, that you can trust the return value of this function only, if you enabled
 *  the stream (see \ref bgav_set_audio_stream) and started the decoders
 *  (see \ref bgav_start).
 */

BGAV_PUBLIC
const char * bgav_get_audio_description(bgav_t * bgav, int stream);

/** \ingroup stream_info
 *  \brief Get additional info about an audio stream
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns An info string about the stream or NULL
 *
 *  This returns additional information about the audio stream e.g. if it contains
 *  directors comments, audio for the visually impaired or whatever.
 *
 *  Note, that you can trust the return value of this function only, if you enabled
 *  the stream (see \ref bgav_set_audio_stream) and started the decoders
 *  (see \ref bgav_start).
 */

BGAV_PUBLIC
const char * bgav_get_audio_info(bgav_t * bgav, int stream);


/** \ingroup stream_info
 *  \brief Get the description of a video stream
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns A technical decription of the stream 
 *
 *  Note, that you can trust the return value of this function only, if you enabled
 *  the stream (see \ref bgav_set_video_stream) and started the decoders
 *  (see \ref bgav_start).
 */

BGAV_PUBLIC
const char * bgav_get_video_description(bgav_t * bgav, int stream);

/** \ingroup stream_info
 *  \brief Get the description of a subtitle stream
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns A technical decription of the stream 
 *
 *  Note, that you can trust the return value of this function only, if you enabled
 *  the stream (see \ref bgav_set_subtitle_stream) and started the decoders
 *  (see \ref bgav_start).
 */

BGAV_PUBLIC
const char * bgav_get_subtitle_description(bgav_t * bgav, int stream);


/** \ingroup stream_info
 *  \brief Get additional info about a subtitle stream
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns An info string about the stream or NULL
 *
 *  This returns additional information about the subtitle stream e.g. if it contains
 *  directors comments or whatever.
 *
 *  Note, that you can trust the return value of this function only, if you enabled
 *  the stream (see \ref bgav_set_subtitle_stream) and started the decoders
 *  (see \ref bgav_start).
 */

BGAV_PUBLIC
const char * bgav_get_subtitle_info(bgav_t * bgav, int stream);

/** \ingroup stream_info
 *  \brief Query if a track is pausable
 *  \param bgav A decoder instance
 *  \returns 1 is decoding can be paused for a longer time, 0 else
 *
 *  Check this if you intend to pause decoding. Pausing a livestream
 *  doesn't make sense, so in this case 0 is returned.
 */

BGAV_PUBLIC
int bgav_can_pause(bgav_t * bgav);

/***************************************************
 * Decoding functions
 ***************************************************/

/** \defgroup decode Decode media frames
 *  \ingroup decoding
 */

/** \ingroup decode
 *  \brief Determine if a still image is available for reading
 *  \param bgav A decoder instance
 *  \param stream Stream index (starting with 0)
 *  \returns 1 if a still image is available, 0 else
 *
 *  This should be used, if the framerate_mode of the video format
 *  is \ref GAVL_FRAMERATE_STILL. In earlier versions, streams
 *  consisting of one or more still images outputted the same image
 *  repeatedly as if it was a video stream. Since 1.0.1 each image
 *  is decoded exactly once. The end-of-file detection happens in a
 *  similar way as for subtitles: \ref bgav_video_has_still returns
 *  1 and the following \ref bgav_read_video returns 0.
 *
 *  Since 1.0.1
 */
  
BGAV_PUBLIC
int bgav_video_has_still(bgav_t * bgav, int stream);
  
/** \ingroup decode
    \brief Decode a video frame
    \param bgav A decoder instance
    \param frame The frame to which the image will be decoded.
    \param stream Stream index (starting with 0)
    \returns 1 if a frame could be decoded, 0 for EOF.
*/

BGAV_PUBLIC
int bgav_read_video(bgav_t * bgav, gavl_video_frame_t * frame, int stream);

/** \ingroup decode
    \brief Skip forward in a video stream
    \param bgav A decoder instance
    \param stream Stream index (starting with 0)
    \param time The time to skip to (will be changed to the true time)
    \param scale Scale by which the time is scaled
    \param exact 1 if an exact skip should be done, 0 for faster approximate skip
    
    Use this function if it turns out, that the machine is too weak to
    decode all frames. Set exact to 0 to make the skipping even faster
    but less accurate.
*/

BGAV_PUBLIC
void bgav_skip_video(bgav_t * bgav, int stream,
                     int64_t * time, int scale,
                     int exact);

/** \ingroup decode
    \brief Return the video source for a video stream
    \param bgav A decoder instance
    \param stream Stream index (starting with 0)
    \returns A video source to get the frames from

    This is an alternative to using \ref bgav_video_has_still and
    \ref bgav_read_video. A \ref gavl_video_source_t lets you read
    frames with optimized buffer management and implicit format
    conversion. For a still stream, if no image is available, the
    source will return GAVL_VIDEO_SOURCE_AGAIN.
*/
  
BGAV_PUBLIC
gavl_video_source_t * bgav_get_video_source(bgav_t * bgav, int stream);

/** \ingroup decode
    \brief Return the video source for an overlay stream
    \param bgav A decoder instance
    \param stream Stream index (starting with 0)
    \returns A video source to get the overlays from
    
*/
  
BGAV_PUBLIC
gavl_video_source_t * bgav_get_overlay_source(bgav_t * bgav, int stream);

  
/** \ingroup decode
    \brief Decode audio samples
    \param bgav A decoder instance
    \param frame The frame to which the samples will be decoded.
    \param stream Stream index (starting with 0)
    \param num_samples Number of samples to be decoded
    \returns The number of actually decoded samples, which can be smaller than num_samples. 0 means EOF.
*/

BGAV_PUBLIC
int bgav_read_audio(bgav_t * bgav, gavl_audio_frame_t * frame, int stream,
                    int num_samples);

/** \ingroup decode
    \brief Return the audio source for this stream
    \param bgav A decoder instance
    \param stream Stream index (starting with 0)
    \returns A audio source to get the frames from

    This is an alternative to using \ref bgav_read_audio. A
    \ref gavl_audio_source_t lets you read
    frames with optimized buffer management and implicit format
    conversion. 
*/
  
BGAV_PUBLIC
gavl_audio_source_t * bgav_get_audio_source(bgav_t * bgav, int stream);

  

/** \ingroup decode
    \brief Check, if a new subtitle is available
    \param bgav A decoder instance
    \param stream Stream index (starting with 0)
    \returns 1 if a subtitle is available.

    Use this function to check in advance, if it would make sense to call
    \ref bgav_read_subtitle_overlay or \ref bgav_read_subtitle_text.

*/

BGAV_PUBLIC
int bgav_has_subtitle(bgav_t * bgav, int stream);

/** \ingroup decode
    \brief Decode an overlay subtitle
    \param bgav A decoder instance
    \param ovl The overlay to which the subtitle will be decoded.
    \param stream Stream index (starting with 0)
    \returns 1 if a subtitle could be decoded, 0 else

    If this function returns 1, a subtitle was decoded. If this function returns
    0 and \ref bgav_has_subtitle returned 0 before as well, there is no subtitle
    yet available, but there might come others at a later point in the stream.
    If this function returns 0 and \ref bgav_has_subtitle returned 1 before,
    it means, that you reached the end of the subtitle stream.

*/

BGAV_PUBLIC
int bgav_read_subtitle_overlay(bgav_t * bgav, gavl_overlay_t * ovl, int stream);

/** \ingroup decode
    \brief Decode a text subtitle
    \param bgav       A decoder instance
    \param ret        The string, where the text will be stored.
    \param ret_alloc  The number of allocated bytes for ret
    \param start_time Returns the start time
    \param duration   Returns the duration
    \param stream     Stream index (starting with 0)
    \returns 1 if a subtitle could be decoded, 0 else

    If this function returns 1, a subtitle was decoded. If this function returns
    0 and \ref bgav_has_subtitle returned 0 before as well, there is no subtitle
    yet available, but there might come others at a later point in the stream.
    If this function returns 0 and \ref bgav_has_subtitle returned 1 before,
    it means, that you reached the end of the subtitle stream.
    start_time and duration are scaled by the timescale of the subtitle stream
    (see \ref bgav_get_subtitle_format).
*/

BGAV_PUBLIC
int bgav_read_subtitle_text(bgav_t * bgav, char ** ret, int *ret_alloc,
                            int64_t * start_time, int64_t * duration,
                            int stream);

/***************************************************
 * Seek to a timestamp. This also resyncs all streams
 ***************************************************/

/** \defgroup seeking Simple seek API
 *  \ingroup decoding
 *
 *  gmerlin_avdecoder supports multiple demultiplexing modes, some
 *  are optimized for linear playback, some are optimized for sample
 *  accurate random access. By default, the demuxer will be in linear
 *  playback mode. The associated seek API is described in this section.
 */

/** \defgroup sampleseek Sample accurate seek API
 *  \ingroup decoding
 *
 *  This mode is optimized for sample accurate access. To use this
 *  API, you must call \ref bgav_options_set_sample_accurate before
 *  opening the file. After you opened the file and selected the track,
 *  you must verify, that sample accurate access is available by checking
 *  the return value of \ref bgav_can_seek_sample.
 *
 *  Sample accurate mode has a little more overhead on the demultiplexer side.
 *  Therefore you should not enable it when not needed.
 *  Some formats don't allow sample accurate access, other formats are *only*
 *  seekable in sample accurate mode. For formats, which need to be parsed completely,
 *  index files are written to $HOME/.gmerlin-avdecoder/indices. Filenames of the
 *  indices are the MD5 sums of the filename passed to \ref bgav_open.
 *
 *  Sample accurate mode also implies, that all streams can be positioned
 *  independently.
 */


/** \ingroup seeking
 *  \brief Check if a track is seekabkle
 *  \param bgav A decoder handle
 *  \returns 1 if the track is seekable, 0 else.
 */

BGAV_PUBLIC
int bgav_can_seek(bgav_t * bgav);

/** \ingroup seeking
 *  \brief Seek to a specific time
 *  \param bgav A decoder handle
 *  \param time The time to seek to. 
 *
 * The time argument is changed to the actually seeked time, which can be different.
 */

BGAV_PUBLIC
void bgav_seek(bgav_t * bgav, gavl_time_t * time);


/** \ingroup seeking
 *  \brief Seek to a specific stream position
 *  \param bgav A decoder handle
 *  \param time The time to seek to. 
 *  \param scale Timescale
 *
 * This function allows sample and frame accurate seeking, if
 * the following conditions are met:
 * - The formats allows sample accurate seeking at all
 * - For audio streams, scale is equal to the samplerate
 * - For video streams, scale is equal to the timescale
 *
 * Typically, only one stream will be positioned accurately. For
 * editing applications, it's recommended, that separate decoder instances
 * are opened for audio and video. They can then be positoned independently.
 * 
 * The time argument might be changed to the actually seeked time, which can be
 * different. For sample accurate formats, it should always be unchanged.
 *
 * This function allows sample accurate seeking for some cases even in linear
 * decoding mode. For more sophisticated sample accurate access, see \ref sampleseek.
 */

BGAV_PUBLIC
void bgav_seek_scaled(bgav_t * bgav, int64_t * time, int scale);

/** \ingroup sampleseek
 *  \brief Time value indicating an invalid time
 */

#define BGAV_TIMESTAMP_UNDEFINED GAVL_TIME_UNDEFINED // Don't change this
/** \ingroup sampleseek
 *  \brief Check if a track is seekabkle with sample accuracy
 *  \param bgav A decoder handle
 *  \returns 1 if the track is seekable with sample accuracy, 0 else.
 *
 *  If this function returns zero,  applications, which rely on
 *  \ref bgav_seek_audio \ref bgav_seek_video and \ref bgav_seek_subtitle
 *  should consider the file as unsupported.
 *
 *  The ability of sample accurate seeking also implies, that streams can
 *  be positioned indepentently.
 *
 *  If \ref bgav_options_set_sample_accurate was not called, this function will
 *  return zero for any file.
 */

BGAV_PUBLIC
int bgav_can_seek_sample(bgav_t * bgav);


/** \ingroup sampleseek
 *  \brief Get the audio duration
 *  \param bgav A decoder handle
 *  \param stream Audio stream index (starting with 0)
 *  \returns Duration in samples
 *
 *  Use this only after \ref bgav_can_seek_sample returned 1.
 *  The duration is calculated from the total number or decodable
 *  samples in the file. The start time (as returned by \ref bgav_audio_start_time)
 *  is <b>not</b> included in the duration.
 */

BGAV_PUBLIC
int64_t bgav_audio_duration(bgav_t * bgav, int stream);

/** \ingroup sampleseek
 *  \brief Get the audio start time
 *  \param bgav A decoder handle
 *  \param stream Audio stream index (starting with 0)
 *  \returns Time (in samplerate tics) of the first sample
 *
 *  Use this only after \ref bgav_can_seek_sample returned 1.
 *  The returned value is equal to the timestamp of the first
 *  decoded audio frame.
 */

BGAV_PUBLIC
int64_t bgav_audio_start_time(bgav_t * bgav, int stream);

/** \ingroup sampleseek
 *  \brief Get the video duration
 *  \param bgav A decoder handle
 *  \param stream Video stream index (starting with 0)
 *  \returns Exact duration in stream tics
 *
 *  Use this only after \ref bgav_can_seek_sample returned 1.
 *  The duration is calculated from the total number or decodable
 *  frames in the file. The start time (as returned by \ref bgav_video_start_time)
 *  is <b>not</b> included in the duration.
 */

BGAV_PUBLIC
int64_t bgav_video_duration(bgav_t * bgav, int stream);

/** \ingroup sampleseek
 *  \brief Get the video start time
 *  \param bgav A decoder handle
 *  \param stream Video stream index (starting with 0)
 *  \returns Time of the first video frame in stream tics
 *
 *  Use this only after \ref bgav_can_seek_sample returned 1.
 *  The returned value is equal to the timestamp of the first
 *  decoded video frame.
 */

BGAV_PUBLIC
int64_t bgav_video_start_time(bgav_t * bgav, int stream);


/** \ingroup sampleseek
 *  \brief Get the subtitle duration
 *  \param bgav A decoder handle
 *  \param stream Subtitle stream index (starting with 0)
 *  \returns Exact duration in stream tics
 *
 *  Use this only after \ref bgav_can_seek_sample returned 1.
 */

BGAV_PUBLIC
int64_t bgav_subtitle_duration(bgav_t * bgav, int stream);

/** \ingroup sampleseek
 *  \brief Get the text duration
 *  \param bgav A decoder handle
 *  \param stream Text stream index (starting with 0)
 *  \returns Exact duration in stream tics
 *
 *  Use this only after \ref bgav_can_seek_sample returned 1.
 */

BGAV_PUBLIC
int64_t bgav_text_duration(bgav_t * bgav, int stream);

/** \ingroup sampleseek
 *  \brief Get the overlay duration
 *  \param bgav A decoder handle
 *  \param stream Overlay stream index (starting with 0)
 *  \returns Exact duration in stream tics
 *
 *  Use this only after \ref bgav_can_seek_sample returned 1.
 */

BGAV_PUBLIC
int64_t bgav_overlay_duration(bgav_t * bgav, int stream);


  
/** \ingroup sampleseek
 *  \brief Seek to a specific audio sample
 *  \param bgav A decoder handle
 *  \param stream Audio stream index (starting with 0)
 *  \param sample The sample to seek to
 *
 *  Use this only after \ref bgav_can_seek_sample returned 1.
 *  The time is relative to the first decodable sample
 *  (always starting with 0), the offset returned by
 *  \ref bgav_audio_start_time is <b>not</b> included here.
 *  
 */
  
BGAV_PUBLIC
void bgav_seek_audio(bgav_t * bgav, int stream, int64_t sample);

/** \ingroup sampleseek
 *  \brief Seek to a specific video time
 *  \param bgav A decoder handle
 *  \param stream Video stream index (starting with 0)
 *  \param time Time
 *
 *  Use this only after \ref bgav_can_seek_sample returned 1.
 *  If time is between 2 frames, the earlier one will be chosen.
 *  The time is relative to the first decodable frame
 *  (always starting with 0), the offset returned by
 *  \ref bgav_video_start_time is <b>not</b> included here.
 */

BGAV_PUBLIC
void bgav_seek_video(bgav_t * bgav, int stream, int64_t time);

/** \ingroup sampleseek
 *  \brief Get the time of the closest keyframe before a given time
 *  \param bgav A decoder handle
 *  \param stream Video stream index (starting with 0)
 *  \param time Time
 *  \returns Time of the previous keyframe.
 *
 *  Use this only after \ref bgav_can_seek_sample returned 1.
 *  The time argument and return value are relative to the first
 *  decodable frame of the file i.e. <b>not</b> including the offset
 *  returned by \ref bgav_video_start_time .
 *  If there is no keyframe before the given time (i.e if time was 0),
 *  this function returns \ref BGAV_TIMESTAMP_UNDEFINED.
 */

BGAV_PUBLIC
int64_t bgav_video_keyframe_before(bgav_t * bgav, int stream, int64_t time);

/** \ingroup sampleseek
 *  \brief Get the time of the closest keyframe after a given time
 *  \param bgav A decoder handle
 *  \param stream Video stream index (starting with 0)
 *  \param time Time
 *  \returns Time of the next keyframe
 *
 *  Use this only after \ref bgav_can_seek_sample returned 1.
 *  The time argument and return value are relative to the first
 *  decodable frame of the file i.e. <b>not</b> including the offset
 *  returned by \ref bgav_video_start_time .
 *  If there is no keyframe after the given time,
 *  this function returns \ref BGAV_TIMESTAMP_UNDEFINED.
 */

BGAV_PUBLIC
int64_t bgav_video_keyframe_after(bgav_t * bgav, int stream, int64_t time);


/** \ingroup sampleseek
 *  \brief Seek to a specific subtitle position
 *  \param bgav A decoder handle
 *  \param stream Subtitle stream index (starting with 0)
 *  \param time Time
 *
 *  Use this only after \ref bgav_can_seek_sample returned 1.
 *  If time is between 2 subtitles, the earlier one will be chosen.
 */

BGAV_PUBLIC
void bgav_seek_subtitle(bgav_t * bgav, int stream, int64_t time);

/** \ingroup sampleseek
 *  \brief Seek to a specific text position
 *  \param bgav A decoder handle
 *  \param stream text stream index (starting with 0)
 *  \param time Time
 *
 *  Use this only after \ref bgav_can_seek_sample returned 1.
 *  If time is between 2 subtitles, the earlier one will be chosen.
 */
  
BGAV_PUBLIC
void bgav_seek_text(bgav_t * bgav, int stream, int64_t time);

/** \ingroup sampleseek
 *  \brief Seek to a specific overlay position
 *  \param bgav A decoder handle
 *  \param stream Overlay stream index (starting with 0)
 *  \param time Time
 *
 *  Use this only after \ref bgav_can_seek_sample returned 1.
 *  If time is between 2 subtitles, the earlier one will be chosen.
 */
  
BGAV_PUBLIC
void bgav_seek_overlay(bgav_t * bgav, int stream, int64_t time);

  
/** \defgroup codec Standalone stream decoders
 *
 *  The standalone decoders can be used for the
 *  decompression of elemtary A/V streams (bypassing the
 *  demultiplexer stage). They obtain compressed packets
 *  via a \ref gavl_packet_source_t and provide a
 *  \ref gavl_audio_source_t or \ref gavl_video_source_t for
 *  reading the uncompressed frames.
 *
 *  @{
 */

/** \brief Forward declaration for a stream decoder
 *
 * You don't want to know, what's inside here
 */
  
typedef struct bgav_stream_decoder_s bgav_stream_decoder_t;

/** \brief Create a stream decoder
 *  \returns A newly allocated stream decoder
 */
  
BGAV_PUBLIC
bgav_stream_decoder_t * bgav_stream_decoder_create();

/** \brief Get options for a stream decoder
 *  \param dec A stream decoder
 *  \returns Options for decoding the stream
 */

BGAV_PUBLIC bgav_options_t *
bgav_stream_decoder_get_options(bgav_stream_decoder_t * dec);

/** \brief Connect an audio stream decoder
 *  \param dec A stream decoder
 *  \param src Packet source
 *  \param ci Compression info
 *  \param fmt Format (possibly incomplete)
 *  \param m Stream metadata (might get changed)
 *  \returns Source for reading the uncompressed frames
 *
 *  You can call either \ref bgav_stream_decoder_connect_audio or
 *  \ref bgav_stream_decoder_connect_video for a decoder instance,
 *  not both.
 *
 *  The updated audio format can be ontained by calling
 *  \ref gavl_audio_source_get_src_format with the returned source.
 */
  
BGAV_PUBLIC gavl_audio_source_t *
bgav_stream_decoder_connect_audio(bgav_stream_decoder_t * dec,
                                  gavl_packet_source_t * src,
                                  const gavl_compression_info_t * ci,
                                  const gavl_audio_format_t * fmt,
                                  gavl_dictionary_t * m);
  
/** \brief Connect a video stream decoder
 *  \param dec A stream decoder
 *  \param src Packet source
 *  \param ci Compression info
 *  \param fmt Format (possibly incomplete)
 *  \param m Stream metadata (might get changed)
 *  \returns Source for reading the uncompressed frames
 *
 *  You can call either \ref bgav_stream_decoder_connect_audio or
 *  \ref bgav_stream_decoder_connect_video for a decoder instance,
 *  not both.
 *
 *  The updated video format can be ontained by calling
 *  \ref gavl_video_source_get_src_format with the returned source.
 */

BGAV_PUBLIC gavl_video_source_t *
bgav_stream_decoder_connect_video(bgav_stream_decoder_t * dec,
                                  gavl_packet_source_t * src,
                                  const gavl_compression_info_t * ci,
                                  const gavl_video_format_t * fmt,
                                  gavl_dictionary_t * m);

/** \brief Connect an overlay stream decoder
 *  \param dec A stream decoder
 *  \param src Packet source
 *  \param ci Compression info
 *  \param fmt Format (possibly incomplete)
 *  \param m Stream metadata (might get changed)
 *  \returns Source for reading the uncompressed overlays
 *
 *  You can call either \ref bgav_stream_decoder_connect_audio or
 *  \ref bgav_stream_decoder_connect_video for a decoder instance,
 *  not both.
 *
 *  The updated video format can be ontained by calling
 *  \ref gavl_video_source_get_src_format with the returned source.
 */

BGAV_PUBLIC gavl_video_source_t *
bgav_stream_decoder_connect_overlay(bgav_stream_decoder_t * dec,
                                    gavl_packet_source_t * src,
                                    const gavl_compression_info_t * ci,
                                    const gavl_video_format_t * fmt,
                                    gavl_dictionary_t * m);

/** \brief Skip to a specified time
 *  \param dec A stream decoder
 *  \param t Time to skip to
 *  \returns True time skipped
 *
 *  This skips to a certain time in the stream. For audio stream
 *  the time is scaled by the samplerate. For video streams it is
 *  in timescale unit of the video format.
 */

BGAV_PUBLIC int64_t
bgav_stream_decoder_skip(bgav_stream_decoder_t * dec, int64_t t, int scale);

/** \brief Reset a stream decoder
 *  \param dec A stream decoder
 *
 *  Reset the decoder to a state as if no packet was read so far.
 */

BGAV_PUBLIC void
bgav_stream_decoder_reset(bgav_stream_decoder_t * dec);

/** \brief Destroy a stream decoder
 *  \param dec A stream decoder
 */

BGAV_PUBLIC void
bgav_stream_decoder_destroy(bgav_stream_decoder_t * dec);

/** \brief Get supported audio compressions
 *  \returns The supported audio compressions
 *
 *  Compressions are returned as an array terminated with
 *  GAVL_CODEC_ID_NONE. The returned array must be free()d by
 *  the caller.
 */

  
BGAV_PUBLIC gavl_codec_id_t *
bgav_supported_audio_compressions();

/** \brief Get supported video compressions
 *  \returns The supported video compressions
 *
 *  Compressions are returned as an array terminated with
 *  GAVL_CODEC_ID_NONE. The returned array must be free()d by
 *  the caller.
 */

BGAV_PUBLIC
gavl_codec_id_t * bgav_supported_video_compressions();

BGAV_PUBLIC
int bgav_set_stream_action_all(bgav_t * bgav, int idx, bgav_stream_action_t action);
  
/**
 *  @}
 */
  

/***************************************************
 * Debugging functions
 ***************************************************/

/** \defgroup debugging Debugging utilities
 */

/** \ingroup debugging
 *  \brief Dump informations of all tracks to stderr
 *  \param bgav A decoder handle
*/

BGAV_PUBLIC
void bgav_dump(bgav_t * bgav);

/* Dump infos about the installed codecs */

/** \ingroup debugging
 *  \brief Dump informations about all available codecs to stderr
 *
 * The output format is html. The main purpose of this function is to generate a feature list
 * for the webpage.
 */

BGAV_PUBLIC
void bgav_codecs_dump();

/* Dump known media formats */

/** \ingroup debugging
 *  \brief Dump informations about all available format demuxers to stderr
 *
 * The output format is html. The main purpose of this function is to generate a feature list
 * for the webpage.
 */

BGAV_PUBLIC
void bgav_formats_dump();

/** \ingroup debugging
 *  \brief Dump informations about all available input modules to stderr
 *
 * The output format is html. The main purpose of this function is to generate a feature list
 * for the webpage.
 */

BGAV_PUBLIC
void bgav_inputs_dump();

/** \ingroup debugging
 *  \brief Dump informations about all available redirectors to stderr
 *
 * The output format is html. The main purpose of this function is to generate a feature list
 * for the webpage.
 */

BGAV_PUBLIC
void bgav_redirectors_dump();

/** \ingroup debugging
 *  \brief Dump informations about all available subtitle readers to stderr
 *
 * The output format is html. The main purpose of this function is to generate a feature list
 * for the webpage.
 */

BGAV_PUBLIC
void bgav_subreaders_dump();


#ifdef __cplusplus
}
#endif

