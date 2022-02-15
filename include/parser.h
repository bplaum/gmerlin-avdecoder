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

#define PARSER_NEED_DATA      0

#define PARSER_HAVE_FORMAT    1 /* Audio parsers */

#define PARSER_HAVE_PACKET    2
#define PARSER_EOF            3
#define PARSER_ERROR          4


#define PARSER_PRIV           5 /* Offset for internally used codes */

/* Video parser */

bgav_video_parser_t *
bgav_video_parser_create(bgav_stream_t * s);

int bgav_video_parser_supported(uint32_t fourcc);

void bgav_video_parser_destroy(bgav_video_parser_t *);

/* Either in_pts or out_pts can (should) be undefined */
void bgav_video_parser_reset(bgav_video_parser_t *,
                             int64_t in_pts, int64_t out_pts);

void bgav_video_parser_add_packet(bgav_video_parser_t * parser,
                                  bgav_packet_t * p);

// const uint8_t * bgav_video_parser_get_header(bgav_video_parser_t * parser,
//                                              int * len);

// int bgav_video_parser_set_header(bgav_video_parser_t * parser,
//                                  const uint8_t * header, int len);

void bgav_video_parser_get_packet(bgav_video_parser_t * parser,
                                  bgav_packet_t * p);

int bgav_video_parser_max_ref_frames(bgav_video_parser_t * parser);



/* Audio parser */

int bgav_audio_parser_supported(uint32_t fourcc);

bgav_audio_parser_t * bgav_audio_parser_create(bgav_stream_t * s);

// int bgav_audio_parser_set_header(bgav_audio_parser_t * parser,
//                                 const uint8_t * header, int len);

void bgav_audio_parser_destroy(bgav_audio_parser_t *);

void bgav_audio_parser_reset(bgav_audio_parser_t *,
                             int64_t in_pts, int64_t out_pts);


void bgav_audio_parser_add_data(bgav_audio_parser_t * parser,
                                uint8_t * data, int len, int64_t position);

