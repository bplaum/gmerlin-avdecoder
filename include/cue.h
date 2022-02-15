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

typedef struct bgav_cue_s bgav_cue_t;

bgav_cue_t *
bgav_cue_read(bgav_input_context_t * audio_file);

gavl_dictionary_t * bgav_cue_get_edl(bgav_cue_t *, int64_t total_samples,
                                     gavl_dictionary_t * parent);

void bgav_cue_destroy(bgav_cue_t *);

// Not implemented
// void bgav_cue_dump(bgav_cue_t *);

void bgav_demuxer_init_cue(bgav_demuxer_context_t * ctx);

