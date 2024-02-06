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
#include <mxf.h>

/* Based on the ffmpeg MXF demuxer with lots of changes */

#define LOG_DOMAIN "mxf"

// #define DUMP_UNKNOWN
// #define DUMP_ESSENCE

#define FREE(ptr) if(ptr) free(ptr);

/* Debug functions */

static void do_indent(int i)
  {
  while(i--)
    gavl_dprintf(" ");
  }

static void dump_ul(const uint8_t * u)
  {
  gavl_dprintf("0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
               u[0], u[1], u[2], u[3], 
               u[4], u[5], u[6], u[7], 
               u[8], u[9], u[10], u[11], 
               u[12], u[13], u[14], u[15]);               
  }

static void dump_ul_nb(const uint8_t * u)
  {
  gavl_dprintf("0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x",
               u[0], u[1], u[2], u[3], 
               u[4], u[5], u[6], u[7], 
               u[8], u[9], u[10], u[11], 
               u[12], u[13], u[14], u[15]);               
  }

static void dump_ul_ptr(const uint8_t * u, void * ptr)
  {
  gavl_dprintf("0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x (%p)\n",
               u[0], u[1], u[2], u[3], 
               u[4], u[5], u[6], u[7], 
               u[8], u[9], u[10], u[11], 
               u[12], u[13], u[14], u[15], ptr);            
  }

static void dump_date(uint64_t d)
  {
  gavl_dprintf("%04d-%02d-%02d %02d:%02d:%02d:%03d",
               (int)(d>>48),
               (int)(d>>40 & 0xff),
               (int)(d>>32 & 0xff),
               (int)(d>>24 & 0xff),
               (int)(d>>16 & 0xff),
               (int)(d>>8 & 0xff),
               (int)(d & 0xff));
  
  }

/* Read list of ULs */

static mxf_ul_t * read_refs(bgav_input_context_t * input, uint32_t * num)
  {
  mxf_ul_t * ret;
  if(!bgav_input_read_32_be(input, num))
    return NULL;
  /* Skip size */
  bgav_input_skip(input, 4);
  if(!num)
    return NULL;
  ret = malloc(sizeof(*ret) * *num);
  if(bgav_input_read_data(input, (uint8_t*)ret, sizeof(*ret) * *num) < sizeof(*ret) * *num)
    {
    free(ret);
    return NULL;
    }
  return ret;
  }

/* Resolve references */

static mxf_metadata_t *
resolve_strong_ref(partition_t * ret, mxf_ul_t u, mxf_metadata_type_t type)
  {
  int i;
  for(i = 0; i < ret->num_metadata; i++)
    {
    if(!memcmp(u, ret->metadata[i]->uid, 16) &&
       (type & ret->metadata[i]->type))
      return ret->metadata[i];
    }
  return NULL;
  }

static mxf_metadata_t *
package_by_ul(partition_t * ret, mxf_ul_t u)
  {
  int i;
  mxf_package_t * mp;
  mxf_preface_t * p = (mxf_preface_t*)ret->preface;
  
  for(i = 0; i < ((mxf_content_storage_t*)(p->content_storage))->num_package_refs; i++)
    {
    mp = (mxf_package_t*)(((mxf_content_storage_t*)(p->content_storage))->packages[i]);
    
    if((mp->common.type == MXF_TYPE_SOURCE_PACKAGE) && !memcmp(u, mp->package_ul, 16))
      return (mxf_metadata_t*)mp;
    }
  return NULL;
  }

static mxf_metadata_t **
resolve_strong_refs(partition_t * p, mxf_ul_t * u, int num, mxf_metadata_type_t type)
  {
  int i;
  mxf_metadata_t ** ret;

  if(!num)
    return NULL;

  ret = calloc(num, sizeof(*ret));
  
  for(i = 0; i < num; i++)
    ret[i] = resolve_strong_ref(p, u[i], type);
  return ret;
  }

/* Operational pattern ULs (taken from ingex) */

#define MXF_OP_L(def, name) \
    g_##name##_op_##def##_label

#define MXF_OP_L_LABEL(regver, complexity, byte14, qualifier) \
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, regver, 0x0d, 0x01, 0x02, 0x01, complexity, byte14, qualifier, 0x00}
    
    
/* OP Atom labels */
 
#define MXF_ATOM_OP_L(byte14) \
    MXF_OP_L_LABEL(0x02, 0x10, byte14, 0x00)

#if 0
static const mxf_ul_t MXF_OP_L(atom, complexity00) = 
    MXF_ATOM_OP_L(0x00);
    
static const mxf_ul_t MXF_OP_L(atom, complexity01) = 
    MXF_ATOM_OP_L(0x01);
    
static const mxf_ul_t MXF_OP_L(atom, complexity02) = 
    MXF_ATOM_OP_L(0x02);
    
static const mxf_ul_t MXF_OP_L(atom, complexity03) = 
    MXF_ATOM_OP_L(0x03);
#endif

//static const mxf_ul_t g_opAtomPrefix = MXF_ATOM_OP_L(0);

static const mxf_ul_t op_atom =
  { 0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0xff, 0x0d, 0x01, 0x02, 0x01, 0x10, 0xff, 0x00, 0x00 };

static int is_op_atom(const mxf_ul_t label)
  {
  int i;
  for(i = 0; i < 16; i++)
    {
    if((op_atom[i] != 0xff) && (op_atom[i] != label[i]))
      return 0;
    }
      
  //  return memcmp(&g_opAtomPrefix, label, 13) == 0;
  return 1;
  }

/* OP-1A labels */
 
#define MXF_1A_OP_L(qualifier) \
    MXF_OP_L_LABEL(0x01, 0x01, 0x01, qualifier)

#if 0
/* internal essence, stream file, multi-track */
static const mxf_ul_t MXF_OP_L(1a, qq09) = 
    MXF_1A_OP_L(0x09);
#endif

// static const mxf_ul_t g_op1APrefix = MXF_1A_OP_L(0);   

// #define MXF_OP_L_LABEL(regver, complexity, byte14, qualifier)
//{ 0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, regver, 0x0d, 0x01, 0x02, 0x01, complexity, byte14, qualifier, 0x00}

static const mxf_ul_t op_1a =
  { 0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0xff,   0x0d, 0x01, 0x02, 0x01,       0x01,   0x01,      0xff, 0x00};

static int is_op_1a(const mxf_ul_t label)
  {
  int i;
  for(i = 0; i < 16; i++)
    {
    if((op_1a[i] != 0xff) && (op_1a[i] != label[i]))
      return 0;
    }
  return 1;
  //  return memcmp(&g_op1APrefix, label, 13) == 0;
  }

/* Partial ULs */

static const uint8_t mxf_klv_key[] = { 0x06,0x0e,0x2b,0x34 };

static const uint8_t mxf_essence_element_key[] =
  { 0x06,0x0e,0x2b,0x34,0x01,0x02,0x01,0x01,0x0d,0x01,0x03,0x01 };

/* Complete ULs */

static const uint8_t mxf_header_partition_pack_key[] =
  { 0x06,0x0e,0x2b,0x34,0x02,0x05,0x01,0x01,0x0d,0x01,0x02,0x01,0x01,0x02 };

static const uint8_t mxf_filler_key[] =
  {0x06,0x0e,0x2b,0x34,0x01,0x01,0x01,0x01,0x03,0x01,0x02,0x10,0x01,0x00,0x00,0x00};

static const uint8_t mxf_primer_pack_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x05,0x01,0x01,0x0d,0x01,0x02,0x01,0x01,0x05,0x01,0x00 };

static const uint8_t mxf_content_storage_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x18,0x00 };

static const uint8_t mxf_source_package_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x37,0x00 };

static const uint8_t mxf_material_package_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x36,0x00 };

static const uint8_t mxf_sequence_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x0F,0x00 };

static const uint8_t mxf_source_clip_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x11,0x00 };

static const uint8_t mxf_static_track_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x3B,0x00 };

static const uint8_t mxf_generic_track_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x04,0x01,0x02,0x02,0x00,0x00 };

static const uint8_t mxf_index_table_segment_key[] =
  { 0x06,0x0e,0x2b,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x02,0x01,0x01,0x10,0x01,0x00 };

static const uint8_t mxf_timecode_component_key[] =
  { 0x06,0x0e,0x2b,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x14,0x00 };

static const uint8_t mxf_identification_key[] =
  { 0x06,0x0e,0x2b,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x30,0x00 };

static const uint8_t mxf_essence_container_data_key[] =
{ 0x06, 0x0e, 0x2b, 0x34, 0x02, 0x53, 0x01, 0x01, 0x0d, 0x01, 0x01, 0x01, 0x01, 0x01, 0x23, 0x00 };

static const uint8_t mxf_preface_key[] =
  { 0x06, 0x0e, 0x2b, 0x34, 0x02, 0x53, 0x01, 0x01, 0x0d, 0x01, 0x01, 0x01, 0x01, 0x01, 0x2f, 0x00 };

static const uint8_t mxf_closed_body_partition_key[] =
  {  0x06, 0x0e, 0x2b, 0x34, 0x02, 0x05, 0x01, 0x01, 0x0d, 0x01, 0x02, 0x01, 0x01, 0x03, 0x04, 0x00 };

static const uint8_t mxf_open_body_partition_key[] =
  {  0x06, 0x0E, 0x2B, 0x34, 0x02, 0x05, 0x01, 0x01, 0x0D, 0x01, 0x02, 0x01, 0x01, 0x03, 0x03, 0x00 };

/* Descriptors */

static const uint8_t mxf_descriptor_multiple_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x44,0x00 };

static const uint8_t mxf_descriptor_generic_sound_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x42,0x00 };

static const uint8_t mxf_descriptor_cdci_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x28,0x00 };

static const uint8_t mxf_descriptor_rgba_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x29,0x00 };

static const uint8_t mxf_descriptor_mpeg2video_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x51,0x00 };

static const uint8_t mxf_descriptor_wave_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x48,0x00 };

static const uint8_t mxf_descriptor_aes3_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x47,0x00 };

/* MPEG-4 extradata */
static const uint8_t mxf_sony_mpeg4_extradata[] =
  { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0e,0x06,0x06,0x02,0x02,0x01,0x00,0x00 };

#define UL_MATCH(d, key) !memcmp(d, key, sizeof(key))

#define UL_MATCH_MOD_REGVER(d, key) (!memcmp(d, key, 7) && !memcmp(&d[8], &key[8], 8))

static int match_ul(const mxf_ul_t u1, const mxf_ul_t u2, int len)
  {
  int i;
  for (i = 0; i < len; i++)
    {
    if (i != 7 && u1[i] != u2[i])
      return 0;
    }
  return 1;
  }

#if 0
/* Operational patterns */

typedef struct
  {
  mxf_ul_t ul;
  mxf_op_t op;
  } op_map_t;

op_map_t op_map[] =
  {
    {
      { 0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x02, 0x0d, 0x01, 0x02, 0x01, 0x10, 0x02, 0x00, 0x00 },
      MXF_OP_ATOM
    },
    {
      { 0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01, 0x0e, 0x06, 0x02, 0x01, 0x40, 0x01, 0x09, 0x00 },
      MXF_OP_1a
    },
  };

static mxf_op_t get_op(mxf_ul_t ul)
  {
  int i;
  for(i = 0; i < sizeof(op_map)/sizeof(op_map[0]); i++)
    {
    if(!memcmp(ul, op_map[i].ul, 13))
      return op_map[i].op;
    }
  return MXF_OP_UNKNOWN;
  }

#endif

static const struct
  {
  mxf_op_t op;
  const char * name;
  }
op_names[] =
  {
    { MXF_OP_UNKNOWN, "Unknown" },
    { MXF_OP_1a,      "1a"      },
    { MXF_OP_1b,      "1b"      },
    { MXF_OP_1c,      "1c"      },
    { MXF_OP_2a,      "2a"      },
    { MXF_OP_2b,      "2b"      },
    { MXF_OP_2c,      "2c"      },
    { MXF_OP_3a,      "3a"      },
    { MXF_OP_3b,      "3b"      },
    { MXF_OP_3c,      "3c"      },
    { MXF_OP_ATOM,    "Atom"    },
  };

static const char * get_op_name(mxf_op_t op)
  {
  int i;
  for(i = 0; i < sizeof(op_names)/sizeof(op_names[0]); i++)
    {
    if(op == op_names[i].op)
      return op_names[i].name;
    }
  return op_names[0].name;
  }

/* Stream types */

typedef struct
  {
  mxf_ul_t ul;
  gavl_stream_type_t type;
  } stream_entry_t;

/* SMPTE RP224 http://www.smpte-ra.org/mdd/index.html */
static const stream_entry_t mxf_data_definition_uls[] = {
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x01,0x03,0x02,0x02,0x01,0x00,0x00,0x00 }, GAVL_STREAM_VIDEO },
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x01,0x03,0x02,0x02,0x02,0x00,0x00,0x00 }, GAVL_STREAM_AUDIO },
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x05,0x01,0x03,0x02,0x02,0x02,0x02,0x00,0x00 }, GAVL_STREAM_AUDIO },
  { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, GAVL_STREAM_NONE /* CODEC_TYPE_DATA */},
};

static const stream_entry_t * match_stream(const stream_entry_t * se, const mxf_ul_t u1)
  {
  int i = 0;
  
  while(se[i].type != GAVL_STREAM_NONE)
    {
    if(match_ul(se[i].ul, u1, 16))
      return &se[i];
    i++;
    }
  return NULL;
  }

typedef struct
  {
  mxf_ul_t ul;
  uint32_t fourcc;
  mxf_wrapping_type_t wrapping_type;
  int compression_flags; // only GAVL_COMPRESSION_HAS_P_FRAMES
  } codec_entry_t;

static const codec_entry_t mxf_video_codec_uls[] =
  {
    /* PictureEssenceCoding */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x01,0x11,0x00 }, BGAV_MK_FOURCC('m', 'p', 'g', 'v'), WRAP_UNKNOWN, GAVL_COMPRESSION_HAS_P_FRAMES }, /* MP@ML Long GoP */

    /* SMPTE D-10 (MPEG-2) */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x01,0x02,0x01,0x01 }, BGAV_MK_FOURCC('m', 'x', '5', 'p'), WRAP_FRAME }, /* D-10 50Mbps PAL */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x01,0x02,0x01,0x02 }, BGAV_MK_FOURCC('m', 'x', '5', 'n'), WRAP_FRAME }, /* D-10 50Mbps NTSC */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x01,0x02,0x01,0x03 }, BGAV_MK_FOURCC('m', 'x', '4', 'p'), WRAP_FRAME }, /* D-10 40Mbps PAL */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x01,0x02,0x01,0x04 }, BGAV_MK_FOURCC('m', 'x', '4', 'n'), WRAP_FRAME }, /* D-10 40Mbps NTSC */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x01,0x02,0x01,0x05 }, BGAV_MK_FOURCC('m', 'x', '3', 'p'), WRAP_FRAME }, /* D-10 30Mbps PAL */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x01,0x02,0x01,0x06 }, BGAV_MK_FOURCC('m', 'x', '3', 'n'), WRAP_FRAME }, /* D-10 30Mbps NTSC */

    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x03,0x03,0x00 }, BGAV_MK_FOURCC('m', 'p', 'g', 'v'), WRAP_UNKNOWN, GAVL_COMPRESSION_HAS_P_FRAMES }, /* MP@HL Long GoP */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x04,0x02,0x00 }, BGAV_MK_FOURCC('m', 'p', 'v', '2') }, /* 422P@HL I-Frame */

    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x20,0x02,0x01 }, BGAV_MK_FOURCC('m', 'p', '4', 'v'), WRAP_FRAME }, /* XDCAM proxy_pal030926.mxf */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x20,0x02,0x02 }, BGAV_MK_FOURCC('m', 'p', '4', 'v'), WRAP_FRAME }, /* XDCAM proxy_pal030926.mxf */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x20,0x02,0x03 }, BGAV_MK_FOURCC('m', 'p', '4', 'v'), WRAP_FRAME }, /* XDCAM proxy_pal030926.mxf */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x20,0x02,0x04 }, BGAV_MK_FOURCC('m', 'p', '4', 'v'), WRAP_FRAME }, /* XDCAM proxy_pal030926.mxf */

    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x20,0x10,0x01 }, BGAV_MK_FOURCC('m', 'p', '4', 'v'), WRAP_FRAME }, /* XDCAM proxy_pal030926.mxf */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x20,0x10,0x02 }, BGAV_MK_FOURCC('m', 'p', '4', 'v'), WRAP_FRAME }, /* XDCAM proxy_pal030926.mxf */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x20,0x10,0x03 }, BGAV_MK_FOURCC('m', 'p', '4', 'v'), WRAP_FRAME }, /* XDCAM proxy_pal030926.mxf */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x20,0x10,0x04 }, BGAV_MK_FOURCC('m', 'p', '4', 'v'), WRAP_FRAME }, /* XDCAM proxy_pal030926.mxf */

    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x20,0x11,0x01 }, BGAV_MK_FOURCC('m', 'p', '4', 'v'), WRAP_FRAME }, /* XDCAM proxy_pal030926.mxf */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x20,0x11,0x02 }, BGAV_MK_FOURCC('m', 'p', '4', 'v'), WRAP_FRAME }, /* XDCAM proxy_pal030926.mxf */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x20,0x11,0x03 }, BGAV_MK_FOURCC('m', 'p', '4', 'v'), WRAP_FRAME }, /* XDCAM proxy_pal030926.mxf */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x20,0x11,0x04 }, BGAV_MK_FOURCC('m', 'p', '4', 'v'), WRAP_FRAME }, /* XDCAM proxy_pal030926.mxf */

    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x02,0x01,0x02,0x00 }, BGAV_MK_FOURCC('D', 'V', ' ', ' ') }, /* DV25 IEC PAL */
    // { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x07,0x04,0x01,0x02,0x02,0x03,0x01,0x01,0x00 }, 14, CODEC_ID_JPEG2000 }, /* JPEG2000 Codestream */
    // { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x01,0x7F,0x00,0x00,0x00 }, 13, CODEC_ID_RAWVIDEO }, /* Uncompressed */

    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0d,0x04,0x01,0x02,0x02,0x03,0x06,0x01,0x00 }, BGAV_MK_FOURCC('a', 'p', 'c', 'o'), WRAP_FRAME }, // FF_PROFILE_PRORES_PROXY
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0d,0x04,0x01,0x02,0x02,0x03,0x06,0x02,0x00 }, BGAV_MK_FOURCC('a', 'p', 'c', 's'), WRAP_FRAME }, // FF_PROFILE_PRORES_LT
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0d,0x04,0x01,0x02,0x02,0x03,0x06,0x03,0x00 }, BGAV_MK_FOURCC('a', 'p', 'c', 'n'), WRAP_FRAME }, // FF_PROFILE_PRORES_STANDARD
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0d,0x04,0x01,0x02,0x02,0x03,0x06,0x04,0x00 }, BGAV_MK_FOURCC('a', 'p', 'c', 'h'), WRAP_FRAME }, // FF_PROFILE_PRORES_HQ
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0d,0x04,0x01,0x02,0x02,0x03,0x06,0x05,0x00 }, BGAV_MK_FOURCC('a', 'p', '4', 'h'), WRAP_FRAME }, // FF_PROFILE_PRORES_4444
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x0d,0x04,0x01,0x02,0x02,0x03,0x06,0x06,0x00 }, BGAV_MK_FOURCC('a', 'p', '4', 'x'), WRAP_FRAME }, // FF_PROFILE_PRORES_XQ
    {  },
  };

static const codec_entry_t mxf_audio_codec_uls[] = {
  /* SoundEssenceCompression */
  /* Uncompressed */
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x01,0x00,0x00,0x00,0x00 }, BGAV_MK_FOURCC('s', 'o', 'w', 't') },
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x01,0x7F,0x00,0x00,0x00 }, BGAV_MK_FOURCC('s', 'o', 'w', 't'), WRAP_FRAME },
  /* From Omneon MXF file */
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x07,0x04,0x02,0x02,0x01,0x7E,0x00,0x00,0x00 }, BGAV_MK_FOURCC('t', 'w', 'o', 's'), WRAP_FRAME },
  /* XDCAM Proxy C0023S01.mxf */
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x04,0x04,0x02,0x02,0x02,0x03,0x01,0x01,0x00 }, BGAV_MK_FOURCC('a', 'l', 'a', 'w'), WRAP_FRAME }, 
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x02,0x03,0x02,0x01,0x00 }, BGAV_MK_FOURCC('.', 'a', 'c', '3') },
  /* MP2 or MP3 */
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x02,0x03,0x02,0x05,0x00 }, BGAV_MK_FOURCC('.', 'm', 'p', '3') },
  //{ { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x02,0x03,0x02,0x1C,0x00 },
  // 15, CODEC_ID_DOLBY_E }, /* Dolby-E */
  {  },
};

static const codec_entry_t  mxf_picture_essence_container_uls[] = {
  /* MPEG-ES Frame wrapped */
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x02,0x0D,0x01,0x03,0x01,0x02,0x04,0x60,0x01 }, BGAV_MK_FOURCC('m','p','g','v'), WRAP_FRAME, GAVL_COMPRESSION_HAS_P_FRAMES },

  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x61,0x01 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_FRAME },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x61,0x02 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_CLIP },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x60,0x01 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_FRAME },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x60,0x02 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_CLIP },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x63,0x01 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_FRAME },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x63,0x02 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_CLIP },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x62,0x01 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_FRAME },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x62,0x02 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_CLIP },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x40,0x01 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_FRAME },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x40,0x02 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_CLIP },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x41,0x01 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_FRAME },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x41,0x02 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_CLIP },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x50,0x01 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_FRAME },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x50,0x02 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_CLIP },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x51,0x01 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_FRAME },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x51,0x02 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_CLIP },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x01,0x01 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_FRAME },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x01,0x02 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_CLIP },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x02,0x01 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_FRAME },
  { { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0d,0x01,0x03,0x01,0x02,0x02,0x02,0x02 }, BGAV_MK_FOURCC('D','V',' ',' '), WRAP_CLIP },
  { },
};

static const codec_entry_t  mxf_sound_essence_container_uls[] = {
  /* BWF Frame wrapped */
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x0D,0x01,0x03,0x01,0x02,0x06,0x01,0x00 }, BGAV_MK_FOURCC('s', 'o', 'w', 't'), WRAP_FRAME },
  /* BWF Clip wrapped */
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x0D,0x01,0x03,0x01,0x02,0x06,0x02,0x00 }, BGAV_MK_FOURCC('s', 'o', 'w', 't'), WRAP_CLIP },
  /* AES3 Frame wrapped */
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x0D,0x01,0x03,0x01,0x02,0x06,0x03,0x00 }, BGAV_MK_FOURCC('s', 'o', 'w', 't'), WRAP_FRAME },
  /* AES3 Clip wrapped */
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x0D,0x01,0x03,0x01,0x02,0x06,0x04,0x00 }, BGAV_MK_FOURCC('s', 'o', 'w', 't'), WRAP_CLIP },


  /* MPEG-ES Frame wrapped, 0x40 ??? stream id */
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x02,0x0D,0x01,0x03,0x01,0x02,0x04,0x40,0x01 }, BGAV_MK_FOURCC('.', 'm', 'p', '3'), WRAP_FRAME }, 
  /* D-10 Mapping 50Mbps PAL Extended Template */
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x0D,0x01,0x03,0x01,0x02,0x01,0x01,0x01 }, BGAV_MK_FOURCC('s', 'o', 'w', 't'), WRAP_FRAME }, 
  /* MXF-GC Frame-wrapped SMPTE D-10 625x50I 30Mbps DefinedTemplate */
  { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x0D,0x01,0x03,0x01,0x02,0x01,0x05,0x01 }, BGAV_MK_FOURCC('s', 'o', 'w', 't'), WRAP_FRAME }, 
  { },
};

static const codec_entry_t * match_codec(const codec_entry_t * ce, const mxf_ul_t u1)
  {
  int i = 0;
  while(ce[i].fourcc)
    {
    if(match_ul(ce[i].ul, u1, 16))
      return &ce[i];
    i++;
    }
  return NULL;
  }

static char * read_utf16_string(bgav_input_context_t * input, int len)
  {
  bgav_charset_converter_t * cnv;
  char * str, * ret;
  cnv = bgav_charset_converter_create("UTF-16BE", BGAV_UTF8);
  if(!cnv)
    {
    bgav_input_skip(input, len);
    return NULL;
    }
  str = malloc(len);
  if(bgav_input_read_data(input, (uint8_t*)str, len) < len)
    {
    bgav_charset_converter_destroy(cnv);
    return NULL;
    }

  ret = bgav_convert_string(cnv, str, len, NULL);
  bgav_charset_converter_destroy(cnv);
  free(str);
  return ret;
  }

int bgav_mxf_klv_read(bgav_input_context_t * input, mxf_klv_t * ret)
  {
  uint8_t c;
  int64_t len;
  /* Key */
  if(bgav_input_read_data(input, ret->key, 16) < 16)
    return 0;

  /* Length */
  if(!bgav_input_read_8(input, &c))
    return 0;

  if(c & 0x80) /* Long */
    {
    len = c & 0x0F;
    ret->length = 0;
    if(len > 8)
      return 0;
    while(len--)
      {
      if(!bgav_input_read_8(input, &c))
        return 0;
      ret->length = (ret->length<<8) | c;
      }
    }
  else
    ret->length = c;

  ret->endpos = input->position + ret->length;
  
  /* Value follows */
  return 1;
  }

void bgav_mxf_klv_dump(int indent, mxf_klv_t * ret)
  {
  bgav_diprintf(indent, "Key: ");
  dump_ul_nb(ret->key);
  gavl_dprintf(", Length: %" PRId64 "\n", ret->length);
  }

int bgav_mxf_sync(bgav_input_context_t * input)
  {
  uint8_t data[4];
  while(1)
    {
    if(bgav_input_get_data(input, data, 4) < 4)
      return 0;
    if(UL_MATCH(data, mxf_klv_key))
      break;
    bgav_input_skip(input, 1);
    }
  return 1;
  }

/* Partition */

int bgav_mxf_partition_read(bgav_input_context_t * input,
                            mxf_klv_t * parent,
                            mxf_partition_t * ret)
  {
  if(!bgav_input_read_16_be(input, &ret->major_version) ||
     !bgav_input_read_16_be(input, &ret->minor_version) ||
     !bgav_input_read_32_be(input, &ret->kag_size) ||
     !bgav_input_read_64_be(input, &ret->this_partition) ||
     !bgav_input_read_64_be(input, &ret->previous_partition) ||
     !bgav_input_read_64_be(input, &ret->footer_partition) ||
     !bgav_input_read_64_be(input, &ret->header_byte_count) ||
     !bgav_input_read_64_be(input, &ret->index_byte_count) ||
     !bgav_input_read_32_be(input, &ret->index_sid) ||
     !bgav_input_read_64_be(input, &ret->body_offset) ||
     !bgav_input_read_32_be(input, &ret->body_sid) ||
     (bgav_input_read_data(input, ret->operational_pattern, 16) < 16) 
     )
    return 0;
  
  ret->essence_container_types = read_refs(input, &ret->num_essence_container_types);
  return 1;
  }

void bgav_mxf_partition_dump(int indent, mxf_partition_t * ret)
  {
  int i;
  bgav_diprintf(indent, "Partition\n");
  bgav_diprintf(indent+2, "major_version:       %d\n", ret->major_version);
  bgav_diprintf(indent+2, "minor_version:       %d\n", ret->minor_version);
  bgav_diprintf(indent+2, "kag_size:            %d\n", ret->kag_size);
  bgav_diprintf(indent+2, "this_partition:      %" PRId64 " \n", ret->this_partition);
  bgav_diprintf(indent+2, "previous_partition:  %" PRId64 " \n", ret->previous_partition);
  bgav_diprintf(indent+2, "footer_partition:    %" PRId64 " \n", ret->footer_partition);
  bgav_diprintf(indent+2, "header_byte_count:    %" PRId64 " \n", ret->header_byte_count);
  bgav_diprintf(indent+2, "index_byte_count:     %" PRId64 " \n", ret->index_byte_count);
  bgav_diprintf(indent+2, "index_sid:           %d\n", ret->index_sid);
  bgav_diprintf(indent+2, "body_offset:         %" PRId64 " \n", ret->body_offset);
  bgav_diprintf(indent+2, "body_sid:            %d\n", ret->body_sid);
  bgav_diprintf(indent+2, "operational_pattern: ");
  dump_ul(ret->operational_pattern);
  
  bgav_diprintf(indent+2, "Essence containers: %d\n", ret->num_essence_container_types);
  for(i = 0; i < ret->num_essence_container_types; i++)
    {
    bgav_diprintf(indent+4, "Essence container: ");
    dump_ul(ret->essence_container_types[i]);
    }
  }

void bgav_mxf_partition_free(mxf_partition_t * ret)
  {
  FREE(ret->essence_container_types);
  }

/* Primer pack */

int bgav_mxf_primer_pack_read(bgav_input_context_t * input,
                              mxf_primer_pack_t * ret)
  {
  int i;
  uint32_t len;
  if(!bgav_input_read_32_be(input, &ret->num_entries) ||
     !bgav_input_read_32_be(input, &len) ||
     (len != 18))
    return 0;
  ret->entries = malloc(ret->num_entries * sizeof(*ret->entries));
  for(i = 0; i < ret->num_entries; i++)
    {
    if(!bgav_input_read_16_be(input, &ret->entries[i].localTag) ||
       (bgav_input_read_data(input, ret->entries[i].uid, 16) < 16))
      return 0;
    }
  return 1;
  }

void bgav_mxf_primer_pack_dump(int indent, mxf_primer_pack_t * ret)
  {
  int i;
  bgav_diprintf(indent, "Primer pack (%d entries)\n", ret->num_entries);
  for(i = 0; i < ret->num_entries; i++)
    {
    bgav_diprintf(indent+2, "LocalTag: %04x, UID: ", ret->entries[i].localTag);
    dump_ul(ret->entries[i].uid);
    }
  }

void bgav_mxf_primer_pack_free(mxf_primer_pack_t * ret)
  {
  if(ret->entries) free(ret->entries);
  }

/* Header metadata */

static mxf_metadata_t *
read_header_metadata(bgav_input_context_t * input,
                     partition_t * ret, mxf_klv_t * klv,
                     int (*read_func)(bgav_input_context_t * input,
                                      partition_t * ret, mxf_metadata_t * m,
                                      int tag, int size, uint8_t * uid),
                     int struct_size, mxf_metadata_type_t type)
  {
  uint16_t tag, len;
  int64_t end_pos;
  mxf_ul_t uid = {0};
  mxf_metadata_t * m;
  if(struct_size)
    {
    m = calloc(1, struct_size);
    m->type = type;
    }
  else
    m = NULL;
  
  while(input->position < klv->endpos)
    {
    if(!bgav_input_read_16_be(input, &tag) ||
       !bgav_input_read_16_be(input, &len))
      return 0;
    end_pos = input->position + len;

    if(!len) continue;

    if(tag > 0x7FFF)
      {
      int i;
      for(i = 0; i < ret->primer_pack.num_entries; i++)
        {
        if(ret->primer_pack.entries[i].localTag == tag)
          memcpy(uid, ret->primer_pack.entries[i].uid, 16);
        }
      if(!read_func(input, ret, m, tag, len, uid))
        return 0;
      }
    else if((tag == 0x3C0A) && m)
      {
      if(bgav_input_read_data(input, m->uid, 16) < 16)
        return 0;
      }
    else if((tag == 0x0102) && m)
      {
      if(bgav_input_read_data(input, m->generation_ul, 16) < 16)
        return 0;
      }
    else if(!read_func(input, ret, m, tag, len, uid))
      return 0;

    if(input->position < end_pos)
      bgav_input_skip(input, end_pos - input->position);
    }

  return m;
  }


/* Header metadata */

/* Content storage */

static int read_content_storage(bgav_input_context_t * input,
                                partition_t * ret, mxf_metadata_t * m,
                                int tag, int size, uint8_t * uid)
  {
  mxf_content_storage_t * p = (mxf_content_storage_t *)m;
  switch(tag)
    {
    case 0x1902:
      if(!(p->essence_container_data_refs =
           read_refs(input, &p->num_essence_container_data_refs)))
        return 0;
      break;
     
    case 0x1901:
      if(!(p->package_refs = read_refs(input, &p->num_package_refs)))
        return 0;
      break;
    default:
#ifdef DUMP_UNKNOWN
      gavl_dprintf("Unknown local tag in content storage: %04x, %d bytes\n", tag, size);
      if(size)
        bgav_input_skip_dump(input, size);
#endif
      break;
    }
  return 1;
  }

void bgav_mxf_content_storage_dump(int indent, mxf_content_storage_t * s)
  {
  int i;
  bgav_diprintf(indent, "Content storage\n");
  bgav_diprintf(indent+2, "UID:           "); dump_ul(s->common.uid); 
  bgav_diprintf(indent+2, "Generation UL: "); dump_ul(s->common.generation_ul); 

  if(s->num_package_refs)
    {
    bgav_diprintf(indent+2, "Package refs:  %d\n", s->num_package_refs);
    for(i = 0; i < s->num_package_refs; i++)
      {
      do_indent(indent+4); dump_ul_ptr(s->package_refs[i], s->packages[i]);
      }
    }
  if(s->num_essence_container_data_refs)
    {
    bgav_diprintf(indent+2, "Essence container refs: %d\n", s->num_essence_container_data_refs);
    for(i = 0; i < s->num_essence_container_data_refs; i++)
      {
      do_indent(indent+4); dump_ul_ptr(s->essence_container_data_refs[i], s->essence_containers[i]);
      }
    }
  }

static int bgav_mxf_content_storage_resolve_refs(partition_t * p, mxf_content_storage_t * s)
  {
  s->packages = resolve_strong_refs(p, s->package_refs, s->num_package_refs,
                                    MXF_TYPE_SOURCE_PACKAGE | MXF_TYPE_MATERIAL_PACKAGE);
  s->essence_containers = resolve_strong_refs(p, s->essence_container_data_refs,
                                              s->num_essence_container_data_refs,
                                              MXF_TYPE_ESSENCE_CONTAINER_DATA);
  
  return 1;
  }




void bgav_mxf_content_storage_free(mxf_content_storage_t * s)
  {
  FREE(s->package_refs);
  FREE(s->essence_container_data_refs);
  FREE(s->packages);
  FREE(s->essence_containers);
  }

/* Material package */

static int read_material_package(bgav_input_context_t * input,
                                 partition_t * ret, mxf_metadata_t * m,
                                 int tag, int size, uint8_t * uid)
  {
  mxf_package_t * p = (mxf_package_t *)m;
  
  switch(tag)
    {
    case 0x4401:
      bgav_input_skip(input, 16);
      if(bgav_input_read_data(input, (uint8_t*)p->package_ul, 16) < 16)
        return 0;
      break;
    case 0x4404:
      if(!bgav_input_read_64_be(input, &p->modification_date))
        return 0;
      break;
    case 0x4405:
      if(!bgav_input_read_64_be(input, &p->creation_date))
        return 0;
      break;
    case 0x4402:
      p->generic_name = read_utf16_string(input, size);
      break;
    case 0x4403:
      if(!(p->track_refs = read_refs(input, &p->num_track_refs)))
        return 0;
      break;
    default:
#ifdef DUMP_UNKNOWN
     gavl_dprintf("Unknown local tag in material package: %04x, %d bytes\n", tag, size);
     if(size)
       bgav_input_skip_dump(input, size);
#endif
     break;
   }
 return 1;
 }

static int read_source_package(bgav_input_context_t * input,
                               partition_t * ret, mxf_metadata_t * m,
                               int tag, int size, uint8_t * uid)
  {
  mxf_package_t * p = (mxf_package_t *)m;
  
  switch(tag)
    {
    case 0x4404:
      if(!bgav_input_read_64_be(input, &p->modification_date))
        return 0;
      break;
    case 0x4405:
      if(!bgav_input_read_64_be(input, &p->creation_date))
        return 0;
      break;
    case 0x4402:
      p->generic_name = read_utf16_string(input, size);
      break;
    case 0x4403:
      if(!(p->track_refs = read_refs(input, &p->num_track_refs)))
        return 0;
      break;
    case 0x4401:
      bgav_input_skip(input, 16);
      if(bgav_input_read_data(input, (uint8_t*)p->package_ul, 16) < 16)
        return 0;
      break;
    case 0x4701:
      if(bgav_input_read_data(input, (uint8_t*)p->descriptor_ref, 16) < 16)
        return 0;
      break;
    default:
#ifdef DUMP_UNKNOWN
     gavl_dprintf("Unknown local tag in source package: %04x, %d bytes\n", tag, size);
     if(size)
       bgav_input_skip_dump(input, size);
#endif
      break;
   }
  return 1;
  }


void bgav_mxf_package_dump(int indent, mxf_package_t * p)
  {
  int i;
  if(p->common.type == MXF_TYPE_SOURCE_PACKAGE)
    bgav_diprintf(indent, "Source package:\n");
  else if(p->common.type == MXF_TYPE_MATERIAL_PACKAGE)
    bgav_diprintf(indent, "Material package:\n");
  bgav_diprintf(indent+2, "UID:           "); dump_ul(p->common.uid); 
  bgav_diprintf(indent+2, "Generation UL: "); dump_ul(p->common.generation_ul); 

  bgav_diprintf(indent+2, "Package UID:       "); dump_ul(p->package_ul); 
  bgav_diprintf(indent+2, "Creation date:     "); dump_date(p->creation_date); gavl_dprintf("\n");
  bgav_diprintf(indent+2, "Modification date: "); dump_date(p->modification_date); gavl_dprintf("\n");
  bgav_diprintf(indent+2, "%d tracks\n", p->num_track_refs);
  for(i = 0; i < p->num_track_refs; i++)
    {
    bgav_diprintf(indent+4, "Track: "); dump_ul_ptr(p->track_refs[i], p->tracks[i]); 
    }
  bgav_diprintf(indent+2, "Descriptor ref: "); dump_ul_ptr(p->descriptor_ref, p->descriptor);
  bgav_diprintf(indent+2, "Generic Name:   %s\n",
                (p->generic_name ? p->generic_name : "Unknown")); 
  }

void bgav_mxf_package_free(mxf_package_t * p)
  {
  FREE(p->track_refs);
  FREE(p->tracks);
  FREE(p->generic_name);
  }

static int bgav_mxf_package_resolve_refs(partition_t * p, mxf_package_t * s)
  {
  s->tracks = resolve_strong_refs(p, s->track_refs, s->num_track_refs, MXF_TYPE_TRACK);
  s->descriptor = resolve_strong_ref(p, s->descriptor_ref,
                                     MXF_TYPE_DESCRIPTOR | MXF_TYPE_MULTIPLE_DESCRIPTOR);
  return 1;
  }

static int bgav_mxf_package_resolve_refs_2(partition_t * p, mxf_package_t * s)
  {
  int i;
  mxf_sequence_t * seq;
  for(i = 0; i < s->num_track_refs; i++)
    {
    if(!s->tracks[i])
      continue;
    seq = (mxf_sequence_t *)(((mxf_track_t *)(s->tracks[i]))->sequence);

    switch(seq->stream_type)
      {
      case GAVL_STREAM_AUDIO:
        s->num_audio_tracks++;
        break;
      case GAVL_STREAM_VIDEO:
        s->num_video_tracks++;
        break;
      case GAVL_STREAM_NONE:
        if(seq->is_timecode)
          {
          s->num_timecode_tracks++;
          s->timecode_track = s->tracks[i];
          }
        break;
      default:
        break;
      }
    }
  return 1;
  }

static int bgav_mxf_package_finalize_descriptors(mxf_file_t * file, mxf_package_t * s)
  {
  int i;
  mxf_sequence_t * seq;
  mxf_track_t * track;
  mxf_descriptor_t * d;
  const codec_entry_t * ce;
  
  for(i = 0; i < s->num_track_refs; i++)
    {
    if(!s->tracks[i])
      continue;
    track = (mxf_track_t *)(s->tracks[i]);
    seq = (mxf_sequence_t *)(track->sequence);
    
    switch(seq->stream_type)
      {
      case GAVL_STREAM_AUDIO:
        d = bgav_mxf_get_source_descriptor(file, s, track);

        ce = match_codec(mxf_sound_essence_container_uls, d->essence_container_ul);
        if(ce)
          {
          d->fourcc = ce->fourcc;
          d->wrapping_type = ce->wrapping_type;
          }
        else
          {
          ce = match_codec(mxf_audio_codec_uls, d->essence_codec_ul);
          if(ce)
            {
            d->fourcc = ce->fourcc;
            d->wrapping_type = ce->wrapping_type;
            }
          }
        
        break;
      case GAVL_STREAM_VIDEO:
        d = bgav_mxf_get_source_descriptor(file, s, track);

        ce = match_codec(mxf_picture_essence_container_uls, d->essence_container_ul);
        if(ce)
          {
          d->fourcc = ce->fourcc;
          d->wrapping_type = ce->wrapping_type;
          }
        else
          {
          ce = match_codec(mxf_video_codec_uls, d->essence_codec_ul);
          if(ce)
            {
            d->fourcc            = ce->fourcc;
            d->wrapping_type     = ce->wrapping_type;
            d->compression_flags = ce->compression_flags;
            }
          else
            {
#ifdef DUMP_UNKNOWN
            gavl_dprintf("Unknown video codec: ");
            dump_ul_nb(d->essence_codec_ul);
            gavl_dprintf("\n");
#endif
            }
          
          }
        break;
      case GAVL_STREAM_NONE:
        break;
      default:
        break;
      }
    }
  return 1;
  
  }


/* source clip */

void bgav_mxf_source_clip_dump(int indent, mxf_source_clip_t * s)
  {
  do_indent(indent);   gavl_dprintf("Source clip:\n");
  bgav_diprintf(indent+2, "UID:                "); dump_ul(s->common.uid);
  bgav_diprintf(indent+2, "Generation UL:      "); dump_ul(s->common.generation_ul); 
  bgav_diprintf(indent+2, "source_package_ref: "); dump_ul_ptr(s->source_package_ref, s->source_package);
  bgav_diprintf(indent+2, "data_definition_ul: "); dump_ul(s->data_definition_ul);
  bgav_diprintf(indent+2, "duration:           %" PRId64 "\n", s->duration);
  bgav_diprintf(indent+2, "start_pos:          %" PRId64 "\n", s->start_position);
  bgav_diprintf(indent+2, "source_track_id:    %d\n", s->source_track_id);
  
  }

void bgav_mxf_source_clip_free(mxf_source_clip_t * s)
  {

  }

static int read_source_clip(bgav_input_context_t * input,
                            partition_t * ret, mxf_metadata_t * m,
                            int tag, int size, uint8_t * uid)
  {
  mxf_source_clip_t * s = (mxf_source_clip_t *)m;
  switch(tag)
    {
    case 0x0201:
      if(bgav_input_read_data(input, s->data_definition_ul, 16) < 16)
        return 0;
      break;
    case 0x0202:
      if(!bgav_input_read_64_be(input, (uint64_t*)&s->duration))
        return 0;
      break;
    case 0x1201:
      if(!bgav_input_read_64_be(input, (uint64_t*)&s->start_position))
        return 0;
      break;
    case 0x1101:
      /* UMID, only get last 16 bytes */
      bgav_input_skip(input, 16);
      if(bgav_input_read_data(input, s->source_package_ref, 16) < 16)
        return 0;
      break;
    case 0x1102:
      if(!bgav_input_read_32_be(input, &s->source_track_id))
        return 0;
      break;
    default:
#ifdef DUMP_UNKNOWN
     gavl_dprintf("Unknown local tag in source clip: %04x, %d bytes\n", tag, size);
     if(size)
       bgav_input_skip_dump(input, size);
#endif
     break;
    }
  return 1;
  }

static int bgav_mxf_source_clip_resolve_refs(partition_t * p, mxf_source_clip_t * s)
  {
  s->source_package = package_by_ul(p, s->source_package_ref);
  return 1;
  }


/* Timecode component */

static int read_timecode_component(bgav_input_context_t * input,
                                   partition_t * ret, mxf_metadata_t * m,
                                   int tag, int size, uint8_t * uid)
  {
  mxf_timecode_component_t * s = (mxf_timecode_component_t *)m;
  switch(tag)
    {
    case 0x0201:
      if(bgav_input_read_data(input, s->data_definition_ul, 16) < 16)
        return 0;
      break;
    case 0x0202:
      if(!bgav_input_read_64_be(input, &s->duration))
        return 0;
      break;
    case 0x1502:
      if(!bgav_input_read_16_be(input, &s->rounded_timecode_base))
        return 0;
      break;
    case 0x1501:
      if(!bgav_input_read_64_be(input, &s->start_timecode))
        return 0;
      break;
    case 0x1503:
      if(!bgav_input_read_8(input, &s->drop_frame))
        return 0;
      break;
    default:
#ifdef DUMP_UNKNOWN
     gavl_dprintf("Unknown local tag in timecode component: %04x, %d bytes\n", tag, size);
     if(size)
       bgav_input_skip_dump(input, size);
#endif
     break;
    }
  return 1;
  }

void bgav_mxf_timecode_component_dump(int indent, mxf_timecode_component_t * s)
  {
  do_indent(indent);   gavl_dprintf("Timecode component\n");
  bgav_diprintf(indent+2, "UID:                   "); dump_ul(s->common.uid); 
  bgav_diprintf(indent+2, "Generation UL:         "); dump_ul(s->common.generation_ul); 

  bgav_diprintf(indent+2, "data_definition_ul:    "); dump_ul(s->data_definition_ul); 
  bgav_diprintf(indent+2, "duration:              %"PRId64"\n", s->duration);
  bgav_diprintf(indent+2, "rounded_timecode_base: %d\n",
                                    s->rounded_timecode_base);
  bgav_diprintf(indent+2, "start_timecode:        %"PRId64"\n", s->start_timecode);
  bgav_diprintf(indent+2, "drop_frame:            %d\n", s->drop_frame);
  }

static int bgav_mxf_timecode_component_resolve_refs(partition_t * p, mxf_timecode_component_t * s)
  {
  return 1;
  }

void bgav_mxf_timecode_component_free(mxf_timecode_component_t * s)
  {
  
  }

/* track */

void bgav_mxf_track_dump(int indent, mxf_track_t * t)
  {
  bgav_diprintf(indent, "Track\n");
  bgav_diprintf(indent+2, "UID:           "); dump_ul(t->common.uid); 
  bgav_diprintf(indent+2, "Generation UL: "); dump_ul(t->common.generation_ul); 

  bgav_diprintf(indent+2, "Track ID:      %d\n", t->track_id);
  bgav_diprintf(indent+2, "Name:          %s\n", (t->name ? t->name : "Unknown"));
  bgav_diprintf(indent+2, "Track number:  [%02x %02x %02x %02x]\n",
                                    t->track_number[0], t->track_number[1],
                                    t->track_number[2], t->track_number[3]);
  bgav_diprintf(indent+2, "Edit rate:     %d/%d\n", t->edit_rate_num, t->edit_rate_den);
  bgav_diprintf(indent+2, "Sequence ref:  ");dump_ul_ptr(t->sequence_ref, t->sequence);
  bgav_diprintf(indent+2, "num_packets:   %d\n", t->num_packets);
  bgav_diprintf(indent+2, "max_size:      %"PRId64"\n", t->max_packet_size);
  }

void bgav_mxf_track_free(mxf_track_t * t)
  {
  FREE(t->name);
  }

static int bgav_mxf_track_resolve_refs(partition_t * p, mxf_track_t * s)
  {
  s->sequence = resolve_strong_ref(p, s->sequence_ref, MXF_TYPE_SEQUENCE);
  return 1;
  }

static int read_track(bgav_input_context_t * input,
                      partition_t * ret, mxf_metadata_t * m,
                      int tag, int size, uint8_t * uid)
  {
  mxf_track_t * t = (mxf_track_t *)m;

  switch(tag)
    {
    case 0x4801:
      if(!bgav_input_read_32_be(input, &t->track_id))
        return 0;
      break;
    case 0x4802:
      t->name = read_utf16_string(input, size);
      break;
    case 0x4804:
      if(bgav_input_read_data(input, t->track_number, 4) < 4)
        return 0;
      break;
    case 0x4B01:
      if(!bgav_input_read_32_be(input, &t->edit_rate_num) ||
         !bgav_input_read_32_be(input, &t->edit_rate_den))
        return 0;
      break;
    case 0x4803:
      if(bgav_input_read_data(input, t->sequence_ref, 16) < 16)
        return 0;
      break;
    case 0x4b02:
      if(!bgav_input_read_64_be(input, &t->origin))
        return 0;
      break;
    default:
#ifdef DUMP_UNKNOWN
      gavl_dprintf("Unknown local tag in track: %04x, %d bytes\n", tag, size);
      if(size)
        bgav_input_skip_dump(input, size);
#endif
      break;
    }
  return 1; 
  }


/* Sequence */

void bgav_mxf_sequence_dump(int indent, mxf_sequence_t * s)
  {
  int i;
  bgav_diprintf(indent, "Sequence\n");
  bgav_diprintf(indent+2, "UID:                "); dump_ul(s->common.uid); 
  bgav_diprintf(indent+2, "Generation UL:      "); dump_ul(s->common.generation_ul); 

  bgav_diprintf(indent+2, "data_definition_ul: ");dump_ul(s->data_definition_ul);
  bgav_diprintf(indent+2, "Structural components (%d):\n", s->num_structural_component_refs);
  for(i = 0; i < s->num_structural_component_refs; i++)
    {
    do_indent(indent+4); dump_ul_ptr(s->structural_component_refs[i], s->structural_components[i]);
    }
  bgav_diprintf(indent+2, "Type: %s\n",
                                    (s->stream_type == GAVL_STREAM_AUDIO ? "Audio" :
                                     (s->stream_type == GAVL_STREAM_VIDEO ? "Video" :
                                      (s->is_timecode ? "Timecode" : "Unknown"))));
  }

void bgav_mxf_sequence_free(mxf_sequence_t * s)
  {
  if(s->structural_component_refs)
    free(s->structural_component_refs);
  if(s->structural_components)
    free(s->structural_components);
  }

static int bgav_mxf_sequence_resolve_refs(partition_t * p, mxf_sequence_t * s)
  {
  const stream_entry_t * se;
  
  s->structural_components = resolve_strong_refs(p, s->structural_component_refs,
                                                 s->num_structural_component_refs,
                                                 MXF_TYPE_SOURCE_CLIP | MXF_TYPE_TIMECODE_COMPONENT);
  
  se = match_stream(mxf_data_definition_uls, s->data_definition_ul);
  if(se)
    s->stream_type = se->type;
  else if(s->structural_components &&
          s->structural_components[0] &&
          (s->structural_components[0]->type == MXF_TYPE_TIMECODE_COMPONENT))
    s->is_timecode = 1;
  return 1;
  }

static int read_sequence(bgav_input_context_t * input,
                         partition_t * ret, mxf_metadata_t * m,
                         int tag, int size, uint8_t * uid)
  {
  mxf_sequence_t * s = (mxf_sequence_t *)m;

  switch(tag)
    {
    case 0x0202:
      if(!bgav_input_read_64_be(input, (uint64_t*)&s->duration))
        return 0;
      break;
    case 0x0201:
      if(bgav_input_read_data(input, s->data_definition_ul, 16) < 16)
        return 0;
      break;
    case 0x1001:
      if(!(s->structural_component_refs = read_refs(input, &s->num_structural_component_refs)))
        return 0;
      break;
    default:
#ifdef DUMP_UNKNOWN
      gavl_dprintf("Unknown local tag in sequence : %04x, %d bytes\n", tag, size);
      if(size)
        bgav_input_skip_dump(input, size);
#endif
      break;
    }
  return 1;
  }

/* Identification */

static int read_version(bgav_input_context_t * input,
                        mxf_version_t * v)
  {
  if(!bgav_input_read_16_be(input, &v->maj) ||
     !bgav_input_read_16_be(input, &v->min) ||
     !bgav_input_read_16_be(input, &v->tweak) ||
     !bgav_input_read_16_be(input, &v->build) ||
     !bgav_input_read_16_be(input, &v->rel))
    return 0;
  return 1;
  }

static int read_identification(bgav_input_context_t * input,
                               partition_t * ret, mxf_metadata_t * m,
                               int tag, int size, uint8_t * uid)
  {
  mxf_identification_t * s = (mxf_identification_t *)m;

  switch(tag)
    {
    case 0x3c01:
      s->company_name = read_utf16_string(input, size);
      break;
    case 0x3c02:
      s->product_name = read_utf16_string(input, size);
      break;
    case 0x3c04:
      s->version_string = read_utf16_string(input, size);
      break;
    case 0x3c08:
      s->platform = read_utf16_string(input, size);
      break;
    case 0x3c09:
      if(bgav_input_read_data(input, s->this_generation_ul, 16) < 16)
        return 0;
      break;
    case 0x3c05:
      if(bgav_input_read_data(input, s->product_ul, 16) < 16)
        return 0;
      break;
    case 0x3c06:
      if(!bgav_input_read_64_be(input, &s->modification_date))
        return 0;
      break;
    case 0x3c03:
      if(!read_version(input, &s->product_version))
        return 0;
      break;
    case 0x3c07:
      if(!read_version(input, &s->toolkit_version))
        return 0;
      break;

    default:
#ifdef DUMP_UNKNOWN
      gavl_dprintf("Unknown local tag in identification : %04x, %d bytes\n", tag, size);
      if(size)
        bgav_input_skip_dump(input, size);
#endif
      break;
    }
  return 1;
  }

void bgav_mxf_identification_dump(int indent, mxf_identification_t * s)
  {
  bgav_diprintf(indent, "Identification\n");

  bgav_diprintf(indent+2, "UID:               "); dump_ul(s->common.uid); 
  bgav_diprintf(indent+2, "Generation UL:     "); dump_ul(s->common.generation_ul); 

  bgav_diprintf(indent+2, "thisGenerationUID: "); dump_ul(s->this_generation_ul);
  bgav_diprintf(indent+2, "Company name:      %s\n",
                                    (s->company_name ? s->company_name :
                                     "(unknown)"));
  bgav_diprintf(indent+2, "Product name:      %s\n",
                                    (s->product_name ? s->product_name :
                                     "(unknown)"));
  bgav_diprintf(indent+2, "Product version:   %d.%d.%d.%d.%d\n", s->product_version.maj, s->product_version.min,
                s->product_version.tweak, s->product_version.build, s->product_version.rel);
  bgav_diprintf(indent+2, "Toolkit version:   %d.%d.%d.%d.%d\n", s->toolkit_version.maj, s->toolkit_version.min,
                s->toolkit_version.tweak, s->toolkit_version.build, s->toolkit_version.rel);
  
  bgav_diprintf(indent+2, "Version string:    %s\n",
                                    (s->version_string ? s->version_string :
                                     "(unknown)"));
  bgav_diprintf(indent+2, "Platform:          %s\n",
                                    (s->platform ? s->platform :
                                     "(unknown)"));

  bgav_diprintf(indent+2, "ProductUID:        "); dump_ul(s->product_ul);
  
  bgav_diprintf(indent+2, "ModificationDate:  "); dump_date(s->modification_date);gavl_dprintf("\n");
  
  }

void bgav_mxf_identification_free(mxf_identification_t * s)
  {
  FREE(s->company_name);
  FREE(s->product_name);
  FREE(s->version_string);
  FREE(s->platform);
  }

static int bgav_mxf_identification_resolve_refs(partition_t * p, mxf_identification_t * s)
  {
  return 1;
  }

/* Descriptor */

void bgav_mxf_descriptor_dump(int indent, mxf_descriptor_t * d)
  {
  int i;
  do_indent(indent);   gavl_dprintf("Descriptor\n");
  bgav_diprintf(indent+2, "UID:                    ");dump_ul(d->common.uid); 
  bgav_diprintf(indent+2, "Generation UL:          ");dump_ul(d->common.generation_ul); 
  bgav_diprintf(indent+2, "essence_container_ul:   ");dump_ul(d->essence_container_ul);
  bgav_diprintf(indent+2, "wrapping_type:          ");
  switch(d->wrapping_type)
    {
    case WRAP_FRAME:
      fprintf(stderr, "Frame wrapped\n");
      break;
    case WRAP_CLIP:
      fprintf(stderr, "Clip wrapped\n");
      break;
    case WRAP_CUSTOM:
      fprintf(stderr, "Custom wrapped\n");
      break;
    case WRAP_UNKNOWN:
      fprintf(stderr, "Unknown\n");
      break;
    }
  bgav_diprintf(indent+2, "essence_codec_ul:       ");dump_ul(d->essence_codec_ul);
  bgav_diprintf(indent+2, "Sample rate:            %d/%d\n", d->sample_rate_num, d->sample_rate_den); 
  bgav_diprintf(indent+2, "Aspect ratio:           %d/%d\n", d->aspect_ratio_num, d->aspect_ratio_den);
  bgav_diprintf(indent+2, "Image size:             %dx%d\n", d->width, d->height);
  bgav_diprintf(indent+2, "Sampled image size:     %dx%d\n", d->sampled_width, d->sampled_height);
  bgav_diprintf(indent+2, "Display size:           %dx%d\n", d->display_width, d->display_height);
  bgav_diprintf(indent+2, "Sampled image offset:   [%d,%d]\n", d->sampled_x_offset, d->sampled_y_offset);
  bgav_diprintf(indent+2, "Bits per sample:        %d\n", d->bits_per_sample);
  bgav_diprintf(indent+2, "Channels:               %d\n", d->channels);
  bgav_diprintf(indent+2, "Locked:                 %d\n", d->locked);
  bgav_diprintf(indent+2, "Frame layout:           %d\n", d->frame_layout);
  bgav_diprintf(indent+2, "Field dominance:        %d\n", d->field_dominance);
  bgav_diprintf(indent+2, "Active bits per sample: %d\n", d->active_bits_per_sample);
  bgav_diprintf(indent+2, "Horizontal subsampling: %d\n", d->horizontal_subsampling);
  bgav_diprintf(indent+2, "Vertical subsampling:   %d\n", d->vertical_subsampling);
  bgav_diprintf(indent+2, "linked track ID:        %d\n", d->linked_track_id);
  bgav_diprintf(indent+2, "Container duration:     %"PRId64"\n", d->container_duration);
  bgav_diprintf(indent+2, "Block align:            %d\n", d->block_align);
  bgav_diprintf(indent+2, "Avg BPS:                %d\n", d->avg_bps);
  bgav_diprintf(indent+2, "Extradata:              %d bytes\n", d->ext_size);
  if(d->ext_size)
    gavl_hexdump(d->ext_data, d->ext_size, 16);
  
  bgav_diprintf(indent+2, "Subdescriptor refs:     %d\n", d->num_subdescriptor_refs);

  for(i = 0; i < d->num_subdescriptor_refs; i++)
    {
    do_indent(indent+4); dump_ul_ptr(d->subdescriptor_refs[i], d->subdescriptors[i]);
    }

  bgav_diprintf(indent+2, "Video line map:         %d entries\n", d->video_line_map_size);
  
  for(i = 0; i < d->video_line_map_size; i++)
    {
    bgav_diprintf(indent+4, "Entry: %d\n", d->video_line_map[i]);
    }
  bgav_diprintf(indent+2, "fourcc:                 ");
  bgav_dump_fourcc(d->fourcc);
  gavl_dprintf("\n");
  }

static int bgav_mxf_descriptor_resolve_refs(partition_t * p, mxf_descriptor_t * d)
  {
  d->subdescriptors = resolve_strong_refs(p, d->subdescriptor_refs, d->num_subdescriptor_refs,
                                          MXF_TYPE_DESCRIPTOR);
  //  if(d->essence_container_ul[15] > 0x01)
  //    d->clip_wrapped = 1;
  return 1;
  }


static int read_pixel_layout(bgav_input_context_t * input, mxf_descriptor_t * d)
  {
  uint8_t code;
  uint8_t w;
  
  do
    {
    if(!bgav_input_read_8(input, &code))
      return 0;
    
    switch (code)
      {
      case 0x52: /* R */
        if(!bgav_input_read_8(input, &w))
          return 0;
        d->bits_per_sample += w;
        break;
      case 0x47: /* G */
        if(!bgav_input_read_8(input, &w))
          return 0;
        d->bits_per_sample += w;
        break;
      case 0x42: /* B */
        if(!bgav_input_read_8(input, &w))
          return 0;
        d->bits_per_sample += w;
        break;
      default:
        bgav_input_skip(input, 1);
        break;
      }
    } while (code != 0); /* SMPTE 377M E.2.46 */
  return 1;
  }

static int read_descriptor(bgav_input_context_t * input,
                           partition_t * ret, mxf_metadata_t * m,
                           int tag, int size, uint8_t * uid)
  {
  int i;
  mxf_descriptor_t * d = (mxf_descriptor_t *)m;
  
  switch(tag)
    {
    case 0x3F01:
      if(!(d->subdescriptor_refs = read_refs(input, &d->num_subdescriptor_refs)))
        return 0;
      break;
    case 0x3002:
      if(!bgav_input_read_64_be(input, &d->container_duration))
        return 0;
      break;
    case 0x3004:
      if(bgav_input_read_data(input, d->essence_container_ul, 16) < 16)
        return 0;
      break;
    case 0x3006:
      if(!bgav_input_read_32_be(input, &d->linked_track_id))
        return 0;
      break;
    case 0x3201: /* PictureEssenceCoding */
      if(bgav_input_read_data(input, d->essence_codec_ul, 16) < 16)
        return 0;
      break;
    case 0x3203:
      if(!bgav_input_read_32_be(input, &d->width))
        return 0;
      break;
    case 0x3202:
      if(!bgav_input_read_32_be(input, &d->height))
        return 0;
      break;
    case 0x3209:
      if(!bgav_input_read_32_be(input, &d->display_width))
        return 0;
      break;
    case 0x3208:
      if(!bgav_input_read_32_be(input, &d->display_height))
        return 0;
      break;
    case 0x3205:
      if(!bgav_input_read_32_be(input, &d->sampled_width))
        return 0;
      break;
    case 0x3204:
      if(!bgav_input_read_32_be(input, &d->sampled_height))
        return 0;
      break;
    case 0x3206:
      if(!bgav_input_read_32_be(input, &d->sampled_x_offset))
        return 0;
      break;
    case 0x3207:
      if(!bgav_input_read_32_be(input, &d->sampled_y_offset))
        return 0;
      break;
    case 0x3301:
      if(!bgav_input_read_32_be(input, &d->active_bits_per_sample))
        return 0;
      break;
    case 0x3302:
      if(!bgav_input_read_32_be(input, &d->horizontal_subsampling))
        return 0;
      break;
    case 0x3308:
      if(!bgav_input_read_32_be(input, &d->vertical_subsampling))
        return 0;
      break;
    case 0x320E:
      if(!bgav_input_read_32_be(input, &d->aspect_ratio_num) ||
         !bgav_input_read_32_be(input, &d->aspect_ratio_den))
        return 0;
      break;
    case 0x3D02:
      if(!bgav_input_read_8(input, &d->locked))
        return 0;
      break;
    case 0x3D0A:
      if(!bgav_input_read_16_be(input, &d->block_align))
        return 0;
      break;
    case 0x3D09:
      if(!bgav_input_read_32_be(input, &d->avg_bps))
        return 0;
      break;
    case 0x3001:
    case 0x3D03:
      if(!bgav_input_read_32_be(input, &d->sample_rate_num) ||
         !bgav_input_read_32_be(input, &d->sample_rate_den))
        return 0;
      break;
    case 0x3D06: /* SoundEssenceCompression */
      if(bgav_input_read_data(input, d->essence_codec_ul, 16) < 16)
        return 0;
      break;
    case 0x3D07:
      if(!bgav_input_read_32_be(input, &d->channels))
        return 0;
      break;
    case 0x3D01:
      if(!bgav_input_read_32_be(input, &d->bits_per_sample))
        return 0;
      break;
    case 0x3401:
      if(!read_pixel_layout(input, d))
        return 0;
      break;
    case 0x320c:
      if(!bgav_input_read_8(input, &d->frame_layout))
        return 0;
      break;
    case 0x3212:
      if(!bgav_input_read_8(input, &d->field_dominance))
        return 0;
      break;
    case 0x320d:
      if(!bgav_input_read_32_be(input, &d->video_line_map_size))
        return 0;
      bgav_input_skip(input, 4);
      d->video_line_map = malloc(d->video_line_map_size * sizeof(*d->video_line_map));
      for(i = 0; i < d->video_line_map_size; i++)
        {
        if(!bgav_input_read_32_be(input, &d->video_line_map[i]))
          return 0;
        }
      break;
    default:
#if 1
      /* Private uid used by SONY C0023S01.mxf */
      if(UL_MATCH(uid, mxf_sony_mpeg4_extradata))
        {
        d->ext_data = malloc(size);
        d->ext_size = size;
        if(bgav_input_read_data(input, d->ext_data, size) < size)
          return 0;
        }
#endif
#ifdef DUMP_UNKNOWN
      else
        {
        gavl_dprintf("Unknown local tag in descriptor : %04x, %d bytes\n", tag, size);
        if(size)
          bgav_input_skip_dump(input, size);
        }
#endif
      break;
    }
  return 1;
  }

void bgav_mxf_descriptor_free(mxf_descriptor_t * d)
  {
  FREE(d->subdescriptor_refs);
  FREE(d->subdescriptors);
  FREE(d->ext_data);
  FREE(d->video_line_map);
  }

/*
 *  Essence Container Data
 */

void bgav_mxf_essence_container_data_dump(int indent, mxf_essence_container_data_t * s)
  {
  bgav_diprintf(indent, "Essence Container Data:\n");
  bgav_diprintf(indent+2, "UID:                  ");dump_ul(s->common.uid); 
  bgav_diprintf(indent+2, "Generation UL:        "); dump_ul(s->common.generation_ul); 
  bgav_diprintf(indent+2, "Linked Package:       ");dump_ul_ptr(s->linked_package_ref, s->linked_package); 
  bgav_diprintf(indent+2, "IndexSID:             %d\n", s->index_sid);
  bgav_diprintf(indent+2, "BodySID:              %d\n", s->body_sid);
  }

static int bgav_mxf_essence_container_data_resolve_refs(partition_t * p, mxf_essence_container_data_t * d)
  {
  d->linked_package = package_by_ul(p, d->linked_package_ref);
  return 1;
  }

void bgav_mxf_essence_container_data_free(mxf_essence_container_data_t * s)
  {
  
  }


static int read_essence_container_data(bgav_input_context_t * input,
                                       partition_t * ret, mxf_metadata_t * m,
                                       int tag, int size, uint8_t * uid)
  {
  mxf_essence_container_data_t * d = (mxf_essence_container_data_t *)m;

  switch(tag)
    {
    case 0x2701:
      /* UMID, only get last 16 bytes */
      bgav_input_skip(input, 16);
      if(bgav_input_read_data(input, d->linked_package_ref, 16) < 16)
        return 0;
      break;
    case 0x3f06:
      if(!bgav_input_read_32_be(input, &d->index_sid))
        return 0;
      break;
    case 0x3f07:
      if(!bgav_input_read_32_be(input, &d->body_sid))
        return 0;
      break;
    default:
      /* Private uid used by SONY C0023S01.mxf */
#ifdef DUMP_UNKNOWN
        gavl_dprintf("Unknown local tag in essence container data : %04x, %d bytes\n", tag, size);
        if(size)
          bgav_input_skip_dump(input, size);
#endif
      break;
    }
  return 1;
  }

/* Preface */

void bgav_mxf_preface_dump(int indent, mxf_preface_t * s)
  {
  int i;
  do_indent(indent);   gavl_dprintf("Preface:\n");
  bgav_diprintf(indent+2, "UID:                  ");dump_ul(s->common.uid); 
  bgav_diprintf(indent+2, "Generation UL:        ");dump_ul(s->common.generation_ul); 
  bgav_diprintf(indent+2, "Last modified date:    ");dump_date(s->last_modified_date); gavl_dprintf("\n");
  bgav_diprintf(indent+2, "Version:               %d\n", s->version);
  bgav_diprintf(indent+2, "ObjectModelVersion:    %d\n", s->object_model_version);
  bgav_diprintf(indent+2, "Identifications:       %d\n", s->num_identification_refs);
  for(i = 0; i < s->num_identification_refs; i++)
    {
    do_indent(indent+4); dump_ul_ptr(s->identification_refs[i], s->identifications[i]);
    }
  bgav_diprintf(indent+2, "Content storage:       ");dump_ul_ptr(s->content_storage_ref, s->content_storage); 
  bgav_diprintf(indent+2, "Operational pattern:   ");dump_ul_nb(s->operational_pattern_ul);gavl_dprintf(" %s\n", get_op_name(s->operational_pattern));
  bgav_diprintf(indent+2, "Essence containers:    %d\n", s->num_essence_container_types);
  for(i = 0; i < s->num_essence_container_types; i++)
    {
    bgav_diprintf(indent+4, "Essence containers: "); dump_ul(s->essence_container_types[i]);
    }
  bgav_diprintf(indent+2, "Primary package:      ");dump_ul(s->primary_package_ul);
  
  }

static int bgav_mxf_preface_resolve_refs(partition_t * p, mxf_preface_t * d)
  {
  d->identifications = resolve_strong_refs(p, d->identification_refs, d->num_identification_refs,
                                           MXF_TYPE_IDENTIFICATION);
  d->content_storage = resolve_strong_ref(p, d->content_storage_ref, MXF_TYPE_CONTENT_STORAGE);

  if(is_op_1a(d->operational_pattern_ul))
    d->operational_pattern = MXF_OP_1a;
  else if(is_op_atom(d->operational_pattern_ul))
    d->operational_pattern = MXF_OP_ATOM;
  return 1;
  }

void bgav_mxf_preface_free(mxf_preface_t * d)
  {
  FREE(d->identification_refs);
  FREE(d->essence_container_types);
  
  FREE(d->identifications);
  
  FREE(d->dm_schemes);
  }

static int read_preface(bgav_input_context_t * input,
                        partition_t * ret, mxf_metadata_t * m,
                        int tag, int size, uint8_t * uid)
  {
  mxf_preface_t * d = (mxf_preface_t *)m;

  switch(tag)
    {
    case 0x3b02:
      if(!bgav_input_read_64_be(input, &d->last_modified_date))
        return 0;
      break;
    case 0x3b05:
      if(!bgav_input_read_16_be(input, &d->version))
        return 0;
      break;
    case 0x3b07:
      if(!bgav_input_read_32_be(input, &d->object_model_version))
        return 0;
      break;
    case 0x3b06:
      if(!(d->identification_refs = read_refs(input, &d->num_identification_refs)))
        return 0;
      break;
    case 0x3b03:
      if(bgav_input_read_data(input, d->content_storage_ref, 16) < 16)
        return 0;
      break;
    case 0x3b08:
      if(bgav_input_read_data(input, d->primary_package_ul, 16) < 16)
        return 0;
      break;
    case 0x3b09:
      if(bgav_input_read_data(input, d->operational_pattern_ul, 16) < 16)
        return 0;
      break;
    case 0x3b0a:
      if(!(d->essence_container_types = read_refs(input, &d->num_essence_container_types)))
        return 0;
      break;
    case 0x3b0b:
      if(!(d->dm_schemes = read_refs(input, &d->num_dm_schemes)))
        return 0;
      break;
  
#if 0
    case 0x3F01:
      if(!(d->subdescriptors_refs = read_refs(input, &d->num_subdescriptors)))
        return 0;
      break;
    case 0x3004:
      if(bgav_input_read_data(input, d->essence_container_ul, 16) < 16)
        return 0;
      break;
    case 0x3201: /* PictureEssenceCoding */
      if(bgav_input_read_data(input, d->essence_codec_ul, 16) < 16)
        return 0;
      break;
    case 0x3203:
      if(!bgav_input_read_32_be(input, &d->width))
        return 0;
      break;
    case 0x3202:
      if(!bgav_input_read_32_be(input, &d->height))
        return 0;
      break;
    case 0x320E:
      if(!bgav_input_read_32_be(input, &d->aspect_ratio_num) ||
         !bgav_input_read_32_be(input, &d->aspect_ratio_den))
        return 0;
      break;
    case 0x3D03:
      if(!bgav_input_read_32_be(input, &d->sample_rate_num) ||
         !bgav_input_read_32_be(input, &d->sample_rate_den))
        return 0;
      break;
    case 0x3D06: /* SoundEssenceCompression */
      if(bgav_input_read_data(input, d->essence_codec_ul, 16) < 16)
        return 0;
      break;
    case 0x3D07:
      if(!bgav_input_read_32_be(input, &d->channels))
        return 0;
      break;
    case 0x3D01:
      if(!bgav_input_read_32_be(input, &d->bits_per_sample))
        return 0;
      break;
    case 0x3401:
      if(!read_pixel_layout(input, d))
        return 0;
      break;
#endif
    default:
#ifdef DUMP_UNKNOWN
        gavl_dprintf("Unknown local tag in preface : %04x, %d bytes\n", tag, size);
        if(size)
          bgav_input_skip_dump(input, size);
#endif
      break;
    }
  return 1;
  }

/* Index table segment */

static int read_index_table_segment(bgav_input_context_t * input,
                                    mxf_file_t * ret, mxf_klv_t * klv)
  {
  uint16_t tag, len;
  int64_t end_pos;
  mxf_ul_t uid = {0};
  int i;
  
  mxf_index_table_segment_t * idx;

  idx = calloc(1, sizeof(*idx));
  
  while(input->position < klv->endpos)
    {
    if(!bgav_input_read_16_be(input, &tag) ||
       !bgav_input_read_16_be(input, &len))
      return 0;
    end_pos = input->position + len;
    
    if(!len) continue;
    
    if(tag > 0x7FFF)
      {
      int i;
      for(i = 0; i < ret->header.primer_pack.num_entries; i++)
        {
        if(ret->header.primer_pack.entries[i].localTag == tag)
          memcpy(uid, ret->header.primer_pack.entries[i].uid, 16);
        }
      }
    else if(tag == 0x3C0A)
      {
      if(bgav_input_read_data(input, idx->uid, 16) < 16)
        return 0;
      }
    else if(tag == 0x3f0b) // IndexEditRate
      {
      if(!bgav_input_read_32_be(input, &idx->edit_rate_num) ||
         !bgav_input_read_32_be(input, &idx->edit_rate_den))
        return 0;
      }
    else if(tag == 0x3f0c) // IndexStartPosition
      {
      if(!bgav_input_read_64_be(input, &idx->start_position))
        return 0;
      }
    else if(tag == 0x3f0d) // IndexDuration
      {
      if(!bgav_input_read_64_be(input, &idx->duration))
        return 0;
      }
    else if(tag == 0x3f05) // EditUnitByteCount
      {
      if(!bgav_input_read_32_be(input, &idx->edit_unit_byte_count))
        return 0;
      }
    else if(tag == 0x3f06) // IndexSID
      {
      if(!bgav_input_read_32_be(input, &idx->index_sid))
        return 0;
      }
    else if(tag == 0x3f07) // BodySID
      {
      if(!bgav_input_read_32_be(input, &idx->body_sid))
        return 0;
      }
    else if(tag == 0x3f08) // SliceCount
      {
      if(!bgav_input_read_8(input, &idx->slice_count))
        return 0;
      }
    else if(tag == 0x3f09) // DeltaEntryArray
      {
      if(!bgav_input_read_32_be(input, &idx->num_delta_entries))
        return 0;
      bgav_input_skip(input, 4);

      idx->delta_entries =
        malloc(idx->num_delta_entries * sizeof(*idx->delta_entries));
      for(i = 0; i < idx->num_delta_entries; i++)
        {
        if(!bgav_input_read_8(input, &idx->delta_entries[i].pos_table_index) ||
           !bgav_input_read_8(input, &idx->delta_entries[i].slice) ||
           !bgav_input_read_32_be(input, &idx->delta_entries[i].element_delta))
          return 0;
        }
      }
    else if(tag == 0x3f0a) // EntryArray
      {
      uint32_t entry_len;
      if(!bgav_input_read_32_be(input, &idx->num_entries))
        return 0;
      if(!bgav_input_read_32_be(input, &entry_len))
        return 0;

      idx->entries =
        malloc(idx->num_entries * sizeof(*idx->entries));
      for(i = 0; i < idx->num_entries; i++)
        {
        if(!bgav_input_read_8(input,     (uint8_t*)&idx->entries[i].temporal_offset) ||
           !bgav_input_read_8(input,     (uint8_t*)&idx->entries[i].anchor_offset) ||
           !bgav_input_read_8(input,     &idx->entries[i].flags) ||
           !bgav_input_read_64_be(input, &idx->entries[i].offset))
          return 0;
        //        bgav_input_skip_dump(input, entry_len - 11);
        bgav_input_skip(input, entry_len - 11);
        }
      }
    else
      {
      bgav_input_skip(input, len);
      }
    if(input->position < end_pos)
      bgav_input_skip(input, end_pos - input->position);
    }

  ret->index_segments =
    realloc(ret->index_segments,
            (ret->num_index_segments+1) * sizeof(*ret->index_segments));
  ret->index_segments[ret->num_index_segments] = idx;
  ret->num_index_segments ++;
  
  return 1;
  }

void bgav_mxf_index_table_segment_dump(int indent, mxf_index_table_segment_t * idx)
  {
  int i;
  bgav_diprintf(indent, "Index table segment:\n");
  bgav_diprintf(indent+2, "UID: "); dump_ul(idx->uid), 
  bgav_diprintf(indent+2, "edit_rate: %d/%d",
                                    idx->edit_rate_num, idx->edit_rate_den);
  bgav_diprintf(indent+2, "start_position: %"PRId64"\n",
                                    idx->start_position);
  bgav_diprintf(indent+2, "duration: %"PRId64"\n",
                                    idx->duration);
  bgav_diprintf(indent+2, "edit_unit_byte_count: %d\n",
                                    idx->edit_unit_byte_count);
  bgav_diprintf(indent+2, "index_sid: %d\n",
                                    idx->index_sid);
  bgav_diprintf(indent+2, "body_sid: %d\n",
                                    idx->body_sid);
  bgav_diprintf(indent+2, "slice_count: %d\n",
                                    idx->slice_count);

  if(idx->num_delta_entries)
    {
    bgav_diprintf(indent+2, "Delta entries: %d\n", idx->num_delta_entries);
    for(i = 0; i < idx->num_delta_entries; i++)
      {
      bgav_diprintf(indent+4, "I: %d, S: %d, delta: %d\n",
                                        idx->delta_entries[i].pos_table_index,
                                        idx->delta_entries[i].slice,
                                        idx->delta_entries[i].element_delta);
      }
    }
  if(idx->num_entries)
    {
    bgav_diprintf(indent+2, "Entries: %d\n", idx->num_entries);
    for(i = 0; i < idx->num_entries; i++)
      {
      bgav_diprintf(indent+4, "T: %d, A: %d, Flags: %02x, offset: %"PRId64"\n",
                    idx->entries[i].temporal_offset,
                    idx->entries[i].anchor_offset,
                    idx->entries[i].flags,
                    idx->entries[i].offset);
      }
    }
  }

void bgav_mxf_index_table_segment_free(mxf_index_table_segment_t * idx)
  {
  if(idx->delta_entries) free(idx->delta_entries);
  
  }


/* File */

static int resolve_refs(mxf_file_t * file, partition_t * ret)
  {
  int i;

  /* First round */

  for(i = 0; i < ret->num_metadata; i++)
    {
    switch(ret->metadata[i]->type)
      {
      case MXF_TYPE_MATERIAL_PACKAGE:
        if(!bgav_mxf_package_resolve_refs(ret, (mxf_package_t*)(ret->metadata[i])))
          return 0;
        ret->num_source_packages++;
        break;
      case MXF_TYPE_SOURCE_PACKAGE:
        if(!bgav_mxf_package_resolve_refs(ret, (mxf_package_t*)(ret->metadata[i])))
          return 0;
        ret->num_material_packages++;
        break;
      case MXF_TYPE_TIMECODE_COMPONENT:
        if(!bgav_mxf_timecode_component_resolve_refs(ret, (mxf_timecode_component_t*)(ret->metadata[i])))
          return 0;
        break;
      case MXF_TYPE_PREFACE:
        if(!bgav_mxf_preface_resolve_refs(ret, (mxf_preface_t*)(ret->metadata[i])))
          return 0;
        ret->preface = ret->metadata[i];
        break;
      case MXF_TYPE_CONTENT_STORAGE:
        if(!bgav_mxf_content_storage_resolve_refs(ret, (mxf_content_storage_t*)(ret->metadata[i])))
          return 0;
        break;
      case MXF_TYPE_SEQUENCE:
        {
        mxf_sequence_t * s;
        s = (mxf_sequence_t*)ret->metadata[i];
        if(!bgav_mxf_sequence_resolve_refs(ret, s))
          return 0;
        }
        break;
      case MXF_TYPE_MULTIPLE_DESCRIPTOR:
        if(!bgav_mxf_descriptor_resolve_refs(ret, (mxf_descriptor_t*)(ret->metadata[i])))
          return 0;
        break;
      case MXF_TYPE_DESCRIPTOR:
        ret->num_descriptors++;
        if(!bgav_mxf_descriptor_resolve_refs(ret, (mxf_descriptor_t*)(ret->metadata[i])))
          return 0;
        break;
      case MXF_TYPE_TRACK:
        if(!bgav_mxf_track_resolve_refs(ret, (mxf_track_t*)(ret->metadata[i])))
          return 0;
        break;
      case MXF_TYPE_IDENTIFICATION:
        if(!bgav_mxf_identification_resolve_refs(ret, (mxf_identification_t*)(ret->metadata[i])))
          return 0;
        break;
      case MXF_TYPE_CRYPTO_CONTEXT:
      case MXF_TYPE_SOURCE_CLIP:
      case MXF_TYPE_ESSENCE_CONTAINER_DATA:
        break;
      }
    }
  
  /* Second round */
  for(i = 0; i < ret->num_metadata; i++)
    {
    switch(ret->metadata[i]->type)
      {
      case MXF_TYPE_SOURCE_CLIP:
        if(!bgav_mxf_source_clip_resolve_refs(ret, (mxf_source_clip_t*)(ret->metadata[i])))
          return 0;
        break;
      case MXF_TYPE_ESSENCE_CONTAINER_DATA:
        if(!bgav_mxf_essence_container_data_resolve_refs(ret, (mxf_essence_container_data_t*)(ret->metadata[i])))
          return 0;
        break;

      case MXF_TYPE_MATERIAL_PACKAGE:
      case MXF_TYPE_SOURCE_PACKAGE:
      case MXF_TYPE_TIMECODE_COMPONENT:
      case MXF_TYPE_PREFACE:
      case MXF_TYPE_CONTENT_STORAGE:
      case MXF_TYPE_SEQUENCE:
      case MXF_TYPE_MULTIPLE_DESCRIPTOR:
      case MXF_TYPE_DESCRIPTOR:
      case MXF_TYPE_TRACK:
      case MXF_TYPE_IDENTIFICATION:
      case MXF_TYPE_CRYPTO_CONTEXT:
        break;
      }
    }

  /* 3rd round */
  for(i = 0; i < ret->num_metadata; i++)
    {
    switch(ret->metadata[i]->type)
      {
      case MXF_TYPE_MATERIAL_PACKAGE:
        if(!bgav_mxf_package_resolve_refs_2(ret, (mxf_package_t*)(ret->metadata[i])))
          return 0;
        break;
      case MXF_TYPE_SOURCE_PACKAGE:
        if(!bgav_mxf_package_resolve_refs_2(ret, (mxf_package_t*)(ret->metadata[i])))
          return 0;
        bgav_mxf_package_finalize_descriptors(file, (mxf_package_t*)(ret->metadata[i]));

        break;
      default:
        break;
      }
    }
  
  return 1;
  }

static int get_max_segments_p(mxf_metadata_t * m)
  {
  int i, ret = 0;
  mxf_track_t * t;
  mxf_package_t * p;
  mxf_sequence_t * s;

  p = (mxf_package_t *)m;
  
  for(i = 0; i < p->num_track_refs; i++)
    {
    t = (mxf_track_t *)p->tracks[i];

    if(!t)
      continue;
    
    s = (mxf_sequence_t *)t->sequence;
    if(ret < s->num_structural_component_refs)
      ret = s->num_structural_component_refs;
    }
  return ret;
  }

static int get_max_segments(partition_t * ret)
  {
  mxf_content_storage_t * cs;
  int i;
  int max_segments;

  if(!ret->preface)
    return 1;
  
  cs = (mxf_content_storage_t*)(((mxf_preface_t*)(ret->preface))->content_storage);

  for(i = 0; i < cs->num_package_refs; i++)
    {
    if(cs->packages[i]->type == MXF_TYPE_MATERIAL_PACKAGE)
      {
      max_segments = get_max_segments_p(cs->packages[i]);
      if(ret->max_material_sequence_components < max_segments)
        ret->max_material_sequence_components = max_segments;
      }
    else if(cs->packages[i]->type == MXF_TYPE_SOURCE_PACKAGE)
      {
      max_segments = get_max_segments_p(cs->packages[i]);
      if(ret->max_source_sequence_components < max_segments)
        ret->max_source_sequence_components = max_segments;
      
      }
    }
  return 1;
  }

uint32_t bgav_mxf_get_audio_fourcc(mxf_descriptor_t * d)
  {
  const codec_entry_t * ce;

  ce = match_codec(mxf_audio_codec_uls, d->essence_codec_ul);
  if(ce)
    return ce->fourcc;

  ce = match_codec(mxf_sound_essence_container_uls, d->essence_container_ul);
  
  if(ce)
    return ce->fourcc;
  
  return 0;
  }

uint32_t bgav_mxf_get_video_fourcc(mxf_descriptor_t * d)
  {
  const codec_entry_t * ce;

  ce = match_codec(mxf_video_codec_uls, d->essence_codec_ul);
  if(ce)
    return ce->fourcc;

  ce = match_codec(mxf_picture_essence_container_uls, d->essence_container_ul);
  
  if(ce)
    return ce->fourcc;
  
  return 0;

  }

static int bgav_mxf_finalize_header(mxf_file_t * ret)
  {
  if(!resolve_refs(ret, &ret->header))
    return 0;
  
  if(!ret->header.preface || !((mxf_preface_t*)(ret->header.preface))->content_storage)
    return 0;

  get_max_segments(&ret->header);
  
  return 1;
  }

static int bgav_mxf_finalize_body(mxf_file_t * ret)
  {
  int i;
  
  for(i = 0; i < ret->num_body_partitions; i++)
    {
    if(!resolve_refs(ret, &ret->body_partitions[i]))
      return 0;

    get_max_segments(&ret->body_partitions[i]);
    }
  return 1;
  }

static void update_source_track(mxf_file_t * f, mxf_klv_t * klv)
  {
  int i, j;
  mxf_track_t * t;
  mxf_package_t * p;
  mxf_preface_t * preface;
  mxf_content_storage_t * cs;
  
  int done = 0;
  /* Find track */
  preface = (mxf_preface_t*)f->header.preface;
  cs = (mxf_content_storage_t *)preface->content_storage;
  for(i = 0; i < cs->num_package_refs; i++)
    {
    if(cs->packages[i] &&
       cs->packages[i]->type == MXF_TYPE_SOURCE_PACKAGE)
      {
      p = (mxf_package_t*)(cs->packages[i]);
      for(j = 0; j < p->num_track_refs; j++)
        {
        t = (mxf_track_t*)(p->tracks[j]);
        if(!memcmp(t->track_number, &klv->key[12], 4))
          {
          done = 1;
          break;
          }
        }
      }
    if(done)
      break;
    }
  if(!done)
    return;
  t->num_packets++;
  if(klv->length > t->max_packet_size)
    t->max_packet_size = klv->length;
  }

static void append_metadata(mxf_metadata_t *** arr, int * num, mxf_metadata_t * m)
  {
  *arr = realloc(*arr, ((*num) + 1) * sizeof(**arr));
  (*arr)[*num] = m;
  (*num)++;
  }

static int read_partition(bgav_input_context_t * input,
                          partition_t * p, mxf_klv_t * partition_klv, int64_t start_pos)
  {
  int64_t last_pos, header_start_pos;
  mxf_klv_t klv;
  mxf_metadata_t * m;

  p->start_pos = start_pos;

  /* end_pos will be changed by subsequent calls */
  p->end_pos = input->total_bytes;
  if(!bgav_mxf_partition_read(input, partition_klv, &p->p))
    return 0;

  if(!p->p.header_byte_count)
    return 1;

  //  if(input->opt.dump_headers)
  //    bgav_mxf_partition_dump(0, &p->p);
  
  header_start_pos = 0;

  while(1) /* Look for primer pack */
    {
    last_pos = input->position;
    
    if(!bgav_mxf_klv_read(input, &klv))
      break;

    if(UL_MATCH(klv.key, mxf_primer_pack_key))
      {
      if(!bgav_mxf_primer_pack_read(input, &p->primer_pack))
        return 0;
      header_start_pos = last_pos;
      break;
      }
    else if(UL_MATCH_MOD_REGVER(klv.key, mxf_filler_key))
      {
      bgav_input_skip(input, klv.length);
      }
    else
      {
#ifdef DUMP_UNKNOWN
      gavl_dprintf("Unknown header chunk:\n");
      bgav_mxf_klv_dump(0, &klv);
      bgav_input_skip_dump(input, klv.length);
#else
      bgav_input_skip(input, klv.length);
#endif
      }
    }

  
  /* Read header metadata */
  while(input->position - header_start_pos < p->p.header_byte_count)
    {
    last_pos = input->position;
    
    if(!bgav_mxf_klv_read(input, &klv))
      break;

    m = NULL;
    
    if(UL_MATCH_MOD_REGVER(klv.key, mxf_filler_key))
      {
      bgav_input_skip(input, klv.length);
      }
    else if(UL_MATCH(klv.key, mxf_content_storage_key))
      {
      if(!(m = read_header_metadata(input, p, &klv,
                                    read_content_storage,
                                    sizeof(mxf_content_storage_t),
                                    MXF_TYPE_CONTENT_STORAGE)))
        return 0;
        
      }
    else if(UL_MATCH(klv.key, mxf_source_package_key))
      {
      if(!(m = read_header_metadata(input, p, &klv,
                                    read_source_package, sizeof(mxf_package_t),
                                    MXF_TYPE_SOURCE_PACKAGE)))
        return 0;
      }
    else if(UL_MATCH(klv.key, mxf_essence_container_data_key))
      {
      if(!(m = read_header_metadata(input, p, &klv,
                                    read_essence_container_data, sizeof(mxf_essence_container_data_t),
                                    MXF_TYPE_ESSENCE_CONTAINER_DATA)))
        return 0;
      }
    else if(UL_MATCH(klv.key, mxf_material_package_key))
      {
      if(!(m = read_header_metadata(input, p, &klv,
                                    read_material_package, sizeof(mxf_package_t),
                                    MXF_TYPE_MATERIAL_PACKAGE)))
        return 0;
      }
    else if(UL_MATCH(klv.key, mxf_sequence_key))
      {
      if(!(m = read_header_metadata(input, p, &klv,
                                    read_sequence, sizeof(mxf_sequence_t),
                                    MXF_TYPE_SEQUENCE)))
        return 0;
      }
    else if(UL_MATCH(klv.key, mxf_source_clip_key))
      {
      //        bgav_input_skip_dump(input, klv.length);
      if(!(m = read_header_metadata(input, p, &klv,
                                    read_source_clip, sizeof(mxf_source_clip_t),
                                    MXF_TYPE_SOURCE_CLIP)))
        return 0;
      }
    else if(UL_MATCH(klv.key, mxf_timecode_component_key))
      {
      if(!(m = read_header_metadata(input, p, &klv,
                                    read_timecode_component, sizeof(mxf_timecode_component_t),
                                    MXF_TYPE_TIMECODE_COMPONENT)))
        return 0;
      }
    else if(UL_MATCH(klv.key, mxf_static_track_key))
      {

      if(!(m = read_header_metadata(input, p, &klv,
                                    read_track, sizeof(mxf_track_t),
                                    MXF_TYPE_TRACK)))
        return 0;
      }
    else if(UL_MATCH(klv.key, mxf_preface_key))
      {
      if(!(m = read_header_metadata(input, p, &klv,
                                    read_preface, sizeof(mxf_preface_t),
                                    MXF_TYPE_PREFACE)))
        return 0;
        
      }
    else if(UL_MATCH(klv.key, mxf_generic_track_key))
      {
      if(!(m = read_header_metadata(input, p, &klv,
                                    read_source_clip, sizeof(mxf_track_t),
                                    MXF_TYPE_TRACK)))
        return 0;
      }
    else if(UL_MATCH(klv.key, mxf_descriptor_multiple_key))
      {
      if(!(m = read_header_metadata(input, p, &klv,
                                    read_descriptor, sizeof(mxf_descriptor_t),
                                    MXF_TYPE_MULTIPLE_DESCRIPTOR)))
        return 0;
        
      }
    else if(UL_MATCH(klv.key, mxf_identification_key))
      {
      if(!(m = read_header_metadata(input, p, &klv,
                                    read_identification, sizeof(mxf_identification_t),
                                    MXF_TYPE_IDENTIFICATION)))
        return 0;
      }
    else if(UL_MATCH(klv.key, mxf_descriptor_generic_sound_key) ||
            UL_MATCH(klv.key, mxf_descriptor_cdci_key) ||
            UL_MATCH(klv.key, mxf_descriptor_rgba_key) ||
            UL_MATCH(klv.key, mxf_descriptor_mpeg2video_key) ||
            UL_MATCH(klv.key, mxf_descriptor_wave_key) ||
            UL_MATCH(klv.key, mxf_descriptor_aes3_key))
      {
      if(!(m = read_header_metadata(input, p, &klv,
                                    read_descriptor, sizeof(mxf_descriptor_t),
                                    MXF_TYPE_DESCRIPTOR)))
        return 0;
      }
    else
      {
#ifdef DUMP_UNKNOWN
      gavl_dprintf("Unknown metadata chunk:\n");
      bgav_mxf_klv_dump(0, &klv);
      bgav_input_skip_dump(input, klv.length);
#else
      bgav_input_skip(input, klv.length);
#endif
      }
    if(m)
      append_metadata(&p->metadata, &p->num_metadata, m);
    
    }
  return 1;
  }

int bgav_mxf_file_read(bgav_input_context_t * input,
                       mxf_file_t * ret)
  {
  int64_t pos;
  mxf_klv_t klv;
  
  if(!(input->flags & BGAV_INPUT_CAN_SEEK_BYTE))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Cannot decode MXF file from non seekable source");
    return 0;
    }
  /* Read header partition pack */
  if(!bgav_mxf_sync(input))
    {
    return 0;
    }
  pos = input->position;
  if(!bgav_mxf_klv_read(input, &klv))
    return 0;

  if(!UL_MATCH(klv.key, mxf_header_partition_pack_key))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Could not find header partition");
    return 0;
    }
  
  if(!read_partition(input, &ret->header, &klv, pos))
    return 0;

  if(!bgav_mxf_finalize_header(ret))
    {
    return 0;
    }

  ret->data_start = input->position;
  
  /* Read rest */
  while(1)
    {
    pos = input->position;
    
    if(!bgav_mxf_klv_read(input, &klv))
      break;
    
    if(UL_MATCH_MOD_REGVER(klv.key, mxf_filler_key))
      {
      bgav_input_skip(input, klv.length);
      }
    else if(UL_MATCH(klv.key, mxf_index_table_segment_key))
      {
      if(!read_index_table_segment(input, ret, &klv))
        return 0;
      }
    else if(UL_MATCH(klv.key, mxf_essence_element_key))
      {
      update_source_track(ret, &klv);
#ifdef DUMP_ESSENCE
      gavl_dprintf("Essence element for track %02x %02x %02x %02x (%"PRId64" bytes)\n",
                   klv.key[12], klv.key[13], klv.key[14], klv.key[15], klv.length);
#endif
      bgav_input_skip(input, klv.length);
      }
    else if(UL_MATCH(klv.key, mxf_closed_body_partition_key) ||
            UL_MATCH(klv.key, mxf_open_body_partition_key))
      {
      //      gavl_dprintf("Got body partition\n");
      
      ret->body_partitions =
        realloc(ret->body_partitions, (ret->num_body_partitions+1)
                * sizeof(*ret->body_partitions));

      memset(ret->body_partitions + ret->num_body_partitions, 0,
             sizeof(*ret->body_partitions));

      if(!ret->num_body_partitions)
        ret->header.end_pos = pos;
      else
        ret->body_partitions[ret->num_body_partitions-1].end_pos = pos;
      
      if(!read_partition(input, ret->body_partitions + ret->num_body_partitions, &klv, pos))
        return 0;
      ret->num_body_partitions++;
      }
    else
      {
      bgav_input_skip(input, klv.length);
#ifdef DUMP_UNKNOWN
      gavl_dprintf("Unknown KLV: "); bgav_mxf_klv_dump(0, &klv);
#endif
      }
    }

  if(!bgav_mxf_finalize_body(ret))
    {
    return 0;
    }

  
  bgav_input_seek(input, ret->data_start, SEEK_SET);
  
  return 1;
  }

static void free_partition(partition_t * p)
  {
  int i;
  bgav_mxf_partition_free(&p->p);
  bgav_mxf_primer_pack_free(&p->primer_pack);

  if(p->metadata)
    {
    for(i = 0; i < p->num_metadata; i++)
      {
      switch(p->metadata[i]->type)
        {
        case MXF_TYPE_MATERIAL_PACKAGE:
        case MXF_TYPE_SOURCE_PACKAGE:
          bgav_mxf_package_free((mxf_package_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_PREFACE:
          bgav_mxf_preface_free((mxf_preface_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_CONTENT_STORAGE:
          bgav_mxf_content_storage_free((mxf_content_storage_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_SOURCE_CLIP:
          bgav_mxf_source_clip_free((mxf_source_clip_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_TIMECODE_COMPONENT:
          bgav_mxf_timecode_component_free((mxf_timecode_component_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_SEQUENCE:
          bgav_mxf_sequence_free((mxf_sequence_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_MULTIPLE_DESCRIPTOR:
        case MXF_TYPE_DESCRIPTOR:
          bgav_mxf_descriptor_free((mxf_descriptor_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_ESSENCE_CONTAINER_DATA:
          bgav_mxf_essence_container_data_free((mxf_essence_container_data_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_TRACK:
          bgav_mxf_track_free((mxf_track_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_CRYPTO_CONTEXT:
          break;
        case MXF_TYPE_IDENTIFICATION:
          bgav_mxf_identification_free((mxf_identification_t*)(p->metadata[i]));
          break;
        }
      if(p->metadata[i])
        free(p->metadata[i]);
      }
    free(p->metadata);
    }
  }

void bgav_mxf_file_free(mxf_file_t * ret)
  {
  int i;

  free_partition(&ret->header);

  if(ret->body_partitions)
    {
    for(i = 0; i < ret->num_body_partitions; i++)
      {
      free_partition(&ret->body_partitions[i]);
      }
    free(ret->body_partitions);
    }
  
  if(ret->index_segments)
    {
    for(i = 0; i < ret->num_index_segments; i++)
      {
      bgav_mxf_index_table_segment_free(ret->index_segments[i]);
      free(ret->index_segments[i]);
      }
    free(ret->index_segments);
    }
  }

static void dump_partition(int indent, partition_t * p)
  {
  int i;
  
  bgav_diprintf(indent, "source packages:                          %d\n", p->num_source_packages);
  bgav_diprintf(indent, "material packages:                        %d\n", p->num_material_packages);
  bgav_diprintf(indent, "maximum components per source sequence:   %d\n", p->max_source_sequence_components);
  bgav_diprintf(indent, "maximum components per material sequence: %d\n", p->max_material_sequence_components);
  bgav_diprintf(indent, "start_pos:                                %"PRId64"\n", p->start_pos);
  bgav_diprintf(indent, "end_pos:                                  %"PRId64"\n", p->end_pos);
  
  bgav_mxf_partition_dump(2, &p->p);

  bgav_mxf_primer_pack_dump(2, &p->primer_pack);

  if(p->metadata)
    {
    for(i = 0; i < p->num_metadata; i++)
      {
      switch(p->metadata[i]->type)
        {
        case MXF_TYPE_MATERIAL_PACKAGE:
          gavl_dprintf("  Material");
          bgav_mxf_package_dump(2, (mxf_package_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_SOURCE_PACKAGE:
          gavl_dprintf("  Source");
          bgav_mxf_package_dump(2, (mxf_package_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_SOURCE_CLIP:
          bgav_mxf_source_clip_dump(2, (mxf_source_clip_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_TIMECODE_COMPONENT:
          bgav_mxf_timecode_component_dump(2, (mxf_timecode_component_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_PREFACE:
          bgav_mxf_preface_dump(2, (mxf_preface_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_CONTENT_STORAGE:
          bgav_mxf_content_storage_dump(2, (mxf_content_storage_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_SEQUENCE:
          bgav_mxf_sequence_dump(2, (mxf_sequence_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_MULTIPLE_DESCRIPTOR:
          bgav_mxf_descriptor_dump(2, (mxf_descriptor_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_DESCRIPTOR:
          bgav_mxf_descriptor_dump(2, (mxf_descriptor_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_TRACK:
          bgav_mxf_track_dump(2, (mxf_track_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_IDENTIFICATION:
          bgav_mxf_identification_dump(2, (mxf_identification_t*)(p->metadata[i]));
          break;
        case MXF_TYPE_ESSENCE_CONTAINER_DATA:
          bgav_mxf_essence_container_data_dump(2, (mxf_essence_container_data_t*)(p->metadata[i]));
          break;

        case MXF_TYPE_CRYPTO_CONTEXT:
          break;
        }
      }
    }
  }

void bgav_mxf_file_dump(mxf_file_t * ret)
  {
  int i;
  gavl_dprintf("\nMXF File structure\n"); 
  
  gavl_dprintf("Header\n");
  dump_partition(0, &ret->header);

  if(ret->num_index_segments)
    {
    for(i = 0; i < ret->num_index_segments; i++)
      {
      bgav_mxf_index_table_segment_dump(0, ret->index_segments[i]);
      }
    }

  gavl_dprintf("Body partitions: %d\n", ret->num_body_partitions);
  for(i = 0; i < ret->num_body_partitions; i++)
    dump_partition(0, &ret->body_partitions[i]);
  
  }

bgav_stream_t * bgav_mxf_find_stream(mxf_file_t * f, bgav_demuxer_context_t * t, mxf_ul_t ul)
  {
  uint32_t stream_id;
  if(!UL_MATCH(ul, mxf_essence_element_key))
    return NULL;

  if(((mxf_preface_t*)(f->header.preface))->operational_pattern == MXF_OP_ATOM)
    {
    int i;
    bgav_stream_t * ret = NULL;

    for(i = 0; i < t->tt->cur->num_streams; i++)
      {
      if(!(t->tt->cur->streams[i]->flags & STREAM_EXTERN) &&
         (ret->action != BGAV_STREAM_MUTE))
        return t->tt->cur->streams[i];
      }
    return NULL;
    }
  
  stream_id = ul[12] << 24 |
    ul[13] << 16 |
    ul[14] <<  8 |
    ul[15];
  return bgav_track_find_stream(t, stream_id);
  }


mxf_descriptor_t * bgav_mxf_get_source_descriptor(mxf_file_t * file, mxf_package_t * p, mxf_track_t * st)
  {
  int i;
  mxf_descriptor_t * desc;
  mxf_descriptor_t * sub_desc;
  if(!p->descriptor)
    {
    if((((mxf_preface_t *)(file->header.preface))->operational_pattern == MXF_OP_ATOM) &&
       (file->header.num_descriptors == 1))
      {
      for(i = 0; i < file->header.num_metadata; i++)
        {
        if(file->header.metadata[i]->type == MXF_TYPE_DESCRIPTOR)
          return (mxf_descriptor_t *)(file->header.metadata[i]);
        }
      }
    return NULL;
    }
  if(p->descriptor->type == MXF_TYPE_DESCRIPTOR)
    return (mxf_descriptor_t *)p->descriptor;
  else if(p->descriptor->type == MXF_TYPE_MULTIPLE_DESCRIPTOR)
    {
    desc = (mxf_descriptor_t*)(p->descriptor);
    for(i = 0; i < desc->num_subdescriptor_refs; i++)
      {
      sub_desc = (mxf_descriptor_t *)(desc->subdescriptors[i]);
      if(sub_desc && (sub_desc->linked_track_id == st->track_id))
        return sub_desc;
      } 
    }
  return NULL;
  }
