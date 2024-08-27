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
#include <qt.h>

#define LOG_DOMAIN "qt_timecode"

void bgav_qt_init_timecodes(bgav_input_context_t * input,
                            bgav_stream_t * s,
                            qt_trak_t * trak, int64_t pts_offset)
  {
  int i, j;
  bgav_timecode_table_t * table;
  int total_samples, num_samples;
  int samples_read;
  int64_t pts;
  int64_t last_pos;
  int stts_pos   = 0;
  int stsc_pos   = 0;
  int stts_count = 0;

  int32_t timecode;
  qt_stts_t * stts;
  qt_stsc_t * stsc;
  qt_stco_t * stco;

  if(!(input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "Timecode tracks in non-seekable sources not supported");
    return;
    }
  
  last_pos = input->position;
  
  stts = &trak->mdia.minf.stbl.stts;
  stsc = &trak->mdia.minf.stbl.stsc;
  stco = &trak->mdia.minf.stbl.stco;

  /* Set timecode format */
  s->data.video.format->timecode_format.int_framerate =
    trak->mdia.minf.stbl.stsd.entries[0].desc.format.timecode.numframes;

  /* Flags:
   *
   * Drop frame
   * Indicates whether the timecode is drop frame.
   * Set it to 1 if the timecode is drop frame. This flag's value is 0x0001.
   *
   * 24 hour max
   * Indicates whether the timecode wraps after 24 hours.
   * Set it to 1 if the timecode wraps. This flag's value is 0x0002.
   *
   * Negative times OK
   * Indicates whether negative time values are allowed.
   * Set it to 1 if the timecode supports negative values. This flag's value is 0x0004.
   *
   * Counter
   * Indicates whether the time value corresponds to a tape counter value.
   * Set it to 1 if the timecode values are tape counter values. This flag's value is 0x0008.
   *
   */
  
  if(trak->mdia.minf.stbl.stsd.entries[0].desc.format.timecode.flags & 0x0001)
    s->data.video.format->timecode_format.flags |= GAVL_TIMECODE_DROP_FRAME;
  
  /* Count samples */

  total_samples = 0;
  for(i = 0; i < stts->num_entries; i++)
    total_samples += stts->entries[i].count;
  
  table = bgav_timecode_table_create(total_samples);

  samples_read = 0;
  pts = 0;
  /* Read samples */
  
  for(i = 0; i < stco->num_entries; i++)
    {
    bgav_input_seek(input, stco->entries[i], SEEK_SET);
    num_samples = stsc->entries[stsc_pos].samples_per_chunk;

    for(j = 0; j < num_samples; j++)
      {
      if(!bgav_input_read_32_be(input, (uint32_t*)(&timecode)))
        {
        gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
                 "EOF while reading timecode");
        goto fail;
        }
      table->entries[samples_read].pts      = pts + pts_offset;
      table->entries[samples_read].timecode =
        gavl_timecode_from_framecount(&s->data.video.format->timecode_format, timecode);

      samples_read++;

      /* Advance pts + stts */
      pts += stts->entries[stts_pos].duration;
      stts_count++;

      if(stts_count >= stts->entries[stts_pos].count)
        {
        stts_pos++;
        stts_count = 0;
        }
      
      }
    
    /* Advance stsc */
    if((stsc_pos <  stsc->num_entries - 1) &&
       (i + 2 == stsc->entries[stsc_pos+1].first_chunk))
      {
      stsc_pos++;
      }
    }
  
  s->timecode_table = table;
  
  bgav_input_seek(input, last_pos, SEEK_SET);
  return;
  
fail:
  bgav_input_seek(input, last_pos, SEEK_SET);
  
  bgav_timecode_table_destroy(table);
  s->data.video.format->timecode_format.int_framerate = 0;
  return;
  
  }
