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

/* Based on the ffmpeg MXF demuxer with lots of changes */

typedef uint8_t mxf_ul_t[16];

typedef struct mxf_metadata_s mxf_metadata_t;
typedef struct mxf_descriptor_s mxf_descriptor_t;
typedef struct mxf_sequence_s mxf_sequence_t;
typedef struct mxf_track_s mxf_track_t;
typedef struct mxf_source_clip_s mxf_source_clip_t;
typedef struct mxf_timecode_component_s mxf_timecode_component_t;
typedef struct mxf_identification_s mxf_identification_t;
typedef struct mxf_essence_container_data_s mxf_essence_container_data_t;
typedef struct mxf_preface_s mxf_preface_t;
typedef struct mxf_content_storage_s mxf_content_storage_t;

typedef struct mxf_file_s mxf_file_t;

typedef struct partition_s partition_t;
typedef struct mxf_partition_s mxf_partition_t;

/* Move to the next startcode */
int bgav_mxf_sync(bgav_input_context_t * input);

typedef enum
  {
    WRAP_UNKNOWN = 0,
    WRAP_FRAME   = 1,
    WRAP_CLIP    = 2,
    WRAP_CUSTOM  = 3,
  } mxf_wrapping_type_t;

#if 1
typedef enum
  {
  MXF_OP_UNKNOWN = 0,
  MXF_OP_1a      = 1,
  MXF_OP_1b      = 2,
  MXF_OP_1c      = 3,
  MXF_OP_2a      = 4,
  MXF_OP_2b      = 5,
  MXF_OP_2c      = 6,
  MXF_OP_3a      = 7,
  MXF_OP_3b      = 8,
  MXF_OP_3c      = 9,
  MXF_OP_ATOM    = 10,
  } mxf_op_t;
#endif

/* Structures found in MXF files and functions to operate on them */

/* KLV */
typedef struct
  {
  mxf_ul_t key;
  uint64_t length;
  int64_t endpos;
  } mxf_klv_t;

int bgav_mxf_klv_read(bgav_input_context_t * input, mxf_klv_t * ret);
void bgav_mxf_klv_dump(int indent, mxf_klv_t * ret);

/* Partition */

struct mxf_partition_s
  {
  uint16_t major_version;
  uint16_t minor_version;
  uint32_t kag_size;
  uint64_t this_partition;
  uint64_t previous_partition;
  uint64_t footer_partition;
  uint64_t header_byte_count;
  uint64_t index_byte_count;
  uint32_t index_sid;
  uint64_t body_offset;
  uint32_t body_sid;
  uint8_t operational_pattern[16];

  uint32_t num_essence_container_types;
  mxf_ul_t * essence_container_types;
  
  };

int bgav_mxf_partition_read(bgav_input_context_t * input,
                            mxf_klv_t * parent,
                            mxf_partition_t * ret);
void bgav_mxf_partition_dump(int indent, mxf_partition_t * ret);

void bgav_mxf_partition_free(mxf_partition_t * ret);

/* Primer pack */

typedef struct
  {
  uint32_t num_entries;

  struct
    {
    uint16_t localTag;
    mxf_ul_t uid;
    } * entries;
  
  } mxf_primer_pack_t;

int bgav_mxf_primer_pack_read(bgav_input_context_t * input,
                              mxf_primer_pack_t * ret);

void bgav_mxf_primer_pack_dump(int indent, mxf_primer_pack_t * ret);
void bgav_mxf_primer_pack_free(mxf_primer_pack_t * ret);

/* Header Metadata */

typedef enum
  {
    MXF_TYPE_MATERIAL_PACKAGE       = (1<<0),
    MXF_TYPE_SOURCE_PACKAGE         = (1<<1),
    MXF_TYPE_SOURCE_CLIP            = (1<<2),
    MXF_TYPE_TIMECODE_COMPONENT     = (1<<3),
    MXF_TYPE_SEQUENCE               = (1<<4),
    MXF_TYPE_MULTIPLE_DESCRIPTOR    = (1<<5),
    MXF_TYPE_DESCRIPTOR             = (1<<6),
    MXF_TYPE_TRACK                  = (1<<7),
    MXF_TYPE_CRYPTO_CONTEXT         = (1<<8),
    MXF_TYPE_IDENTIFICATION         = (1<<9),
    MXF_TYPE_ESSENCE_CONTAINER_DATA = (1<<10),
    MXF_TYPE_CONTENT_STORAGE        = (1<<11),
    MXF_TYPE_PREFACE                = (1<<12)
  } mxf_metadata_type_t;

#define MXF_TYPE_ALL 0xFFFFFFFF


struct mxf_metadata_s
  {
  mxf_metadata_type_t type;
  mxf_ul_t uid;
  mxf_ul_t generation_ul;
  };

void bgav_mxf_metadata_dump_common(int indent, mxf_metadata_t * m);

/* Descriptor */

struct  mxf_descriptor_s
  {
  mxf_metadata_t common;

  mxf_ul_t essence_container_ul;
  mxf_ul_t essence_codec_ul;
  uint32_t sample_rate_num;
  uint32_t sample_rate_den;
  uint32_t aspect_ratio_num;
  uint32_t aspect_ratio_den;
  uint32_t width;
  uint32_t height;
  uint32_t sampled_width;
  uint32_t sampled_height;
  uint32_t display_width;
  uint32_t display_height;

  uint32_t sampled_x_offset;
  uint32_t sampled_y_offset;
  
  uint32_t channels;
  uint32_t bits_per_sample;
  uint32_t num_subdescriptor_refs;
  mxf_ul_t *subdescriptor_refs;
  uint32_t linked_track_id;
  uint8_t locked;

  /*
   * 0 FULL_FRAME:      frame consists of a full sample in progressive scan lines
   * 1 SEPARATE_FIELDS: sample consists of two fields, which when interlaced produce a full sample
   * 2 ONE_FIELD:       sample consists of two interlaced fields, but only one field is stored in
   *                    the data stream
   * 3 MIXED_FIELDS
   * 4 SEGMENTED FRAME
   */
  
  uint8_t frame_layout;
  /* The number of the field which is considered temporally to come first */
  uint8_t field_dominance;

  uint32_t video_line_map_size;
  uint32_t * video_line_map;

  uint32_t active_bits_per_sample;
  uint32_t horizontal_subsampling;
  uint32_t vertical_subsampling;

  uint64_t container_duration;
  uint16_t block_align;
  uint32_t avg_bps;
  
  /* MPEG-4 extradata (in the future maybe others too) */
  uint8_t *ext_data;
  int ext_size;

  /* Secondary */
  mxf_metadata_t ** subdescriptors;
  
  mxf_wrapping_type_t wrapping_type;
  uint32_t            fourcc;
  };

void bgav_mxf_descriptor_dump(int indent, mxf_descriptor_t * d);
void bgav_mxf_descriptor_free(mxf_descriptor_t * d);

/* Sequence */

struct mxf_sequence_s
  {
  mxf_metadata_t common;

  mxf_ul_t data_definition_ul;
  uint32_t num_structural_component_refs;
  mxf_ul_t *structural_component_refs;
  int64_t duration;

  /* Secondary */
  int is_timecode; /* stream_type will be GAVF_STREAM_NONE then */
  gavl_stream_type_t stream_type;
  mxf_metadata_t ** structural_components;
  };

void bgav_mxf_sequence_dump(int indent, mxf_sequence_t * s);
void bgav_mxf_sequence_free(mxf_sequence_t * s);

/* Track */

struct mxf_track_s
  {
  mxf_metadata_t common;
  mxf_ul_t sequence_ref;
  uint32_t track_id;
  uint8_t track_number[4];
  
  uint32_t edit_rate_num;
  uint32_t edit_rate_den;
  uint64_t origin;
  char * name;
  
  /* Secondary */
  mxf_metadata_t * sequence; /* mandatory, and only one */
  
  int num_packets;         /* If it's a source track: Number of klv packets found for this track */
  int64_t max_packet_size; /* Maximum size of a klv packet */
  };

void bgav_mxf_track_dump(int indent, mxf_track_t * t);
void bgav_mxf_track_free(mxf_track_t * t);

/* Package */

typedef struct
  {
  mxf_metadata_t common;

  mxf_ul_t * track_refs;
  uint32_t num_track_refs;
  
  mxf_ul_t package_ul;
  mxf_ul_t descriptor_ref;

  uint64_t creation_date;
  uint64_t modification_date;
  char * generic_name;
  /* Secondary */
  mxf_metadata_t * descriptor;
  mxf_metadata_t ** tracks;

  /* Tertiary */
  int num_audio_tracks;
  int num_video_tracks;
  int num_timecode_tracks;
  mxf_metadata_t * timecode_track;

  } mxf_package_t;

void bgav_mxf_package_dump(int indent, mxf_package_t * p);
void bgav_mxf_package_free(mxf_package_t * p);

/* Structural component */

struct mxf_source_clip_s
  {
  mxf_metadata_t common;
  mxf_ul_t source_package_ref;
  mxf_ul_t data_definition_ul;
  int64_t duration;
  int64_t start_position;
  uint32_t source_track_id;
  /* Secondary */
  mxf_metadata_t    * source_package;
  //  mxf_track_t      * source_track;
  //  mxf_descriptor_t * source_descriptor;
  };

void bgav_mxf_source_clip_dump(int indent,
                                        mxf_source_clip_t * s);
void bgav_mxf_source_clip_free(mxf_source_clip_t * s);

/* Timecode component */

struct mxf_timecode_component_s
  {
  mxf_metadata_t common;
  mxf_ul_t data_definition_ul;
  uint16_t rounded_timecode_base;
  uint64_t start_timecode;
  uint64_t duration;
  uint8_t drop_frame;
  };

void bgav_mxf_timecode_component_dump(int indent,
                                      mxf_timecode_component_t * s);
void bgav_mxf_timecode_component_free(mxf_timecode_component_t * s);

/* Identification */

typedef struct
  {
  uint16_t maj;
  uint16_t min;
  uint16_t tweak;
  uint16_t build;
  uint16_t rel;
  } mxf_version_t;

struct mxf_identification_s
  {
  mxf_metadata_t common;
  mxf_ul_t this_generation_ul;
  char * company_name;
  char * product_name;
  char * version_string;
  char * platform;
  mxf_ul_t product_ul;
  uint64_t modification_date;
  
  mxf_version_t product_version;
  mxf_version_t toolkit_version;
  };

void bgav_mxf_identification_dump(int indent, mxf_identification_t * s);
void bgav_mxf_identification_free(mxf_identification_t * s);

/* Essence container data */

struct mxf_essence_container_data_s
  {
  mxf_metadata_t common;
  mxf_ul_t linked_package_ref;
  uint32_t index_sid;
  uint32_t body_sid;

  mxf_metadata_t * linked_package;
  
  };

void bgav_mxf_essence_container_data_dump(int indent, mxf_essence_container_data_t * s);
void bgav_mxf_essence_container_data_free(mxf_essence_container_data_t * s);

/* Preface */

struct mxf_preface_s
  {
  mxf_metadata_t common;
  uint64_t last_modified_date;
  uint16_t version;

  mxf_ul_t * identification_refs;
  uint32_t num_identification_refs;

  mxf_ul_t content_storage_ref;
  mxf_ul_t operational_pattern_ul;

  mxf_ul_t * essence_container_types;
  uint32_t num_essence_container_types;

  mxf_ul_t * dm_schemes;
  uint32_t num_dm_schemes;

  mxf_ul_t primary_package_ul;

  uint32_t object_model_version;
  
  /* Secondary */
  mxf_metadata_t * content_storage;
  mxf_metadata_t ** identifications;
  
  mxf_op_t operational_pattern;
  };

void bgav_mxf_preface_dump(int indent, mxf_preface_t * s);
void bgav_mxf_preface_free(mxf_preface_t * s);

/* Content storage */

struct mxf_content_storage_s
  {
  mxf_metadata_t common;
  
  uint32_t num_package_refs;
  mxf_ul_t * package_refs;

  uint32_t num_essence_container_data_refs;
  mxf_ul_t * essence_container_data_refs;
  
  mxf_metadata_t ** packages;
  mxf_metadata_t ** essence_containers;
  };

void bgav_mxf_content_storage_dump(int indent, mxf_content_storage_t * s);
void bgav_mxf_content_storage_free(mxf_content_storage_t * s);


/* Index table segment */

typedef struct 
  {
  mxf_ul_t uid;
  uint32_t edit_rate_num;
  uint32_t edit_rate_den;
  uint64_t start_position;
  uint64_t duration;
  uint32_t edit_unit_byte_count;
  uint32_t index_sid;
  uint32_t body_sid;
  uint8_t slice_count;

  struct
    {
    uint8_t pos_table_index;
    uint8_t slice;
    uint32_t element_delta;
    } * delta_entries;
  uint32_t num_delta_entries;

  struct
    {
    int8_t temporal_offset;
    int8_t anchor_offset;
    uint8_t flags;
    uint64_t offset;
    } * entries;
  uint32_t num_entries;
  
  } mxf_index_table_segment_t;

void bgav_mxf_index_table_segment_dump(int indent, mxf_index_table_segment_t * idx);
void bgav_mxf_index_table_segment_free(mxf_index_table_segment_t * idx);

/* Toplevel file structure */

struct partition_s
  {
  mxf_partition_t p;
  mxf_primer_pack_t primer_pack;
  mxf_metadata_t ** metadata;
  int num_metadata;
  
  /* Preface is the parent of all metadata contained in the below array */
  mxf_metadata_t * preface; 

  /* Secondary variables */
  int num_source_packages;
  int num_material_packages;
  int num_descriptors;
  
  int max_source_sequence_components;
  int max_material_sequence_components;

  int64_t start_pos;
  int64_t end_pos;
  
  };

struct mxf_file_s
  {
  
  partition_t header;

  /* Index table segments */
  mxf_index_table_segment_t ** index_segments;
  int num_index_segments;

  partition_t * body_partitions;
  int num_body_partitions;
  
  int64_t data_start;
  };

int bgav_mxf_file_read(bgav_input_context_t * input,
                       mxf_file_t * ret);

void bgav_mxf_file_dump(mxf_file_t * ret);
void bgav_mxf_file_free(mxf_file_t * ret);

uint32_t bgav_mxf_get_audio_fourcc(mxf_descriptor_t * d);
uint32_t bgav_mxf_get_video_fourcc(mxf_descriptor_t * d);

bgav_stream_t * bgav_mxf_find_stream(mxf_file_t * f,
                                     bgav_demuxer_context_t * t,
                                     mxf_ul_t ul);

mxf_descriptor_t * bgav_mxf_get_source_descriptor(mxf_file_t * file,
                                                  mxf_package_t * p, mxf_track_t * st);
