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
