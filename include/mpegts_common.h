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


#define MAX_PAT_SECTION_LENGTH 1021

/* Maximum number of programs in one pat section */
#define MAX_PROGRAMS ((MAX_PAT_SECTION_LENGTH-9)/4)

#define MAX_PMT_SECTION_LENGTH 1021

/* Maximum number of streams in one pmt section */
#define MAX_STREAMS ((MAX_PMT_SECTION_LENGTH-13)/5)

/* Transport packet */

/* Stream types (from libavformat) */

#define STREAM_TYPE_VIDEO_MPEG1     0x01
#define STREAM_TYPE_VIDEO_MPEG2     0x02
#define STREAM_TYPE_AUDIO_MPEG1     0x03
#define STREAM_TYPE_AUDIO_MPEG2     0x04
#define STREAM_TYPE_PRIVATE_SECTION 0x05
#define STREAM_TYPE_PRIVATE_DATA    0x06
#define STREAM_TYPE_AUDIO_AAC       0x0f
#define STREAM_TYPE_VIDEO_MPEG4     0x10
#define STREAM_TYPE_VIDEO_H264      0x1b

#define STREAM_TYPE_AUDIO_AC3       0x81
#define STREAM_TYPE_AUDIO_DTS       0x8a

typedef struct
  {
  int transport_error;
  uint16_t pid;
  
  int has_adaption_field;
  int has_payload;

  int payload_start; /* Payload start indicator */
      
  uint8_t continuity_counter;

  int payload_size;

  /* Adaption field */

  struct
    {
    int64_t pcr;
    int random_access_indicator;
    } adaption_field;
  } transport_packet_t;

void bgav_transport_packet_dump(transport_packet_t * p);

int bgav_transport_packet_parse(const bgav_options_t * opt,
                                uint8_t ** data, transport_packet_t * ret);


/* Program association table section */

typedef struct
  {
  uint8_t table_id;
  uint16_t section_length;
  uint16_t transport_stream_id;
  int current_next_indicator;
  uint8_t section_number;
  uint8_t last_section_number;

  int num_programs; /* Number of program definitions in this section */

  struct
    {
    uint16_t program_number;
    uint16_t program_map_pid;
    } programs[MAX_PROGRAMS];
  } pat_section_t;

int bgav_pat_section_read(uint8_t * data, int size,
                          pat_section_t * ret);

void bgav_pat_section_dump(pat_section_t * pats);

/* Program map section */

typedef struct
  {
  uint8_t table_id;
  uint16_t section_length;
  uint16_t program_number;
  int current_next_indicator;
  uint8_t section_number;
  uint8_t last_section_number;

  uint16_t pcr_pid;
  uint8_t descriptor[4096];
  int descriptor_len;
  
  int num_streams;
  struct
    {
    uint8_t type;
    uint16_t pid;
    
    uint8_t descriptor[4096];
    int descriptor_len;
    int present; // Set by the demuxer to signal, that the stream is present
    } streams[MAX_STREAMS];
  } pmt_section_t;

int bgav_pmt_section_read(uint8_t * data, int size,
                          pmt_section_t * ret);

void bgav_pmt_section_dump(pmt_section_t * pmts);

/* Returns number of added streams */
int bgav_pmt_section_setup_track(pmt_section_t * pmts,
                                  bgav_track_t * track,
                                  const bgav_options_t * opt,
                                  int max_audio_streams,
                                  int max_video_streams,
                                  int max_ac3_streams,
                                  int * num_ac3_streams,
                                  int * extra_pcr_pid);
