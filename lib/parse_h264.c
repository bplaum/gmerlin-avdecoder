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
#include <parser.h>
#include <mpv_header.h>
#include <h264_header.h>

#include <gavl/metatags.h>


// #define DUMP_AVCHD_SEI
#define DUMP_SEI

#define LOG_DOMAIN "parse_h264"

/* H.264 */

#define STATE_SPS             1
#define STATE_PPS             2
#define STATE_SEI             3
#define STATE_SLICE_HEADER    4
#define STATE_SLICE_PARTITION 5
#define STATE_SYNC            100

#define FLAG_HAVE_DATE         (1<<0)
#define FLAG_HAVE_SPS          (1<<1)
#define FLAG_HAVE_PPS          (1<<2)
// #define FLAG_USE_PTS           (1<<3)
#define FLAG_PES_TIMESTAMPS    (1<<3)

typedef struct
  {
  /* Sequence header */
  bgav_h264_sps_t sps;
  
  int state;
  
  uint8_t * rbsp;
  int rbsp_alloc;
  int rbsp_len;
  
  int has_aud;
  int is_avc;
  int nal_size_length;

  int flags;
  } h264_priv_t;

static void get_rbsp(bgav_packet_parser_t * parser, const uint8_t * pos, int len)
  {
  h264_priv_t * priv = parser->priv;
  if(priv->rbsp_alloc < len)
    {
    priv->rbsp_alloc = len;
    priv->rbsp = realloc(priv->rbsp, priv->rbsp_alloc);
    }
  priv->rbsp_len = bgav_h264_decode_nal_rbsp(pos, len, priv->rbsp);
  }

static const uint8_t avchd_mdpm[] =
  { 0x17,0xee,0x8c,0x60,0xf8,0x4d,0x11,0xd9,0x8c,0xd6,0x08,0x00,0x20,0x0c,0x9a,0x66,
    'M','D','P','M' };

#define BCD_2_INT(c) ((c >> 4)*10+(c&0xf))

static void reset_h264(bgav_packet_parser_t * parser)
  {
  h264_priv_t * priv = parser->priv;
  priv->state = STATE_SYNC;
  //  priv->have_sps = 0;
  }

static void cleanup_h264(bgav_packet_parser_t * parser)
  {
  h264_priv_t * priv = parser->priv;
  bgav_h264_sps_free(&priv->sps);
  if(priv->rbsp)
    free(priv->rbsp);

  free(priv);
  }

static void
handle_sei_new(bgav_packet_parser_t * parser, bgav_packet_t * p)
  {
  int sei_type, sei_size;
  uint8_t * ptr, * ptr_start;
  int header_len;
  bgav_h264_sei_pic_timing_t pt;
  bgav_h264_sei_recovery_point_t rp;
  
  h264_priv_t * priv = parser->priv;
  
  ptr_start = priv->rbsp;
  ptr = ptr_start;
  
  while(ptr_start - priv->rbsp < priv->rbsp_len - 2)
    {
    header_len =
      bgav_h264_decode_sei_message_header(ptr_start,
                                          priv->rbsp_len -
                                          (ptr - priv->rbsp),
                                          &sei_type, &sei_size);
    
    ptr_start += header_len;
    
    ptr = ptr_start;

#ifdef DUMP_SEI
    bgav_dprintf("Got SEI: %d (%d bytes)\n", sei_type, sei_size);
    gavl_hexdump(ptr, sei_size, 16);
#endif    
    switch(sei_type)
      {
      case 0: // buffering_period
        break;
      case 1:
        bgav_h264_decode_sei_pic_timing(ptr, priv->rbsp_len -
                                        (ptr - priv->rbsp),
                                        &priv->sps,
                                        &pt);
        // fprintf(stderr, "Got SEI pic_timing, pic_struct: %d\n", pt.pic_struct);

        p->duration = parser->vfmt->frame_duration;
        
        switch(pt.pic_struct)
          {
          case 0: // frame
            p->interlace_mode = GAVL_INTERLACE_NONE;
            break;
          case 1: // top field
            PACKET_SET_FIELD_PIC(p);
            p->interlace_mode = GAVL_INTERLACE_TOP_FIRST;
            break;
          case 2: // bottom field
            PACKET_SET_FIELD_PIC(p);
            p->interlace_mode = GAVL_INTERLACE_BOTTOM_FIRST;
            break;
          case 3: // top field, bottom field, in that order
            p->interlace_mode = GAVL_INTERLACE_TOP_FIRST;
            break;
          case 4: // bottom field, top field, in that order 
            p->interlace_mode = GAVL_INTERLACE_BOTTOM_FIRST;
            break;
          case 5: // top field, bottom field, top field repeated, in that order
            p->interlace_mode = GAVL_INTERLACE_TOP_FIRST;
            p->duration = (parser->vfmt->frame_duration*3)/2;
            break;
          case 6: // bottom field, top field, bottom field 
            p->interlace_mode = GAVL_INTERLACE_BOTTOM_FIRST;
            p->duration = (parser->vfmt->frame_duration*3)/2;
            break;
          case 7: // frame doubling                         
            p->duration = parser->vfmt->frame_duration*2;
            break;
          case 8: // frame tripling
            p->duration = parser->vfmt->frame_duration*3;
            break;
          }
        /* Output timecode */
#if 0
        if(pt.have_timecode)
          {
          if(!parser->format.timecode_format.int_framerate)
            {
            parser->format.timecode_format.int_framerate =
              parser->format.timescale / parser->format.frame_duration;
            if(parser->format.timescale % parser->format.frame_duration)
              parser->format.timecode_format.int_framerate++;
            if(pt.counting_type == 4)
              parser->format.timecode_format.flags |= GAVL_TIMECODE_DROP_FRAME;
            }
          gavl_timecode_from_hmsf(&parser->cache[parser->cache_size-1].tc,
                                  pt.tc_hours,
                                  pt.tc_minutes,
                                  pt.tc_seconds,
                                  pt.tc_frames);
          gavl_timecode_dump(&parser->format.timecode_format,
                             parser->cache[parser->cache_size-1].tc);
          fprintf(stderr, "\n");
          }
#endif
        break;
      case 2: // pan_scan_rect
        break;
      case 3: // filler_payload
        break;
      case 4: // user_data_registered_itu_t_t35
        break;
      case 5: // user_data_unregistered
        if(!memcmp(ptr, avchd_mdpm, 20))
          {
          /* AVCHD Timecodes: Since every scene is written to a new file
             it is sufficient to output just the start of the recording */
          int tag, num_tags, i;
          int year = -1, month = -1, day = -1, hour = -1, minute = -1, second = -1;
#ifdef DUMP_AVCHD_SEI
          bgav_dprintf( "Got AVCHD SEI message\n");
#endif
          
          /* Skip GUID + MDPM */
          ptr += 20;
          // sei_size -= 20;

          num_tags = *ptr; ptr++;
          
          /* 16 bytes GUID + 4 bytes MDPR + 1 byte num_tags */
          if((sei_size - 21) != num_tags * 5)
            continue;
          
          for(i = 0; i < num_tags; i++)
            {
            tag = *ptr; ptr++;

#ifdef DUMP_AVCHD_SEI
            bgav_dprintf( "Tag: 0x%02x, Data: %02x %02x %02x %02x\n",
                    tag, ptr[0], ptr[1], ptr[2], ptr[3]);
#endif
            switch(tag)
              {
              case 0x18:
                year  = BCD_2_INT(ptr[1])*100 + BCD_2_INT(ptr[2]);
                month = BCD_2_INT(ptr[3]);
                break;
              case 0x19:
                day    = BCD_2_INT(ptr[0]);
                hour   = BCD_2_INT(ptr[1]);
                minute = BCD_2_INT(ptr[2]);
                second = BCD_2_INT(ptr[3]);
                break;
              }
            ptr += 4;
            }
          
          if((year >= 0) && (month >= 0) && (day >= 0) &&
             (hour >= 0) && (minute >= 0) && (second >= 0))
            {
//            fprintf(stderr, "%04d-%02d-%02d %02d:%02d:%02d\n", 
//                    year, month, day, hour, minute, second);
            if(!parser->vfmt->timecode_format.int_framerate)
              {
              /* Get the timecode framerate */
              parser->vfmt->timecode_format.int_framerate =
                parser->vfmt->timescale / parser->vfmt->frame_duration;
              if(parser->vfmt->timescale % parser->vfmt->frame_duration)
                parser->vfmt->timecode_format.int_framerate++;
              
              /* For NTSC framerate we make a drop frame timecode */
              
              if((int64_t)parser->vfmt->timescale * 1001 ==
                 (int64_t)parser->vfmt->frame_duration * 30000)
                parser->vfmt->timecode_format.flags |= GAVL_TIMECODE_DROP_FRAME;
              
              /* We output only the first timecode in the file since the rest is redundant */
              gavl_timecode_from_hmsf(&p->timecode,
                                      hour,
                                      minute,
                                      second,
                                      0);
              gavl_timecode_from_ymd(&p->timecode,
                                     year,
                                     month,
                                     day);
              }
            
            }
          }
        break;
      case 6:
        if(!bgav_h264_decode_sei_recovery_point(ptr,
                                                priv->rbsp_len - (ptr - priv->rbsp), &rp))
          return;
        // parser->cache[parser->cache_size-1].recovery_point = rp.recovery_frame_cnt;
        break;
      case 7: // dec_ref_pic_marking_repetition
        break;
      case 8: // spare_pic
        break;
      case 9: // scene_info
        break;
      case 10: // sub_seq_info
        break;
      case 11: // sub_seq_layer_characteristics
        break;
      case 12: // sub_seq_characteristics
        break;
      case 13: // full_frame_freeze
        break;
      case 14: // full_frame_freeze_release
        break;
      case 15: // full_frame_snapshot
        break;
      case 16: // progressive_refinement_segment_start
        break;
      case 17: // progressive_refinement_segment_end
        break;
      case 18: // motion_constrained_slice_group_set
        break;
      case 19: // film_grain_characteristics
        break;
      case 20: // deblocking_filter_display_preference
        break;
      case 21: // stereo_video_info
        break;
      case 22: // post_filter_hint
        break;
      case 23: // tone_mapping_info
        break;
      case 24: // scalability_info (specified in Annex G)
        break;
      case 25: // sub_pic_scalable_layer (specified in Annex G)
        break;
      case 26: // non_required_layer_rep(specified in Annex G)
        break;
      case 27: // priority_layer_info(specified in Annex G)
        break;
      case 28: // layers_not_present(specified in Annex G)
        break;
      case 29: // layer_dependency_change(specified in Annex G)
        break;
      case 30: // scalable_nesting(specified in Annex G)
        break;
      case 31: // base_layer_temporal_hrd(specified in Annex G)
        break;
      case 32: // quality_layer_integrity_check(specified in Annex G)
        break;
      case 33: // redundant_pic_property(specified in Annex G)
        break;
      case 34: // tl0_picture_index(specified in Annex G)
        break;
      case 35: // tl_switching_point(specified in Annex G)
        break;

      
      }
    ptr_start += sei_size; 
    }
  }


static int parse_frame_avc(bgav_packet_parser_t * parser,
                           bgav_packet_t * p)
  {
  int nal_len = 0;
  bgav_h264_nal_header_t nh;
  //  bgav_h264_slice_header_t sh;
  
  h264_priv_t * priv = parser->priv;
  const uint8_t * ptr =   p->buf.buf;
  const uint8_t * end = p->buf.buf + p->buf.len;

  while(ptr < end)
    {
    switch(priv->nal_size_length)
      {
      case 1:
        nal_len = *ptr;
        ptr++;
        break;
      case 2:
        nal_len = GAVL_PTR_2_16BE(ptr);
        ptr += 2;
        break;
      case 4:
        nal_len = GAVL_PTR_2_32BE(ptr);
        ptr += 4;
        break;
      default:
        break;
      }

    if(!nal_len)
      return 0;
    
    nh.ref_idc   = ptr[0] >> 5;
    nh.unit_type = ptr[0] & 0x1f;
    ptr++;

    if((nh.unit_type == H264_NAL_NON_IDR_SLICE) ||
       (nh.unit_type == H264_NAL_IDR_SLICE) ||
       (nh.unit_type == H264_NAL_SLICE_PARTITION_A))
      {
      bgav_h264_slice_header_t sh;
      
      if(nh.ref_idc)
        PACKET_SET_REF(p);
      
      get_rbsp(parser, ptr, nal_len - 1);

      bgav_h264_slice_header_parse(priv->rbsp, priv->rbsp_len,
                                   &priv->sps,
                                   &sh);
      
      //          bgav_h264_slice_header_dump(&priv->sps,
      //                                      &sh);
      
      if(!(p->flags & GAVL_PACKET_TYPE_MASK))
        {
        switch(sh.slice_type)
          {
          case 2:
          case 7:
            p->flags |= BGAV_CODING_TYPE_I;
            break;
          case 0:
          case 5:
            p->flags |= BGAV_CODING_TYPE_P;
            break;
          case 1:
          case 6:
            p->flags |= BGAV_CODING_TYPE_B;
            break;
          default: /* Assume the worst */
            fprintf(stderr, "Unknown slice type %d\n", sh.slice_type);
            break;
          }
        }
      }
    ptr += (nal_len - 1);
    }
  return 1;
  }

static int find_frame_boundary_h264(bgav_packet_parser_t * parser, int * skip)
  {
  int header_len;
  bgav_h264_nal_header_t nh;
  h264_priv_t * priv = parser->priv;
  int new_state;
  
  const uint8_t * sc;

  while(1)
    {
    //    fprintf(stderr, "find_frame_boundary_h264\n");
    //    gavl_hexdump(parser->buf.buffer + parser->pos,
    //                 parser->buf.size - parser->pos >= 16 ? 16 : parser->buf.size - parser->pos,
    //                 16);

    sc =
      bgav_h264_find_nal_start(parser->buf.buf + parser->buf.pos,
                               parser->buf.len - parser->buf.pos);
    if(!sc)
      {
      parser->buf.pos = parser->buf.len - 5;
      if(parser->buf.pos < 0)
        parser->buf.pos = 0;
      return 0;
      }
    
    parser->buf.pos = sc - parser->buf.buf;
  
    header_len = bgav_h264_decode_nal_header(parser->buf.buf + parser->buf.pos,
                                             parser->buf.len - parser->buf.pos,
                                             &nh);

    if(priv->has_aud)
      {
      if(nh.unit_type == H264_NAL_ACCESS_UNIT_DEL)
        {
        //        fprintf(stderr, "Got frame boundary %d\n", parser->pos);
        *skip = header_len;
        return 1;
        }
      else
        {
        parser->buf.pos += header_len;
        continue;
        }
      }
    new_state = -1;
    switch(nh.unit_type)
      {
      case H264_NAL_NON_IDR_SLICE:
      case H264_NAL_SLICE_PARTITION_A:
      case H264_NAL_IDR_SLICE:
        new_state = STATE_SLICE_HEADER;
        break;
      case H264_NAL_SLICE_PARTITION_B:
      case H264_NAL_SLICE_PARTITION_C:
        new_state = STATE_SLICE_PARTITION;
        break;
      case H264_NAL_SEI:
        new_state = STATE_SEI;
        break;
      case H264_NAL_SPS:
        new_state = STATE_SPS;
        break;
      case H264_NAL_PPS:
        new_state = STATE_SPS;
        break;
      case H264_NAL_ACCESS_UNIT_DEL:
        priv->has_aud = 1;
        *skip = header_len;
        parser->buf.pos = sc - parser->buf.buf;
        return 1;
        break;
      case H264_NAL_END_OF_SEQUENCE:
      case H264_NAL_END_OF_STREAM:
      case H264_NAL_FILLER_DATA:
        break;
      }
    parser->buf.pos = sc - parser->buf.buf;
    
    if(new_state < 0)
      {
      parser->buf.pos += header_len;
      }
    else if((new_state < priv->state) ||
            /* We assume that multiple slices belong to different pictures
               if they are not separated by access unit delimiters.
               Of course this assumption is wrong, but seems to work for most
               streams */
            ((priv->state == STATE_SLICE_HEADER) &&
             (new_state == STATE_SLICE_HEADER)))
      {
      *skip = header_len;
      priv->state = new_state;
      return 1;
      }
    else
      {
      parser->buf.pos += header_len;
      priv->state = new_state;
      }
    }
  return 0;
  }

static void handle_sps(bgav_packet_parser_t * parser)
  {
  h264_priv_t * priv = parser->priv;

  //  fprintf(stderr, "Got SPS:\n");
  //  bgav_h264_sps_dump(&priv->sps);
  
  if(!parser->vfmt->timescale)
    {
    parser->vfmt->timescale = priv->sps.vui.time_scale;
    parser->vfmt->frame_duration = priv->sps.vui.num_units_in_tick * 2;
    }
  
  // bgav_packet_parser_set_framerate(parser);
        
  bgav_h264_sps_get_image_size(&priv->sps,
                               parser->vfmt);
  
  if(!priv->sps.frame_mbs_only_flag)
    parser->ci.flags |= GAVL_COMPRESSION_HAS_FIELD_PICTURES;
  
  if(!priv->sps.vui.bitstream_restriction_flag ||
     priv->sps.vui.num_reorder_frames)
    parser->ci.flags |= GAVL_COMPRESSION_HAS_B_FRAMES;
  else
    parser->ci.flags &= ~GAVL_COMPRESSION_HAS_B_FRAMES;
    
  }

static const uint8_t * get_nal_end(bgav_packet_t * p,
                                   const uint8_t * ptr)
  {
  const uint8_t * ret;
  ret = bgav_h264_find_nal_start(ptr, p->buf.len - (ptr - p->buf.buf));
  
  if(!ret)
    ret = p->buf.buf + p->buf.len;
  return ret;
  }

static int parse_frame_h264(bgav_packet_parser_t * parser, bgav_packet_t * p)
  {
  bgav_h264_nal_header_t nh;
  const uint8_t * nal_end;
  const uint8_t * nal_start;
  const uint8_t * ptr;
  int header_len;
  //  int primary_pic_type;
  bgav_h264_slice_header_t sh;

  /* For extracting the extradata */
  const uint8_t * sps_start = NULL;
  const uint8_t * sps_end = NULL;
  const uint8_t * pps_start = NULL;
  const uint8_t * pps_end = NULL;

  h264_priv_t * priv = parser->priv;

  //  fprintf(stderr, "parse_frame h264\n");
  
  nal_start = p->buf.buf; // Assume that we have a startcode
  
  while(nal_start < p->buf.buf + p->buf.len)
    {
    nal_end = NULL;

    ptr = nal_start;
    
    header_len = bgav_h264_decode_nal_header(ptr, p->buf.len - (ptr - p->buf.buf), &nh);
    
    ptr += header_len;
    
    //    fprintf(stderr, "Got NAL: %d\n", nh.unit_type);
    
    switch(nh.unit_type)
      {
      case H264_NAL_SPS:
        //        fprintf(stderr, "Got SPS\n");
        if(!(priv->flags & FLAG_HAVE_SPS))
          {
          // fprintf(stderr, "Got SPS %d bytes\n", priv->nal_len);
          // gavl_hexdump(parser->buf.buffer + parser->pos,
          //              priv->nal_len, 16);

          nal_end = get_nal_end(p, ptr);
          get_rbsp(parser, ptr, nal_end - ptr);
          
          bgav_h264_sps_parse(&priv->sps,
                              priv->rbsp, priv->rbsp_len);
          //          bgav_h264_sps_dump(&priv->sps);

          handle_sps(parser);
          sps_start = nal_start;
          sps_end = nal_end;
          
          priv->flags |= FLAG_HAVE_SPS;

          if(!parser->vfmt->timescale)
            {
            const gavl_dictionary_t * m;
            int timescale = 0;

            if(!(m = gavl_stream_get_metadata(parser->info)) ||
               !gavl_dictionary_get_int(m, GAVL_META_STREAM_PACKET_TIMESCALE, &timescale))
              {
              gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Stream has no timing info and no PES timescale");
              return 0;
              }
            
            gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Stream has no timing info, using PES timestamps");
            parser->vfmt->timescale = timescale;
            parser->vfmt->framerate_mode = GAVL_FRAMERATE_VARIABLE;

            priv->flags |= FLAG_PES_TIMESTAMPS;

            // parser->s->flags |= (STREAM_NO_DURATIONS | STREAM_PES_TIMESTAMPS);
            }
          
          //          if(parser->s->flags & STREAM_PES_TIMESTAMPS)
          //            parser->flags &= ~PARSER_GEN_PTS;
          }
        break;
      case H264_NAL_PPS:
        //        fprintf(stderr, "Got PPS\n");
        if(!(priv->flags & FLAG_HAVE_PPS))
          {
          nal_end = get_nal_end(p, ptr);
          pps_start = nal_start;
          pps_end = nal_end;
          priv->flags |= FLAG_HAVE_PPS;
          }
        break;
      case H264_NAL_SEI:
        nal_end = get_nal_end(p, ptr);
        get_rbsp(parser, ptr, nal_end - ptr);
        handle_sei_new(parser, p);
        break;
      case H264_NAL_NON_IDR_SLICE:
      case H264_NAL_IDR_SLICE:
      case H264_NAL_SLICE_PARTITION_A:
#if 0
        fprintf(stderr, "Got slice\n");
#endif
        
        if(sps_start && sps_end &&
           pps_start && pps_end)
          {
          if(!parser->ci.codec_header.len)
            {
            gavl_buffer_append_data(&parser->ci.codec_header, sps_start, sps_end - sps_start);
            gavl_buffer_append_data(&parser->ci.codec_header, pps_start, pps_end - pps_start);
            }
          
          }

        
        /* Decode slice header if necessary */
        if((priv->flags & (FLAG_HAVE_SPS|FLAG_HAVE_PPS)) != (FLAG_HAVE_SPS|FLAG_HAVE_PPS))
          {
          PACKET_SET_SKIP(p);
          fprintf(stderr, "Skipping frame before SPS and PPS\n");
          return 1;
          }
#if 0
        if((p->duration > 0) && // PIC timing present
           has_aud)           // Frame type known
          return 1;
#endif        
        /* Here we can be sure that the frame duration is already set */
        p->duration = parser->vfmt->frame_duration;
        
        /* has_picture_start is also set if the sps was found, so we must check for
           coding_type as well */
        //        if(!priv->has_picture_start || !parser->cache[parser->cache_size-1].coding_type)
        //          {
        nal_end = get_nal_end(p, ptr);
        get_rbsp(parser, ptr, nal_end - ptr);
        
        bgav_h264_slice_header_parse(priv->rbsp, priv->rbsp_len,
                                     &priv->sps,
                                     &sh);
        
        //          bgav_h264_slice_header_dump(&priv->sps,
        //                                      &sh);
        
        if(!(p->flags & GAVL_PACKET_TYPE_MASK))
          {
          switch(sh.slice_type)
            {
            case 2:
            case 7:
              p->flags |= BGAV_CODING_TYPE_I;
              break;
            case 0:
            case 5:
              p->flags |= BGAV_CODING_TYPE_P;
              break;
            case 1:
            case 6:
              p->flags |= BGAV_CODING_TYPE_B;
              break;
            default: /* Assume the worst */
              fprintf(stderr, "Unknown slice type %d\n", sh.slice_type);
              break;
            }
          }
        if(sh.field_pic_flag)
          p->flags |= GAVL_PACKET_FIELD_PIC;
        
        goto ok;
        break;
      case H264_NAL_SLICE_PARTITION_B:
      case H264_NAL_SLICE_PARTITION_C:
        goto ok;
        break;
      case H264_NAL_ACCESS_UNIT_DEL:
#if 0 // Too unreliable??
        primary_pic_type = *ptr >> 5;
        fprintf(stderr, "Got access unit delimiter, pic_type: %d\n",
                primary_pic_type);
        switch(primary_pic_type)
          {
          case 0:
            p->flags |= BGAV_CODING_TYPE_I;
            break;
          case 1:
            p->flags |= BGAV_CODING_TYPE_P;
            break;
          default: /* Assume the worst */
            p->flags |= BGAV_CODING_TYPE_B;
            break;
          }
#endif
        //           has_aud = 1;
           break;
      case H264_NAL_END_OF_SEQUENCE:
        break;
      case H264_NAL_END_OF_STREAM:
        break;
      case H264_NAL_FILLER_DATA:
        break;
      default:
        fprintf(stderr, "Unknown NAL unit type %d", nh.unit_type);
        break;
      }
    if(!nal_end)
      nal_end = get_nal_end(p, ptr);
    
    nal_start = nal_end;
    }
  return 0;

  ok:
  
  //  fprintf(stderr, "parse_frame h264 done\n");
  //  gavl_packet_dump(p);
  
  if(priv->flags & FLAG_PES_TIMESTAMPS)
    {
    p->pts = p->pes_pts;
    p->duration = GAVL_TIME_UNDEFINED;
    }
  
  return 1;
  }

static int parse_avc_extradata(bgav_packet_parser_t * parser)
  {
  const uint8_t * ptr;
  bgav_h264_nal_header_t nh;
  int nal_len;
  h264_priv_t * priv = parser->priv;
  
  ptr = parser->ci.codec_header.buf;
  //  end = ptr + parser->s->ci->global_header_len;

  ptr += 4; // Version, profile, profile compat, level
  priv->nal_size_length = (*ptr & 0x3) + 1;
  ptr++;
  
  /* SPS (we parse just the first one) */
  ptr++; // num_units = *ptr & 0x1f; ptr++;

  nal_len = GAVL_PTR_2_16BE(ptr); ptr += 2;

#if 1
  nh.ref_idc   = ptr[0] >> 5;
  nh.unit_type = ptr[0] & 0x1f;
  ptr++;

  if(nh.unit_type != H264_NAL_SPS)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "No SPS found");
    }
  
#endif
  
  get_rbsp(parser, ptr, nal_len - 1);
  
  bgav_h264_sps_parse(&priv->sps,
                      priv->rbsp, priv->rbsp_len);

  priv->flags |= (FLAG_HAVE_SPS|FLAG_HAVE_PPS);

  if(!parser->vfmt->image_width || !parser->vfmt->image_height)
    {
    bgav_h264_sps_get_image_size(&priv->sps,
                                 parser->vfmt);
    }
  
  return 1;
  }

#if 0
void bgav_video_parser_init_h264(bgav_video_parser_t * parser)
  {
  h264_priv_t * priv;
  priv = calloc(1, sizeof(*priv));
  parser->priv = priv;

  parser->cleanup = cleanup_h264;
  parser->reset = reset_h264;
  
  if(parser->s->data.video.format->interlace_mode == GAVL_INTERLACE_UNKNOWN)
    parser->s->data.video.format->interlace_mode = GAVL_INTERLACE_MIXED;

  /* Parse avc1 extradata */
  if(parser->s->fourcc == BGAV_MK_FOURCC('a', 'v', 'c', '1'))
    {
    if(!parser->s->ci->codec_header.len)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "avc1 stream needs extradata");
      return;
      }
    parse_avc_extradata(parser);
    parser->parse_frame = parse_frame_avc;
    }
  else
    parser->parse_frame = parse_frame_h264;

  parser->find_frame_boundary = find_frame_boundary_h264;

  priv->state = STATE_SYNC;
  
  }
#endif

void bgav_packet_parser_init_h264(bgav_packet_parser_t * parser)
  {
  h264_priv_t * priv;
  priv = calloc(1, sizeof(*priv));
  parser->priv = priv;

  parser->cleanup = cleanup_h264;
  parser->reset = reset_h264;
  
  if(parser->vfmt->interlace_mode == GAVL_INTERLACE_UNKNOWN)
    parser->vfmt->interlace_mode = GAVL_INTERLACE_MIXED;
  
  /* Parse avc1 extradata */
  if(parser->fourcc == BGAV_MK_FOURCC('a', 'v', 'c', '1'))
    {
    if(!parser->ci.codec_header.len)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "avc1 stream needs extradata");
      return;
      }
    parse_avc_extradata(parser);
    parser->parse_frame = parse_frame_avc;
    }
  else
    parser->parse_frame = parse_frame_h264;

  parser->find_frame_boundary = find_frame_boundary_h264;

  priv->state = STATE_SYNC;
  }
