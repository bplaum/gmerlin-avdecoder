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




#include <avdec_private.h>

#include <string.h>
#include <stdio.h>

#include <pes_header.h>

#define PADDING_STREAM   0xbe
#define PRIVATE_STREAM_2 0xbf

int bgav_pes_header_read(bgav_input_context_t * input,
                         bgav_pes_header_t * ret)
  {
  //  int i;
  uint8_t c;
  uint16_t len;
  int64_t pos;
  int64_t header_start;
  uint16_t tmp_16;
  
  uint8_t header_flags;
  uint8_t header_size;

  uint32_t header;

  memset(ret, 0, sizeof(*ret));
  ret->pts = GAVL_TIME_UNDEFINED;
  ret->dts = GAVL_TIME_UNDEFINED;
  
  if(!bgav_input_read_32_be(input, &header))
    return 0;
  ret->stream_id = header & 0x000000ff;

  if(!bgav_input_read_16_be(input, &len))
    return 0;

  if((ret->stream_id == PADDING_STREAM) ||
     (ret->stream_id == PRIVATE_STREAM_2))
    {
    ret->payload_size = len;
    return 1;
    }

  pos = input->position;

  if(!bgav_input_read_8(input, &c))
    return 0;

  if((c & 0xC0) == 0x80) /* MPEG-2 */
    {
    if(!bgav_input_read_8(input, &header_flags))
      return 0;
    if(!bgav_input_read_8(input, &header_size))
      return 0;

    header_start = input->position;
    
    if(header_flags)
      {
      
      /* Read stuff */
      if((header_flags & 0xc0) == 0x80) /* PTS present */
        {
        bgav_input_read_8(input, &c);
        ret->pts = (int64_t)((c >> 1) & 7) << 30;
        bgav_input_read_16_be(input, &tmp_16);
        ret->pts |= (int64_t)(tmp_16 >> 1) << 15;
        bgav_input_read_16_be(input, &tmp_16);
        ret->pts |= (int64_t)(tmp_16 >> 1);
        }
      else if((header_flags & 0xc0) == 0xc0) /* PTS+DTS present */
        {
        bgav_input_read_8(input, &c);
        ret->pts = (int64_t)((c >> 1) & 7) << 30;
        bgav_input_read_16_be(input, &tmp_16);
        ret->pts |= (int64_t)(tmp_16 >> 1) << 15;
        bgav_input_read_16_be(input, &tmp_16);
        ret->pts |= (int64_t)(tmp_16 >> 1);

        bgav_input_read_8(input, &c);
        ret->dts = (int64_t)((c >> 1) & 7) << 30;
        bgav_input_read_16_be(input, &tmp_16);
        ret->dts |= (int64_t)(tmp_16 >> 1) << 15;
        bgav_input_read_16_be(input, &tmp_16);
        ret->dts |= (int64_t)(tmp_16 >> 1);
        }
      
      if(header_flags & 0x20) // ESCR
        {
        bgav_input_skip(input, 6);
        }

      if(header_flags & 0x10) // ES rate
        {
        bgav_input_skip(input, 3);
        }

      if(header_flags & 0x08) // DSM trick mode
        {
        bgav_input_skip(input, 1);
        }

      if(header_flags & 0x04) // Additional copyright info
        {
        bgav_input_skip(input, 1);
        }

      if(header_flags & 0x02) // CRC
        {
        bgav_input_skip(input, 2);
        }

      if(header_flags & 0x01)
        {
        uint8_t ext_flags;
        uint8_t ext_size;

        bgav_input_read_8(input, &ext_flags);

        if(ext_flags & 0x80) // PES Private data
          {
          bgav_input_skip(input, 128);
          }
        
        if(ext_flags & 0x40) // Pack header
          {
          bgav_input_read_8(input, &c);
          bgav_input_skip(input, c);
          }
        
        if(ext_flags & 0x20) // Sequence counter
          {
          bgav_input_skip(input, 2);
          }

        if(ext_flags & 0x10) // P-STD Buffer
          {
          bgav_input_skip(input, 2);
          }

        if(ext_flags & 0x01) // PES Extension
          {
          bgav_input_read_8(input, &ext_size);
          ext_size &= 0x7f;
          if(ext_size > 0)
            {
            bgav_input_read_8(input, &c);
            if(!(c & 0x80))
              ret->stream_id = (ret->stream_id << 8) | c;
            }
          }
        }
      }
    bgav_input_skip(input, header_size - (input->position - header_start));
    }
  else /* MPEG-1 */
    {
    while(input->position < pos + len)
      {
      if((c & 0x80) != 0x80)
        break;
      bgav_input_read_8(input, &c);
      }
    /* Skip STD Buffer scale */
    
    if((c & 0xC0) == 0x40)
      {
      bgav_input_skip(input, 1);
      bgav_input_read_8(input, &c);
      }

    if((c & 0xf0) == 0x20)
      {
      ret->pts = (int64_t)((c >> 1) & 7) << 30;
      bgav_input_read_16_be(input, &tmp_16);
      ret->pts |= (int64_t)((tmp_16 >> 1) << 15);
      bgav_input_read_16_be(input, &tmp_16);
      ret->pts |= (int64_t)(tmp_16 >> 1);
      }
    else if((c & 0xf0) == 0x30)
      {
      /* PTS */
      ret->pts = (int64_t)((c >> 1) & 7) << 30;
      bgav_input_read_16_be(input, &tmp_16);
      ret->pts |= (int64_t)((tmp_16 >> 1) << 15);
      bgav_input_read_16_be(input, &tmp_16);
      ret->pts |= (int64_t)(tmp_16 >> 1);
      /* DTS */

      bgav_input_read_data(input, &c, 1);
      ret->dts = (int64_t)((c >> 1) & 7) << 30;
      bgav_input_read_16_be(input, &tmp_16);
      ret->dts |= (int64_t)((tmp_16 >> 1) << 15);
      bgav_input_read_16_be(input, &tmp_16);
      ret->dts |= (int64_t)(tmp_16 >> 1);

      //  bgav_input_skip(input, 5);
      }
    }
  ret->payload_size = len - (input->position - pos);
//   if(ret->payload_size < 0)
//    fprintf(stderr, "payload size < 0, len was %d\n", len);
  //  bgav_pes_header_dump(ret);
  return 1;
  }

static void dump_timestamp(int64_t ts)
  {
  if(ts > 0)
    gavl_dprintf("%"PRId64" (%f)", ts, (float)ts / 90000.0);
  else
    gavl_dprintf("Unknown");
  }

void bgav_pes_header_dump(bgav_pes_header_t * p)
  {
  gavl_dprintf("PES Header: PTS: ");
  dump_timestamp(p->pts);
  gavl_dprintf(" DTS: ");
  dump_timestamp(p->dts);
  gavl_dprintf(" Stream ID: %02x, payload_size: %d\n",
               p->stream_id, p->payload_size);
  }

/* Pack header */

void bgav_pack_header_dump(bgav_pack_header_t * h)
  {
  gavl_dprintf(
          "Pack header: MPEG-%d, SCR: %" PRId64 " (%f secs), Mux rate: %d bits/s\n",
          h->version, h->scr, (float)(h->scr)/90000.0,
          h->mux_rate * 400);
  }

int bgav_pack_header_read(bgav_input_context_t * input,
                          bgav_pack_header_t * ret)
  {
  uint8_t c;
  uint16_t tmp_16;
  uint32_t tmp_32;

  int stuffing;
  
  bgav_input_skip(input, 4);
    
  if(!bgav_input_read_8(input, &c))
    return 0;

  if((c & 0xf0) == 0x20) /* MPEG-1 */
    {
    //    bgav_input_read_8(input, &c);
    
    ret->scr = ((c >> 1) & 7) << 30;
    bgav_input_read_16_be(input, &tmp_16);
    ret->scr |= ((tmp_16 >> 1) << 15);
    bgav_input_read_16_be(input, &tmp_16);
    ret->scr |= (tmp_16 >> 1);
    
    bgav_input_read_8(input, &c);
    ret->mux_rate = (c & 0x7F) << 15;
                                                                              
    bgav_input_read_8(input, &c);
    ret->mux_rate |= ((c & 0x7F) << 7);
                                                                              
    bgav_input_read_8(input, &c);
    ret->mux_rate |= (((c & 0xFE)) >> 1);
    ret->version = 1;
    }
  else if(c & 0x40) /* MPEG-2 */
    {
    /* SCR */
    if(!bgav_input_read_32_be(input, &tmp_32))
      return 0;
    
    ret->scr = c & 0x03;

    ret->scr <<= 13;
    ret->scr |= ((tmp_32 & 0xfff80000) >> 19);
    
    ret->scr <<= 15;
    ret->scr |= ((tmp_32 & 0x0003fff8) >> 3);

    /* Skip SCR extension (would give 27 MHz resolution) */

    bgav_input_skip(input, 1);
        
    /* Mux rate (22 bits) */
    
    if(!bgav_input_read_8(input, &c))
      return 0;
    ret->mux_rate = c;

    ret->mux_rate <<= 8;
    if(!bgav_input_read_8(input, &c))
      return 0;
    ret->mux_rate |= c;

    ret->mux_rate <<= 6;
    if(!bgav_input_read_8(input, &c))
      return 0;
    ret->mux_rate |= (c>>2);

    ret->version = 2;
    
    /* Now, some stuffing bytes might come.
       They are set to 0xff and will be skipped by the
       next_start_code function */
    bgav_input_read_8(input, &c);
    stuffing = c & 0x03;
    bgav_input_skip(input, stuffing);

    }
  
  return 1;
  }
