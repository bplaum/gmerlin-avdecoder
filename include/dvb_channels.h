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


#ifdef HAVE_LINUXDVB

typedef struct
  {
  char * name;
  int tone;
  
  int audio_pid; // Unused
  int video_pid; // Unused
  int service_id;
    
  struct dvb_frontend_parameters front_param;
  
  int pol;    // 1: Vertical, 0 horizontal
  int sat_no; // satellite number
  int initialized;

  int pcr_pid;
  int extra_pcr_pid; // 1 if the PCR is contained in a non-A/V stream
  int num_ac3_streams;
  } bgav_dvb_channel_info_t;

#else

typedef struct
  {
  int dummy;
  }

#endif

char *
bgav_dvb_channels_seek(const bgav_options_t * opt,
                       fe_type_t type);

bgav_dvb_channel_info_t *
bgav_dvb_channels_load(bgav_t * b,
                       fe_type_t type, int * num, const char * filename);

void dvb_channels_destroy(bgav_dvb_channel_info_t *, int num);

void dvb_channels_dump(bgav_dvb_channel_info_t *, fe_type_t type, int num);

