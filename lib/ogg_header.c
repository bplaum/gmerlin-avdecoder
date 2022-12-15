

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
  bgav_dprintf("Ogg Page:\n");
  bgav_dprintf("  file position: %"PRId64":\n", p->position);

  bgav_dprintf("  stream_structure_version: %d:\n", p->stream_structure_version);
  bgav_dprintf("  header_type_flags:        %d: (Continued: %d, BOS: %d EOS: %d)\n",
               p->header_type_flags,
               p->header_type_flags & BGAV_OGG_HEADER_TYPE_CONTINUED,
               p->header_type_flags & BGAV_OGG_HEADER_TYPE_BOS,
               p->header_type_flags & BGAV_OGG_HEADER_TYPE_EOS);
  bgav_dprintf("  granulepos:               %"PRId64"\n", p->granulepos);
  bgav_dprintf("  serialno:                 %d\n", p->serialno);

  bgav_dprintf("  sequenceno:               %d\n", p->sequenceno);
  bgav_dprintf("  crc:                      %08x\n", p->crc);
  bgav_dprintf("  num_page_segments:        %d\n", p->num_page_segments);

  num = bgav_ogg_page_num_packets(p);
  
  bgav_dprintf("  (partial) packets:        %d\n", num);
  bgav_dprintf("  Page segments:\n");

  //  for(i = 0; i < p->num_page_segments; i++)
  //    bgav_dprintf("    %d\n", p->page_segments[i]);

  for(i = 0; i < num; i++)
    bgav_dprintf("    %d\n", bgav_ogg_page_get_packet_size(p, i));
  
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
  bgav_dprintf( "OGM Header\n");
  bgav_dprintf( "  Type              %.8s\n", h->type);
  bgav_dprintf( "  Subtype:          ");
  bgav_dump_fourcc(h->subtype);
  bgav_dprintf( "\n");

  bgav_dprintf( "  Size:             %d\n", h->size);
  bgav_dprintf( "  Time unit:        %" PRId64 "\n", h->time_unit);
  bgav_dprintf( "  Samples per unit: %" PRId64 "\n", h->samples_per_unit);
  bgav_dprintf( "  Default len:      %d\n", h->default_len);
  bgav_dprintf( "  Buffer size:      %d\n", h->buffersize);
  bgav_dprintf( "  Bits per sample:  %d\n", h->bits_per_sample);
  if(gavl_string_starts_with(h->type, "video"))
    {
    bgav_dprintf( "  Width:            %d\n", h->data.video.width);
    bgav_dprintf( "  Height:           %d\n", h->data.video.height);
    }
  if(gavl_string_starts_with(h->type, "audio"))
    {
    bgav_dprintf( "  Channels:         %d\n", h->data.audio.channels);
    bgav_dprintf( "  Block align:      %d\n", h->data.audio.blockalign);
    bgav_dprintf( "  Avg bytes per sec: %d\n", h->data.audio.avgbytespersec);
    }
  }
