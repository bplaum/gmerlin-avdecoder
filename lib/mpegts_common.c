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
#include <stdio.h>

#include <avdec_private.h>
#include <mpegts_common.h>

#define LOG_DOMAIN "mpeg_transport"


typedef struct
  {
  int      ts_type;   /* From pmt */
  int      bgav_type; /* #defines from avdec_private.h */
  uint32_t fourcc;
  const char * description; /* For debugging only! */
  } stream_type_t;

static const stream_type_t stream_types[] =
  {
    {
      .ts_type =     STREAM_TYPE_VIDEO_MPEG1,
      .bgav_type =   GAVL_STREAM_VIDEO,
      .fourcc =      BGAV_MK_FOURCC('m', 'p', 'v', '1'),
      .description = "MPEG-1 Video",
    },
    {
      .ts_type =     STREAM_TYPE_VIDEO_MPEG2,
      .bgav_type =   GAVL_STREAM_VIDEO,
      .fourcc =      BGAV_MK_FOURCC('m', 'p', 'v', '2'),
      .description = "MPEG-2 Video",
    },
    {
      .ts_type =     STREAM_TYPE_AUDIO_MPEG1,
      .bgav_type =   GAVL_STREAM_AUDIO,
      .fourcc =      BGAV_MK_FOURCC('.', 'm', 'p', '2'),
      .description = "MPEG-1 Audio",
    },
    {
      .ts_type =     STREAM_TYPE_AUDIO_MPEG2,
      .bgav_type =   GAVL_STREAM_AUDIO,
      .fourcc =      BGAV_MK_FOURCC('m', 'p', 'g', 'a'),
      .description = "MPEG-2 Audio",
    },
    {
      .ts_type =     STREAM_TYPE_AUDIO_AAC,
      .bgav_type =   GAVL_STREAM_AUDIO,
      .fourcc =      BGAV_MK_FOURCC('A', 'D', 'T', 'S'),
      .description = "MPEG-4 Audio (AAC)",
    },
    {
      .ts_type =     STREAM_TYPE_VIDEO_MPEG4,
      .bgav_type =   GAVL_STREAM_VIDEO,
      .fourcc =      BGAV_MK_FOURCC('m', 'p', '4', 'v'),
      .description = "MPEG-4 Video",
    },
    {
      .ts_type =     STREAM_TYPE_VIDEO_H264,
      .bgav_type =   GAVL_STREAM_VIDEO,
      .fourcc =      BGAV_MK_FOURCC('H', '2', '6', '4'),
      .description = "H264 Video",
    },
    {
      .ts_type =     STREAM_TYPE_AUDIO_AC3,
      .bgav_type =   GAVL_STREAM_AUDIO,
      .fourcc =      BGAV_MK_FOURCC('.', 'a', 'c', '3'),
      .description = "AC3 Audio",
    },
    {
      .ts_type =     STREAM_TYPE_AUDIO_DTS,
      .bgav_type =   GAVL_STREAM_AUDIO,
      .fourcc =      BGAV_MK_FOURCC('.', 'd', 't', 's'),
      .description = "DTS Audio",
    },
  };

static const stream_type_t * get_stream_type(int ts_type)
  {
  int i;

  for(i = 0; i < sizeof(stream_types)/sizeof(stream_types[0]); i++)
    {
    if(stream_types[i].ts_type == ts_type)
      return &stream_types[i];
    }
  return NULL;
  }


int bgav_pmt_section_read(uint8_t * data, int size,
                          pmt_section_t * ret)
  {
  int len;
  uint8_t * ptr, *ptr_start;

  memset(ret, 0, sizeof(*ret));

  ptr = data;

  ret->table_id = *ptr; ptr++;
  ret->section_length = GAVL_PTR_2_16BE(ptr) & 0x0fff; ptr+=2;

  ptr_start = ptr;
  
  if(ret->section_length > size - (ptr - data))
    return 0; /* Not enough data */

  ret->program_number = GAVL_PTR_2_16BE(ptr); ptr+=2;
  ret->current_next_indicator = (*ptr) & 0x01; ptr++;
  ret->section_number = *ptr; ptr++;
  ret->last_section_number = *ptr; ptr++;
  ret->pcr_pid = (GAVL_PTR_2_16BE(ptr)) & 0x1fff; ptr+=2;
  len = GAVL_PTR_2_16BE(ptr) & 0x0fff; ptr += 2;
  
  if(len)
    {
    memcpy(ret->descriptor, ptr, len);
    ret->descriptor[len] = '\0';
    ret->descriptor_len = len;
    ptr += len;
    }
  ret->num_streams = 0;

  /* 3 bytes before section_length, 4 bytes CRC */
  while((ptr - ptr_start) < ret->section_length - 4)
    {
    ret->streams[ret->num_streams].type = *ptr; ptr++;
    ret->streams[ret->num_streams].pid  =
        GAVL_PTR_2_16BE(ptr) & 0x1fff; ptr+=2;
    len = GAVL_PTR_2_16BE(ptr) & 0x0fff; ptr += 2;

    if(len)
      {
      memcpy(ret->streams[ret->num_streams].descriptor, ptr, len);
      ret->streams[ret->num_streams].descriptor[len] = '\0';
      ret->streams[ret->num_streams].descriptor_len = len;      
      ptr += len;
      }
    ret->num_streams++;
    }
  return 1;
  }

void bgav_pmt_section_dump(pmt_section_t * pmts)
  {
  int i;
  const stream_type_t * stream_type;
  
  bgav_dprintf( "PMT section:\n");
  bgav_dprintf( "  table_id:               %02x\n",   pmts->table_id);
  bgav_dprintf( "  section_length:         %d\n",     pmts->section_length);
  bgav_dprintf( "  program_number:         %d\n",     pmts->program_number);
  bgav_dprintf( "  current_next_indicator: %d\n",     pmts->current_next_indicator);
  bgav_dprintf( "  section_number:         %d\n",     pmts->section_number);
  bgav_dprintf( "  last_section_number:    %d\n",     pmts->last_section_number);
  bgav_dprintf( "  pcr_pid:                0x%04x (%d)\n",
                pmts->pcr_pid, pmts->pcr_pid);
  bgav_dprintf( "  descriptor:             ");
  if(pmts->descriptor_len)
    gavl_hexdump((uint8_t*)pmts->descriptor, pmts->descriptor_len, pmts->descriptor_len);
  else
    bgav_dprintf( "[none]\n");
  bgav_dprintf( "  Number of streams:      %d\n",     pmts->num_streams);

  for(i = 0; i < pmts->num_streams; i++)
    {
    stream_type = get_stream_type(pmts->streams[i].type);
    
    bgav_dprintf( "  Stream %d, present: %d\n", i+1, pmts->streams[i].present);

    if(stream_type)
      bgav_dprintf( "    type:       0x%02x (%s)\n",
                    pmts->streams[i].type, stream_type->description);
    else
      bgav_dprintf( "    type:       0x%02x (unknown)\n",
                    pmts->streams[i].type);
    
    bgav_dprintf( "    PID:        0x%04x (%d)\n",
                  pmts->streams[i].pid, pmts->streams[i].pid);
    bgav_dprintf( "    descriptor: ");
    
    if(pmts->streams[i].descriptor_len)
      gavl_hexdump((uint8_t*)pmts->streams[i].descriptor,
                   pmts->streams[i].descriptor_len,
                   pmts->streams[i].descriptor_len);
    else
      bgav_dprintf( "[none]\n");
    }
  }

int bgav_pat_section_read(uint8_t * data, int size,
                          pat_section_t * ret)
  {
  int i;
  uint8_t * ptr;

  memset(ret, 0, sizeof(*ret));
  
  //  ptr = data+1+data[0];
  ptr = data;
  
  ret->table_id = *ptr; ptr++;
  
  ret->section_length = GAVL_PTR_2_16BE(ptr) & 0x0fff; ptr+=2;
  if(ret->section_length > size - (ptr - data))
    return 0; /* Not enough data */

  ret->transport_stream_id = GAVL_PTR_2_16BE(ptr);ptr+=2;
  ret->current_next_indicator = *ptr & 0x01; ptr++;
  ret->section_number = *ptr; ptr++;
  ret->last_section_number = *ptr; ptr++;

  ret->num_programs = (ret->section_length - 9) / 4;

  for(i = 0; i < ret->num_programs; i++)
    {
    ret->programs[i].program_number = GAVL_PTR_2_16BE(ptr); ptr+=2;
    ret->programs[i].program_map_pid = GAVL_PTR_2_16BE(ptr) & 0x1fff; ptr+=2;
    }
  return 1;
  }

void bgav_pat_section_dump(pat_section_t * pats)
  {
  int i;
  bgav_dprintf( "PAT section:\n");
  bgav_dprintf( "  table_id:               %d\n", pats->table_id);
  bgav_dprintf( "  section_length:         %d\n", pats->section_length);
  bgav_dprintf( "  transport_stream_id:    %d\n", pats->transport_stream_id);
  bgav_dprintf( "  current_next_indicator: %d\n", pats->current_next_indicator);
  bgav_dprintf( "  section_number:         %d\n", pats->section_number);
  bgav_dprintf( "  last_section_number:    %d\n", pats->last_section_number);
  
  bgav_dprintf( "  Number of programs: %d\n", pats->num_programs);
  for(i = 0; i < pats->num_programs; i++)
    {
    bgav_dprintf( "    Program: %d ", pats->programs[i].program_number);
    bgav_dprintf( "Program map PID: 0x%04x (%d)\n",
                  pats->programs[i].program_map_pid,
                  pats->programs[i].program_map_pid);
    }
  }

void bgav_transport_packet_dump(transport_packet_t * p)
  {
  bgav_dprintf( "Transport packet:\n");
  bgav_dprintf( "  Payload start:      %d\n", p->payload_start);
  bgav_dprintf( "  PID:                0x%04x\n", p->pid);
  bgav_dprintf( "  Adaption field:     %s\n",
          (p->has_adaption_field ? "Yes" : "No"));
  bgav_dprintf( "  Payload:            %s\n",
          (p->has_payload ? "Yes" : "No"));
  bgav_dprintf( "  Continuity counter: %d\n", p->continuity_counter);
  bgav_dprintf( "  Payload size: %d\n", p->payload_size);
  
  if(p->has_adaption_field)
    {
    bgav_dprintf( "    Adaption field:\n");
    if(p->adaption_field.pcr >= 0)
      bgav_dprintf( "    PCR: %f\n",
                    (float)p->adaption_field.pcr / 90000.0);
    else
      bgav_dprintf( "    PCR: None\n");

    bgav_dprintf( "    random_access_indicator: %d\n",
                  p->adaption_field.random_access_indicator);
    }
  }

int bgav_transport_packet_parse(uint8_t ** data, transport_packet_t * ret)
  {
  int tmp;
  int adaption_field_length, adaption_field_flags;
  uint8_t * ptr = *data;

  if(ptr[0] != 0x47)
    {
    return 0;
    }
  
  ret->transport_error = !!(ptr[1] & 0x80);
  
  
  ret->adaption_field.pcr = -1;
  
  ret->payload_start = !!(ptr[1] & 0x40);
  ret->pid = ((ptr[1] & 0x1f) << 8) | ptr[2];

  tmp = (ptr[3] & 0x30) >> 4;

  switch(tmp)
    {
    case 0x01:
      ret->has_adaption_field = 0;
      ret->has_payload        = 1;
      break;
    case 0x02:
      ret->has_adaption_field = 1;
      ret->has_payload        = 0;
      break;
    case 0x03:
      ret->has_adaption_field = 1;
      ret->has_payload        = 1;
      break;
    default:

      ret->has_adaption_field = 0;
      ret->has_payload        = 0;

      //      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Invalid packet");
      //      return 0;
      break;
    }
  ret->continuity_counter = ptr[3] & 0x0f;

  ptr += 4;

  if(ret->has_adaption_field)
    {
    adaption_field_length = *ptr; ptr++;
    ret->payload_size = 184 - adaption_field_length - 1;
    
    if(adaption_field_length)
      {
      adaption_field_flags = *ptr; ptr++;
      adaption_field_length--;

      if(adaption_field_flags & 0x40) /* random_access_indicator */
        ret->adaption_field.random_access_indicator = 1;
      else
        ret->adaption_field.random_access_indicator = 0;

      if(adaption_field_flags & 0x10) /* PCR_flag */
        {
        ret->adaption_field.pcr  = (int64_t)(ptr[0]) << 25;
        ret->adaption_field.pcr += (int64_t)(ptr[1]) << 17;
        ret->adaption_field.pcr += (int64_t)(ptr[2]) << 9;
        ret->adaption_field.pcr += (int64_t)(ptr[3]) << 1;
        ret->adaption_field.pcr += ((int64_t)(ptr[4]) & 0x80) >> 7;
        
        ptr += 6;
        adaption_field_length -= 6;
        }
      
      }
    /* TODO: Something useful in the adaption field?? */
    ptr += adaption_field_length;
    }
  else
    ret->payload_size = 184;
  *data = ptr;
  return 1;
  }

static uint8_t * find_descriptor(uint8_t * data, int len,
                                 int tag, int * desc_len)
  {
  uint8_t * ptr_end, *ptr;
  ptr     = data;
  ptr_end = data + len;

  while(ptr + 2 < ptr_end)
    {
    if(ptr[0] == tag)
      {
      *desc_len = ptr[1];
      return ptr;
      }
    ptr += ptr[1] + 2;
    }
  return NULL;
  }

int bgav_pmt_section_setup_track(pmt_section_t * pmts,
                                  bgav_track_t * track,
                                  const bgav_options_t * opt,
                                  int max_audio_streams,
                                  int max_video_streams,
                                  int max_ac3_streams,
                                  int * num_ac3_streams,
                                  int * extra_pcr_pid)
  {
  int i;
  const stream_type_t * st;
  bgav_stream_t * s;
  uint8_t * desc;
  int desc_len;
  int ac3_streams = 0;
  int ret = 0;
  
  if(extra_pcr_pid)
    *extra_pcr_pid = 0;
  if(num_ac3_streams)
    *num_ac3_streams = 0;
  
  for(i = 0; i < pmts->num_streams; i++)
    {
    //    if(!pmts->streams[i].present)
    //      continue;
    
    st = get_stream_type(pmts->streams[i].type);
    
    if(st && (st->bgav_type == GAVL_STREAM_AUDIO) &&
       ((max_audio_streams < 0) ||
        (track->num_audio_streams < max_audio_streams)))
      {
      s = bgav_track_add_audio_stream(track, opt);
      s->index_mode = INDEX_MODE_SIMPLE;
      s->fourcc = st->fourcc;

      if(s->fourcc == BGAV_MK_FOURCC('A','D','T','S'))
        s->flags |= STREAM_FILTER_PACKETS;
      }
    else if(st && (st->bgav_type == GAVL_STREAM_VIDEO) &&
       ((max_video_streams < 0) ||
        (track->num_video_streams < max_video_streams)))
      {
      s = bgav_track_add_video_stream(track, opt);
      s->index_mode = INDEX_MODE_SIMPLE;
      s->fourcc = st->fourcc;
      }
    /* ISO/IEC 13818-1 PES packets containing private data */
    else if(pmts->streams[i].type == 0x06)
      {
      desc = find_descriptor(pmts->streams[i].descriptor,
                             pmts->streams[i].descriptor_len,
                             0x05, &desc_len);
      if(desc && (desc_len >= 4) &&
         (BGAV_PTR_2_FOURCC(desc+2) == BGAV_MK_FOURCC('A','C','-','3')) &&
         ((max_ac3_streams < 0) || (ac3_streams < max_ac3_streams) ))
        {
        s = bgav_track_add_audio_stream(track, opt);
        s->fourcc = BGAV_MK_FOURCC('.','a','c','3');
        s->index_mode = INDEX_MODE_SIMPLE;
        ac3_streams++;
        }
      else
        {
        s = NULL;
        if(extra_pcr_pid && (pmts->streams[i].pid == pmts->pcr_pid))
          *extra_pcr_pid = 1;
        }
      }
    else if(pmts->streams[i].type == 0xd1)
      {
      desc = find_descriptor(pmts->streams[i].descriptor,
                             pmts->streams[i].descriptor_len,
                             0x05, &desc_len);
      if(desc && (desc_len >= 4) &&
         (BGAV_PTR_2_FOURCC(desc+2) == BGAV_MK_FOURCC('d','r','a','c')) &&
         ((max_video_streams < 0) ||
          (track->num_video_streams < max_video_streams)))
        {
        s = bgav_track_add_video_stream(track, opt);
        s->fourcc = BGAV_MK_FOURCC('d','r','a','c');
        s->index_mode = INDEX_MODE_SIMPLE;
        }
      else
        {
        s = NULL;
        if(extra_pcr_pid && (pmts->streams[i].pid == pmts->pcr_pid))
          *extra_pcr_pid = 1;
        }
      }
    else
      {
      /* No usable stream, but can carry the PCR! */
      s = NULL;
      if(extra_pcr_pid && (pmts->streams[i].pid == pmts->pcr_pid))
        *extra_pcr_pid = 1;
      }
    
    if(s)
      {
      if(s->type == GAVL_STREAM_AUDIO)
        {
        desc = find_descriptor(pmts->streams[i].descriptor,
                               pmts->streams[i].descriptor_len,
                               0x0a, &desc_len);
        if(desc)
          {
          char language[4];
          
          language[0] = desc[2];
          language[1] = desc[3];
          language[2] = desc[4];
          language[3] = '\0';
          bgav_correct_language(language);
          gavl_dictionary_set_string(s->m, GAVL_META_LANGUAGE,
                           language);
          }
        }

      if(s->fourcc == BGAV_MK_FOURCC('d','r','a','c'))
        s->flags |= (STREAM_PARSE_FRAME);
      else
        s->flags |= (STREAM_PARSE_FULL);
      
      s->stats.pts_start = GAVL_TIME_UNDEFINED;
      s->timescale = 90000;
      s->stream_id = pmts->streams[i].pid;
      ret++;
      }
    }
  if(num_ac3_streams)
    *num_ac3_streams = ac3_streams;
  return ret;
  }
