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

#ifndef BGAV_BSF_PRIVATE_H_INCLUDED
#define BGAV_BSF_PRIVATE_H_INCLUDED

struct bgav_bsf_s
  {
  /* Where to get packets */
  bgav_packet_source_t src;
  
  void (*cleanup)(bgav_bsf_t*);
  void (*filter)(bgav_bsf_t*, bgav_packet_t * in, bgav_packet_t * out);
  bgav_stream_t * s;
  void * priv;
  
  gavl_compression_info_t ci;
  
  bgav_packet_t * out_packet;
  };

void bgav_bsf_run(bgav_bsf_t * bsf, bgav_packet_t * in, bgav_packet_t * out);

int bgav_bsf_init_avcC(bgav_bsf_t*);


int
bgav_bsf_init_adts(bgav_bsf_t * bsf);

#endif // BGAV_BSF_PRIVATE_H_INCLUDED

