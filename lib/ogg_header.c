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




// #include <string.h>
#include <config.h>


#include <avdec_private.h>
#include <ogg_header.h>
#include <gavl/log.h>

#define LOG_DOMAIN "ogg"

int bgav_ogg_page_read_header(bgav_input_context_t * ctx,
                              bgav_ogg_page_t * ret)
  {
  ret->position = ctx->position;

  bgav_input_skip(ctx, 4); // OggS
  if((bgav_input_read_data(ctx, &ret->stream_structure_version, 1) < 1) ||
     (bgav_input_read_data(ctx, &ret->header_type_flags, 1) < 1) ||
     (!bgav_input_read_64_le(ctx, (uint64_t*)&ret->granulepos)) ||
     (!bgav_input_read_32_le(ctx, &ret->serialno)) ||
     (!bgav_input_read_32_le(ctx, &ret->sequenceno)) ||
     (!bgav_input_read_32_le(ctx, &ret->crc)) ||
     (bgav_input_read_data(ctx, &ret->num_page_segments, 1) < 1) ||
     (bgav_input_read_data(ctx, ret->page_segments, ret->num_page_segments) < ret->num_page_segments))
    return 0;
  else
    {
    //    bgav_ogg_page_dump_header(ret);
    ret->valid = 1;
    return 1;
    }
  }

void bgav_ogg_page_dump_header(const bgav_ogg_page_t * p)
  {
  int i, num;
  gavl_dprintf("Ogg Page:\n");
  gavl_dprintf("  file position: %"PRId64":\n", p->position);

  gavl_dprintf("  stream_structure_version: %d:\n", p->stream_structure_version);
  gavl_dprintf("  header_type_flags:        %d: (Continued: %d, BOS: %d EOS: %d)\n",
               p->header_type_flags,
               p->header_type_flags & BGAV_OGG_HEADER_TYPE_CONTINUED,
               p->header_type_flags & BGAV_OGG_HEADER_TYPE_BOS,
               p->header_type_flags & BGAV_OGG_HEADER_TYPE_EOS);
  gavl_dprintf("  granulepos:               %"PRId64"\n", p->granulepos);
  gavl_dprintf("  serialno:                 %d\n", p->serialno);

  gavl_dprintf("  sequenceno:               %d\n", p->sequenceno);
  gavl_dprintf("  crc:                      %08x\n", p->crc);
  gavl_dprintf("  num_page_segments:        %d\n", p->num_page_segments);

  num = bgav_ogg_page_num_packets(p);
  
  gavl_dprintf("  (partial) packets:        %d\n", num);
  gavl_dprintf("  Page segments:\n");

  //  for(i = 0; i < p->num_page_segments; i++)
  //    gavl_dprintf("    %d\n", p->page_segments[i]);

  for(i = 0; i < num; i++)
    gavl_dprintf("    %d\n", bgav_ogg_page_get_packet_size(p, i));
  
  }


int bgav_ogg_page_num_packets(const bgav_ogg_page_t * h)
  {
  int i;
  int ret = 0;
  
  if(!h->num_page_segments)
    return 0;
  
  for(i = 0; i < h->num_page_segments; i++)
    {
    if(h->page_segments[i] < 255)
      ret++;
    }
  
  if(h->page_segments[h->num_page_segments-1] == 255)
    ret++;
  
  return ret;
  }

int bgav_ogg_page_get_packet_size(const bgav_ogg_page_t * h, int idx)
  {
  int i;
  int ret = 0;
  int count = 0;
  
  for(i = 0; i < h->num_page_segments; i++)
    {
    if(count < idx)
      {
      if(h->page_segments[i] < 255)
        count++;
      continue;
      }
    ret += h->page_segments[i];
    if(h->page_segments[i] < 255)
      break;
    }
  return ret;
  }

int bgav_ogg_probe(bgav_input_context_t * input)
  {
  uint8_t probe_data[4];

  if(bgav_input_get_data(input, probe_data, 4) < 4)
    return 0;

  if((probe_data[0] == 'O') &&
     (probe_data[1] == 'g') &&
     (probe_data[2] == 'g') &&
     (probe_data[3] == 'S'))
    return 1;
  return 0;
  }


void bgav_ogg_page_skip(bgav_input_context_t * ctx,
                       const bgav_ogg_page_t * h)
  {
  int i, len = 0;

  for(i = 0; i < h->num_page_segments; i++)
    len += h->page_segments[i];
  bgav_input_skip(ctx, len);
  }


/* OGM header */


int bgav_ogm_header_read(bgav_input_context_t * input, bgav_ogm_header_t * ret)
  {
  if((bgav_input_read_data(input, (uint8_t*)ret->type, 8) < 8) ||
     !bgav_input_read_fourcc(input, &ret->subtype) ||
     !bgav_input_read_32_le(input, &ret->size) ||
     !bgav_input_read_64_le(input, &ret->time_unit) ||
     !bgav_input_read_64_le(input, &ret->samples_per_unit) ||
     !bgav_input_read_32_le(input, &ret->default_len) ||
     !bgav_input_read_32_le(input, &ret->buffersize) ||
     !bgav_input_read_16_le(input, &ret->bits_per_sample) ||
     !bgav_input_read_16_le(input, &ret->padding))
    return 0;

  if(gavl_string_starts_with(ret->type, "video"))
    {
    if(!bgav_input_read_32_le(input, &ret->data.video.width) ||
       !bgav_input_read_32_le(input, &ret->data.video.height))
      return 0;
    return 1;
    }
  else if(gavl_string_starts_with(ret->type, "audio"))
    {
    if(!bgav_input_read_16_le(input, &ret->data.audio.channels) ||
       !bgav_input_read_16_le(input, &ret->data.audio.blockalign) ||
       !bgav_input_read_32_le(input, &ret->data.audio.avgbytespersec))
      return 0;
    return 1;
    }
  else if(gavl_string_starts_with(ret->type, "text"))
    {
    return 1;
    }
  else
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "Unknown stream type \"%.8s\" in OGM header", ret->type);
    return 0;
    }
  }

void bgav_ogm_header_dump(bgav_ogm_header_t * h)
  {
  gavl_dprintf( "OGM Header\n");
  gavl_dprintf( "  Type              %.8s\n", h->type);
  gavl_dprintf( "  Subtype:          ");
  bgav_dump_fourcc(h->subtype);
  gavl_dprintf( "\n");

  gavl_dprintf( "  Size:             %d\n", h->size);
  gavl_dprintf( "  Time unit:        %" PRId64 "\n", h->time_unit);
  gavl_dprintf( "  Samples per unit: %" PRId64 "\n", h->samples_per_unit);
  gavl_dprintf( "  Default len:      %d\n", h->default_len);
  gavl_dprintf( "  Buffer size:      %d\n", h->buffersize);
  gavl_dprintf( "  Bits per sample:  %d\n", h->bits_per_sample);
  if(gavl_string_starts_with(h->type, "video"))
    {
    gavl_dprintf( "  Width:            %d\n", h->data.video.width);
    gavl_dprintf( "  Height:           %d\n", h->data.video.height);
    }
  if(gavl_string_starts_with(h->type, "audio"))
    {
    gavl_dprintf( "  Channels:         %d\n", h->data.audio.channels);
    gavl_dprintf( "  Block align:      %d\n", h->data.audio.blockalign);
    gavl_dprintf( "  Avg bytes per sec: %d\n", h->data.audio.avgbytespersec);
    }
  }
