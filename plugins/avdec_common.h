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

#include <config.h>

typedef struct
  {
  int num_tracks;
  bgav_t * dec;
  bgav_options_t * opt;
  bg_controllable_t ctrl;
  
  bg_media_source_t src;

  pthread_mutex_t mutex;
  
  } avdec_priv;

bg_media_source_t * bg_avdec_get_src(void * priv);

// const gavl_dictionary_t * bg_avdec_get_edl(void * priv);

void * bg_avdec_create();

void bg_avdec_close(void * priv);
void bg_avdec_destroy(void * priv);

void bg_avdec_lock(void * priv);
void bg_avdec_unlock(void * priv);

bg_controllable_t *
bg_avdec_get_controllable(void * priv);



void bg_avdec_skip_video(void * priv, int stream, int64_t * time,
                         int scale, int exact);


int bg_avdec_has_still(void * priv,
                       int stream);

int bg_avdec_read_audio(void * priv,
                        gavl_audio_frame_t * frame,
                        int stream,
                        int num_samples);


gavl_frame_table_t * bg_avdec_get_frame_table(void * priv, int stream);


const char * bg_avdec_get_disc_name(void * priv);


void
bg_avdec_set_parameter(void * p, const char * name,
                       const gavl_value_t * val);

gavl_dictionary_t * bg_avdec_get_media_info(void * p);



#if 0
int bg_avdec_get_audio_compression_info(void * priv, int stream,
                                        gavl_compression_info_t * info);

int bg_avdec_get_overlay_compression_info(void * priv, int stream,
                                          gavl_compression_info_t * info);


int bg_avdec_get_video_compression_info(void * priv, int stream,
                                        gavl_compression_info_t * info);
#endif

int bg_avdec_read_audio_packet(void * priv, int stream, gavl_packet_t * p);


#include "options.h"
