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

#include <stdlib.h>
#include <string.h>

#include <avdec_private.h>


bgav_packet_t * bgav_packet_create()
  {
  bgav_packet_t * ret = calloc(1, sizeof(*ret));
  return ret;
  }

void bgav_packet_free(bgav_packet_t * p)
  {
  if(p->data)
    free(p->data);
  if(p->audio_frame)
    gavl_audio_frame_destroy(p->audio_frame);
  
  bgav_packet_free_palette(p);
  memset(p, 0, sizeof(*p));
  }

void bgav_packet_destroy(bgav_packet_t * p)
  {
  bgav_packet_free(p);
  free(p);
  }

void bgav_packet_alloc(bgav_packet_t * p, int size)
  {
  if(size + GAVL_PACKET_PADDING > p->data_alloc)
    {
    p->data_alloc = size + GAVL_PACKET_PADDING + 1024;
    p->data = realloc(p->data, p->data_alloc);
    }
  /* Pad in advance */
  memset(p->data + size, 0, GAVL_PACKET_PADDING);
  }

void bgav_packet_pad(bgav_packet_t * p)
  {
  /* Padding */
  memset(p->data + p->data_size, 0, GAVL_PACKET_PADDING);
  }


void bgav_packet_dump(const bgav_packet_t * p)
  {
  bgav_dprintf("pos: %"PRId64", K: %d, ", p->position, !!PACKET_GET_KEYFRAME(p));

  if(p->field2_offset)
    bgav_dprintf("f2: %d, ", p->field2_offset);
  
  bgav_dprintf("T: %s ", bgav_coding_type_to_string(p->flags));
  
  if(p->dts != GAVL_TIME_UNDEFINED)
    bgav_dprintf("dts: %"PRId64", ", p->dts);

  if(p->pts == GAVL_TIME_UNDEFINED)
    bgav_dprintf("pts: (none), ");
  else
    bgav_dprintf("pts: %"PRId64", ", p->pts);
  
  bgav_dprintf("Len: %d, dur: %"PRId64, p->data_size, p->duration);

  if(p->header_size)
    bgav_dprintf(", head: %d", p->header_size);

  if(p->sequence_end_pos)
    bgav_dprintf(", end: %d", p->sequence_end_pos);

  if(PACKET_GET_SKIP(p))
    bgav_dprintf(", skip");

  if(p->flags & GAVL_PACKET_NOOUTPUT)
    bgav_dprintf(", nooutput");

  if(p->flags & GAVL_PACKET_REF)
    bgav_dprintf(", ref");

  if(p->flags & PACKET_FLAG_FIELD_PIC)
    bgav_dprintf(", field-pic");
  
  if(p->timecode != GAVL_TIMECODE_UNDEFINED)
    {
    bgav_dprintf(", TC: ");
    gavl_timecode_dump(NULL, p->timecode);
    }
  
  if(p->interlace_mode == GAVL_INTERLACE_TOP_FIRST)
    bgav_dprintf(", il: t");
  else if(p->interlace_mode == GAVL_INTERLACE_BOTTOM_FIRST)
    bgav_dprintf(", il: b");

  if(p->end_pts != GAVL_TIME_UNDEFINED)
    bgav_dprintf(" end_pts: %"PRId64", ", p->end_pts);
  
  bgav_dprintf("\n");
  //  gavl_hexdump(p->data, p->data_size < 16 ? p->data_size : 16, 16);
  }

void bgav_packet_dump_data(bgav_packet_t * p, int bytes)
  {
  if(bytes > p->data_size)
    bytes = p->data_size;
  gavl_hexdump(p->data, bytes, 16);
  }

#define SWAP(n1, n2) \
  swp = n1; n1 = n2; n2 = swp;

void bgav_packet_swap_data(bgav_packet_t * p1, bgav_packet_t * p2)
  {
  uint8_t * swp_ptr;
  int64_t swp;
  
  swp_ptr = p1->data;
  p1->data = p2->data;
  p2->data = swp_ptr;
  
  SWAP(p1->data_size, p2->data_size);
  SWAP(p1->data_alloc, p2->data_alloc);
  }

void bgav_packet_reset(bgav_packet_t * p)
  {
  p->pts     = GAVL_TIME_UNDEFINED;
  p->dts     = GAVL_TIME_UNDEFINED;
  p->end_pts = GAVL_TIME_UNDEFINED;

  p->timecode = GAVL_TIMECODE_UNDEFINED;
  p->flags = 0;
  p->data_size = 0;
  p->header_size = 0;
  p->sequence_end_pos = 0;
  p->duration = -1;
  bgav_packet_free_palette(p);
  }

void bgav_packet_copy_metadata(bgav_packet_t * dst,
                               const bgav_packet_t * src)
  {
  dst->pts      = src->pts;
  dst->dts      = src->dts;
  dst->duration = src->duration;
  dst->flags    = src->flags;
  dst->timecode       = src->timecode;
  }

void bgav_packet_copy(bgav_packet_t * dst,
                      const bgav_packet_t * src)
  {
  uint32_t data_alloc;
  uint8_t * data;

  data_alloc = dst->data_alloc;
  data = dst->data;

  memcpy(dst, src, sizeof(*dst));

  dst->data = data;
  dst->data_alloc = data_alloc;

  bgav_packet_alloc(dst, src->data_size);
  memcpy(dst->data, src->data, src->data_size);
  }


void bgav_packet_source_copy(bgav_packet_source_t * dst,
                             const bgav_packet_source_t * src)
  {
  memcpy(dst, src, sizeof(*dst));
  }

void bgav_packet_alloc_palette(bgav_packet_t * p, int size)
  {
  p->palette = malloc(sizeof(*p->palette) * size);
  p->palette_size = size;
  }

void bgav_packet_free_palette(bgav_packet_t * p)
  {
  if(p->palette)
    {
    free(p->palette);
    p->palette = NULL;
    }
  p->palette_size = 0;
  }

void bgav_packet_merge_field2(bgav_packet_t * p,
                              const bgav_packet_t * field2)
  {
  bgav_packet_alloc(p, p->data_size + field2->data_size);
  memcpy(p->data + p->data_size, field2->data, field2->data_size);
  p->data_size += field2->data_size;
  }

void bgav_packet_2_gavl(bgav_packet_t * src,
                        gavl_packet_t * dst)
  {
  dst->pts      = src->pts;
  dst->duration = src->duration;

  dst->header_size      = src->header_size;
  dst->field2_offset    = src->field2_offset;
  dst->sequence_end_pos = src->sequence_end_pos;

  dst->flags    = src->flags & 0xFFFF;

  dst->buf.buf = src->data;
  dst->buf.len = src->data_size;
  //  dst->buf.alloc = src->data_alloc;
  dst->interlace_mode = src->interlace_mode;
  dst->timecode = src->timecode;

  dst->dst_x = src->dst_x;
  dst->dst_y = src->dst_y;

  gavl_rectangle_i_copy(&dst->src_rect, &src->src_rect);
  }

void bgav_packet_from_gavl(gavl_packet_t * src,
                           bgav_packet_t * dst)
  {
  dst->pts      = src->pts;
  dst->duration = src->duration;
  dst->flags    = src->flags;

  dst->header_size      = src->header_size;
  dst->field2_offset    = src->field2_offset;
  dst->sequence_end_pos = src->sequence_end_pos;
  dst->interlace_mode = src->interlace_mode;
  dst->timecode = src->timecode;

  dst->dst_x = src->dst_x;
  dst->dst_y = src->dst_y;

  gavl_rectangle_i_copy(&dst->src_rect, &src->src_rect);
  }

void bgav_packet_save(bgav_packet_t * p, const char * filename)
  {
  gavl_packet_t gp;
  gavl_packet_init(&gp);
  bgav_packet_2_gavl(p, &gp);
  gavl_packet_save(&gp, filename);
  }
