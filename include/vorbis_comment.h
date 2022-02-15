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

typedef struct
  {
  char * vendor;
  int num_user_comments;

  char ** user_comments;
  } bgav_vorbis_comment_t;

int bgav_vorbis_comment_read(bgav_vorbis_comment_t * ret,
                             bgav_input_context_t * input);

void bgav_vorbis_comment_2_metadata(bgav_vorbis_comment_t * comment,
                                    bgav_metadata_t * m);

void bgav_vorbis_comment_free(bgav_vorbis_comment_t * ret);

void bgav_vorbis_comment_dump(bgav_vorbis_comment_t * ret);

const char *
bgav_vorbis_comment_get_field(bgav_vorbis_comment_t * vc, const char * key, int idx);

/* doesn't belong here actually */
void bgav_vorbis_set_channel_setup(gavl_audio_format_t * format);
