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

/* Basic stuff */
   
int bgav_mkv_read_id(bgav_input_context_t * ctx, int * ret);
int bgav_mkv_read_size(bgav_input_context_t * ctx, int64_t * ret);

/* EMBL element */

typedef struct
  {
  int id;
  int64_t size;
  int64_t end;
  } bgav_mkv_element_t;


int bgav_mkv_element_read(bgav_input_context_t * ctx, bgav_mkv_element_t * ret);
void bgav_mkv_element_dump(const bgav_mkv_element_t * ret);
void bgav_mkv_element_skip(bgav_input_context_t * ctx,
                           const bgav_mkv_element_t * el, const char * parent_name);

/* EMBL header */

typedef struct
  {
  int EBMLVersion;
  int EBMLReadVersion;
  int EBMLMaxIDLength;
  int EBMLMaxSizeLength;
  char * DocType;
  int DocTypeVersion;
  int DocTypeReadVersion;
  } bgav_mkv_ebml_header_t;

int bgav_mkv_ebml_header_read(bgav_input_context_t * ctx,
                              bgav_mkv_ebml_header_t * ret);
void bgav_mkv_ebml_header_dump(const bgav_mkv_ebml_header_t * h);
void bgav_mkv_ebml_header_free(bgav_mkv_ebml_header_t * h);

/* Meta seek info */

typedef struct
  {
  int num_entries;
  int entries_alloc;
  
  struct
    {
    int SeekID;
    uint64_t SeekPosition;
    } * entries;
  } bgav_mkv_meta_seek_info_t;

int bgav_mkv_meta_seek_info_read(bgav_input_context_t * ctx,
                                 bgav_mkv_meta_seek_info_t * info,
                                 bgav_mkv_element_t * parent);

void bgav_mkv_meta_seek_info_dump(const bgav_mkv_meta_seek_info_t * info);
void bgav_mkv_meta_seek_info_free(bgav_mkv_meta_seek_info_t * info);

/* Chapter display */

typedef struct
  {
  char * ChapString;
  char * ChapLanguage;
  char * ChapCountry;
  } bgav_mkv_chapter_display_t;

int bgav_mkv_chapter_display_read(bgav_input_context_t * ctx,
                                  bgav_mkv_chapter_display_t * ret,
                                  bgav_mkv_element_t * parent);
void bgav_mkv_chapter_display_dump(bgav_mkv_chapter_display_t * cd);
void bgav_mkv_chapter_display_free(bgav_mkv_chapter_display_t * cd);

/* Chapter track */

typedef struct
  {
  uint64_t ChapterTrackNumber;
  } bgav_mkv_chapter_track_t;

int bgav_mkv_chapter_track_read(bgav_input_context_t * ctx,
                                  bgav_mkv_chapter_track_t * ret,
                                  bgav_mkv_element_t * parent);
void bgav_mkv_chapter_track_dump(bgav_mkv_chapter_track_t * ct);
void bgav_mkv_chapter_track_free(bgav_mkv_chapter_track_t * ct);


/* Chapter Atom */

typedef struct
  {
  uint64_t ChapterUID;
  uint64_t ChapterTimeStart;
  uint64_t ChapterTimeEnd;
  int ChapterFlagHidden;
  int ChapterFlagEnabled;

  uint8_t * ChapterSegmentUID;
  int ChapterSegmentUIDLen;
  
  uint8_t * ChapterSegmentEditionUID;
  int ChapterSegmentEditionUIDLen;
  
  int num_tracks;
  bgav_mkv_chapter_track_t * tracks;
  
  int num_displays;
  bgav_mkv_chapter_display_t * displays;
  
  } bgav_mkv_chapter_atom_t;

int bgav_mkv_chapter_atom_read(bgav_input_context_t * ctx,
                               bgav_mkv_chapter_atom_t * ret,
                               bgav_mkv_element_t * parent);
void bgav_mkv_chapter_atom_dump(bgav_mkv_chapter_atom_t * ca);
void bgav_mkv_chapter_atom_free(bgav_mkv_chapter_atom_t * ca);


/* Edition entry */

typedef struct
  {
  uint64_t EditionUID;
  int EditionFlagHidden;
  int EditionFlagDefault;
  int EditionFlagOrdered;
  bgav_mkv_chapter_atom_t * atoms;
  int num_atoms;
  } bgav_mkv_edition_entry_t;

int bgav_mkv_edition_entry_read(bgav_input_context_t * ctx,
                               bgav_mkv_edition_entry_t * ret,
                               bgav_mkv_element_t * parent);
void bgav_mkv_edition_entry_dump(bgav_mkv_edition_entry_t * ca);
void bgav_mkv_edition_entry_free(bgav_mkv_edition_entry_t * ca);


/* Chapters */

typedef struct
  {
  /* TODO: Can there be more than one?? */
  bgav_mkv_edition_entry_t * editions;
  int num_editions;
  } bgav_mkv_chapters_t;

int bgav_mkv_chapters_read(bgav_input_context_t * ctx,
                           bgav_mkv_chapters_t * ret,
                           bgav_mkv_element_t * parent);

void bgav_mkv_chapters_dump(bgav_mkv_chapters_t * cd);
void bgav_mkv_chapters_free(bgav_mkv_chapters_t * cd);

/* Target */

typedef struct
  {
  int TargetTypeValue;
  char * TargetType;
  uint64_t * TagTrackUID;
  int num_TagTrackUID;
  
  uint64_t * TagEditionUID;
  int num_TagEditionUID;
  
  uint64_t * TagChapterUID;
  int num_TagChapterUID;
  
  uint64_t * TagAttachmentUID;
  int num_TagAttachmentUID;
  } bgav_mkv_targets_t;

int bgav_mkv_targets_read(bgav_input_context_t * ctx,
                         bgav_mkv_targets_t * ret,
                         bgav_mkv_element_t * parent);

void bgav_mkv_targets_dump(bgav_mkv_targets_t * t);
void bgav_mkv_targets_free(bgav_mkv_targets_t * t);

/* SimpleTag */

typedef struct
  {
  char * TagName;
  char * TagLanguage;
  int TagDefault;
  char * TagString;
  uint8_t * TagBinary;
  int TagBinaryLen;
  } bgav_mkv_simple_tag_t;

int bgav_mkv_simple_tag_read(bgav_input_context_t * ctx,
                             bgav_mkv_simple_tag_t * ret,
                             bgav_mkv_element_t * parent);

void bgav_mkv_simple_tag_dump(bgav_mkv_simple_tag_t * t);
void bgav_mkv_simple_tag_free(bgav_mkv_simple_tag_t * t);

/* Tag */

typedef struct
  {
  bgav_mkv_targets_t targets;
  
  bgav_mkv_simple_tag_t * st;
  int num_st;
  } bgav_mkv_tag_t;

int bgav_mkv_tag_read(bgav_input_context_t * ctx,
                         bgav_mkv_tag_t * ret,
                         bgav_mkv_element_t * parent);

void bgav_mkv_tag_dump(bgav_mkv_tag_t * t);
void bgav_mkv_tag_free(bgav_mkv_tag_t * t);

/* Tags */

int bgav_mkv_tags_read(bgav_input_context_t * ctx,
                       bgav_mkv_tag_t ** ret,
                       int * num_ret,
                       bgav_mkv_element_t * parent);

void bgav_mkv_tags_dump(bgav_mkv_tag_t * t, int num_tags);
void bgav_mkv_tags_free(bgav_mkv_tag_t * t, int num_tags);


/* SegmentInfo */

typedef struct
  {
  uint8_t SegmentUID[8];
  char *SegmentFilename;
  uint8_t PrevUID[8];
  char * PrevFilename;
  uint8_t NextUID[8];
  char * NextFilename;
  uint8_t SegmentFamily[8];

  /* TODO: Chapter translate */
  
  uint64_t TimecodeScale;
  double Duration;
  int64_t DateUTC;

  char * Title;
  char * MuxingApp;
  char * WritingApp;
  
  } bgav_mkv_segment_info_t;

int bgav_mkv_segment_info_read(bgav_input_context_t * ctx,
                               bgav_mkv_segment_info_t * ret,
                               bgav_mkv_element_t * parent);

void  bgav_mkv_segment_info_dump(const bgav_mkv_segment_info_t * si);
void  bgav_mkv_segment_info_free(bgav_mkv_segment_info_t * si);

/* Content Compression */

#define MKV_CONTENT_COMP_ALGO_ZLIB        0
#define MKV_CONTENT_COMP_ALGO_BZLIB       1
#define MKV_CONTENT_COMP_ALGO_LZO1X       2
#define MKV_CONTENT_COMP_ALGO_HEADER_STRIPPING 3

typedef struct
  {
  int ContentCompAlgo;

  uint8_t * ContentCompSettings;
  int ContentCompSettingsLen;
  
  } bgav_mkv_content_compression_t;

int bgav_mkv_content_compression_read(bgav_input_context_t * ctx,
                                      bgav_mkv_content_compression_t * ret,
                                      bgav_mkv_element_t * parent);
void bgav_mkv_content_compression_dump(bgav_mkv_content_compression_t * cc);
void bgav_mkv_content_compression_free(bgav_mkv_content_compression_t * cc);

/* Content Encryption */

#define MKV_CONTENT_ENC_ALGO_NONE             0 // Only signed
#define MKV_CONTENT_ENC_ALGO_DES              1
#define MKV_CONTENT_ENC_ALGO_3DES             2
#define MKV_CONTENT_ENC_ALGO_TWOFISH          3
#define MKV_CONTENT_ENC_ALGO_BLOWFISH         4
#define MKV_CONTENT_ENC_ALGO_AES              5

#define MKV_CONTENT_SIG_ALGO_NONE             0 // Only encrypted
#define MKV_CONTENT_SIG_ALGO_RSA              1

#define MKV_CONTENT_SIG_HASH_ALGO_NONE        0 // Only encrypted
#define MKV_CONTENT_SIG_HASH_ALGO_SHA1_160    1
#define MKV_CONTENT_SIG_HASH_ALGO_MD5         2

typedef struct
  {
  int ContentEncAlgo;
  uint8_t * ContentEncKeyID;
  int       ContentEncKeyIDLen;

  uint8_t * ContentSignature;
  int       ContentSignatureLen;

  uint8_t * ContentSigKeyID;
  int       ContentSigKeyIDLen;

  int ContentSigAlgo;
  int ContentSigHashAlgo;
  
  } bgav_mkv_content_encryption_t;

int bgav_mkv_content_encryption_read(bgav_input_context_t * ctx,
                                     bgav_mkv_content_encryption_t * ret,
                                     bgav_mkv_element_t * parent);
void bgav_mkv_content_encryption_dump(bgav_mkv_content_encryption_t * cc);
void bgav_mkv_content_encryption_free(bgav_mkv_content_encryption_t * cc);

/* Content Encoding */

#define MKV_CONTENT_ENCODING_COMPRESSION 0
#define MKV_CONTENT_ENCODING_ENCRYPTION  1

typedef struct
  {
  int ContentEncodingOrder;
  int ContentEncodingScope;
  int ContentEncodingType;

  bgav_mkv_content_compression_t ContentCompression;
  bgav_mkv_content_encryption_t  ContentEncryption;
  
  } bgav_mkv_content_encoding_t;

int bgav_mkv_content_encoding_read(bgav_input_context_t * ctx,
                                     bgav_mkv_content_encoding_t * ret,
                                     bgav_mkv_element_t * parent);
void bgav_mkv_content_encoding_dump(bgav_mkv_content_encoding_t * cc);
void bgav_mkv_content_encoding_free(bgav_mkv_content_encoding_t * cc);

int bgav_mkv_content_encodings_read(bgav_input_context_t * ctx,
                                    bgav_mkv_content_encoding_t ** ret,
                                    int * num_ret,
                                    bgav_mkv_element_t * parent);


/* Track */

#define MKV_FlagInterlaced (1<<0)

typedef struct
  {
  int flags;
  int StereoMode;
  int PixelWidth;
  int PixelHeight;
  int PixelCropBottom;
  int PixelCropTop;
  int PixelCropLeft;
  int PixelCropRight;
  int DisplayWidth;
  int DisplayHeight;
  int DisplayUnit;
  int AspectRatioType;
  uint8_t * ColourSpace;
  int ColourSpaceLen;
  double FrameRate;
  } bgav_mkv_track_video_t;

typedef struct
  {
  double SamplingFrequency;
  double OutputSamplingFrequency;
  int Channels;
  int BitDepth;
  } bgav_mkv_track_audio_t;

#define MKV_FlagEnabled (1<<0)
#define MKV_FlagDefault (1<<1)
#define MKV_FlagForced  (1<<2)
#define MKV_FlagLacing  (1<<3)

#define MKV_TRACK_VIDEO   1
#define MKV_TRACK_AUDIO   2
#define MKV_TRACK_COMPLEX 3
#define MKV_TRACK_LOGO     0x10
#define MKV_TRACK_SUBTITLE 0x11
#define MKV_TRACK_BUTTONS  0x12
#define MKV_TRACK_CONTROL  0x20

typedef struct
  {
  uint64_t TrackNumber;
  uint64_t TrackUID;
  int TrackType;
  int flags;
  uint64_t MinCache;
  uint64_t MaxCache;
  uint64_t DefaultDuration;
  double   TrackTimecodeScale;
  uint64_t MaxBlockAdditionID;
  char * Name;
  char * Language;
  char * CodecID;
  uint8_t * CodecPrivate;
  int CodecPrivateLen;
  char * CodecName;
  uint64_t AttachmentLink;
  int CodecDecodeAll;
  uint64_t TrackOverlay;
  /* TODO: TrackTranslate */

  uint64_t CodecDelay;
  uint64_t SeekPreroll;
  
  bgav_mkv_track_video_t video;
  bgav_mkv_track_audio_t audio;
  
  /* TODO: Content encodings */
  bgav_mkv_content_encoding_t * encodings;
  int num_encodings;
  
  /* For codecs with constant framesizes set this to enable sample
     accurate positioning */
  int frame_samples;
  
  int64_t pts; /* If we do our own pts calculation */
  } bgav_mkv_track_t;

int bgav_mkv_track_read(bgav_input_context_t * ctx,
                        bgav_mkv_track_t * ret,
                        bgav_mkv_element_t * parent);

void  bgav_mkv_track_dump(const bgav_mkv_track_t * t);
void  bgav_mkv_track_free(bgav_mkv_track_t * t);

int bgav_mkv_tracks_read(bgav_input_context_t * ctx,
                         bgav_mkv_track_t ** ret1,
                         int * ret_num1,
                         bgav_mkv_element_t * parent);

/* Cue points (== Packet index) */

typedef struct
  {
  uint64_t CueRefTime;
  } bgav_mkv_cue_reference_t;
  
typedef struct
  {
  uint64_t CueTrack;
  uint64_t CueClusterPosition;
  uint64_t CueRelativePosition;
  uint64_t CueBlockNumber;
  uint64_t CueCodecState;
  uint64_t CueDuration;
  
  int num_references;
  int references_alloc;
  bgav_mkv_cue_reference_t * references;
  } bgav_mkv_cue_track_t;

typedef struct
  {
  uint64_t CueTime;

  int num_tracks;
  bgav_mkv_cue_track_t * tracks;
  
  } bgav_mkv_cue_point_t;

typedef struct
  {
  int num_points;
  int points_alloc;
  bgav_mkv_cue_point_t * points;
  } bgav_mkv_cues_t;

int bgav_mkv_cues_read(bgav_input_context_t * ctx,
                       bgav_mkv_cues_t * ret, int num_tracks);

void bgav_mkv_cues_dump(const bgav_mkv_cues_t * cues);
void bgav_mkv_cues_free(bgav_mkv_cues_t * cues);

/* Cluster */

typedef struct
  {
  uint64_t Timecode;

  int num_silent_tracks;
  uint64_t * silent_tracks;

  uint64_t Position;
  uint64_t PrevSize;
  } bgav_mkv_cluster_t;

int bgav_mkv_cluster_read(bgav_input_context_t * ctx,
                          bgav_mkv_cluster_t * ret,
                          bgav_mkv_element_t * parent);

void bgav_mkv_cluster_dump(const bgav_mkv_cluster_t * cluster);
void bgav_mkv_cluster_free(bgav_mkv_cluster_t * cluster);

/* Block */

#define MKV_LACING_MASK  0x06
#define MKV_LACING_NONE  0x00
#define MKV_LACING_XIPH  0x02
#define MKV_LACING_EBML  0x06
#define MKV_LACING_FIXED 0x04
#define MKV_INVISIBLE    0x10
#define MKV_KEYFRAME     0x80
#define MKV_DISCARDABLE  0x01

#define MKV_LACING_MASK  0x06

typedef struct
  {
  int flags;
  int16_t timecode;
  int64_t track;
  int num_laces;
  
  int data_size;
  uint8_t * data;
  int data_alloc;
  } bgav_mkv_block_t;

int bgav_mkv_block_read(bgav_input_context_t * ctx,
                        bgav_mkv_block_t * ret,
                        bgav_mkv_element_t * parent);

void bgav_mkv_block_dump(int indent, bgav_mkv_block_t * b);
void bgav_mkv_block_free(bgav_mkv_block_t * b);

/* Block group */

typedef struct
  {
  uint64_t BlockDuration;
  int ReferencePriority;
  
  bgav_mkv_block_t block;
  
  int64_t * reference_blocks;
  int reference_blocks_alloc;
  int num_reference_blocks;
  } bgav_mkv_block_group_t;

int bgav_mkv_block_group_read(bgav_input_context_t * ctx,
                              bgav_mkv_block_group_t * ret,
                              bgav_mkv_element_t * parent);

void bgav_mkv_block_group_dump(bgav_mkv_block_group_t * g);
void bgav_mkv_block_group_free(bgav_mkv_block_group_t * g);


/* Known IDs */
#define MKV_ID_EBML                   0x1a45dfa3
#define MKV_ID_EBMLVersion            0x4286
#define MKV_ID_EBMLReadVersion        0x42f7
#define MKV_ID_EBMLMaxIDLength        0x42f2
#define MKV_ID_EBMLMaxSizeLength      0x42f3
#define MKV_ID_DocType                0x4282
#define MKV_ID_DocTypeVersion         0x4287
#define MKV_ID_DocTypeReadVersion     0x4285

#define MKV_ID_CRC32                  0xbf
#define MKV_ID_Void                   0xec

/* Segment */
#define MKV_ID_Segment                    0x18538067

/* Meta seek information */

#define MKV_ID_SeekHead                   0x114d9b74
#define MKV_ID_Seek                       0x4dbb
#define MKV_ID_SeekID                     0x53ab
#define MKV_ID_SeekPosition               0x53ac

/* Segment Info */

#define MKV_ID_Info                       0x1549a966
#define MKV_ID_SegmentUID                 0x73a4
#define MKV_ID_SegmentFilename            0x7384
#define MKV_ID_PrevUID                    0x3cb923
#define MKV_ID_PrevFilename               0x3c83ab
#define MKV_ID_NextUID                    0x3eb923
#define MKV_ID_NextFilename               0x3c83bb
#define MKV_ID_SegmentFamily              0x4444
#define MKV_ID_ChapterTranslate           0x6924
#define MKV_ID_ChapterTranslateEditionUID 0x69fc
#define MKV_ID_ChapterTranslateCodec      0x69bf
#define MKV_ID_ChapterTranslateID         0x69a5
#define MKV_ID_TimecodeScale              0x2ad7b1
#define MKV_ID_Duration                   0x4489
#define MKV_ID_DateUTC                    0x4461
#define MKV_ID_Title                      0x7ba9
#define MKV_ID_MuxingApp                  0x4d80
#define MKV_ID_WritingApp                 0x5741

/* Chapter */

#define MKV_ID_Chapters                   0x1043a770
#define MKV_ID_EditionEntry               0x45b9
#define MKV_ID_EditionUID                 0x45bc
#define MKV_ID_EditionFlagHidden          0x45bd
#define MKV_ID_EditionFlagDefault         0x45db
#define MKV_ID_EditionFlagOrdered         0x45dd
#define MKV_ID_ChapterAtom                0xb6
#define MKV_ID_ChapterUID                 0x73c4
#define MKV_ID_ChapterTimeStart           0x91
#define MKV_ID_ChapterTimeEnd             0x92
#define MKV_ID_ChapterFlagHidden          0x98
#define MKV_ID_ChapterFlagEnabled         0x4598
#define MKV_ID_ChapterSegmentUID          0x6e67
#define MKV_ID_ChapterSegmentEditionUID   0x6ebc
#define MKV_ID_ChapterTrack               0x8f
#define MKV_ID_ChapterTrackNumber         0x89
#define MKV_ID_ChapterDisplay             0x80
#define MKV_ID_ChapString                 0x85
#define MKV_ID_ChapLanguage               0x437c
#define MKV_ID_ChapCountry                0x437e
#define MKV_ID_ChapProcess                0x6944
#define MKV_ID_ChapProcessCodecID         0x6955
#define MKV_ID_ChapProcessPrivate         0x450d
#define MKV_ID_ChapProcessCommand         0x6911
#define MKV_ID_ChapProcessTime            0x6922
#define MKV_ID_ChapProcessData            0x6933


/* Tracks */

#define MKV_ID_Tracks                   0x1654AE6B
#define MKV_ID_TrackEntry               0xae
#define MKV_ID_TrackNumber              0xd7
#define MKV_ID_TrackUID                 0x73c5
#define MKV_ID_TrackType                0x83
#define MKV_ID_FlagEnabled              0xb9
#define MKV_ID_FlagDefault              0x88
#define MKV_ID_FlagForced               0x55aa
#define MKV_ID_FlagLacing               0x9c
#define MKV_ID_MinCache                 0x6de7
#define MKV_ID_MaxCache                 0x6df8
#define MKV_ID_DefaultDuration          0x23e383
#define MKV_ID_TrackTimecodeScale       0x23314f
#define MKV_ID_MaxBlockAdditionID       0x55ee
#define MKV_ID_Name                     0x536e
#define MKV_ID_Language                 0x22b59c
#define MKV_ID_CodecID                  0x86
#define MKV_ID_CodecPrivate             0x63a2
#define MKV_ID_CodecName                0x258688
#define MKV_ID_AttachmentLink           0x7446
#define MKV_ID_CodecDecodeAll           0xaa
#define MKV_ID_TrackOverlay             0x6fab
#define MKV_ID_TrackTranslate           0x6624
#define MKV_ID_TrackTranslateEditionUID 0x66fc
#define MKV_ID_TrackTranslateCodec      0x66bf
#define MKV_ID_TrackTranslateTrackID    0x66a5
#define MKV_ID_CodecDelay               0x56aa
#define MKV_ID_SeekPreRoll              0x56bb


/* Video specific */
#define MKV_ID_Video           0xe0
#define MKV_ID_FlagInterlaced  0x9a
#define MKV_ID_StereoMode      0x53b8
#define MKV_ID_PixelWidth      0xb0
#define MKV_ID_PixelHeight     0xba
#define MKV_ID_PixelCropBottom 0x54aa
#define MKV_ID_PixelCropTop    0x54bb
#define MKV_ID_PixelCropLeft   0x54cc
#define MKV_ID_PixelCropRight  0x54dd
#define MKV_ID_DisplayWidth    0x54b0
#define MKV_ID_DisplayHeight   0x54ba
#define MKV_ID_DisplayUnit     0x54b2
#define MKV_ID_AspectRatioType 0x54b3
#define MKV_ID_ColourSpace     0x2EB524
#define MKV_ID_FrameRate       0x2383E3

/* Audio specific */
#define MKV_ID_Audio                   0xe1
#define MKV_ID_SamplingFrequency       0xb5
#define MKV_ID_OutputSamplingFrequency 0x78b5
#define MKV_ID_Channels                0x9f
#define MKV_ID_BitDepth                0x6264

/* Encoding (== Compression/Encryption) */

#define MKV_ID_ContentEncodings        0x6d80
#define MKV_ID_ContentEncoding         0x6240
#define MKV_ID_ContentEncodingOrder    0x5031
#define MKV_ID_ContentEncodingScope    0x5032
#define MKV_ID_ContentEncodingType     0x5033
#define MKV_ID_ContentCompression      0x5034
#define MKV_ID_ContentCompAlgo         0x4254
#define MKV_ID_ContentCompSettings     0x4255
#define MKV_ID_ContentEncryption       0x5035
#define MKV_ID_ContentEncAlgo          0x47e1
#define MKV_ID_ContentEncKeyID         0x47e2
#define MKV_ID_ContentSignature        0x47e3
#define MKV_ID_ContentSigKeyID         0x47e4
#define MKV_ID_ContentSigAlgo          0x47e5
#define MKV_ID_ContentSigHashAlgo      0x47e6

/* Cueing Data */

#define MKV_ID_Cues                0x1C53BB6B
#define MKV_ID_CuePoint            0xbb
#define MKV_ID_CueTime             0xb3
#define MKV_ID_CueTrackPositions   0xb7
#define MKV_ID_CueTrack            0xf7
#define MKV_ID_CueClusterPosition  0xf1
#define MKV_ID_CueRelativePosition 0xf0
#define MKV_ID_CueDuration         0xb2
#define MKV_ID_CueBlockNumber      0x5378
#define MKV_ID_CueCodecState       0xea
#define MKV_ID_CueReference        0xdb
#define MKV_ID_CueRefTime          0x96

/* Cluster */

#define MKV_ID_Cluster           0x1f43b675
#define MKV_ID_Timecode          0xe7
#define MKV_ID_SilentTracks      0x5854
#define MKV_ID_SilentTrackNumber 0x58d7
#define MKV_ID_Position          0xa7
#define MKV_ID_PrevSize          0xab
#define MKV_ID_BlockGroup        0xa0


/* Block */

#define MKV_ID_Block             0xa1
#define MKV_ID_BlockAdditions    0x75A1
#define MKV_ID_BlockMore         0xa6
#define MKV_ID_BlockAddID        0xee
#define MKV_ID_BlockAdditional   0xa5
#define MKV_ID_BlockDuration     0x9b
#define MKV_ID_ReferencePriority 0xfa
#define MKV_ID_ReferenceBlock    0xfb
#define MKV_ID_CodecState        0xa4
#define MKV_ID_Slices            0x8e
#define MKV_ID_TimeSlice         0xe8
#define MKV_ID_LaceNumber        0xcc
#define MKV_ID_SimpleBlock       0xa3

/* Tagging */

#define MKV_ID_Tags              0x1254c367
#define MKV_ID_Tag               0x7373
#define MKV_ID_Targets           0x63c0
#define MKV_ID_TargetTypeValue   0x68ca
#define MKV_ID_TargetType        0x63ca
#define MKV_ID_TagTrackUID       0x63c5
#define MKV_ID_TagEditionUID     0x63c9
#define MKV_ID_TagChapterUID     0x63c4
#define MKV_ID_TagAttachmentUID  0x63c6
#define MKV_ID_SimpleTag         0x67c8
#define MKV_ID_TagName           0x45a3
#define MKV_ID_TagLanguage       0x447a
#define MKV_ID_TagDefault        0x4484
#define MKV_ID_TagString         0x4487
#define MKV_ID_TagBinary         0x4485


#if 0
#define MKV_ID_ 0x
#endif
