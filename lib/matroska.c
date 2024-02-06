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
#include <matroska.h>

#define LOG_DOMAIN "matroska"

#define MY_FREE(ptr) \
  if(ptr) \
    free(ptr);


static int mkv_read_num(bgav_input_context_t * ctx, int64_t * ret, int do_mask)
  {
  int shift = 0;

  uint8_t byte;
  uint8_t mask = 0x80;

  /* Get first byte */
  if(!bgav_input_read_8(ctx, &byte))
    return 0;

  while(!(mask & byte) && mask)
    {
    mask >>= 1;
    shift++;
    }

  if(!mask)
    return 0;

  *ret = byte;

  if(do_mask)
    *ret &= (0xff >> (shift+1));
  
  while(shift--)
    {
    if(!bgav_input_read_8(ctx, &byte))
      return 0;

    *ret <<= 8;
    *ret |= byte;
    }
  return 1;
  }

static int mkv_read_uint(bgav_input_context_t * ctx, uint64_t * ret, int bytes)
  {
  uint8_t byte;
  *ret = 0;

  while(bytes--)
    {
    if(!bgav_input_read_8(ctx, &byte))
      return 0;
    *ret <<= 8;
    *ret |= byte;
    }
  return 1;
  }

int64_t vsint_subtr [] =
  {
    0x3FLL,
    0x1FFFLL,
    0x0FFFFFLL,
    0x07FFFFFFLL,
    0x03FFFFFFFFLL,
    0x01FFFFFFFFFFLL,
    0x00FFFFFFFFFFFFLL,
    0x007FFFFFFFFFFFFFLL
  };

static int mkv_read_int(bgav_input_context_t * ctx, int64_t * ret, int bytes)
  {
  uint64_t ret_u;
  if(!mkv_read_uint(ctx, &ret_u, bytes))
    return 0;
  *ret -= vsint_subtr[bytes-1];
  return 1;
  }


static int mkv_read_uint_small(bgav_input_context_t * ctx, int * ret, int bytes)
  {
  uint64_t val;
  if(!mkv_read_uint(ctx, &val, bytes))
    return 0;
  *ret = val;
  return 1;
  }

static int mkv_read_flag(bgav_input_context_t * ctx, int * ret, int flag, int bytes)
  {
  uint64_t val;
  if(!mkv_read_uint(ctx, &val, bytes))
    return 0;

  if(val)
    *ret |= flag;

  return 1;
  }
  
static int mkv_read_string(bgav_input_context_t * ctx, char ** ret, int bytes)
  {
  *ret = calloc(bytes+1, 1);
  if(bgav_input_read_data(ctx, (uint8_t*)(*ret), bytes) < bytes)
    return 0;
  return 1;
  }

static int mkv_read_binary(bgav_input_context_t * ctx, uint8_t ** ret,
                           int * ret_len, int bytes)
  {
  *ret = malloc(bytes);
  if(bgav_input_read_data(ctx, *ret, bytes) < bytes)
    return 0;
  *ret_len = bytes;
  return 1;
  }


static int mkv_read_uid(bgav_input_context_t * ctx, uint8_t * ret, int bytes)
  {
  int bytes_to_read = bytes < 8 ? bytes : 8;
  
  if(bgav_input_read_data(ctx, ret, bytes_to_read) < bytes_to_read)
    return 0;

  if(bytes > bytes_to_read)
    bgav_input_skip(ctx, bytes - bytes_to_read);
  
  return 1;
  }

static int mkv_read_float(bgav_input_context_t * ctx, double * ret, int bytes)
  {
  int result;
  float flt;
  
  switch(bytes)
    {
    case 4:
      result = bgav_input_read_float_32_be(ctx, &flt);
      if(result)
        *ret = flt;
      return result;
      break;
    case 8:
      return bgav_input_read_double_64_be(ctx, ret);
      break;
    }
  return 0;
  }

static void mkv_dump_uid(const uint8_t * uid)
  {
  gavl_dprintf("%02x %02x %02x %02x %02x %02x %02x %02x",
               uid[0], uid[1], uid[2], uid[3],
               uid[4], uid[5], uid[6], uid[7]);
  }

int bgav_mkv_read_size(bgav_input_context_t * ctx, int64_t * ret)
  {
  return mkv_read_num(ctx, ret, 1);
  }

int bgav_mkv_read_id(bgav_input_context_t * ctx, int * ret)
  {
  int64_t ret1;
  if(!mkv_read_num(ctx, &ret1, 0))
    return 0;
  *ret = ret1;
  return 1;
  }
  
int bgav_mkv_element_read(bgav_input_context_t * ctx, bgav_mkv_element_t * ret)
  {
  if(!bgav_mkv_read_id(ctx, &ret->id) ||
     !bgav_mkv_read_size(ctx, &ret->size))
    return 0;
  ret->end = ctx->position + ret->size;
  return 1;
  }

void bgav_mkv_element_dump(const bgav_mkv_element_t * ret)
  {
  gavl_dprintf("Matroska element\n");
  gavl_dprintf("  ID:   %x\n", ret->id);
  gavl_dprintf("  Size: %"PRId64"\n", ret->size);
  gavl_dprintf("  End:  %"PRId64"\n", ret->end);
  }

void bgav_mkv_element_skip(bgav_input_context_t * ctx,
                           const bgav_mkv_element_t * el, const char * parent_name)
  {
  if((el->id != MKV_ID_Void) &&
     (el->id != MKV_ID_CRC32))
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "Skipping %"PRId64" (%"PRIx64") bytes of element %x in %s\n",
             el->size, el->size, el->id, parent_name);
    }
  bgav_input_skip(ctx, el->size);
  }

int bgav_mkv_ebml_header_read(bgav_input_context_t * ctx,
                              bgav_mkv_ebml_header_t * ret)
  {
  bgav_mkv_element_t e;
  bgav_mkv_element_t e1;
  uint64_t tmp_64;

  // Set defaults
  ret->EBMLVersion      = 1;
  ret->EBMLReadVersion = 1;
  ret->EBMLMaxIDLength     = 4;
  ret->EBMLMaxSizeLength   = 8;
  
  ret->DocTypeVersion      = 1;
  ret->DocTypeReadVersion = 1;

  /* Read stuff */

  if(!bgav_mkv_element_read(ctx, &e))
    return 0;

  if(e.id != MKV_ID_EBML)
    return 0;

  //  bgav_mkv_element_dump(&e);

  while(ctx->position < e.end)
    {
    if(!bgav_mkv_element_read(ctx, &e1))
      return 0;

    switch(e1.id)
      {
      case MKV_ID_EBMLVersion:
        if(!mkv_read_uint(ctx, &tmp_64, e1.size))
          return 0;
        ret->EBMLVersion = tmp_64;
        break;
      case MKV_ID_EBMLReadVersion:
        if(!mkv_read_uint(ctx, &tmp_64, e1.size))
          return 0;
        ret->EBMLReadVersion = tmp_64;
        break;
      case MKV_ID_EBMLMaxIDLength:
        if(!mkv_read_uint(ctx, &tmp_64, e1.size))
          return 0;
        ret->EBMLMaxIDLength = tmp_64;
        break;
      case MKV_ID_EBMLMaxSizeLength:
        if(!mkv_read_uint(ctx, &tmp_64, e1.size))
          return 0;
        ret->EBMLMaxSizeLength = tmp_64;
        break;
      case MKV_ID_DocType:
        if(!mkv_read_string(ctx, &ret->DocType, e1.size)) 
          return 0;
        break;
      case MKV_ID_DocTypeVersion:
        if(!mkv_read_uint(ctx, &tmp_64, e1.size))
          return 0;
        ret->DocTypeVersion = tmp_64;
        break;
      case MKV_ID_DocTypeReadVersion:
        if(!mkv_read_uint(ctx, &tmp_64, e1.size))
          return 0;
        ret->DocTypeReadVersion = tmp_64;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e1, "ebml_header");
        break;
      }
    }
  return 1;
  }

void bgav_mkv_ebml_header_dump(const bgav_mkv_ebml_header_t * ret)
  {
  gavl_dprintf("EBML Header\n");
  gavl_dprintf("  EBMLVersion:        %d\n", ret->EBMLVersion);
  gavl_dprintf("  EBMLReadVersion:    %d\n", ret->EBMLReadVersion);
  gavl_dprintf("  EBMLMaxIDLength:    %d\n", ret->EBMLMaxIDLength);
  gavl_dprintf("  EBMLMaxSizeLength:  %d\n", ret->EBMLMaxSizeLength);
  gavl_dprintf("  DocType:            %s\n", ret->DocType);
  gavl_dprintf("  DocTypeVersion:     %d\n", ret->DocTypeVersion);
  gavl_dprintf("  DocTypeReadVersion: %d\n", ret->DocTypeReadVersion);
  
  }

void bgav_mkv_ebml_header_free(bgav_mkv_ebml_header_t * h)
  {
  MY_FREE(h->DocType);
  }

/* Meta seek info */

int bgav_mkv_meta_seek_info_read(bgav_input_context_t * ctx,
                                 bgav_mkv_meta_seek_info_t * info,
                                 bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  bgav_mkv_element_t e1;
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_Seek:
        if(info->num_entries+1 > info->entries_alloc)
          {
          info->entries_alloc = info->num_entries + 10;
          info->entries = realloc(info->entries,
                                 sizeof(*info->entries) * info->entries_alloc);
          memset(info->entries + info->num_entries, 0,
                 (info->entries_alloc - info->num_entries) * sizeof(*info->entries));
          }
        while(ctx->position < e.end)
          {
          if(!bgav_mkv_element_read(ctx, &e1))
            return 0;
          
          switch(e1.id)
            {
            case MKV_ID_SeekID:
              if(!bgav_mkv_read_id(ctx, &info->entries[info->num_entries].SeekID))
                return 0;
              break;
            case MKV_ID_SeekPosition:
              if(!mkv_read_uint(ctx, &info->entries[info->num_entries].SeekPosition, e1.size))
                return 0;
              break;
            default:
              bgav_mkv_element_skip(ctx, &e1, "meta_seek");
              break;
            }
          }
        info->num_entries++;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "meta_seek");
      }
    }
  return 1;
  }

void bgav_mkv_meta_seek_info_dump(const bgav_mkv_meta_seek_info_t * info)
  {
  int i;
  gavl_dprintf("Meta seek information (%d entries)\n", info->num_entries);
  for(i = 0; i < info->num_entries; i++)
    {
    gavl_dprintf("  Entry:\n");
    gavl_dprintf("    ID: %x\n", info->entries[i].SeekID);
    gavl_dprintf("    Position: %"PRId64"\n", info->entries[i].SeekPosition);
    }
  }

void bgav_mkv_meta_seek_info_free(bgav_mkv_meta_seek_info_t * info)
  {
  MY_FREE(info->entries);
  }

/* Chapter display */

int bgav_mkv_chapter_display_read(bgav_input_context_t * ctx,
                                  bgav_mkv_chapter_display_t * ret,
                                  bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_ChapString:
        if(!mkv_read_string(ctx, &ret->ChapString, e.size)) 
          return 0;
        break;
      case MKV_ID_ChapLanguage:
        if(!mkv_read_string(ctx, &ret->ChapLanguage, e.size)) 
          return 0;
        break;
      case MKV_ID_ChapCountry:
        if(!mkv_read_string(ctx, &ret->ChapCountry, e.size)) 
          return 0;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "chapter_display");
        break;
      }
    }
  return 1; 
  }

void bgav_mkv_chapter_display_dump(bgav_mkv_chapter_display_t * cd)
  {
  gavl_dprintf("      ChapterDisplay:\n");
  gavl_dprintf("        ChapString:    %s\n", cd->ChapString);
  gavl_dprintf("        ChapLanguage:  %s\n", cd->ChapLanguage);
  gavl_dprintf("        ChapCountry:   %s\n", cd->ChapCountry);
  }

void bgav_mkv_chapter_display_free(bgav_mkv_chapter_display_t * cd)
  {
  MY_FREE(cd->ChapString);
  MY_FREE(cd->ChapLanguage);
  MY_FREE(cd->ChapCountry);
  }

/* Chapter Track */

int bgav_mkv_chapter_track_read(bgav_input_context_t * ctx,
                                  bgav_mkv_chapter_track_t * ret,
                                  bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_ChapterTrackNumber:
        if(!mkv_read_uint(ctx, &ret->ChapterTrackNumber, e.size))
          return 0;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "chapter_track");
        break;
      }
    }
  return 1;
  }

void bgav_mkv_chapter_track_dump(bgav_mkv_chapter_track_t * ct)
  {
  gavl_dprintf("      ChapterTrack:\n");
  gavl_dprintf("        Chapter track: %"PRId64"\n", ct->ChapterTrackNumber);
  }

void bgav_mkv_chapter_track_free(bgav_mkv_chapter_track_t * ct)
  {
  
  }


/* Chapter Atom */

int bgav_mkv_chapter_atom_read(bgav_input_context_t * ctx,
                               bgav_mkv_chapter_atom_t * ret,
                               bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_ChapterUID:
        if(!mkv_read_uint(ctx, &ret->ChapterUID, e.size))
          return 0;
        break;
      case MKV_ID_ChapterTimeStart:
        if(!mkv_read_uint(ctx, &ret->ChapterTimeStart, e.size))
          return 0;
        break;
      case MKV_ID_ChapterTimeEnd:
        if(!mkv_read_uint(ctx, &ret->ChapterTimeEnd, e.size))
          return 0;
        break;
      case MKV_ID_ChapterFlagHidden:
        if(!mkv_read_uint_small(ctx, &ret->ChapterFlagHidden, e.size))
          return 0;
        break;
      case MKV_ID_ChapterFlagEnabled:
        if(!mkv_read_uint_small(ctx, &ret->ChapterFlagEnabled, e.size))
          return 0;
        break;
      case MKV_ID_ChapterSegmentUID:
        if(!mkv_read_binary(ctx, &ret->ChapterSegmentUID,
                            &ret->ChapterSegmentUIDLen, e.size))
          return 0;
        break;
      case MKV_ID_ChapterSegmentEditionUID:
        if(!mkv_read_binary(ctx, &ret->ChapterSegmentEditionUID,
                            &ret->ChapterSegmentEditionUIDLen, e.size))
          return 0;
        break;
      case MKV_ID_ChapterTrack:
        ret->tracks = realloc(ret->tracks, (ret->num_tracks+1)*sizeof(*ret->tracks));
        memset(ret->tracks + ret->num_tracks, 0, sizeof(*ret->tracks));
        if(!bgav_mkv_chapter_track_read(ctx, ret->tracks + ret->num_tracks, &e))
          return 0;
        ret->num_tracks++;
        break;
      case MKV_ID_ChapterDisplay:
        ret->displays = realloc(ret->displays, (ret->num_displays+1)*sizeof(*ret->displays));
        memset(ret->displays + ret->num_displays, 0, sizeof(*ret->displays));
        if(!bgav_mkv_chapter_display_read(ctx, ret->displays + ret->num_displays, &e))
          return 0;
        ret->num_displays++;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "chapter_atom");
        break;
      }
    }
  return 1;
  }

void bgav_mkv_chapter_atom_dump(bgav_mkv_chapter_atom_t * ca)
  {
  int i;
  gavl_dprintf("    ChapterAtom:\n");
  gavl_dprintf("      ChapterUID:           %"PRId64"\n", ca->ChapterUID);
  gavl_dprintf("      ChapterTimeStart:     %"PRId64"\n", ca->ChapterTimeStart);
  gavl_dprintf("      ChapterTimeEnd:       %"PRId64"\n", ca->ChapterTimeEnd);
  gavl_dprintf("      ChapterFlagHidden:    %d\n", ca->ChapterFlagHidden);
  gavl_dprintf("      ChapterFlagEnabled:   %d\n", ca->ChapterFlagEnabled);
  gavl_dprintf("      ChapterSegmentUIDLen: %d\n", ca->ChapterSegmentUIDLen);
  if(ca->ChapterSegmentUIDLen)
    gavl_hexdump(ca->ChapterSegmentUID, ca->ChapterSegmentUIDLen, 16);
  gavl_dprintf("      ChapterSegmentEditionLen: %d\n", ca->ChapterSegmentEditionUIDLen);
  if(ca->ChapterSegmentEditionUIDLen)
    gavl_hexdump(ca->ChapterSegmentEditionUID, ca->ChapterSegmentEditionUIDLen, 16);

  for(i = 0; i < ca->num_tracks; i++)
    bgav_mkv_chapter_track_dump(ca->tracks+i);
  for(i = 0; i < ca->num_displays; i++)
    bgav_mkv_chapter_display_dump(ca->displays+i);
  }

void bgav_mkv_chapter_atom_free(bgav_mkv_chapter_atom_t * ca)
  {
  int i;
  MY_FREE(ca->ChapterSegmentUID);
  MY_FREE(ca->ChapterSegmentEditionUID);

  if(ca->tracks)
    {
    for(i = 0; i < ca->num_tracks; i++)
      bgav_mkv_chapter_track_free(&ca->tracks[i]);
    free(ca->tracks);
    }
  if(ca->displays)
    {
    for(i = 0; i < ca->num_displays; i++)
      bgav_mkv_chapter_display_free(&ca->displays[i]);
    free(ca->displays);
    }
  }

/* Edition entry */

int bgav_mkv_edition_entry_read(bgav_input_context_t * ctx,
                               bgav_mkv_edition_entry_t * ret,
                               bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_EditionUID:
        if(!mkv_read_uint(ctx, &ret->EditionUID, e.size))
          return 0;
        break;
      case MKV_ID_EditionFlagHidden:
        if(!mkv_read_uint_small(ctx, &ret->EditionFlagHidden, e.size))
          return 0;
        break;
      case MKV_ID_EditionFlagDefault:
        if(!mkv_read_uint_small(ctx, &ret->EditionFlagDefault, e.size))
          return 0;
        break;
      case MKV_ID_EditionFlagOrdered:
        if(!mkv_read_uint_small(ctx, &ret->EditionFlagOrdered, e.size))
          return 0;
        break;
      case MKV_ID_ChapterAtom:
        ret->atoms = realloc(ret->atoms, (ret->num_atoms+1)*sizeof(*ret->atoms));
        memset(ret->atoms + ret->num_atoms, 0, sizeof(*ret->atoms));
        if(!bgav_mkv_chapter_atom_read(ctx, ret->atoms + ret->num_atoms, &e))
          return 0;
        ret->num_atoms++;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "edition_entry");
        break;
      }
    }
  return 1; 
  
  }

void bgav_mkv_edition_entry_dump(bgav_mkv_edition_entry_t * ee)
  {
  int i;
  gavl_dprintf("  EditionEntry:\n");
  gavl_dprintf("    EditionUID: %"PRId64"\n", ee->EditionUID);
  gavl_dprintf("    EditionFlagHidden: %d\n", ee->EditionFlagHidden);
  gavl_dprintf("    EditionFlagDefault: %d\n", ee->EditionFlagDefault);
  gavl_dprintf("    EditionFlagOrdered: %d\n", ee->EditionFlagOrdered);

  for(i = 0; i < ee->num_atoms; i++)
    bgav_mkv_chapter_atom_dump(&ee->atoms[i]);
  }

void bgav_mkv_edition_entry_free(bgav_mkv_edition_entry_t * ee)
  {
  int i;
  if(ee->atoms)
    {
    for(i = 0; i < ee->num_atoms; i++)
      bgav_mkv_chapter_atom_free(&ee->atoms[i]);
    free(ee->atoms);
    }
  }


/* Chapters */

int bgav_mkv_chapters_read(bgav_input_context_t * ctx,
                           bgav_mkv_chapters_t * ret,
                           bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_EditionEntry:
        ret->editions = realloc(ret->editions, (ret->num_editions+1)* sizeof(*ret->editions));
        memset(ret->editions + ret->num_editions, 0, sizeof(*ret->editions));
        if(!bgav_mkv_edition_entry_read(ctx, ret->editions + ret->num_editions, &e))
          return 0;
        ret->num_editions++;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "chapters");
        break;
      }
    }
  return 1; 
  }

void bgav_mkv_chapters_dump(bgav_mkv_chapters_t * cd)
  {
  int i;
  gavl_dprintf("Chapters:\n");
  
  for(i = 0; i < cd->num_editions; i++)
    bgav_mkv_edition_entry_dump(&cd->editions[i]);
  }

void bgav_mkv_chapters_free(bgav_mkv_chapters_t * cd)
  {
  int i;
  if(cd->editions)
    {
    for(i = 0; i < cd->num_editions; i++)
      bgav_mkv_edition_entry_free(&cd->editions[i]);
    free(cd->editions);
    }
  
  }

/* Target */

int bgav_mkv_targets_read(bgav_input_context_t * ctx,
                         bgav_mkv_targets_t * ret,
                         bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_TargetTypeValue:   // 0x68ca
        if(!mkv_read_uint_small(ctx, &ret->TargetTypeValue, e.size))
          return 0;
        break;
      case MKV_ID_TargetType:        // 0x63ca
        if(!mkv_read_string(ctx, &ret->TargetType, e.size))
          return 0;
        break;
      case MKV_ID_TagTrackUID:       // 0x63c5
        ret->TagTrackUID =
          realloc(ret->TagTrackUID,
                  (ret->num_TagTrackUID+1)*sizeof(*ret->TagTrackUID));
        if(!mkv_read_uint(ctx, ret->TagTrackUID + ret->num_TagTrackUID, e.size))
          return 0;
        ret->num_TagTrackUID++;
        break;
      case MKV_ID_TagEditionUID:     // 0x63c9
        ret->TagEditionUID =
          realloc(ret->TagEditionUID,
                  (ret->num_TagEditionUID+1)*sizeof(*ret->TagEditionUID));
        if(!mkv_read_uint(ctx, ret->TagEditionUID + ret->num_TagEditionUID, e.size))
          return 0;
        ret->num_TagEditionUID++;
        break;
      case MKV_ID_TagChapterUID:     // 0x63c4
        ret->TagChapterUID =
          realloc(ret->TagChapterUID,
                  (ret->num_TagChapterUID+1)*sizeof(*ret->TagChapterUID));
        if(!mkv_read_uint(ctx, ret->TagChapterUID + ret->num_TagChapterUID, e.size))
          return 0;
        ret->num_TagChapterUID++;
        break;
      case MKV_ID_TagAttachmentUID:  // 0x63c6
        ret->TagAttachmentUID =
          realloc(ret->TagAttachmentUID,
                  (ret->num_TagAttachmentUID+1)*sizeof(*ret->TagAttachmentUID));
        if(!mkv_read_uint(ctx, ret->TagAttachmentUID + ret->num_TagAttachmentUID, e.size))
          return 0;
        ret->num_TagAttachmentUID++;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "target");
        break;
      }
    }
  return 1;
  }

void bgav_mkv_targets_dump(bgav_mkv_targets_t * t)
  {
  int i;
  gavl_dprintf("  Targets\n");
  gavl_dprintf("    TargetTypeValue:  %d\n",        t->TargetTypeValue);
  gavl_dprintf("    TargetType:       %s\n",        t->TargetType);
  for(i = 0; i < t->num_TagTrackUID; i++)
    gavl_dprintf("    TagTrackUID:      %"PRId64"\n", t->TagTrackUID[i]);
  for(i = 0; i < t->num_TagEditionUID; i++)
    gavl_dprintf("    TagEditionUID:    %"PRId64"\n", t->TagEditionUID[i]);
  for(i = 0; i < t->num_TagChapterUID; i++)
    gavl_dprintf("    TagChapterUID:    %"PRId64"\n", t->TagChapterUID[i]);
  for(i = 0; i < t->num_TagAttachmentUID; i++)
    gavl_dprintf("    TagAttachmentUID: %"PRId64"\n", t->TagAttachmentUID[i]);
  }

void bgav_mkv_targets_free(bgav_mkv_targets_t * t)
  {
  MY_FREE(t->TargetType);
  MY_FREE(t->TagTrackUID);
  MY_FREE(t->TagEditionUID);
  MY_FREE(t->TagChapterUID);
  MY_FREE(t->TagAttachmentUID);
  }

/* Simple tag */

int bgav_mkv_simple_tag_read(bgav_input_context_t * ctx,
                             bgav_mkv_simple_tag_t * ret,
                             bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;
    
    switch(e.id)
      {
      case MKV_ID_TagName:
        if(!mkv_read_string(ctx, &ret->TagName, e.size))
          return 0;
        break;
      case MKV_ID_TagLanguage:
        if(!mkv_read_string(ctx, &ret->TagLanguage, e.size))
          return 0;
        break;
      case MKV_ID_TagDefault:
        if(!mkv_read_uint_small(ctx, &ret->TagDefault, e.size))
          return 0;
        break;
      case MKV_ID_TagString:
        if(!mkv_read_string(ctx, &ret->TagString, e.size))
          return 0;
        break;
      case MKV_ID_TagBinary:
        if(!mkv_read_binary(ctx, &ret->TagBinary, &ret->TagBinaryLen, e.size))
          return 0;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "simple_tag");
        break;
      }
    }

  return 1;
  }

void bgav_mkv_simple_tag_dump(bgav_mkv_simple_tag_t * t)
  {
  gavl_dprintf("  SimpleTag\n");
  gavl_dprintf("    TagName:      %s\n", t->TagName);
  gavl_dprintf("    TagLanguage:  %s\n", t->TagLanguage);
  gavl_dprintf("    TagDefault:   %d\n", t->TagDefault);
  gavl_dprintf("    TagString:    %s\n", t->TagString);
  gavl_dprintf("    TagBinaryLen: %d\n", t->TagBinaryLen);
  if(t->TagBinaryLen)
    gavl_hexdump(t->TagBinary, t->TagBinaryLen, 16);
  }

void bgav_mkv_simple_tag_free(bgav_mkv_simple_tag_t * t)
  {
  MY_FREE(t->TagName);
  MY_FREE(t->TagLanguage);
  MY_FREE(t->TagString);
  MY_FREE(t->TagBinary);
  }

/* Tag */

int bgav_mkv_tag_read(bgav_input_context_t * ctx,
                      bgav_mkv_tag_t * ret,
                      bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;
    
    switch(e.id)
      {
      case MKV_ID_SimpleTag:
        ret->st = realloc(ret->st, (ret->num_st + 1)*sizeof(*ret->st));
        memset(ret->st + ret->num_st, 0, sizeof(*ret->st));
        if(!bgav_mkv_simple_tag_read(ctx, ret->st + ret->num_st, &e))
          return 0;
        ret->num_st++;
        break;
      case MKV_ID_Targets:
        if(!bgav_mkv_targets_read(ctx, &ret->targets, &e))
          return 0;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "tag");
        break;
      }
    }
  return 1;
  }

void bgav_mkv_tag_dump(bgav_mkv_tag_t * t)
  {
  int i;
  gavl_dprintf("Tag:\n");
  
  bgav_mkv_targets_dump(&t->targets);
  
  for(i = 0; i < t->num_st; i++)
    {
    bgav_mkv_simple_tag_dump(t->st + i);
    }
  
  }

void bgav_mkv_tag_free(bgav_mkv_tag_t * t)
  {
  int i;

  for(i = 0; i < t->num_st; i++)
    bgav_mkv_simple_tag_free(t->st + i);
  MY_FREE(t->st);
  bgav_mkv_targets_free(&t->targets);
  }

/* Tags */

int bgav_mkv_tags_read(bgav_input_context_t * ctx,
                       bgav_mkv_tag_t ** ret,
                       int * num_ret,
                       bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;
    switch(e.id)
      {
      case MKV_ID_Tag:
        *ret = realloc(*ret, ((*num_ret)+1)*sizeof(**ret));
        memset(*ret + *num_ret, 0, sizeof(**ret));
        if(!bgav_mkv_tag_read(ctx, *ret + *num_ret, &e))
          return 0;
        (*num_ret)++;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "tags");
        break;
      }
    
    }
  return 1;
  }

void bgav_mkv_tags_dump(bgav_mkv_tag_t * t, int num_tags)
  {
  int i;
  for(i = 0; i < num_tags; i++)
    bgav_mkv_tag_dump(t+i);
  }

void bgav_mkv_tags_free(bgav_mkv_tag_t * t, int num_tags)
  {
  int i;
  for(i = 0; i < num_tags; i++)
    bgav_mkv_tag_free(t+i);
  MY_FREE(t);
  }

/* Segment info */

int bgav_mkv_segment_info_read(bgav_input_context_t * ctx,
                               bgav_mkv_segment_info_t * ret,
                               bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  ret->TimecodeScale = 1000000;
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_SegmentUID:
        if(!mkv_read_uid(ctx, ret->SegmentUID, e.size)) 
          return 0;
        break;
      case MKV_ID_SegmentFilename:
        if(!mkv_read_string(ctx, &ret->SegmentFilename, e.size)) 
          return 0;
        break;
      case MKV_ID_PrevUID:
        if(!mkv_read_uid(ctx, ret->PrevUID, e.size)) 
          return 0;
        break;
      case MKV_ID_PrevFilename:
        if(!mkv_read_string(ctx, &ret->PrevFilename, e.size)) 
          return 0;
        break;
      case MKV_ID_NextUID:
        if(!mkv_read_uid(ctx, ret->NextUID, e.size)) 
          return 0;
        break;
      case MKV_ID_NextFilename:
        if(!mkv_read_string(ctx, &ret->NextFilename, e.size)) 
          return 0;
        break;
      case MKV_ID_SegmentFamily:
        if(!mkv_read_uid(ctx, ret->SegmentFamily, e.size)) 
          return 0;
        break;
#if 0
      case MKV_ID_ChapterTranslate:
        break;
      case MKV_ID_ChapterTranslateEditionUID:
        break;
      case MKV_ID_ChapterTranslateCodec:
        break;
      case MKV_ID_ChapterTranslateID:
        break;
#endif
      case MKV_ID_TimecodeScale:
        if(!mkv_read_uint(ctx, &ret->TimecodeScale, e.size))
          return 0;
        break;
      case MKV_ID_Duration:
        if(!mkv_read_float(ctx, &ret->Duration, e.size))
          return 0;
        break;
      case MKV_ID_DateUTC:
        if(!bgav_input_read_64_be(ctx, (uint64_t*)(&ret->DateUTC)))
          return 0;
        break;
      case MKV_ID_Title:
        if(!mkv_read_string(ctx, &ret->Title, e.size)) 
          return 0;
        break;
      case MKV_ID_MuxingApp:
        if(!mkv_read_string(ctx, &ret->MuxingApp, e.size)) 
          return 0;
        break;
      case MKV_ID_WritingApp:
        if(!mkv_read_string(ctx, &ret->WritingApp, e.size)) 
          return 0;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "segment_info");
        break;
      }
    }
  return 1;
  }

void  bgav_mkv_segment_info_dump(const bgav_mkv_segment_info_t * si)
  {
  gavl_dprintf("SegmentInfo:\n");

  gavl_dprintf("  SegmentUID:      ");
  mkv_dump_uid(si->SegmentUID);
  gavl_dprintf("\n");

  gavl_dprintf("  SegmentFilename: %s\n", si->SegmentFilename);

  gavl_dprintf("  PrevUID:         ");
  mkv_dump_uid(si->PrevUID);
  gavl_dprintf("\n");
  gavl_dprintf("  PrevFilename:    %s\n", si->PrevFilename);
  
  gavl_dprintf("  NextUID:         ");
  mkv_dump_uid(si->NextUID);
  gavl_dprintf("\n");
  gavl_dprintf("  NextFilename:    %s\n", si->NextFilename);

  gavl_dprintf("  SegmentFamily:   ");
  mkv_dump_uid(si->SegmentFamily);
  gavl_dprintf("\n");
  
  /* TODO: Chapter translate */

  gavl_dprintf("  TimecodeScale:   %"PRId64"\n", si->TimecodeScale);
  gavl_dprintf("  Duration:        %f\n", si->Duration);
  gavl_dprintf("  DateUTC:         %"PRId64"\n", si->DateUTC);
  gavl_dprintf("  Title:           %s\n", si->Title);
  gavl_dprintf("  MuxingApp:       %s\n", si->MuxingApp);
  gavl_dprintf("  WritingApp:      %s\n", si->WritingApp);
  }

void  bgav_mkv_segment_info_free(bgav_mkv_segment_info_t * si)
  {
  MY_FREE(si->SegmentFilename);
  MY_FREE(si->PrevFilename);
  MY_FREE(si->NextFilename);
  MY_FREE(si->Title);
  MY_FREE(si->MuxingApp);
  MY_FREE(si->WritingApp);
  }

/* ContentCompression */

int bgav_mkv_content_compression_read(bgav_input_context_t * ctx,
                                      bgav_mkv_content_compression_t * ret,
                                      bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_ContentCompAlgo:
        if(!mkv_read_uint_small(ctx, &ret->ContentCompAlgo, e.size))
          return 0;
        break;
      case MKV_ID_ContentCompSettings:
        if(!mkv_read_binary(ctx, &ret->ContentCompSettings,
                            &ret->ContentCompSettingsLen, e.size))
          return 0;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "compression");
        break;
      }
    }
  return 1;
  }

void bgav_mkv_content_compression_dump(bgav_mkv_content_compression_t * cc)
  {
  gavl_dprintf("    ContentCompression:\n");
  gavl_dprintf("      ContentCompAlgo: %d ", cc->ContentCompAlgo);
  switch(cc->ContentCompAlgo)
    {
    case MKV_CONTENT_COMP_ALGO_ZLIB:
      gavl_dprintf("(zlib)");
      break;
    case MKV_CONTENT_COMP_ALGO_BZLIB:
      gavl_dprintf("(bzlib)");
      break;
    case MKV_CONTENT_COMP_ALGO_LZO1X:
      gavl_dprintf("(lzo1x)");
      break;
    case MKV_CONTENT_COMP_ALGO_HEADER_STRIPPING:
      gavl_dprintf("(header stripping)");
      break;
    default:
      gavl_dprintf("(unknown)");
      break;
    }
  gavl_dprintf("\n");

  gavl_dprintf("      ContentCompSettingsLen: %d\n", cc->ContentCompSettingsLen);
  
  if(cc->ContentCompSettingsLen)
    gavl_hexdump(cc->ContentCompSettings, cc->ContentCompSettingsLen, 16);
  }
  

void bgav_mkv_content_compression_free(bgav_mkv_content_compression_t * cc)
  {
  MY_FREE(cc->ContentCompSettings);
  }


/* ContentEncryption */

int bgav_mkv_content_encryption_read(bgav_input_context_t * ctx,
                                      bgav_mkv_content_encryption_t * ret,
                                      bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_ContentEncAlgo:
        if(!mkv_read_uint_small(ctx, &ret->ContentEncAlgo, e.size))
          return 0;
        break;
      case MKV_ID_ContentEncKeyID:
        if(!mkv_read_binary(ctx, &ret->ContentEncKeyID,
                            &ret->ContentEncKeyIDLen, e.size))
          return 0;
        break;
      case MKV_ID_ContentSignature:
        if(!mkv_read_binary(ctx, &ret->ContentSignature,
                            &ret->ContentSignatureLen, e.size))
          return 0;
        break;
      case MKV_ID_ContentSigKeyID:
        if(!mkv_read_binary(ctx, &ret->ContentSigKeyID,
                            &ret->ContentSigKeyIDLen, e.size))
          return 0;
        break;
      case MKV_ID_ContentSigAlgo:
        if(!mkv_read_uint_small(ctx, &ret->ContentSigAlgo, e.size))
          return 0;
        break;
      case MKV_ID_ContentSigHashAlgo:
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "encryption");
        break;
      }
    }
  return 1;
  }

void bgav_mkv_content_encryption_dump(bgav_mkv_content_encryption_t * ce)
  {
  gavl_dprintf("    ContentEncryption:\n");
  gavl_dprintf("      ContentEncAlgo: %d ", ce->ContentEncAlgo);
  switch(ce->ContentEncAlgo)
    {
    case MKV_CONTENT_ENC_ALGO_NONE: // Only signed
      gavl_dprintf("(sig only)");
      break;
    case MKV_CONTENT_ENC_ALGO_DES:
      gavl_dprintf("(des)");
      break;
    case MKV_CONTENT_ENC_ALGO_3DES:
      gavl_dprintf("(3des)");
      break;
    case MKV_CONTENT_ENC_ALGO_TWOFISH:
      gavl_dprintf("(twofish)");
      break;
    case MKV_CONTENT_ENC_ALGO_BLOWFISH:
      gavl_dprintf("(blowfish)");
      break;
    case MKV_CONTENT_ENC_ALGO_AES:
      gavl_dprintf("(aes)");
      break;
    default:
      gavl_dprintf("(unknown)");
      break;
    }
  gavl_dprintf("\n");

  gavl_dprintf("      ContentEncKeyIDLen: %d\n", ce->ContentEncKeyIDLen);
  if(ce->ContentEncKeyIDLen)
    gavl_hexdump(ce->ContentEncKeyID, ce->ContentEncKeyIDLen, 16);

  gavl_dprintf("      ContentSigAlgo: %d ", ce->ContentSigAlgo);
  switch(ce->ContentSigAlgo)
    {
    case MKV_CONTENT_SIG_ALGO_NONE: // Only encrypted
      gavl_dprintf("(enc only)");
      break;
    case MKV_CONTENT_SIG_ALGO_RSA:
      gavl_dprintf("(rsa)");
      break;
    default:
      gavl_dprintf("(unknown)");
      break;
    }
  gavl_dprintf("\n");

  gavl_dprintf("      ContentSigHashAlgo: %d ", ce->ContentSigHashAlgo);
  switch(ce->ContentSigHashAlgo)
    {
    case MKV_CONTENT_SIG_HASH_ALGO_NONE:     // Only encrypted
      gavl_dprintf("(enc only)");
      break;
    case MKV_CONTENT_SIG_HASH_ALGO_SHA1_160:
      gavl_dprintf("(sha1-160)");
      break;
    case MKV_CONTENT_SIG_HASH_ALGO_MD5:
      gavl_dprintf("(md5)");
      break;
    }
  gavl_dprintf("\n");
  
  gavl_dprintf("      ContentSignatureLen: %d\n", ce->ContentSignatureLen);
  if(ce->ContentSignatureLen)
    gavl_hexdump(ce->ContentSignature, ce->ContentSignatureLen, 16);

  gavl_dprintf("      ContentSigKeyIDLen: %d\n", ce->ContentSigKeyIDLen);
  if(ce->ContentSigKeyIDLen)
    gavl_hexdump(ce->ContentSigKeyID, ce->ContentSigKeyIDLen, 16);

  
  
  }

void bgav_mkv_content_encryption_free(bgav_mkv_content_encryption_t * ce)
  {
  MY_FREE(ce->ContentEncKeyID);
  MY_FREE(ce->ContentSignature);
  MY_FREE(ce->ContentSigKeyID);
  }

/* ContentEncoding */

int bgav_mkv_content_encoding_read(bgav_input_context_t * ctx,
                                   bgav_mkv_content_encoding_t * ret,
                                   bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_ContentEncodingOrder:
        if(!mkv_read_uint_small(ctx, &ret->ContentEncodingOrder, e.size))
          return 0;
        break;
      case MKV_ID_ContentEncodingScope:
        if(!mkv_read_uint_small(ctx, &ret->ContentEncodingScope, e.size))
          return 0;
        break;
      case MKV_ID_ContentEncodingType:
        if(!mkv_read_uint_small(ctx, &ret->ContentEncodingType, e.size))
          return 0;
        break;
      case MKV_ID_ContentCompression:
        if(!bgav_mkv_content_compression_read(ctx, &ret->ContentCompression, &e))
          return 0;
      case MKV_ID_ContentEncryption:
        if(!bgav_mkv_content_encryption_read(ctx, &ret->ContentEncryption, &e))
          return 0;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "content_encoding");
        break;
        
      }
    }
  return 1;
  }

void bgav_mkv_content_encoding_dump(bgav_mkv_content_encoding_t * ce)
  {
  gavl_dprintf("  ContentEncoding:\n");
  gavl_dprintf("    ContentEncodingOrder: %d\n", ce->ContentEncodingOrder);
  gavl_dprintf("    ContentEncodingScope: %d\n", ce->ContentEncodingScope);
  gavl_dprintf("    ContentEncodingType:  %d ", ce->ContentEncodingType);

  switch(ce->ContentEncodingType)
    {
    case MKV_CONTENT_ENCODING_COMPRESSION:
      gavl_dprintf("(compression)\n");
      bgav_mkv_content_compression_dump(&ce->ContentCompression);
      break;
    case MKV_CONTENT_ENCODING_ENCRYPTION:
      gavl_dprintf("(encryption)\n");
      bgav_mkv_content_encryption_dump(&ce->ContentEncryption);
      break;
    }
  }

void bgav_mkv_content_encoding_free(bgav_mkv_content_encoding_t * ce)
  {
  bgav_mkv_content_compression_free(&ce->ContentCompression);
  bgav_mkv_content_encryption_free(&ce->ContentEncryption);
  }

int bgav_mkv_content_encodings_read(bgav_input_context_t * ctx,
                                    bgav_mkv_content_encoding_t ** ret,
                                    int * num_ret,
                                    bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;
    switch(e.id)
      {
      case MKV_ID_ContentEncoding:
        *ret = realloc(*ret, ((*num_ret)+1) * sizeof(**ret));
        memset(*ret + *num_ret, 0, sizeof(**ret));
        if(!bgav_mkv_content_encoding_read(ctx, *ret + *num_ret, &e))
          return 0;
        (*num_ret)++;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "content_encodings");
        break;
      }
    }
  return 1;
  }



/* Track */

static int track_read_video(bgav_input_context_t * ctx,
                            bgav_mkv_track_video_t * ret,
                            bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_FlagInterlaced:
        if(!mkv_read_flag(ctx, &ret->flags, MKV_FlagInterlaced, e.size))
          return 0;
        break;
      case MKV_ID_StereoMode:
        if(!mkv_read_uint_small(ctx, &ret->StereoMode, e.size))
          return 0;
        break;
      case MKV_ID_PixelWidth:
        if(!mkv_read_uint_small(ctx, &ret->PixelWidth, e.size))
          return 0;
        break;
      case MKV_ID_PixelHeight:
        if(!mkv_read_uint_small(ctx, &ret->PixelHeight, e.size))
          return 0;
        break;
      case MKV_ID_PixelCropBottom:
        if(!mkv_read_uint_small(ctx, &ret->PixelCropBottom, e.size))
          return 0;
        break;
      case MKV_ID_PixelCropTop:
        if(!mkv_read_uint_small(ctx, &ret->PixelCropTop, e.size))
          return 0;
        break;
      case MKV_ID_PixelCropLeft:
        if(!mkv_read_uint_small(ctx, &ret->PixelCropLeft, e.size))
          return 0;
        break;
      case MKV_ID_PixelCropRight:
        if(!mkv_read_uint_small(ctx, &ret->PixelCropRight, e.size))
          return 0;
        break;
      case MKV_ID_DisplayWidth:
        if(!mkv_read_uint_small(ctx, &ret->DisplayWidth, e.size))
          return 0;
        break;
      case MKV_ID_DisplayHeight:
        if(!mkv_read_uint_small(ctx, &ret->DisplayHeight, e.size))
          return 0;
        break;
      case MKV_ID_DisplayUnit:
        if(!mkv_read_uint_small(ctx, &ret->DisplayUnit, e.size))
          return 0;
        break;
      case MKV_ID_AspectRatioType:
        if(!mkv_read_uint_small(ctx, &ret->AspectRatioType, e.size))
          return 0;
        break;
      case MKV_ID_ColourSpace:
        if(!mkv_read_binary(ctx, &ret->ColourSpace, &ret->ColourSpaceLen, e.size))
          return 0;
        break;
      case MKV_ID_FrameRate:
        if(!mkv_read_float(ctx, &ret->FrameRate, e.size))
          return 0;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "video");
        break;
      }
    }
  return 1;
  }

static int track_read_audio(bgav_input_context_t * ctx,
                            bgav_mkv_track_audio_t * ret,
                            bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_SamplingFrequency:
        if(!mkv_read_float(ctx, &ret->SamplingFrequency, e.size))
          return 0;
        break;
      case MKV_ID_OutputSamplingFrequency:
        if(!mkv_read_float(ctx, &ret->OutputSamplingFrequency, e.size))
          return 0;
        break;
      case MKV_ID_Channels:
        if(!mkv_read_uint_small(ctx, &ret->Channels, e.size))
          return 0;
        break;
      case MKV_ID_BitDepth:
        if(!mkv_read_uint_small(ctx, &ret->BitDepth, e.size))
          return 0;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "audio");
        break;
      }
    }
  return 1;
  }


int bgav_mkv_track_read(bgav_input_context_t * ctx,
                        bgav_mkv_track_t * ret,
                        bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_TrackNumber:
        if(!mkv_read_uint(ctx, &ret->TrackNumber, e.size))
          return 0;
        break;
      case MKV_ID_TrackUID:
        if(!mkv_read_uint(ctx, &ret->TrackUID, e.size))
          return 0;
        break;
      case MKV_ID_TrackType:
        if(!mkv_read_uint_small(ctx, &ret->TrackType, e.size))
          return 0;
        break;
      case MKV_ID_FlagEnabled:
        if(!mkv_read_flag(ctx, &ret->flags, MKV_FlagEnabled, e.size))
          return 0;
        break;
      case MKV_ID_FlagDefault:
        if(!mkv_read_flag(ctx, &ret->flags, MKV_FlagDefault, e.size))
          return 0;
        break;
      case MKV_ID_FlagForced:
        if(!mkv_read_flag(ctx, &ret->flags, MKV_FlagForced, e.size))
          return 0;
        break;
      case MKV_ID_FlagLacing:
        if(!mkv_read_flag(ctx, &ret->flags, MKV_FlagLacing, e.size))
          return 0;
        break;
      case MKV_ID_MinCache:
        if(!mkv_read_uint(ctx, &ret->MinCache, e.size))
          return 0;
        break;
      case MKV_ID_MaxCache:
        if(!mkv_read_uint(ctx, &ret->MaxCache, e.size))
          return 0;
        break;
      case MKV_ID_DefaultDuration:
        if(!mkv_read_uint(ctx, &ret->DefaultDuration, e.size))
          return 0;
        break;
      case MKV_ID_TrackTimecodeScale:
        if(!mkv_read_float(ctx, &ret->TrackTimecodeScale, e.size))
          return 0;
        break;
      case MKV_ID_MaxBlockAdditionID:
        if(!mkv_read_uint(ctx, &ret->MaxBlockAdditionID, e.size))
          return 0;
        break;
      case MKV_ID_Name:
        if(!mkv_read_string(ctx, &ret->Name, e.size)) 
          return 0;
        break;
      case MKV_ID_Language:
        if(!mkv_read_string(ctx, &ret->Language, e.size)) 
          return 0;
        break;
      case MKV_ID_CodecID:
        if(!mkv_read_string(ctx, &ret->CodecID, e.size)) 
          return 0;
        break;
      case MKV_ID_CodecPrivate:
        if(!mkv_read_binary(ctx, &ret->CodecPrivate,
                            &ret->CodecPrivateLen, e.size))
          return 0;
        break;
      case MKV_ID_CodecName:
        if(!mkv_read_string(ctx, &ret->CodecName, e.size)) 
          return 0;
        break;
      case MKV_ID_AttachmentLink:
        if(!mkv_read_uint(ctx, &ret->AttachmentLink, e.size))
          return 0;
        break;
      case MKV_ID_CodecDecodeAll:
        if(!mkv_read_uint_small(ctx, &ret->CodecDecodeAll, e.size))
          return 0;
        break;
      case MKV_ID_TrackOverlay:
        if(!mkv_read_uint(ctx, &ret->TrackOverlay, e.size))
          return 0;
        break;
      case MKV_ID_CodecDelay:
        if(!mkv_read_uint(ctx, &ret->CodecDelay, e.size))
          return 0;
        break;
      case MKV_ID_SeekPreRoll:
        if(!mkv_read_uint(ctx, &ret->SeekPreroll, e.size))
          return 0;
        break;
#if 0
      case MKV_ID_TrackTranslate:
        break;
      case MKV_ID_TrackTranslateEditionUID:
        break;
      case MKV_ID_TrackTranslateCodec:
        break;
      case MKV_ID_TrackTranslateTrackID:
        break;
#endif
      case MKV_ID_Video:
        if(!track_read_video(ctx, &ret->video, &e))
          return 0;
        break;
      case MKV_ID_Audio:
        if(!track_read_audio(ctx, &ret->audio, &e))
          return 0;
        break;
      case MKV_ID_ContentEncodings:
        if(!bgav_mkv_content_encodings_read(ctx,
                                            &ret->encodings,
                                            &ret->num_encodings,
                                            &e))
          return 0;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "track");
        break;
      
      }
    }
  return 1;
  }

static void track_dump_audio(const bgav_mkv_track_audio_t * a)
  {
  gavl_dprintf("  Audio\n");
  gavl_dprintf("    SamplingFrequency:       %f\n", a->SamplingFrequency);
  gavl_dprintf("    OutputSamplingFrequency: %f\n", a->OutputSamplingFrequency);
  gavl_dprintf("    Channels:                %d\n", a->Channels);
  gavl_dprintf("    BitDepth:                %d\n", a->BitDepth);
  }

static void track_dump_video(const bgav_mkv_track_video_t * v)
  {
  gavl_dprintf("  Video\n");
  gavl_dprintf("    flags:           %d\n", v->flags);
  gavl_dprintf("    StereoMode:      %d\n", v->StereoMode);
  gavl_dprintf("    PixelWidth:      %d\n", v->PixelWidth);
  gavl_dprintf("    PixelHeight:     %d\n", v->PixelHeight );
  gavl_dprintf("    PixelCropBottom: %d\n", v->PixelCropBottom);
  gavl_dprintf("    PixelCropTop:    %d\n", v->PixelCropTop );
  gavl_dprintf("    PixelCropLeft:   %d\n", v->PixelCropLeft );
  gavl_dprintf("    PixelCropRight:  %d\n", v->PixelCropRight );
  gavl_dprintf("    DisplayWidth:    %d\n", v->DisplayWidth );
  gavl_dprintf("    DisplayHeight:   %d\n", v->DisplayHeight );
  gavl_dprintf("    DisplayUnit:     %d\n", v->DisplayUnit );
  gavl_dprintf("    AspectRatioType: %d\n", v->AspectRatioType );
  gavl_dprintf("    ColourSpace:     %d bytes\n", v->ColourSpaceLen );
  gavl_dprintf("    FrameRate:       %f\n", v->FrameRate);
  }

void  bgav_mkv_track_dump(const bgav_mkv_track_t * t)
  {
  int i;
  gavl_dprintf("Matroska track\n");
  gavl_dprintf("  TrackNumber:        %"PRId64"\n", t->TrackNumber);
  gavl_dprintf("  TrackUID:           %"PRId64"\n", t->TrackUID);
  gavl_dprintf("  TrackType:          %d ",        t->TrackType);
  switch(t->TrackType)
    {
    case MKV_TRACK_VIDEO:
      gavl_dprintf("(video)\n");
      break;
    case MKV_TRACK_AUDIO:
      gavl_dprintf("(audio)\n");
      break;
    case MKV_TRACK_COMPLEX:
      gavl_dprintf("(complex)\n");
      break;
    case MKV_TRACK_LOGO:
      gavl_dprintf("(logo)\n");
      break;
    case MKV_TRACK_SUBTITLE:
      gavl_dprintf("(subtitle)\n");
      break;
    case MKV_TRACK_BUTTONS:
      gavl_dprintf("(buttons)\n");
      break;
    case MKV_TRACK_CONTROL:
      gavl_dprintf("(control)\n");
      break;
    default:
      gavl_dprintf("(unknown)\n");
      break;
    }
  
  gavl_dprintf("  flags:              %x\n", t->flags);
  gavl_dprintf("  MinCache:           %"PRId64"\n", t->MinCache);
  gavl_dprintf("  MaxCache:           %"PRId64"\n", t->MaxCache);
  gavl_dprintf("  DefaultDuration:    %"PRId64"\n", t->DefaultDuration);
  gavl_dprintf("  TrackTimecodeScale: %f\n", t->TrackTimecodeScale);
  gavl_dprintf("  MaxBlockAdditionID: %"PRId64"\n", t->MaxBlockAdditionID);
  gavl_dprintf("  Name:               %s\n", t->Name);
  gavl_dprintf("  Language:           %s\n", t->Language);
  gavl_dprintf("  CodecID:            %s\n", t->CodecID);
  gavl_dprintf("  CodecPrivate        %d bytes\n", t->CodecPrivateLen);

  if(t->CodecPrivateLen)
    gavl_hexdump(t->CodecPrivate, t->CodecPrivateLen, 16);

  gavl_dprintf("  CodecName:          %s\n", t->CodecName);
  gavl_dprintf("  AttachmentLink:     %"PRId64"\n", t->AttachmentLink);
  gavl_dprintf("  CodecDecodeAll:     %d\n", t->CodecDecodeAll);
  gavl_dprintf("  TrackOverlay:       %"PRId64"\n", t->TrackOverlay);
  gavl_dprintf("  CodecDelay:         %"PRId64"\n", t->CodecDelay);
  gavl_dprintf("  SeekPreroll:        %"PRId64"\n", t->SeekPreroll);
  
  for(i = 0; i < t->num_encodings; i++)
    bgav_mkv_content_encoding_dump(&t->encodings[i]);
  
  switch(t->TrackType)
    {
    case MKV_TRACK_VIDEO:
      track_dump_video(&t->video);
      break;
    case MKV_TRACK_AUDIO:
      track_dump_audio(&t->audio);
      break;
    case MKV_TRACK_COMPLEX:
      track_dump_audio(&t->audio);
      track_dump_video(&t->video);
      break;
    }
  }

void  bgav_mkv_track_free(bgav_mkv_track_t * t)
  {
  int i;
  
  MY_FREE(t->Name);
  MY_FREE(t->Language);
  MY_FREE(t->CodecID);
  MY_FREE(t->CodecPrivate);
  MY_FREE(t->CodecName);
  MY_FREE(t->video.ColourSpace);

  if(t->num_encodings)
    {
    for(i = 0; i < t->num_encodings; i++)
      bgav_mkv_content_encoding_free(t->encodings + i);
    free(t->encodings);
    }
  }

int bgav_mkv_tracks_read(bgav_input_context_t * ctx,
                         bgav_mkv_track_t ** ret1,
                         int * ret_num1,
                         bgav_mkv_element_t * parent)
  {
  bgav_mkv_track_t * ret = NULL;
  int ret_num = 0;
  
  bgav_mkv_element_t e;
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_TrackEntry:
        ret = realloc(ret, (ret_num+1)*sizeof(*ret));
        memset(ret + ret_num, 0, sizeof(*ret));
        if(!bgav_mkv_track_read(ctx, ret + ret_num, &e))
          return 0;

        // bgav_mkv_track_dump(ret + ret_num);
        ret_num++;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "tracks");
        break;
      }
    }
  *ret1 = ret;
  *ret_num1 = ret_num;
  return 1;
  }

/* Cue points */

static int mkv_cue_track_read(bgav_input_context_t * ctx,
                              bgav_mkv_cue_track_t * ret,
                              bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  ret->CueBlockNumber = 1;
  
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;

    switch(e.id)
      {
      case MKV_ID_CueTrack:
        if(!mkv_read_uint(ctx, &ret->CueTrack, e.size))
          return 0;
        break;
      case MKV_ID_CueClusterPosition:
        if(!mkv_read_uint(ctx, &ret->CueClusterPosition, e.size))
          return 0;
        break;
      case MKV_ID_CueRelativePosition:
        if(!mkv_read_uint(ctx, &ret->CueRelativePosition, e.size))
          return 0;
        break;
      case MKV_ID_CueDuration:
        if(!mkv_read_uint(ctx, &ret->CueDuration, e.size))
          return 0;
        break;
      case MKV_ID_CueBlockNumber:
        if(!mkv_read_uint(ctx, &ret->CueBlockNumber, e.size))
          return 0;
        break;
      case MKV_ID_CueCodecState:
        if(!mkv_read_uint(ctx, &ret->CueCodecState, e.size))
          return 0;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "cue track");
        break;
      }
    }
  return 1;
  }
                                        

static int mkv_cue_point_read(bgav_input_context_t * ctx,
                              bgav_mkv_cue_point_t * ret,
                              int num_tracks, bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;

  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;
    switch(e.id)
      {
      case MKV_ID_CueTime:
        if(!mkv_read_uint(ctx, &ret->CueTime, e.size))
          return 0;
        break;
      case MKV_ID_CueTrackPositions:
        ret->tracks = realloc(ret->tracks,
                              (ret->num_tracks+1)*sizeof(*ret->tracks));
        memset(ret->tracks + ret->num_tracks, 0, sizeof(*ret->tracks));
        if(!mkv_cue_track_read(ctx, ret->tracks + ret->num_tracks, &e))
          return 0;
        ret->num_tracks++;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "cue point");
        break;
      }
    }
  return 1;
  }

int bgav_mkv_cues_read(bgav_input_context_t * ctx,
                       bgav_mkv_cues_t * ret, int num_tracks)
  {
  bgav_mkv_element_t e;
  bgav_mkv_element_t e1;
  if(!bgav_mkv_element_read(ctx, &e))
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "Couldn't read header element for cues (truncated file?)");
    return 0;
    }
  if(e.id != MKV_ID_Cues)
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "Didn't find cues where I expected them (truncated file?)");
    return 0;
    }
  while(ctx->position < e.end)
    {
    if(!bgav_mkv_element_read(ctx, &e1))
      return 0;

    switch(e1.id)
      {
      case MKV_ID_CuePoint:
        /* Realloc */
        if(ret->num_points + 1 >= ret->points_alloc)
          {
          ret->points_alloc = ret->num_points + 1024;
          ret->points = realloc(ret->points,
                                ret->points_alloc * sizeof(*ret->points));
          memset(ret->points + ret->num_points, 0,
                 (ret->points_alloc - ret->num_points) * sizeof(*ret->points));
          }

        if(!mkv_cue_point_read(ctx, ret->points + ret->num_points,
                               num_tracks, &e1))
          return 0;
        ret->num_points++;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e1, "cues");
        break;
      }
    }
  return 1;
  }

void bgav_mkv_cues_dump(const bgav_mkv_cues_t * cues)
  {
  int i, j;
  gavl_dprintf("Cues\n");
  for(i = 0; i < cues->num_points; i++)
    {
    gavl_dprintf("  Cue point, time: %"PRId64"\n", cues->points[i].CueTime);
    
    for(j = 0; j < cues->points[i].num_tracks; j++)
      {
      gavl_dprintf("    Track: %"PRId64"\n", cues->points[i].tracks[j].CueTrack);
      gavl_dprintf("      CueClusterPosition:  %"PRId64"\n",
                   cues->points[i].tracks[j].CueClusterPosition);
      gavl_dprintf("      CueRelativePosition: %"PRId64"\n",
                   cues->points[i].tracks[j].CueRelativePosition);
      gavl_dprintf("      CueBlockNumber:      %"PRId64"\n",
                   cues->points[i].tracks[j].CueBlockNumber);
      gavl_dprintf("      CueCodecState:       %"PRId64"\n",
                   cues->points[i].tracks[j].CueCodecState);
      }
    }
  }

void bgav_mkv_cues_free(bgav_mkv_cues_t * cues)
  {
  int i, j;

  for(i = 0; i < cues->num_points; i++)
    {
    for(j = 0; j < cues->points[i].num_tracks; j++)
      MY_FREE(cues->points[i].tracks[j].references);
    MY_FREE(cues->points[i].tracks);
    }
  MY_FREE(cues->points);
  }

/* Cluster */

int bgav_mkv_cluster_read(bgav_input_context_t * ctx,
                          bgav_mkv_cluster_t * ret,
                          bgav_mkv_element_t * parent)
  {
  int done = 0;
  bgav_mkv_element_t e;
  bgav_input_context_t * input_mem;
  uint8_t buf[64];
  int buf_len;
  
  input_mem = bgav_input_open_memory(NULL, 0);
  
  while((ctx->position < parent->end) && !done)
    {
    buf_len = bgav_input_get_data(ctx, buf, 64);
    if(!buf_len)
      return 0;
    
    bgav_input_reopen_memory(input_mem, buf, buf_len);
    
    if(!bgav_mkv_element_read(input_mem, &e))
      {
      bgav_input_close(input_mem);
      return 0;
      }
    switch(e.id)
      {
      case MKV_ID_Timecode:
        bgav_input_skip(ctx, input_mem->position);
        if(!mkv_read_uint(ctx, &ret->Timecode, e.size))
          {
          bgav_input_close(input_mem);
          return 0;
          }
        break;
#if 0
      case MKV_ID_SilentTracks:
        break;
      case MKV_ID_SilentTrackNumber:
        break;
#endif
      case MKV_ID_Position:
        bgav_input_skip(ctx, input_mem->position);
        if(!mkv_read_uint(ctx, &ret->Position, e.size))
          {
          bgav_input_close(input_mem);
          return 0;
          }
        break;
      case MKV_ID_PrevSize:
        bgav_input_skip(ctx, input_mem->position);
        if(!mkv_read_uint(ctx, &ret->PrevSize, e.size))
          {
          bgav_input_close(input_mem);
          return 0;
          }
        break;
      case MKV_ID_BlockGroup:
        //        fprintf(stderr, "Got block group\n");
        done = 1;
        break;
      case MKV_ID_Block:
        //        fprintf(stderr, "Got block\n");
        done = 1;
        break;
      case MKV_ID_SimpleBlock:
        //        fprintf(stderr, "Got simple block\n");
        done = 1;
        break;
      default:
        bgav_input_skip(ctx, input_mem->position + e.size);
        break;
      }
    }
  bgav_input_close(input_mem);
  bgav_input_destroy(input_mem);
  return 1;
  }

void bgav_mkv_cluster_dump(const bgav_mkv_cluster_t * c)
  {
  int i;
  gavl_dprintf("Cluster\n");
  gavl_dprintf("  Timecode: %"PRId64"\n", c->Timecode);
  gavl_dprintf("  PrevSize: %"PRId64"\n", c->PrevSize);
  gavl_dprintf("  Position: %"PRId64"\n", c->Position);
  gavl_dprintf("  SilentTracks: %d tracks\n", c->num_silent_tracks);
  for(i = 0; i < c->num_silent_tracks; i++)
    gavl_dprintf("    SilentTrack: %"PRId64"\n", c->silent_tracks[i]);
  }

void bgav_mkv_cluster_free(bgav_mkv_cluster_t * c)
  {
  MY_FREE(c->silent_tracks);
  }

/* Block */

int bgav_mkv_block_read(bgav_input_context_t * ctx,
                         bgav_mkv_block_t * ret,
                         bgav_mkv_element_t * parent)
  {
  uint8_t tmp_8;
  int data_alloc_save;
  uint8_t * data_save;
  int64_t pos = ctx->position;

  data_alloc_save = ret->data_alloc;
  data_save = ret->data;
  
  memset(ret, 0, sizeof(*ret));

  ret->data_alloc = data_alloc_save;
  ret->data       = data_save;
  
  //  bgav_mkv_element_dump(parent);
  
  /* It's no size but has the same encoding */
  if(!bgav_mkv_read_size(ctx, &ret->track) ||
     !bgav_input_read_16_be(ctx, (uint16_t*)(&ret->timecode)) ||
     !bgav_input_read_8(ctx, &tmp_8))
    return 0;

  ret->flags = tmp_8;

  if(parent->id == MKV_ID_Block)
    ret->flags &= ~(MKV_DISCARDABLE|MKV_KEYFRAME);

  if((ret->flags & MKV_LACING_MASK) != MKV_LACING_NONE)
    {
    if(!bgav_input_read_8(ctx, &tmp_8))
      return 0;
    ret->num_laces = tmp_8 + 1;
    }

  ret->data_size = parent->size - (ctx->position - pos);

  if(ret->data_alloc < ret->data_size)
    {
    ret->data_alloc = ret->data_size + 1024;
    ret->data = realloc(ret->data, ret->data_alloc);
    }
  
  if(bgav_input_read_data(ctx, ret->data, ret->data_size) < ret->data_size)
    return 0;
  return 1;
  }

void bgav_mkv_block_dump(int indent, bgav_mkv_block_t * b)
  {
  bgav_diprintf(indent, "Block\n");
  bgav_diprintf(indent, "  Timecode: %d\n", b->timecode);
  bgav_diprintf(indent, "  Track:    %"PRId64"\n", b->track);
  bgav_diprintf(indent, "  Flags:    %02x\n", b->flags);
  bgav_diprintf(indent, "  NumLaces: %d\n", b->num_laces);
  bgav_diprintf(indent, "  DataSize: %d\n", b->data_size);
  }

void bgav_mkv_block_free(bgav_mkv_block_t * b)
  {
  MY_FREE(b->data);
  }

/* Block group */

int bgav_mkv_block_group_read(bgav_input_context_t * ctx,
                              bgav_mkv_block_group_t * ret,
                              bgav_mkv_element_t * parent)
  {
  bgav_mkv_element_t e;
  ret->block.data_size = 0;
  ret->num_reference_blocks = 0;

  //  fprintf(stderr, "Read block group\n");
  //  bgav_mkv_element_dump(parent);
  
  while(ctx->position < parent->end)
    {
    if(!bgav_mkv_element_read(ctx, &e))
      return 0;
    switch(e.id)
      {
      case MKV_ID_BlockDuration:
        if(!mkv_read_uint(ctx, &ret->BlockDuration, e.size))
          return 0;
        break;
      case MKV_ID_ReferencePriority:
        if(!mkv_read_uint_small(ctx, &ret->ReferencePriority,
                                e.size))
          return 0;
        break;
      case MKV_ID_ReferenceBlock:
        if(ret->num_reference_blocks + 1 > ret->reference_blocks_alloc)
          {
          ret->reference_blocks_alloc = ret->num_reference_blocks + 1;
          ret->reference_blocks = realloc(ret->reference_blocks,
                                          ret->reference_blocks_alloc *
                                          sizeof(*ret->reference_blocks));
          }

        if(!mkv_read_int(ctx, &ret->reference_blocks[ret->num_reference_blocks],
                         e.size))
          return 0;
        ret->num_reference_blocks++;
        break;
      case MKV_ID_Block:
      case MKV_ID_SimpleBlock:
        if(!bgav_mkv_block_read(ctx, &ret->block, &e))
          return 0;
        break;
      default:
        bgav_mkv_element_skip(ctx, &e, "block group");
        break;
      }
    }
  return 1;
  }

void bgav_mkv_block_group_dump(bgav_mkv_block_group_t * g)
  {
  gavl_dprintf("BlockGroup\n");
  gavl_dprintf("  BlockDuration:     %"PRId64"\n", g->BlockDuration);
  gavl_dprintf("  ReferencePriority: %d\n", g->ReferencePriority);
  bgav_mkv_block_dump(2, &g->block);
  }

void bgav_mkv_block_group_free(bgav_mkv_block_group_t * g)
  {
  bgav_mkv_block_free(&g->block);
  MY_FREE(g->reference_blocks);
  }
