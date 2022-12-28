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


#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <avdec_private.h>


#define ID3V2_FRAME_TAG_ALTER_PRESERVATION  (1<<14)
#define ID3V2_FRAME_FILE_ALTER_PRESERVATION (1<<13)
#define ID3V2_FRAME_READ_ONLY               (1<<12)
#define ID3V2_FRAME_GROUPING                (1<<6)
#define ID3V2_FRAME_COMPRESSION             (1<<3)
#define ID3V2_FRAME_ENCRYPTION              (1<<2)
#define ID3V2_FRAME_UNSYNCHRONIZED          (1<<1)
#define ID3V2_FRAME_DATA_LENGTH             (1<<0)

typedef struct
  {
  char * mimetype;
  int picture_type;
  char * description;

  int data_offset; // From frame data start
  int data_size;

  } bgav_id3v2_picture_t;

typedef struct
  {
  struct
    {
    uint64_t start; // Relative to tag start (which is the file start) 
    uint32_t fourcc;
    uint32_t data_size;
    uint32_t header_size;
    uint16_t flags;
    
    } header;

  /* Either data or strings or picture is non-null */

  uint8_t * data;  /* Raw data from the file */
  char ** strings; /* NULL terminated array  */
  
  bgav_id3v2_picture_t * picture;

  } bgav_id3v2_frame_t;

/* Flags for ID3V2 Tag header */

#define ID3V2_TAG_UNSYNCHRONIZED  (1<<7)
#define ID3V2_TAG_EXTENDED_HEADER (1<<6)
#define ID3V2_TAG_EXPERIMENTAL    (1<<5)
#define ID3V2_TAG_FOOTER_PRESENT  (1<<4)

struct bgav_id3v2_tag_s
  {
  struct
    {
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t flags;
    uint32_t data_size;
    uint32_t header_size;
    } header;

  struct
    {
    uint32_t data_size;
    uint32_t num_flags;
    uint8_t * flags;
    } extended_header;
  
  int total_bytes;
 
  int num_frames;
  bgav_id3v2_frame_t * frames;
  const bgav_options_t * opt;
  };

int bgav_id3v2_total_bytes(bgav_id3v2_tag_t* tag)
  {
  return tag->total_bytes;
  }



static bgav_id3v2_frame_t * bgav_id3v2_find_frame(bgav_id3v2_tag_t*t,
                                                  const uint32_t * fourccs);


#define ENCODING_LATIN1    0x00
#define ENCODING_UTF16_BOM 0x01
#define ENCODING_UTF16_BE  0x02
#define ENCODING_UTF8      0x03

static void dump_frame(bgav_id3v2_frame_t * frame)
  {
  int i;
  bgav_dprintf( "Header:\n");
  bgav_dprintf( "  Fourcc:      ");
  bgav_dump_fourcc(frame->header.fourcc);
  bgav_dprintf( "\n");
  
  bgav_dprintf( "  Start:       %"PRId64"\n", frame->header.start);
  bgav_dprintf( "  Data Size:   %d\n", frame->header.data_size);
  bgav_dprintf( "  Header Size: %d\n", frame->header.header_size);

  bgav_dprintf( "  Flags:       ");

  if(frame->header.flags & ID3V2_FRAME_TAG_ALTER_PRESERVATION)
    bgav_dprintf( "ALTER_PRESERVATION ");
  if(frame->header.flags & ID3V2_FRAME_READ_ONLY)
    bgav_dprintf( "READ_ONLY ");
  if(frame->header.flags & ID3V2_FRAME_GROUPING)
    bgav_dprintf( "GOUPING ");
  if(frame->header.flags & ID3V2_FRAME_COMPRESSION)
    bgav_dprintf( "COMPRESSION ");
  if(frame->header.flags & ID3V2_FRAME_ENCRYPTION)
    bgav_dprintf( "ENCRYPTION ");
  if(frame->header.flags & ID3V2_FRAME_UNSYNCHRONIZED)
    bgav_dprintf( "UNSYNCHRONIZED ");
  if(frame->header.flags & ID3V2_FRAME_DATA_LENGTH)
    bgav_dprintf( "DATA_LENGTH");

  bgav_dprintf( "\n");

  if(frame->data)
    {
    bgav_dprintf( "Raw data:\n");
    gavl_hexdump(frame->data, frame->header.data_size, 16);
    }
  if(frame->strings)
    {
    bgav_dprintf( "Strings:\n");
    i = 0;
    while(frame->strings[i])
      {
      bgav_dprintf( "%02x: %s\n", i, frame->strings[i]);
      i++;
      }
    }
  if(frame->picture)
    {
    bgav_dprintf("Picture:\n");
    bgav_dprintf("  Mimetype: %s\n", frame->picture->mimetype);
    bgav_dprintf("  Size:     %d\n", frame->picture->data_size);
    bgav_dprintf("  Offset:   %d\n", frame->picture->data_offset);
    bgav_dprintf("  Type:     ");
    switch(frame->picture->picture_type)
      {
      case 0x00:  
        bgav_dprintf("Other\n");
        break;
      case 0x01:  
        bgav_dprintf("32x32 pixels 'file icon' (PNG only)\n");
        break;
      case 0x02:  
        bgav_dprintf("Other file icon\n");
        break;
      case 0x03:  
        bgav_dprintf("Cover (front)\n");
        break;
      case 0x04:  
        bgav_dprintf("Cover (back)\n");
        break;
      case 0x05:  
        bgav_dprintf("Leaflet page\n");
        break;
      case 0x06:  
        bgav_dprintf("Media (e.g. label side of CD)\n");
        break;
      case 0x07:  
        bgav_dprintf("Lead artist/lead performer/soloist\n");
        break;
      case 0x08:  
        bgav_dprintf("Artist/performer\n");
        break;
      case 0x09:  
        bgav_dprintf("Conductor\n");
        break;
      case 0x0A:  
        bgav_dprintf("Band/Orchestra\n");
        break;
      case 0x0B:  
        bgav_dprintf("Composer\n");
        break;
      case 0x0C:  
        bgav_dprintf("Lyricist/text writer\n");
        break;
      case 0x0D:  
        bgav_dprintf("Recording Location\n");
        break;
      case 0x0E:  
        bgav_dprintf("During recording\n");
        break;
      case 0x0F:  
        bgav_dprintf("During performance\n");
        break;
      case 0x10:  
        bgav_dprintf("Movie/video screen capture\n");
        break;
      case 0x11:  
        bgav_dprintf("A bright coloured fish\n");
        break;
      case 0x12:  
        bgav_dprintf("Illustration\n");
        break;
      case 0x13:  
        bgav_dprintf("Band/artist logotype\n");
        break;
      case 0x14:  
        bgav_dprintf("Publisher/Studio logotype\n");
        break;
      }
    if(frame->picture->description)
      bgav_dprintf("  Description: %s\n", frame->picture->description);
//    gavl_hexdump(frame->picture->data, frame->header.data_size, 16);
    }
  }

static void free_frame(bgav_id3v2_frame_t * frame)
  {
  int j;
  if(frame->strings)
    {
    j = 0;
    
    while(frame->strings[j])
      {
      free(frame->strings[j]);
      j++;
      }
    free(frame->strings);
    }

  if(frame->data)
    free(frame->data);

  if(frame->picture)
    {
    if(frame->picture->mimetype)
      free(frame->picture->mimetype);
    if(frame->picture->description)
      free(frame->picture->description);
    free(frame->picture);
    } 
  }

void bgav_id3v2_dump(bgav_id3v2_tag_t * t)
  {
  int i;
  bgav_dprintf( "============= ID3V2 tag =============\n");
  
  /* Dump header */

  bgav_dprintf( "Header:\n");
  bgav_dprintf( "  Major version: %d\n", t->header.major_version);
  bgav_dprintf( "  Minor version: %d\n", t->header.minor_version);
  bgav_dprintf( "  Flags:         ");
  if(t->header.flags & ID3V2_TAG_UNSYNCHRONIZED)
    bgav_dprintf( "UNSYNCHRONIZED ");
  if(t->header.flags & ID3V2_TAG_EXTENDED_HEADER)
    bgav_dprintf( " EXTENDED_HEADER");
  if(t->header.flags & ID3V2_TAG_EXPERIMENTAL)
    bgav_dprintf( " EXPERIMENTAL");
  if(t->header.flags & ID3V2_TAG_EXPERIMENTAL)
    bgav_dprintf( " FOOTER_PRESENT");
  bgav_dprintf( "\n");
  bgav_dprintf( "  Size: %d\n", t->header.data_size);
  
  for(i = 0; i < t->num_frames; i++)
    {
    bgav_dprintf( "========== Frame %d ==========\n", i+1);
    dump_frame(&t->frames[i]);
    }
  }

static int read_32_syncsave(bgav_input_context_t * input, uint32_t * ret)
  {
  uint8_t data[4];
  if(bgav_input_read_data(input, data, 4) < 4)
    return 0;

  *ret = (uint32_t)data[0] << 24;
  *ret >>= 1;
  *ret |= ((uint32_t)data[1] << 16);
  *ret >>= 1;
  *ret |= ((uint32_t)data[2] << 8);
  *ret >>= 1;
  *ret |= (uint32_t)data[3];
  return 1;
  }

/* Return size of the detected id3v2 tag or 0 */
int bgav_id3v2_detect(const uint8_t * buf)
  {
  int ret  = 0;
  
  if((buf[0] == 'I') &&
     (buf[1] == 'D') &&
     (buf[2] == '3'))
    {
    ret = (uint32_t)buf[6] << 24;
    ret >>= 1;
    ret |= (uint32_t)buf[7] << 16;
    ret >>= 1;
    ret |= (uint32_t)buf[8] << 8;
    ret >>= 1;
    ret |= (uint32_t)buf[9];
    ret += 10;
    }
  return ret;
  }

int bgav_id3v2_probe(bgav_input_context_t * input)
  {
  uint8_t data[BGAV_ID3V2_DETECT_LEN];
  if(bgav_input_get_data(input, data, BGAV_ID3V2_DETECT_LEN) < BGAV_ID3V2_DETECT_LEN)
    return 0;

  return !!bgav_id3v2_detect(data);
  }

static int is_null(const char * ptr, int num_bytes, int len)
  {
  int i;

  if(len < num_bytes)
    num_bytes = len;
  
  for(i = 0; i < num_bytes; i++)
    {
    if(ptr[i] != '\0')
      return 0;
    }
  return 1;
  }

static char * get_charset(uint8_t * data,
                          uint8_t cs, 
                          bgav_charset_converter_t ** cnv,
                          int * bytes_per_char)
  {
  char * pos = NULL;
  switch(cs)
    {
    case ENCODING_LATIN1:
      *bytes_per_char = 1;
      *cnv = bgav_charset_converter_create("LATIN1", BGAV_UTF8);
      pos = (char*)data;
      break;
    case ENCODING_UTF16_BOM:
      *bytes_per_char = 2;

      if((data[0] == 0xFF) && (data[1] == 0xFE))
        *cnv = bgav_charset_converter_create("UTF16LE", BGAV_UTF8);
      else if((data[1] == 0xFF) && (data[0] == 0xFE))
        *cnv = bgav_charset_converter_create("UTF16BE", BGAV_UTF8);
      pos = ((char*)data) + 2;
      break;
    case ENCODING_UTF16_BE:
      *bytes_per_char = 2;
      *cnv = bgav_charset_converter_create("UTF16BE", BGAV_UTF8);
      pos = (char*)data;
      break;
    case ENCODING_UTF8:
      *bytes_per_char = 1;
      pos = (char*)data;
      break;
    }
  return pos;
  }

static char ** read_string_list(uint8_t * data, int data_size)
  {
  int bytes_per_char = 0;
  int i;
  uint8_t encoding;
  char * pos;
  char * end_pos;
  int num_strings;
  char ** ret;
  bgav_charset_converter_t * cnv = NULL;
  char * data_end = (char*)data + data_size;

  encoding = *data;

  if(data_size == 1)
    return NULL;
  
  data++; 
 
  pos = get_charset(data, encoding, &cnv, &bytes_per_char);
  
  end_pos = pos;

  /* Count the strings */

  num_strings = 1;

  // fprintf(stderr, "bytes_per_char: %d pos: %d\n", bytes_per_char, (int)((uint8_t*)pos - data));
  // gavl_hexdump(data, data_size, 16);
               
  for(i = (int)((uint8_t*)pos - data) / bytes_per_char;
      i < data_size; i+= bytes_per_char)
    {
    char * ptr = pos + (i*bytes_per_char);
    
    if(is_null(ptr, bytes_per_char, (int)(data_end - ptr)))
      num_strings++;
    }

  ret = calloc(num_strings+1, sizeof(*ret));

  for(i = 0; i < num_strings; i++)
    {
    end_pos = pos;
    
    while(!is_null(end_pos, bytes_per_char, data_size - (int)((uint8_t*)end_pos - data)))
      {
      end_pos += bytes_per_char;
      if(end_pos - (char * )data >= data_size)
        break;
      }

    if(end_pos > pos)
      {
      if(cnv)
        {
        ret[i] = bgav_convert_string(cnv,
                                     pos, end_pos - pos,
                                     NULL);
        }
      else
        ret[i] = gavl_strndup(pos, end_pos);
      }
    
    pos = end_pos;
    pos += bytes_per_char;
    }
  if(cnv)
    bgav_charset_converter_destroy(cnv);
  return ret;
  }

static bgav_id3v2_picture_t * 
read_picture(uint8_t * data, int data_size)
  {
  int charset;
  int bytes_per_char = 1;
  char * pos;
  char * end_pos;
  uint8_t * data_start = data;
  uint8_t * data_end = data + data_size;
  
  bgav_id3v2_picture_t * ret;
  bgav_charset_converter_t * cnv = NULL;

  ret = calloc(1, sizeof(*ret));

  charset = *data;
  data++;
  
  if(strchr((char*)data, '/'))
    ret->mimetype = gavl_strdup((char*)data);
  else
    ret->mimetype = bgav_sprintf("image/%s", (char*)data);

  data += strlen((char*)data); // Mimetype
  data++;                      // "\0"

  ret->picture_type = *data;
  data++;

  pos = get_charset(data, charset, &cnv, &bytes_per_char);
 
  end_pos = pos;

  while(!is_null(end_pos, bytes_per_char, (int)((char*)data_end - end_pos)))
    {
    end_pos += bytes_per_char;
    if(end_pos - (char * )data_start >= data_size)
      break;
    }

  if(end_pos > pos)
    {
    if(cnv)
      ret->description = bgav_convert_string(cnv,
                                             pos, end_pos - pos,
                                             NULL);
    else
      ret->description = gavl_strndup(pos, end_pos);
    }

  if(cnv)
    bgav_charset_converter_destroy(cnv);

  pos = end_pos + bytes_per_char;

  ret->data_offset = (pos - (char*)data_start);
  ret->data_size = data_size - ret->data_offset;

  return ret;
  }

static int read_frame(bgav_input_context_t * input,
                      bgav_id3v2_frame_t * ret,
                      uint8_t * probe_data,
                      int major_version,
                      int tag_header_size)
  {
  uint8_t buf[4];
  uint8_t * data;

  ret->header.start = input->position - 3; 
 
  if(major_version < 4)
    {
    switch(major_version)
      {
      case 2:
        /* 3 bytes for ID and size */
        ret->header.fourcc =
          (((uint32_t)probe_data[0] << 24) | 
           ((uint32_t)probe_data[1] << 16) |
           ((uint32_t)probe_data[2] << 8));
        if(bgav_input_read_data(input, buf, 3) < 3)
          return 0;
        ret->header.data_size =
          (((uint32_t)buf[0] << 16) | 
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2]));
        break;
      case 3:
        /* 4 bytes for ID and size */
        if(bgav_input_read_data(input, buf, 1) < 1)
          return 0;

        /* 3 char tags in v3 frames are skipped */
        if(!buf[0])
          return 1;
        
        ret->header.fourcc = (((uint32_t)probe_data[0] << 24) | 
                              ((uint32_t)probe_data[1] << 16) |
                              ((uint32_t)probe_data[2] << 8) |
                              ((uint32_t)buf[0]));
        if(!bgav_input_read_32_be(input, &ret->header.data_size) ||
           !bgav_input_read_16_be(input, &ret->header.flags))
          return 0;
        break;       
      }
    }
  else
    {
    if(bgav_input_read_data(input, buf, 1) < 1)
      return 0;
    ret->header.fourcc = (((uint32_t)probe_data[0] << 24) | 
                          ((uint32_t)probe_data[1] << 16) |
                          ((uint32_t)probe_data[2] << 8) |
                          ((uint32_t)buf[0]));
    
    if(!read_32_syncsave(input, &ret->header.data_size) ||
       !bgav_input_read_16_be(input, &ret->header.flags))
      return 0;
    }

  //  fprintf(stderr, "Reading ID3 frame %d ", major_version);
  //  bgav_dump_fourcc(ret->header.fourcc);
  //  fprintf(stderr, "\n");
  

  if(ret->header.data_size > input->total_bytes - input->position)
    return 0;

  ret->header.header_size = input->position - ret->header.start;
  
  data = calloc(ret->header.data_size+2, 1);
  if(bgav_input_read_data(input, data, ret->header.data_size) <
     ret->header.data_size)
    return 0;
  
  if(((ret->header.fourcc & 0xFF000000) ==
     BGAV_MK_FOURCC('T', 0x00, 0x00, 0x00)) &&
     (ret->header.fourcc != BGAV_MK_FOURCC('T', 'X', 'X', 'X')))
    {
    ret->strings = read_string_list(data, ret->header.data_size);
    }
  else if(ret->header.fourcc == BGAV_MK_FOURCC('A', 'P', 'I', 'C'))
    {
    ret->picture = read_picture(data, ret->header.data_size);
    
    }
  else /* Copy raw data */
    {
    ret->data = data;
    data = NULL;    
    }
  
  if(data)
    free(data);

  ret->header.start += tag_header_size;

//  dump_frame(ret);  
  return 1;
  }

#define FRAMES_TO_ALLOC 16

bgav_id3v2_tag_t * bgav_id3v2_read(bgav_input_context_t * input)
  {
  uint8_t probe_data[3];
  int64_t tag_start_pos;
  int frames_alloc = 0;
  bgav_input_context_t * input_mem;
  uint8_t * data;
  int data_size;

  int64_t start = input->position;
      
  bgav_id3v2_tag_t * ret = NULL;
  
  if(bgav_input_read_data(input, probe_data, 3) < 3)
    goto fail;
  
  if((probe_data[0] != 'I') ||
     (probe_data[1] != 'D') ||
     (probe_data[2] != '3'))
    goto fail;
  
  ret = calloc(1, sizeof(*ret));
  ret->opt = &input->opt;
  /* Read header */
  
  if(!bgav_input_read_data(input, &ret->header.major_version, 1) ||
     !bgav_input_read_data(input, &ret->header.minor_version, 1) ||
     !bgav_input_read_data(input, &ret->header.flags, 1) ||
     !read_32_syncsave(input, &ret->header.data_size))
    goto fail;

  tag_start_pos = input->position;
  
  /* Check for extended header */

  if(ret->header.flags & ID3V2_TAG_EXTENDED_HEADER)
    {
    if(!read_32_syncsave(input, &ret->extended_header.data_size))
      goto fail;
    bgav_input_skip(input, ret->extended_header.data_size-4);
    }

  ret->header.header_size = input->position - start;
  
  /* Read frames */

  data_size = tag_start_pos + ret->header.data_size - input->position;
  data = malloc(data_size);
  if(bgav_input_read_data(input, data, data_size) < data_size)
    goto fail;

  input_mem = bgav_input_open_memory(data, data_size);
    
  while(input_mem->position < data_size)
    {
    if(input_mem->position >= data_size - 4)
      break;
    
    if(bgav_input_read_data(input_mem, probe_data, 3) < 3)
      goto fail;

    if(!probe_data[0] && !probe_data[1] && !probe_data[2])
      break;

    else if((probe_data[0] == '3') && 
            (probe_data[1] == 'D') && 
            (probe_data[2] == 'I'))
      {
      bgav_input_skip(input_mem, 7);
      break;
      }
    if(frames_alloc < ret->num_frames + 1)
      {
      frames_alloc += FRAMES_TO_ALLOC;
      ret->frames = realloc(ret->frames, frames_alloc * sizeof(*ret->frames));
      memset(ret->frames + ret->num_frames, 0,
             FRAMES_TO_ALLOC * sizeof(*ret->frames));
      }
    
    if(!read_frame(input_mem,
                   &ret->frames[ret->num_frames],
                   probe_data,
                   ret->header.major_version, ret->header.header_size))
      {
      free_frame(&ret->frames[ret->num_frames]);
      break;
      }
    ret->num_frames++;
    }
  bgav_input_close(input_mem);
  bgav_input_destroy(input_mem);
  free(data);
  
  ret->total_bytes = ret->header.data_size + 10;
  /* Read footer */
  
  if(ret->header.flags & ID3V2_TAG_FOOTER_PRESENT)
    {
    bgav_input_skip(input, 10);
    ret->total_bytes += 10;
    }

  /* Skip padding */
  
  else if(tag_start_pos + ret->header.data_size > input->position)
    {
    bgav_input_skip(input, tag_start_pos + ret->header.data_size - input->position);
    }
  //  bgav_id3v2_dump(ret);
  return ret;

  fail:
  if(ret)
    bgav_id3v2_destroy(ret);
  return NULL;
  }

static bgav_id3v2_frame_t * bgav_id3v2_find_frame(bgav_id3v2_tag_t*t,
                                                  const uint32_t * fourcc)
  {
  int i, j;
  
  for(i = 0; i < t->num_frames; i++)
    {
    j = 0;

    while(fourcc[j])
      {
      
      if(t->frames[i].header.fourcc == fourcc[j])
        {
        return &t->frames[i];
        }
      j++;
      }
    }
  return NULL;
  }

static const uint32_t title_tags[] =
  {
    BGAV_MK_FOURCC('T','I','T','2'),
    BGAV_MK_FOURCC('T','T','2', 0x00),
    0x0
  };

static const uint32_t album_tags[] =
  {
    BGAV_MK_FOURCC('T','A','L','B'),
    BGAV_MK_FOURCC('T','A','L', 0x00),
    0x0
  };


static const uint32_t copyright_tags[] =
  {
    BGAV_MK_FOURCC('T','C','O','P'),
    BGAV_MK_FOURCC('T','C','R', 0x00),
    0x0
  };

static const uint32_t artist_tags[] =
  {
    BGAV_MK_FOURCC('T','P','E','1'),
    BGAV_MK_FOURCC('T','P','1',0x00),
    0x00,
  };

static const uint32_t albumartist_tags[] =
  {
    BGAV_MK_FOURCC('T','P','E','2'),
    BGAV_MK_FOURCC('T','P','2',0x00),
    0x00,
  };

static const uint32_t year_tags[] =
  {
    BGAV_MK_FOURCC('T','Y','E',0x00),
    BGAV_MK_FOURCC('T','Y','E','R'),
    0x00,
  };

static const uint32_t track_tags[] =
  {
    BGAV_MK_FOURCC('T', 'R', 'C', 'K'),
    BGAV_MK_FOURCC('T', 'R', 'K', 0x00),
    0x00,
  };

static const uint32_t genre_tags[] =
  {
    BGAV_MK_FOURCC('T', 'C', 'O', 'N'),
    BGAV_MK_FOURCC('T', 'C', 'O', 0x00),
    0x00,
  };

/* Author == composer */

static const uint32_t author_tags[] =
  {
    BGAV_MK_FOURCC('T', 'C', 'O', 'M'),
    BGAV_MK_FOURCC('T', 'C', 'M', 0x00),
    0x00,
  };


static const uint32_t comment_tags[] =
  {
    BGAV_MK_FOURCC('C', 'O', 'M', 'M'),
    BGAV_MK_FOURCC('C', 'O', 'M', 0x00),
    0x00,
  };

static const uint32_t cover_tags[] =
  {
    BGAV_MK_FOURCC('A', 'P', 'I', 'C'),
    0x00,
  };

static const uint32_t start_pts_tags[] =
  {
    BGAV_MK_FOURCC('P', 'R', 'I', 'V'),
    0x00,
  };

static char * get_comment(const bgav_options_t * opt,
                          bgav_id3v2_frame_t* frame)
  {
  char * ret;
  uint8_t encoding;
  uint8_t * pos;
  int bytes_per_char;
  bgav_charset_converter_t * cnv = NULL;
  
  if(!frame->data)
    return NULL;
  
  encoding = *frame->data;
    
  switch(encoding)
    {
    case ENCODING_LATIN1:
      bytes_per_char = 1;
      cnv = bgav_charset_converter_create("LATIN1", BGAV_UTF8);
      pos = frame->data + 4;
      break;
    case ENCODING_UTF16_BOM:
      bytes_per_char = 2;

      if((frame->data[4] == 0xFF) && (frame->data[5] == 0xFE))
        cnv = bgav_charset_converter_create("UTF16LE", BGAV_UTF8);
      else if((frame->data[5] == 0xFF) && (frame->data[4] == 0xFE))
        cnv = bgav_charset_converter_create("UTF16BE", BGAV_UTF8);
      pos = frame->data + 6;
      break;
    case ENCODING_UTF16_BE:
      bytes_per_char = 2;
      cnv = bgav_charset_converter_create("UTF16BE", BGAV_UTF8);
      pos = frame->data + 4;
      break;
    case ENCODING_UTF8:
      bytes_per_char = 1;
      pos = frame->data + 4;
      break;
    default:
      return NULL;
    }
  
//  pos = frame->data + 4; /* Skip encoding and language */

  /* Skip short description */
  
  while(!is_null((char*)pos, bytes_per_char,
                 frame->header.data_size - (int)(pos - frame->data)))
    pos += bytes_per_char;

  pos += bytes_per_char;

  if(encoding == ENCODING_UTF16_BOM)
    pos += 2; // Skip BOM
    
  if(cnv)
    ret = bgav_convert_string(cnv, 
                              (char*)pos, frame->header.data_size - 
                              (int)(pos - frame->data),
                              NULL);
  else
    ret = gavl_strdup((char*)pos);

  if(cnv)
    bgav_charset_converter_destroy(cnv);
  return ret;
  }

void bgav_id3v2_2_metadata(bgav_id3v2_tag_t * t, gavl_dictionary_t*m)
  {
  int i;
  int i_tmp;
  bgav_id3v2_frame_t * frame;
  
  /* Title */

  frame = bgav_id3v2_find_frame(t, title_tags);
  if(frame && frame->strings)
    gavl_dictionary_set_string(m, GAVL_META_TITLE, frame->strings[0]);

  /* Album */
    
  frame = bgav_id3v2_find_frame(t, album_tags);
  if(frame && frame->strings)
    gavl_dictionary_set_string(m, GAVL_META_ALBUM, frame->strings[0]);

  /* Copyright */
    
  frame = bgav_id3v2_find_frame(t, copyright_tags);
  if(frame && frame->strings)
    gavl_dictionary_set_string(m, GAVL_META_COPYRIGHT, frame->strings[0]);

  /* Artist */

  frame = bgav_id3v2_find_frame(t, artist_tags);
  if(frame && frame->strings)
    {
    i = 0;
    while(frame->strings[i])
      {
      gavl_dictionary_append_string_array(m,GAVL_META_ARTIST, frame->strings[i]);
      i++;
      }
    }
  /* Albumartist */

  frame = bgav_id3v2_find_frame(t, albumartist_tags);
  if(frame && frame->strings)
    {
    i = 0;
    while(frame->strings[i])
      {
      gavl_dictionary_append_string_array(m,GAVL_META_ALBUMARTIST, frame->strings[i]);
      i++;
      }
    }
  
  /* Author */

  frame = bgav_id3v2_find_frame(t, author_tags);
  if(frame && frame->strings)
    {
    i = 0;
    while(frame->strings[i])
      {
      gavl_dictionary_append_string_array(m,GAVL_META_AUTHOR, frame->strings[i]);
      i++;
      }
    }
  
  /* Year */
  
  frame = bgav_id3v2_find_frame(t, year_tags);
  if(frame && frame->strings)
    gavl_dictionary_set_string(m, GAVL_META_YEAR, frame->strings[0]);

  /* Track */

  frame = bgav_id3v2_find_frame(t, track_tags);
  if(frame && frame->strings)
    gavl_dictionary_set_int(m, GAVL_META_TRACKNUMBER, atoi(frame->strings[0]));
  
  /* Genre */
  
  frame = bgav_id3v2_find_frame(t, genre_tags);
  if(frame && frame->strings)
    {
    if((frame->strings[0][0] == '(') && isdigit(frame->strings[0][1]))
      {
      i_tmp = atoi(&frame->strings[0][1]);
      gavl_dictionary_set_string(m, GAVL_META_GENRE, bgav_id3v1_get_genre(i_tmp));
      }
    else
      {
      i = 0;
      while(frame->strings[i])
        {
        gavl_dictionary_append_string_array(m,GAVL_META_GENRE, frame->strings[i]);
        i++;
        }
      }
    }

  /* Comment */
  
  frame = bgav_id3v2_find_frame(t, comment_tags);

  if(frame)
    gavl_dictionary_set_string_nocopy(m, GAVL_META_COMMENT, get_comment(t->opt, frame));

  /* Cover */
  if((frame = bgav_id3v2_find_frame(t, cover_tags)) &&
     frame->picture && (frame->picture->picture_type == 3))
    {
    gavl_metadata_add_image_embedded(m, GAVL_META_COVER_EMBEDDED, 
                                     -1, -1, frame->picture->mimetype,
                                     frame->picture->data_offset + frame->header.header_size + frame->header.start,
                                     frame->picture->data_size);
    }

  /* Start PTS */
  if((frame = bgav_id3v2_find_frame(t, start_pts_tags)))
    {
    
    // fprintf(stderr, "Got PRIV tag:\n");
    // gavl_hexdump(frame->data, frame->header.data_size, 16);

    if((frame->header.data_size == 53) &&
       !memcmp("com.apple.streaming.transportStreamTimestamp", frame->data, 45))
      {
      int64_t pts;
      uint8_t * ptr = frame->data+45;
      pts = GAVL_PTR_2_64BE(ptr);

      gavl_dictionary_set_int(m, META_START_PTS_DEN, 90000);
      gavl_dictionary_set_long(m, META_START_PTS_NUM, pts);
      }
    }
  }

int64_t bgav_id3v2_get_pts(bgav_id3v2_tag_t * t)
  {
  bgav_id3v2_frame_t * frame;

  /* Start PTS */
  if((frame = bgav_id3v2_find_frame(t, start_pts_tags)))
    {
    
    // fprintf(stderr, "Got PRIV tag:\n");
    // gavl_hexdump(frame->data, frame->header.data_size, 16);

    if((frame->header.data_size == 53) &&
       !memcmp("com.apple.streaming.transportStreamTimestamp", frame->data, 45))
      {
      int64_t pts;
      uint8_t * ptr = frame->data+45;
      pts = GAVL_PTR_2_64BE(ptr);
      return pts;
      }
    }
  return GAVL_TIME_UNDEFINED;
  }

void bgav_id3v2_destroy(bgav_id3v2_tag_t * t)
  {
  int i;
  for(i = 0; i < t->num_frames; i++)
    {
    free_frame(&t->frames[i]);
    }
  free(t->frames);
  free(t);
  }

