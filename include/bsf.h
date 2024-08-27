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



#ifndef BGAV_BSF_H_INCLUDED
#define BGAV_BSF_H_INCLUDED

#if 0

bgav_bsf_t * bgav_bsf_create(bgav_stream_t * s);


void bgav_bsf_destroy(bgav_bsf_t * bsf);

gavl_source_status_t
bgav_bsf_get_packet(void * bsf, bgav_packet_t **);

gavl_source_status_t
bgav_bsf_peek_packet(void * bsf, bgav_packet_t **, int force);
#endif

typedef struct bgav_packet_filter_s
  {
  int src_flags;
  
  gavl_packet_source_t * prev;
  gavl_packet_source_t * src;
  void * priv;

  void (*reset)(struct bgav_packet_filter_s*);
  void (*cleanup)(struct bgav_packet_filter_s*);
  gavl_packet_source_func_t source_func;
  
  } bgav_packet_filter_t;

bgav_packet_filter_t * bgav_packet_filter_create(uint32_t fourcc);
void bgav_packet_filter_reset(bgav_packet_filter_t *);

gavl_packet_source_t *
bgav_packet_filter_connect(bgav_packet_filter_t *, gavl_packet_source_t * src);

int bgav_packet_filter_init_avcC(bgav_packet_filter_t *);

#ifdef HAVE_AVCODEC
int bgav_packet_filter_init_adts(bgav_packet_filter_t *);
#endif

void bgav_packet_filter_destroy(bgav_packet_filter_t * f);


#endif // BGAV_BSF_H_INCLUDED

