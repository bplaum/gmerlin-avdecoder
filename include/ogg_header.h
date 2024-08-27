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




#define BGAV_OGG_HEADER_TYPE_CONTINUED 0x01
#define BGAV_OGG_HEADER_TYPE_BOS       0x02 // BOS
#define BGAV_OGG_HEADER_TYPE_EOS       0x04 // EOS

typedef struct
  {
  uint8_t stream_structure_version;
  uint8_t header_type_flags;
  int64_t granulepos;
  
  uint32_t  serialno;
  uint32_t sequenceno;
  uint32_t crc;
  
  uint8_t num_page_segments;
  uint8_t page_segments[256];

  /* Set by bgav_ogg_page_read_header, clear if all packets are read */
  int valid;
  
  /* Byte position of the page in the file */
  int64_t position;
  
  } bgav_ogg_page_t;

int bgav_ogg_page_read_header(bgav_input_context_t * ctx,
                              bgav_ogg_page_t * ret);

void bgav_ogg_page_dump_header(const bgav_ogg_page_t * p);

int bgav_ogg_page_num_packets(const bgav_ogg_page_t * p);
int bgav_ogg_page_get_packet_size(const bgav_ogg_page_t * h, int idx);

void bgav_ogg_page_skip(bgav_input_context_t * ctx,
                        const bgav_ogg_page_t * h);

// int bgav_ogg_next_packet_size(const bgav_ogg_page_t * h, int * idx);


int bgav_ogg_probe(bgav_input_context_t * ctx);

/* OGM header */

/* Special header for OGM files */

typedef struct
  {
  char     type[8];
  uint32_t subtype;
  uint32_t size;
  uint64_t time_unit;
  uint64_t samples_per_unit;
  uint32_t default_len;
  uint32_t buffersize;
  uint16_t bits_per_sample;
  uint16_t padding;

  union
    {
    struct
      {
      uint32_t width;
      uint32_t height;
      } video;
    struct
      {
      uint16_t channels;
      uint16_t blockalign;
      uint32_t avgbytespersec;
      } audio;
    } data;
  } bgav_ogm_header_t;

int bgav_ogm_header_read(bgav_input_context_t * input, bgav_ogm_header_t * ret);
void bgav_ogm_header_dump(bgav_ogm_header_t * h);

