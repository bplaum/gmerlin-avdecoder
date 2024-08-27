/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
 *
 * Copyright (c) 2001 - 2024 Members of the Gmerlin project
 * http://github.com/bplaum
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


#include <vdpau/vdpau_x11.h>

typedef struct bgav_vdpau_context_s bgav_vdpau_context_t;

bgav_vdpau_context_t * bgav_vdpau_context_create(const bgav_options_t * opt);

void bgav_vdpau_context_destroy(bgav_vdpau_context_t *);

VdpVideoSurface
bgav_vdpau_context_create_video_surface(bgav_vdpau_context_t * ctx,
                                        VdpChromaType chroma_type,
                                        uint32_t width, uint32_t height);

void bgav_vdpau_context_destroy_video_surface(bgav_vdpau_context_t * ctx,
                                              VdpVideoSurface s);

void bgav_vdpau_context_surface_to_frame(bgav_vdpau_context_t * ctx,
                                         VdpVideoSurface s, gavl_video_frame_t * dst);


VdpDecoder
bgav_vdpau_context_create_decoder(bgav_vdpau_context_t * ctx,
                                  VdpDecoderProfile profile,
                                  uint32_t width, uint32_t height,
                                  uint32_t max_references);

VdpStatus
bgav_vdpau_context_decoder_render(bgav_vdpau_context_t * ctx,
                                  VdpDecoder dec,
                                  VdpVideoSurface target,
                                  VdpPictureInfo const *picture_info,
                                  uint32_t bitstream_buffer_count,
                                  VdpBitstreamBuffer const *bitstream_buffers);

void bgav_vdpau_context_destroy_decoder(bgav_vdpau_context_t * ctx,
                                        VdpDecoder s);
