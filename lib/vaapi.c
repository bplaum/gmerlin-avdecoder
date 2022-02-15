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

#include <config.h>
#include <avdec_private.h>

#include <X11/Xlib.h>

#include AVCODEC_HEADER
#include VAAPI_HEADER

#include <va/va.h>
#include <bgav_vaapi.h>


#include <gavl/hw_vaapi_x11.h>

static int get_profile(enum AVCodecID id)
  {
  int profile;
  switch(id)
    {
    case AV_CODEC_ID_MPEG2VIDEO:
      profile = VAProfileMPEG2Main;
      break;
    case AV_CODEC_ID_MPEG4:
    case AV_CODEC_ID_H263:
      profile = VAProfileMPEG4AdvancedSimple;
      break;
    case AV_CODEC_ID_H264:
      profile = VAProfileH264High;
      break;
    case AV_CODEC_ID_WMV3:
      profile = VAProfileVC1Main;
      break;
    case AV_CODEC_ID_VC1:
      profile = VAProfileVC1Advanced;
      break;
    default:
      profile = VAProfileNone;
      break;
    }
  return profile;
  }

static int has_profile(VADisplay dpy, VAProfile profile)
  {
  VAProfile * p = NULL;
  int num, i;
  VAStatus status;
  int ret = 0;

  num = vaMaxNumProfiles(dpy);

  p = calloc(num, sizeof(*p));

  if((status = vaQueryConfigProfiles(dpy, p, &num)) != VA_STATUS_SUCCESS)
    goto fail;

  for(i = 0; i < num; i++)
    {
    if(p[i] == profile)
      {
      ret = 1;
      break;
      }
    }

  fail:

  if(p)
    free(p);
  return ret;
  }

static int has_entrypoint(VADisplay dpy, VAProfile profile,
                          VAEntrypoint entrypoint)
  {
  int ret = 0;
  int num;
  VAStatus status;
  VAEntrypoint * ep = NULL;
  int i;

  num = vaMaxNumEntrypoints(dpy);
  ep = calloc(num, sizeof(*ep));
  
  if((status = vaQueryConfigEntrypoints(dpy, profile, ep, &num)) != VA_STATUS_SUCCESS)
    goto fail;

  for(i = 0; i < num; i++)
    {
    if(ep[i] == entrypoint)
      {
      ret = 1;
      break;
      }
    }
  fail:
  if(ep)
    free(ep);

  return ret;
  }

static VAEntrypoint get_entrypoint(enum AVPixelFormat pfmt)
  {
  switch(pfmt)
    {
    case AV_PIX_FMT_VAAPI_MOCO:
      return VAEntrypointMoComp;
      break;
    case AV_PIX_FMT_VAAPI_IDCT:
      return VAEntrypointIDCT;
      break;
    case AV_PIX_FMT_VAAPI_VLD:
      return VAEntrypointVLD;
      break;
    default:
      break;
    }
  return 0;
  }

/* Noop because we don't have dynamic memory in the buffer */
static void free_buffer(void * opaque, uint8_t * data)
  {
  }

int bgav_vaapi_init(bgav_vaapi_t * v,
                    AVCodecContext * avctx,
                    enum AVPixelFormat pfmt)
  {
  int i;
  VAProfile profile;
  VAEntrypoint ep;
  VAConfigAttrib attr;
  VAStatus status;
  int context_flags = 0;
  int surface_width, surface_height, context_width, context_height;

  bgav_stream_t * s = avctx->opaque;
  
  if((profile = get_profile(avctx->codec_id)) == VAProfileNone)
    goto fail;

  /* Connect to hardware */
  if(!(v->hwctx = gavl_hw_ctx_create_vaapi_x11(NULL)))
    goto fail;
  v->vaapi_ctx.display = gavl_hw_ctx_vaapi_x11_get_va_display(v->hwctx);

  /* Make IDs invalid */
  v->vaapi_ctx.config_id = VA_INVALID_ID;
  v->vaapi_ctx.context_id = VA_INVALID_ID;

  if(!has_profile(v->vaapi_ctx.display, profile))
    goto fail;

  if(!(ep = get_entrypoint(pfmt)))
    goto fail;

  if(!has_entrypoint(v->vaapi_ctx.display, profile, ep))
    goto fail;

  if(!s->ci.max_ref_frames)
    goto fail;

  /* Get dimensions. What about padding here? */
  
  surface_width  = s->data.video.format->image_width;
  surface_height = s->data.video.format->image_height;

  context_width  = s->data.video.format->image_width;
  context_height = s->data.video.format->image_height;

  /* Get surface format */
  
  attr.type = VAConfigAttribRTFormat;
  vaGetConfigAttributes(v->vaapi_ctx.display, profile, ep, &attr, 1);

  if(!(attr.value & VA_RT_FORMAT_YUV420))
    goto fail;
  
  /* Create config */
  
  if((status = vaCreateConfig(v->vaapi_ctx.display, profile, ep,
                               &attr, 1, &v->vaapi_ctx.config_id)) != VA_STATUS_SUCCESS)
    goto fail;

  /* Create surfaces */
  
  v->num_surfaces = s->ci.max_ref_frames + 2; // Reference frames + render frame + 1 safety frame 
                                                
  v->surfaces = calloc(v->num_surfaces, sizeof(*v->surfaces));
  
  /* Make invalid so we won't destroy invalid surfaces if the create call fails */
  v->surfaces[0] = VA_INVALID_ID;

  if((status = vaCreateSurfaces(v->vaapi_ctx.display,
                                VA_RT_FORMAT_YUV420,
                                surface_width, surface_height,
                                v->surfaces, v->num_surfaces, NULL, 0) != VA_STATUS_SUCCESS))
    goto fail;
  
  
  /* Create context */
  if(s->data.video.format->interlace_mode == GAVL_INTERLACE_NONE)
    context_flags |= VA_PROGRESSIVE;

  if((status = vaCreateContext(v->vaapi_ctx.display,
                               v->vaapi_ctx.config_id,
                               context_width,
                               context_height,
                               context_flags,
                               v->surfaces,
                               v->num_surfaces,
                               &v->vaapi_ctx.context_id) != VA_STATUS_SUCCESS))
    goto fail;
 
  avctx->hwaccel_context = &v->vaapi_ctx;

  /* Create frames */
  
  v->frames = calloc(v->num_surfaces, sizeof(*v->frames));

  for(i = 0; i < v->num_surfaces; i++)
    {
    v->frames[i].s = v->surfaces[i];
    v->frames[i].f = gavl_video_frame_create(NULL);
    v->frames[i].f->user_data = v->surfaces + i;
    v->frames[i].f->hwctx = v->hwctx;
    v->frames[i].buf = av_buffer_create((uint8_t*)(uintptr_t)v->frames[i].s, 0,
                                        free_buffer, NULL, 0);
    }
  
  s->data.video.format->pixelformat = GAVL_YUV_420_P;
  s->data.video.format->hwctx = v->hwctx;
  return 1; 
  
  fail: // Cleanup

  bgav_vaapi_cleanup(v);
  return 0;
  }

bgav_vaapi_frame_t * bgav_vaapi_get_frame_by_id(bgav_vaapi_t * v, VASurfaceID id)
  {
  int i;
  for(i = 0; i < v->num_surfaces; i++)
    {
    if(v->frames[i].s == id)
      return &v->frames[i];
    }
  return NULL;
  }

bgav_vaapi_frame_t * bgav_vaapi_get_frame(bgav_vaapi_t * v)
  {
  int i;
  for(i = 0; i < v->num_surfaces; i++)
    {
    if(av_buffer_get_ref_count(v->frames[i].buf) < 2)
      {
      //      fprintf(stderr, "bgav_vaapi_get_frame %d %d\n", i + 1, v->num_surfaces);
      return &v->frames[i];
      }
    }
  //  fprintf(stderr, "Out of frames\n");
  return NULL;
  }


void bgav_vaapi_cleanup(bgav_vaapi_t * v)
  {
  int i;

  if(!v->hwctx)
    return;

  if(v->vaapi_ctx.config_id != VA_INVALID_ID)
    vaDestroyConfig(v->vaapi_ctx.display, v->vaapi_ctx.config_id);

  if(v->vaapi_ctx.context_id != VA_INVALID_ID)
    vaDestroyContext(v->vaapi_ctx.display, v->vaapi_ctx.context_id);

  if(v->surfaces)
    {
    if(v->surfaces[0] != VA_INVALID_ID)
      vaDestroySurfaces(v->vaapi_ctx.display, v->surfaces, v->num_surfaces);
    free(v->surfaces);
    }
 
  if(v->frames)
    {
    for(i = 0; i < v->num_surfaces; i++)
      {
      if(v->frames[i].f)
        {
        gavl_video_frame_null(v->frames[i].f);
        v->frames[i].f->hwctx = NULL;
        gavl_video_frame_destroy(v->frames[i].f);
        }
      if(v->frames[i].buf)
        av_buffer_unref(&v->frames[i].buf);
      }

    free(v->frames);
    }

  gavl_hw_ctx_destroy(v->hwctx);
  v->hwctx = NULL;
  }
