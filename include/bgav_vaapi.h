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

#if 0
typedef struct
  {
  VASurfaceID s;
  gavl_video_frame_t * f;
  AVBufferRef *buf;
  } bgav_vaapi_frame_t;

typedef struct
  {
  gavl_hw_context_t * hwctx;
  
  /* Passed to libavcodec */
  struct vaapi_context vaapi_ctx;

  int num_surfaces;
  VASurfaceID * surfaces;

  bgav_vaapi_frame_t * frames;
  
  } bgav_vaapi_t;

int bgav_vaapi_init(bgav_vaapi_t *, AVCodecContext * avctx, enum AVPixelFormat pfmt);

void bgav_vaapi_cleanup(bgav_vaapi_t *);

bgav_vaapi_frame_t * bgav_vaapi_get_frame(bgav_vaapi_t *);

bgav_vaapi_frame_t * bgav_vaapi_get_frame_by_id(bgav_vaapi_t *, VASurfaceID id);
#endif
