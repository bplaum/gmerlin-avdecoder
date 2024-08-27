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



#include <stdlib.h>
#include <string.h>

#include <avdec_private.h>
#include <stdio.h>
#include <rmff.h>
#include <asmrp.h>

#define LOG_DOMAIN "rmff"

#define FILE_ID BGAV_MK_FOURCC('.', 'R', 'M', 'F')
#define PROP_ID BGAV_MK_FOURCC('P', 'R', 'O', 'P')
#define MDPR_ID BGAV_MK_FOURCC('M', 'D', 'P', 'R')
#define CONT_ID BGAV_MK_FOURCC('C', 'O', 'N', 'T')
#define DATA_ID BGAV_MK_FOURCC('D', 'A', 'T', 'A')
#define INDX_ID BGAV_MK_FOURCC('I', 'N', 'D', 'X')

static char * read_data(bgav_input_context_t * input, int len)
  {
  char * ret;
  ret = malloc(len+1);
  if(bgav_input_read_data(input, (uint8_t*)ret, len) < len)
    {
    free(ret);
    return NULL;
    }
  ret[len] = '\0';
  return ret;
  }

static void dump_string(const char * str, int len)
  {
  int  i;
  for(i = 0; i < len; i++)
    fputc(str[i], stderr);
  }

/* File header */

int bgav_rmff_file_header_read(bgav_rmff_chunk_t * c,
                               bgav_input_context_t * input,
                               bgav_rmff_file_header_t * ret)
  {
  return bgav_input_read_32_be(input, &ret->file_version) &&
    bgav_input_read_32_be(input, &ret->num_headers);
  }


/* Chunk header */

int bgav_rmff_chunk_header_read(bgav_rmff_chunk_t * c, bgav_input_context_t * input)
  {
  c->start_position = input->position;
  return bgav_input_read_fourcc(input, &c->id) &&
    bgav_input_read_32_be(input, &c->size)&&
    bgav_input_read_16_be(input, &c->version);
  }


/* PROP */

void bgav_rmff_prop_dump(bgav_rmff_prop_t * p)
  {
  gavl_dprintf( "PROP: %p\n", p);
  
  gavl_dprintf( "  max_bit_rate:    %d\n", p->max_bit_rate);
  gavl_dprintf( "  avg_bit_rate:    %d\n", p->avg_bit_rate);
  gavl_dprintf( "  max_packet_size: %d\n", p->max_packet_size);
  gavl_dprintf( "  avg_packet_size: %d\n", p->avg_packet_size);
  gavl_dprintf( "  num_packets:     %d\n", p->num_packets);
  gavl_dprintf( "  duration:        %d\n", p->duration);
  gavl_dprintf( "  preroll:         %d\n", p->preroll);
  gavl_dprintf( "  index_offset:    %d\n", p->index_offset);
  gavl_dprintf( "  data_offset:     %d\n", p->data_offset);
  gavl_dprintf( "  num_streams:     %d\n", p->num_streams);
  gavl_dprintf( "  flags:           %d\n", p->flags);
  }

int bgav_rmff_prop_read(bgav_rmff_chunk_t * c,
                        bgav_input_context_t * input,
                        bgav_rmff_prop_t * ret)
  {
  int result;
  result =
    bgav_input_read_32_be(input, &ret->max_bit_rate) &&
    bgav_input_read_32_be(input, &ret->avg_bit_rate) &&
    bgav_input_read_32_be(input, &ret->max_packet_size) &&
    bgav_input_read_32_be(input, &ret->avg_packet_size) &&
    bgav_input_read_32_be(input, &ret->num_packets) &&
    bgav_input_read_32_be(input, &ret->duration) &&
    bgav_input_read_32_be(input, &ret->preroll) &&
    bgav_input_read_32_be(input, &ret->index_offset) &&
    bgav_input_read_32_be(input, &ret->data_offset) &&
    bgav_input_read_16_be(input, &ret->num_streams) &&
    bgav_input_read_16_be(input, &ret->flags);
  //  dump_prop(ret);
  return result;
  }

/* Logical stream */

int bgav_rmff_logical_stream_read(bgav_input_context_t * input,
                                  bgav_rmff_logical_stream_t * ret)
  {
  int i;
  uint32_t * ptr_32;
  uint16_t * ptr_16;
  
  if(!bgav_input_read_16_be(input, &ret->num_physical_streams))
    return 0;

  ret->physical_stream_numbers = malloc(ret->num_physical_streams *
                                           sizeof(*ret->physical_stream_numbers));
  
  ret->data_offsets = malloc(ret->num_physical_streams *
                             sizeof(*ret->data_offsets));
  
  ptr_16 = ret->physical_stream_numbers;
  for(i = 0; i < ret->num_physical_streams; i++)
    {
    if(!bgav_input_read_16_be(input, ptr_16))
      return 0;
    ptr_16++;
    }
  ptr_32 = ret->data_offsets;
  for(i = 0; i < ret->num_physical_streams; i++)
    {
    if(!bgav_input_read_32_be(input, ptr_32))
      return 0;
    ptr_32++;
    }
  
  if(!bgav_input_read_16_be(input, &ret->num_rules))
    return 0;

  ret->rule_to_physical_stream_number_map =
    malloc(ret->num_rules *
           sizeof(*ret->rule_to_physical_stream_number_map));

  ptr_16 = ret->rule_to_physical_stream_number_map;
  for(i = 0; i < ret->num_rules; i++)
    {
    if(!bgav_input_read_16_be(input, ptr_16))
      return 0;
    ptr_16++;
    }

  if(!bgav_input_read_16_be(input, &ret->num_properties))
    return 0;

  ret->properties = malloc(ret->num_properties * sizeof(*ret->properties));
  
  for(i = 0; i < ret->num_properties; i++)
    {
    bgav_input_skip(input, 6); /* Skip size and object_version */

    if(!bgav_input_read_data(input, &ret->properties[i].name_length, 1))
      return 0;
    ret->properties[i].name =
      calloc(ret->properties[i].name_length + 1, sizeof(*ret->properties[i].name));
    
    if(bgav_input_read_data(input, ret->properties[i].name,
                            ret->properties[i].name_length)
       < ret->properties[i].name_length)
      return 0;

    if(!bgav_input_read_32_be(input, (uint32_t*)(&ret->properties[i].type)))
      return 0;

    if(!bgav_input_read_16_be(input, &ret->properties[i].value_length))
      return 0;
    
    ret->properties[i].value_data = malloc(ret->properties[i].value_length);

    if(bgav_input_read_data(input, ret->properties[i].value_data,
                            ret->properties[i].value_length)
       < ret->properties[i].value_length)
      return 0;
    
    }
  return 1;
  }

void bgav_rmff_logical_stream_dump(bgav_rmff_logical_stream_t * l)
  {
  int i;
  gavl_dprintf( "logical_stream:\n");
  gavl_dprintf( "  physical streams: %d\n", l->num_physical_streams);

  for(i = 0; i < l->num_physical_streams; i++)
    {
    gavl_dprintf( "    stream: %d, stream_number: %d, data_offset: %d\n",
            i, l->physical_stream_numbers[i], l->data_offsets[i]);
    }
  gavl_dprintf( "  num_rules: %d\n", l->num_rules);
  for(i = 0; i < l->num_rules; i++)
    {
    gavl_dprintf( "    rule_to_physical_stream_number_map: %d\n",
            l->rule_to_physical_stream_number_map[i]);
    }

  gavl_dprintf( "  num_properties: %d\n", l->num_properties);
  for(i = 0; i < l->num_properties; i++)
    {
    gavl_dprintf( "  Property %d\n", i);
    gavl_dprintf( "    name:  %s\n", l->properties[i].name);
    gavl_dprintf( "    type:  %d\n", l->properties[i].type);
    gavl_dprintf( "    value, %d bytes\n", l->properties[i].value_length);
    gavl_hexdump(l->properties[i].value_data, l->properties[i].value_length, 16);
    }
  }

#define MY_FREE(p) if(p) free(p);

void bgav_rmff_logical_stream_free(bgav_rmff_logical_stream_t * l)
  {
  int i;
  MY_FREE(l->physical_stream_numbers);
  MY_FREE(l->data_offsets);
  MY_FREE(l->rule_to_physical_stream_number_map);

  for(i = 0; i < l->num_properties; i++)
    {
    MY_FREE(l->properties[i].name);
    MY_FREE(l->properties[i].value_data);
    }
  MY_FREE(l->properties);
  }

#undef MY_FREE

/* MDPR */

void bgav_rmff_mdpr_dump(bgav_rmff_mdpr_t * m)
  {
  gavl_dprintf( "MDPR:\n");
  gavl_dprintf( "  stream_number:    %d\n", m->stream_number);
  gavl_dprintf( "  max_bit_rate:     %d\n", m->max_bit_rate);
  gavl_dprintf( "  avg_bit_rate:     %d\n", m->avg_bit_rate);
  gavl_dprintf( "  max_packet_size:  %d\n", m->max_packet_size);
  gavl_dprintf( "  avg_packet_size:  %d\n", m->avg_packet_size);
  gavl_dprintf( "  start_time:       %d\n", m->start_time);
  gavl_dprintf( "  preroll:          %d\n", m->preroll);
  gavl_dprintf( "  duration:         %d\n", m->duration);
  gavl_dprintf( "  stream_name:      ");
  dump_string(m->stream_name, m->stream_name_size);
  gavl_dprintf( "\n  mime_type:        ");
  dump_string(m->mime_type, m->mime_type_size);
  gavl_dprintf( "\n  type_specific_len:  %d", m->type_specific_len);
  
  if(m->is_logical_stream)
    {
    gavl_dprintf( "\n");
    bgav_rmff_logical_stream_dump(&m->logical_stream);
    }
  else
    {
    gavl_dprintf( "\n  type_specific_data:\n");
    gavl_hexdump(m->type_specific_data, m->type_specific_len, 16);
    }
  }

int bgav_rmff_mdpr_read(bgav_rmff_chunk_t * c,
                        bgav_input_context_t * input,
                        bgav_rmff_mdpr_t * ret)
  {
  bgav_input_context_t * input_mem;
  
  if(!bgav_input_read_16_be(input, &ret->stream_number) ||
     !bgav_input_read_32_be(input, &ret->max_bit_rate) ||
     !bgav_input_read_32_be(input, &ret->avg_bit_rate) ||
     !bgav_input_read_32_be(input, &ret->max_packet_size) ||
     !bgav_input_read_32_be(input, &ret->avg_packet_size) ||
     !bgav_input_read_32_be(input, &ret->start_time) ||
     !bgav_input_read_32_be(input, &ret->preroll) ||
     !bgav_input_read_32_be(input, &ret->duration) ||
     !bgav_input_read_8(input, &ret->stream_name_size))
    return 0;

  ret->stream_name = read_data(input, ret->stream_name_size);
  if(!ret->stream_name)
    return 0;

  if(!bgav_input_read_8(input, &ret->mime_type_size))
    return 0;

  ret->mime_type = read_data(input, ret->mime_type_size);
  if(!ret->mime_type)
    return 0;

  if(!bgav_input_read_32_be(input, &ret->type_specific_len))
    return 0;

  if(!ret->type_specific_len)
    return 1;
  
  ret->type_specific_data = (uint8_t*)read_data(input, ret->type_specific_len);
  if(!ret->type_specific_data)
    return 0;

  /* Check for logical stream */

  if(!strncmp(ret->mime_type, "logical-audio", 13) ||
     !strncmp(ret->mime_type, "logical-video", 13))
    {
    input_mem =
      bgav_input_open_memory(ret->type_specific_data,
                             ret->type_specific_len);

    bgav_input_skip(input_mem, 6); /* Skip size and object_version */

    if(!bgav_rmff_logical_stream_read(input_mem, &ret->logical_stream))
      {
      bgav_input_destroy(input_mem);
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Reading logical stream failed");
      return 0;
      }
    ret->is_logical_stream = 1;
    bgav_input_destroy(input_mem);
    }
  
  return 1;
  }

void bgav_rmff_mdpr_free(bgav_rmff_mdpr_t * m)
  {
  if(m->stream_name)
    free(m->stream_name);
  if(m->mime_type)
    free(m->mime_type);
  if(m->type_specific_data)
    free(m->type_specific_data);
  if(m->is_logical_stream)
    bgav_rmff_logical_stream_free(&m->logical_stream);
  }

/* Content description */

/*
 *  The metadata are ASCII format, so we can read them
 *  here as UTF-8
 */

#define READ_STRING(dst, dst_len)                 \
  if(!bgav_input_read_16_be(input, &dst_len))   \
    return 0;\
  if(dst_len) \
    { \
    ret->dst = malloc(dst_len); \
    if(bgav_input_read_data(input, (uint8_t*)(ret->dst), dst_len) < dst_len) \
      { \
      free(ret->dst); \
      ret->dst = NULL; \
      return 0; \
      } \
    }

int bgav_rmff_cont_read(bgav_rmff_chunk_t * c,
                        bgav_input_context_t * input,
                        bgav_rmff_cont_t * ret)
  {
  READ_STRING(title, ret->title_len);
  READ_STRING(author, ret->author_len);
  READ_STRING(copyright, ret->copyright_len);
  READ_STRING(comment, ret->comment_len);
  return 1;
  }

void bgav_rmff_cont_dump(bgav_rmff_cont_t * cont)
  {
  gavl_dprintf( "CONT:");
  gavl_dprintf( "\n  Title:     ");
  dump_string(cont->title, cont->title_len);
  gavl_dprintf( "\n  Author:    ");
  dump_string(cont->author, cont->author_len);
  gavl_dprintf( "\n  Copyright: ");
  dump_string(cont->copyright, cont->copyright_len);
  gavl_dprintf( "\n  Comment:   ");
  dump_string(cont->comment, cont->comment_len);
  gavl_dprintf( "\n");
  }

#define MY_FREE(ptr) if(ptr)free(ptr)

void bgav_rmff_cont_free(bgav_rmff_cont_t * c)
  {
  MY_FREE(c->title);
  MY_FREE(c->author);
  MY_FREE(c->copyright);
  MY_FREE(c->comment);
  }

/* Index stuff */

int bgav_rmff_index_record_read(bgav_input_context_t * input,
                                bgav_rmff_index_record_t * ret)
  {
  return
    bgav_input_read_16_be(input, &ret->version) &&
    bgav_input_read_32_be(input, &ret->timestamp) &&
    bgav_input_read_32_be(input, &ret->offset) &&
    bgav_input_read_32_be(input, &ret->packet_count_for_this_packet);
  }

int bgav_rmff_indx_read(bgav_input_context_t * input,
                        bgav_rmff_indx_t * ret)
  {
  int i;
  if(!bgav_input_read_32_be(input, &ret->num_indices) ||
     !bgav_input_read_16_be(input, &ret->stream_number) ||
     !bgav_input_read_32_be(input, &ret->next_index_header))
    goto fail;
  if(ret->num_indices)
    {
    ret->records = malloc(ret->num_indices * sizeof(*(ret->records)));
    for(i = 0; i < ret->num_indices; i++)
      {
      if(!bgav_rmff_index_record_read(input, &ret->records[i]))
        goto fail;
      }
    }
  return 1;
  fail:
  if(ret->records)
    free(ret->records);
  memset(ret, 0, sizeof(*ret));
  return 0;
  }

void bgav_rmff_indx_free(bgav_rmff_indx_t * ret)
  {
  if(ret->records)
    free(ret->records);
  memset(ret, 0, sizeof(*ret));
  }

void bgav_rmff_indx_dump(bgav_rmff_indx_t * indx)
  {
  int i;
  gavl_dprintf( "  num_indices:       %d\n",   indx->num_indices);
  gavl_dprintf( "  stream_number:     %d\n", indx->stream_number);
  gavl_dprintf( "  next_index_header: %d\n", indx->next_index_header);
  /*
    uint32_t timestamp;
    uint32_t offset;
    uint32_t packet_count_for_this_packet;
  */

  for(i = 0; i < indx->num_indices; i++)
    {
    gavl_dprintf( "  Time: %d, offset: %d, count: %d\n",
            indx->records[i].timestamp,
            indx->records[i].offset,
            indx->records[i].packet_count_for_this_packet);
    }
  }

/* Data header */

int bgav_rmff_data_header_read(bgav_input_context_t * input,
                               bgav_rmff_data_header_t * data)
  {
  return bgav_input_read_32_be(input, &data->num_packets) &&
    bgav_input_read_32_be(input, &data->next_data_header);
  }


/* Overall file header */

void bgav_rmff_header_dump(bgav_rmff_header_t * header)
  {
  int i;
  gavl_dprintf( "rmff_header:\n");
  bgav_rmff_prop_dump(&header->prop);
  bgav_rmff_cont_dump(&header->cont);

  gavl_dprintf( "Number of streams: %d\n", header->num_streams);
  
  for(i = 0; i < header->num_streams; i++)
    {
    bgav_rmff_mdpr_dump(&header->streams[i].mdpr);
    }
  }

bgav_rmff_header_t * bgav_rmff_header_read(bgav_input_context_t * ctx)
  {
  int i;
  bgav_rmff_indx_t indx;
  bgav_rmff_chunk_t chunk;
  int keep_going = 1;
  uint32_t next_index_header;
  
  bgav_rmff_header_t * ret = calloc(1, sizeof(*ret));

  if(!bgav_rmff_chunk_header_read(&chunk, ctx))
    return 0;

  if(chunk.id != FILE_ID)
    return 0;

  if(!bgav_rmff_file_header_read(&chunk, ctx, &ret->file_header))
    return 0;

  while(keep_going)
    {
    if(!bgav_rmff_chunk_header_read(&chunk, ctx))
      return 0;
    switch(chunk.id)
      {
      case PROP_ID:
        if(!bgav_rmff_prop_read(&chunk,ctx, &ret->prop))
          goto fail;
        break;
      case CONT_ID:
        if(!bgav_rmff_cont_read(&chunk,ctx,&ret->cont))
          goto fail;
        break;
      case MDPR_ID:
        ret->streams = realloc(ret->streams, (ret->num_streams+1) *
                               sizeof(*(ret->streams)));

        memset(ret->streams + ret->num_streams, 0, sizeof(*(ret->streams)));
        
        if(!bgav_rmff_mdpr_read(&chunk,ctx,&ret->streams[ret->num_streams].mdpr))
          goto fail;
        ret->num_streams++;
        break;
      case DATA_ID:
        /* Read data chunk header */
        if(!bgav_rmff_data_header_read(ctx, &ret->data_header))
          goto fail;
        
        /* We reached the data section. Now check if the file is seekabkle
           and read the indices */
        ret->data_start = ctx->position;
        if(chunk.size > 10)
          ret->data_size = chunk.size - 10;
        else if(ret->prop.index_offset)
          ret->data_size = ret->prop.index_offset - ret->data_start;
        else
          ret->data_size = 0;
        if((ctx->flags & BGAV_INPUT_CAN_SEEK_BYTE) && ret->prop.index_offset)
          {
          bgav_input_seek(ctx, ret->prop.index_offset, SEEK_SET);
          
          while(1)
            {
            bgav_rmff_chunk_header_read(&chunk, ctx);
            if(chunk.id != INDX_ID)
              {
              gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                       "No index found, where I expected one");
              break;
              }
            memset(&indx, 0, sizeof(indx));
            if(!bgav_rmff_indx_read(ctx, &indx))
              {
              keep_going = 0;
              break;
              }
            next_index_header = indx.next_index_header;
            
            //            bgav_rmff_indx_dump(&indx);
            
            for(i = 0; i < ret->num_streams; i++)
              {
              if(ret->streams[i].mdpr.stream_number == indx.stream_number)
                {
                memcpy(&ret->streams[i].indx, &indx, sizeof(indx));
                memset(&indx, 0, sizeof(indx));
                ret->streams[i].has_indx = 1;
                break;
                }
              }
            
            bgav_rmff_indx_free(&indx);

            if(next_index_header)
              {
              bgav_input_skip(ctx,
                              next_index_header - ctx->position);
              }
            else
              break;
            }
          bgav_input_seek(ctx, ret->data_start, SEEK_SET);
          }
        keep_going = 0;
        //        bgav_input_skip(ctx->input, chunk.size - 10);
        break;
      case INDX_ID:
        bgav_input_skip(ctx, chunk.size - 10);
        break;
      }
    }
  //  bgav_rmff_header_dump(ret);
  return ret;
  fail:
  bgav_rmff_header_destroy(ret);
  return NULL;
  }

bgav_rmff_header_t * bgav_rmff_header_create(int num_streams)
  {
  bgav_rmff_header_t * ret;
  ret = calloc(1, sizeof(*ret));

  ret->num_streams = num_streams;
  ret->prop.num_streams = num_streams;
  ret->streams = calloc(ret->num_streams, sizeof(*(ret->streams)));
  return ret;
  }

void bgav_rmff_header_destroy(bgav_rmff_header_t * h)
  {
  int i;
  if(h->streams)
    {
    for(i = 0; i < h->num_streams; i++)
      {
      bgav_rmff_indx_free(&h->streams[i].indx);
      bgav_rmff_mdpr_free(&h->streams[i].mdpr);
      }
    free(h->streams);
    }
  bgav_rmff_cont_free(&h->cont);
  free(h);
  }

/* Ripped from mplayer */

/*
 * takes a MLTI-Chunk and a rule number got from match_asm_rule,
 * returns a pointer to selected data and number of bytes in that.
 */

static int select_mlti_data(const uint8_t *mlti_chunk, int mlti_size, int selection, uint8_t **out)
  {
  int numrules, codec, size;
  int i;
  
  /* MLTI chunk should begin with MLTI */

  if ((mlti_chunk[0] != 'M')
      ||(mlti_chunk[1] != 'L')
      ||(mlti_chunk[2] != 'T')
      ||(mlti_chunk[3] != 'I'))
  {
  *out = malloc(mlti_size);
  memcpy(*out, mlti_chunk, mlti_size);
  return mlti_size;
  }

  mlti_chunk+=4;

  /* next 16 bits are the number of rules */
  numrules=GAVL_PTR_2_16BE(mlti_chunk);
  if (selection >= numrules) return 0;

  /* now <numrules> indices of codecs follows */
  /* we skip to selection                     */
  mlti_chunk+=(selection+1)*2;

  /* get our index */
  codec=GAVL_PTR_2_16BE(mlti_chunk);

  /* skip to number of codecs */
  mlti_chunk+=(numrules-selection)*2;

  /* get number of codecs */
  numrules=GAVL_PTR_2_16BE(mlti_chunk);

  if (codec >= numrules) {
    printf("codec index >= number of codecs. %i %i\n", codec, numrules);
    return 0;
  }

  mlti_chunk+=2;
 
  /* now seek to selected codec */
  for (i=0; i<codec; i++) {
    size=GAVL_PTR_2_32BE(mlti_chunk);
    mlti_chunk+=size+4;
  }
  
  size=GAVL_PTR_2_32BE(mlti_chunk);

  *out = malloc(size);
  memcpy(*out, mlti_chunk+4, size);

  return size;
}




#define GET_ATTR_INT(attrs, num_attrs, name, dst) \
  if(bgav_sdp_get_attr_int(attrs, num_attrs, name, &i_tmp)) \
    dst = i_tmp;

#define GET_ATTR_LENGTH(attrs, num_attrs, name, dst) \
  if(bgav_sdp_get_attr_string(attrs, num_attrs, name, &str)) \
    { \
    double t; \
    char * rest; \
    if(!strncmp(str, "npt=", 4)) \
      { \
      t = strtod(str+ 4, &rest); \
      if(rest != str + 4) \
        dst = (int)(t * 1000.0+0.5); \
      } \
    }

#define GET_ATTR_DATA(attrs, num_attrs, name, dst, dst_len)  \
  if(bgav_sdp_get_attr_data(attrs, num_attrs, name, &buffer, &i_tmp) \
     && i_tmp)       \
    { \
    dst_len = i_tmp; \
    dst = malloc(dst_len); \
    memcpy(dst, buffer, dst_len); \
    }
#define GET_ATTR_STRING(attrs, num_attrs, name, dst, dst_len)   \
  if(bgav_sdp_get_attr_string(attrs, num_attrs, name, &str))    \
    {                                                           \
    dst = gavl_strdup(str);                              \
    dst_len = strlen(dst);                                      \
    }

/* Create a real media header from an sdp object */

bgav_rmff_header_t *
bgav_rmff_header_create_from_sdp(const bgav_options_t * opt, bgav_sdp_t * sdp,
                                 char ** stream_rules)
  {
  char * buf;
  char * pos;
  int i, j;
  int i_tmp;
  uint8_t * buffer;
  char * str;
  bgav_rmff_header_t * ret;
  char * asm_rulebook;
  int matches[16];
  int num_matches;

  uint8_t * opaque_data;
  int opaque_data_len;
    
  //  bgav_sdp_dump(sdp);

  /* Set up global stuff */

  ret = bgav_rmff_header_create(sdp->num_media);
  
  GET_ATTR_INT(sdp->attributes, sdp->num_attributes,
               "MaxBitRate", ret->prop.max_bit_rate );
  GET_ATTR_INT(sdp->attributes, sdp->num_attributes,
               "AvgBitRate", ret->prop.avg_bit_rate);
  GET_ATTR_INT(sdp->attributes, sdp->num_attributes,
               "MaxPacketSize", ret->prop.max_packet_size );
  GET_ATTR_INT(sdp->attributes, sdp->num_attributes,
               "AvgPacketSize", ret->prop.avg_packet_size );
  GET_ATTR_INT(sdp->attributes, sdp->num_attributes,
               "Preroll", ret->prop.preroll  );
  GET_ATTR_INT(sdp->attributes, sdp->num_attributes,
               "IndexOffset", ret->prop.index_offset );
  GET_ATTR_INT(sdp->attributes, sdp->num_attributes,
               "DataOffset", ret->prop.data_offset );
  GET_ATTR_INT(sdp->attributes, sdp->num_attributes,
               "Flags", ret->prop.flags );

  GET_ATTR_DATA(sdp->attributes, sdp->num_attributes,
                "Author", ret->cont.author, ret->cont.author_len);
  GET_ATTR_DATA(sdp->attributes, sdp->num_attributes,
                "Title", ret->cont.title, ret->cont.title_len);
  GET_ATTR_DATA(sdp->attributes, sdp->num_attributes,
                "Copyright", ret->cont.copyright, ret->cont.copyright_len);
  GET_ATTR_DATA(sdp->attributes, sdp->num_attributes,
                "Comment", ret->cont.comment, ret->cont.comment_len);
  
  /* Set up streams */

  for(i = 0; i < sdp->num_media; i++)
    {
    GET_ATTR_INT(sdp->media[i].attributes, sdp->media[i].num_attributes,
                 "MaxBitRate", ret->streams[i].mdpr.max_bit_rate );
    GET_ATTR_INT(sdp->media[i].attributes, sdp->media[i].num_attributes,
                 "AvgBitRate", ret->streams[i].mdpr.avg_bit_rate);
    GET_ATTR_INT(sdp->media[i].attributes, sdp->media[i].num_attributes,
                 "MaxPacketSize", ret->streams[i].mdpr.max_packet_size );
    GET_ATTR_INT(sdp->media[i].attributes, sdp->media[i].num_attributes,
                 "AvgPacketSize", ret->streams[i].mdpr.avg_packet_size );
    GET_ATTR_INT(sdp->media[i].attributes, sdp->media[i].num_attributes,
                 "Preroll", ret->streams[i].mdpr.preroll );
    GET_ATTR_LENGTH(sdp->media[i].attributes, sdp->media[i].num_attributes,
                 "length", ret->streams[i].mdpr.duration );
    GET_ATTR_STRING(sdp->media[i].attributes, sdp->media[i].num_attributes,
                    "StreamName", ret->streams[i].mdpr.stream_name,
                    ret->streams[i].mdpr.stream_name_size);
    GET_ATTR_STRING(sdp->media[i].attributes, sdp->media[i].num_attributes,
                    "mimetype", ret->streams[i].mdpr.mime_type,
                    ret->streams[i].mdpr.mime_type_size);

    /* Get stream ID */
    if(!bgav_sdp_get_attr_string(sdp->media[i].attributes, sdp->media[i].num_attributes,
                                 "control", &str))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Control attribute missing");
      goto fail;
      }

    if(!(pos = strstr(str, "streamid=")))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Stream number missing");
      goto fail;
      }
    pos += 9;
    ret->streams[i].mdpr.stream_number = atoi(pos);
    
    if(!bgav_sdp_get_attr_string(sdp->media[i].attributes, sdp->media[i].num_attributes,
                                 "ASMRuleBook", &asm_rulebook))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "No ASMRuleBook found");
      goto fail;
      }

    num_matches = bgav_asmrp_match(asm_rulebook, opt->network_bandwidth, matches);

    if(!num_matches)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Bad ASMRuleBook");
      goto fail;
      }
    for(j = 0; j < num_matches; j++)
      {
      buf = gavl_sprintf("stream=%u;rule=%u,", ret->streams[i].mdpr.stream_number, matches[j]);
      *stream_rules = gavl_strcat(*stream_rules, buf);
      free(buf);
      }

    if(!bgav_sdp_get_attr_data(sdp->media[i].attributes, sdp->media[i].num_attributes,
                               "OpaqueData", &opaque_data, &opaque_data_len))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "No Opaque data there");
      goto fail;
      }

    ret->streams[i].mdpr.type_specific_len =
      select_mlti_data(opaque_data, opaque_data_len, matches[0],
                       &ret->streams[i].mdpr.type_specific_data);

    if(ret->prop.duration < ret->streams[i].mdpr.duration)
      ret->prop.duration = ret->streams[i].mdpr.duration;
    }

    
  //  bgav_rmff_header_dump(ret);
  
  return ret;

  fail:
  bgav_rmff_header_destroy(ret);
  return NULL;
  }

int bgav_rmff_packet_header_read(bgav_input_context_t * input,
                                 bgav_rmff_packet_header_t * ret)
  {
  if(!bgav_input_read_16_be(input, &ret->object_version))
    return 0;

  if((ret->object_version == 0) || (ret->object_version == 1))
    {
    if(!bgav_input_read_16_be(input, &ret->length) ||
       !bgav_input_read_16_be(input, &ret->stream_number) ||
       !bgav_input_read_32_be(input, &ret->timestamp))
      return 0;

    if(ret->object_version == 0)
      {
      if(!bgav_input_read_8(input, &ret->packet_group) ||
         !bgav_input_read_8(input, &ret->flags))
        return 0;
      
      }
    else if(ret->object_version == 1)
      {
      if(!bgav_input_read_16_be(input, &ret->asm_rule) ||
         !bgav_input_read_8(input, &ret->asm_flags))
        return 0;
      }
    else
      return 0;
    }
  else
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Unsupported packet header version: %d",
             ret->object_version);
    return 0;
    }
  return 1;
  }

void bgav_rmff_packet_header_dump(bgav_rmff_packet_header_t * h)
  {
  gavl_dprintf( "Packet V: %d, ", h->object_version);
  gavl_dprintf( "L: %d, ",        h->length);
  gavl_dprintf( "S: %d, ",        h->stream_number);
  gavl_dprintf( "T: %d, ",        h->timestamp);
  if(h->object_version == 0)
    {
    gavl_dprintf( "F: 0x%02x\n", h->flags);
    }
  else if(h->object_version == 1)
    {
    gavl_dprintf( "asm_rule: %d, asm_flags: 0x%02x\n", h->asm_rule, h->asm_flags);
    
    }
  else
    {
    gavl_dprintf("Error: packet header version: %d\n", h->object_version);
    }
  }

void bgav_rmff_packet_header_to_pointer(bgav_rmff_packet_header_t * h,
                                        uint8_t * ptr)
  {
  GAVL_16BE_2_PTR(h->object_version, ptr);ptr+=2;
  GAVL_16BE_2_PTR(h->length, ptr);ptr+=2;
  GAVL_16BE_2_PTR(h->stream_number, ptr);ptr+=2;
  GAVL_32BE_2_PTR(h->timestamp, ptr);ptr+=4;
  *ptr = h->packet_group;ptr++;
  *ptr = h->flags;ptr++;
  }
