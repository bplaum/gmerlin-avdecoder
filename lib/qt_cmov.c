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

#include <string.h>
#include <stdlib.h>

#include <avdec_private.h>

#include <qt.h>

#include <zlib.h>
#include <stdio.h>

#define LOG_DOMAIN "quicktime.cmov"

int bgav_qt_cmov_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_moov_t * ret)
  {
  qt_atom_header_t ch;
  uint32_t compression_id;
  uint32_t size_compressed;
  uint32_t size_uncompressed;
  uLongf size_uncompressed_ret; /* use zlib type to keep gcc happy */
  int result;
  uint8_t * buf_compressed;
  uint8_t * buf_uncompressed;
  
  bgav_input_context_t * input_mem;
  
  while(input->position < h->start_position + h->size)
    {
    if(!bgav_qt_atom_read_header(input, &ch))
      return 0;

    switch(ch.fourcc)
      {
      case BGAV_MK_FOURCC('d', 'c', 'o', 'm'):
        if(!bgav_input_read_fourcc(input, &compression_id))
          return 0;
        switch(compression_id)
          {
          case BGAV_MK_FOURCC('z', 'l', 'i', 'b'):
            // have_dcom = 1;
            break;
          default:
            gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                     "Unknown compression method: %c%c%c%c",
                     compression_id >> 24,
                     (compression_id >> 16) & 0xff,
                     (compression_id >> 8) & 0xff,
                     compression_id & 0xff);
            return 0;
          }
        break;
      case BGAV_MK_FOURCC('c', 'm', 'v', 'd'):
        if(!bgav_input_read_32_be(input, &size_uncompressed))
          return 0;
        size_compressed = h->size - (input->position - h->start_position);

        /* Decompress header and parse it */

        buf_compressed   = malloc(size_compressed);
        buf_uncompressed = malloc(size_uncompressed);
        
        if(bgav_input_read_data(input, buf_compressed, size_compressed) <
           size_compressed)
          return 0;
        size_uncompressed_ret = size_uncompressed;
        if(uncompress(buf_uncompressed, &size_uncompressed_ret,
                      buf_compressed, size_compressed) != Z_OK)
          {
          gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Uncompression failed");
          return 0;
          }
        input_mem = bgav_input_open_memory(buf_uncompressed,
                                           size_uncompressed_ret,
                                           input->opt);

        if(!bgav_qt_atom_read_header(input_mem, &ch) ||
           ch.fourcc != BGAV_MK_FOURCC('m', 'o', 'o', 'v'))
          {
          bgav_input_destroy(input_mem);
          return 0;
          }
        result = bgav_qt_moov_read(&ch, input_mem, ret);
        bgav_input_destroy(input_mem);
        free(buf_compressed);
        free(buf_uncompressed);
        
        return result;
        break;
      default:
        bgav_qt_atom_skip_unknown(input, &ch, h->fourcc);
      }
    }
  return 1;
  }
