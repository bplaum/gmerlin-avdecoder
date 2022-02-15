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

#ifdef HAVE_NEAACDEC_H
#include <neaacdec.h>


/*
 *  Backwards compatibility names (currently in neaacdec.h,
 *  but might be removed in future versions)
 */
#ifndef faacDecHandle
/* structs */
#define faacDecHandle                  NeAACDecHandle
#define faacDecConfiguration           NeAACDecConfiguration
#define faacDecConfigurationPtr        NeAACDecConfigurationPtr
#define faacDecFrameInfo               NeAACDecFrameInfo
/* functions */
#define faacDecGetErrorMessage         NeAACDecGetErrorMessage
#define faacDecSetConfiguration        NeAACDecSetConfiguration
#define faacDecGetCurrentConfiguration NeAACDecGetCurrentConfiguration
#define faacDecInit                    NeAACDecInit
#define faacDecInit2                   NeAACDecInit2
#define faacDecInitDRM                 NeAACDecInitDRM
#define faacDecPostSeekReset           NeAACDecPostSeekReset
#define faacDecOpen                    NeAACDecOpen
#define faacDecClose                   NeAACDecClose
#define faacDecDecode                  NeAACDecDecode
#define AudioSpecificConfig            NeAACDecAudioSpecificConfig
#endif

#else
#include <faad.h>
#endif

typedef struct bgav_aac_frame_s bgav_aac_frame_t;

bgav_aac_frame_t * bgav_aac_frame_create(const bgav_options_t * opt,
                                         uint8_t * header, int header_len);

void bgav_aac_frame_destroy(bgav_aac_frame_t *);

/* Return value:
   0:        Need more data
   1:        Done
   Negative: Error
*/
int bgav_aac_frame_parse(bgav_aac_frame_t *,
                         uint8_t * data, int data_len,
                         int * bytes_used, int * samples);

void bgav_aac_frame_get_audio_format(bgav_aac_frame_t * frame,
                                     gavl_audio_format_t * format);

/* Used by parser and decoder */

void bgav_faad_set_channel_setup(faacDecFrameInfo * frame_info,
                                 gavl_audio_format_t * format);

