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

/* Jpeg2000 decoder */

#include <stdlib.h>
#include <string.h>
#include <openjpeg.h>

#include <config.h>
#include <avdec_private.h>
#include <codecs.h>

#define LOG_DOMAIN "video_openjpeg"
#define LOG_DOMAIN_OJP "openjpeg"

#define J2K_CFMT 0
#define JP2_CFMT 1
#define JPT_CFMT 2

static void error_callback(const char *msg, void *client_data)
  {
  bgav_options_t * opt = client_data;
  gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN_OJP, "%s", msg);
  }

static void warning_callback(const char *msg, void *client_data)
  {
  bgav_options_t * opt = client_data;
  gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN_OJP, "%s", msg);
  }

static void info_callback(const char *msg, void *client_data)
  {
  bgav_options_t * opt = client_data;
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN_OJP, "%s", msg);
  }

typedef struct
  {
  int need_format;

  opj_dparameters_t parameters;	/* decompression parameters */
  opj_event_mgr_t event_mgr;		/* event manager */
  opj_dinfo_t* dinfo;	/* handle to a decompressor */

  opj_image_t *img;
  
  } openjpeg_priv_t;

/* Debayer functions (from blender) */

/* pretty simple but astonishingly very effective "debayer" function 
 */

#define CLIP(arg, dst) tmp = (arg); \
   /* Clip */ \
   tmp = (tmp & ~0xFFFF)?0xFFFF:((tmp<0)?0:tmp); \
   /* Saturate */ \
   dst = tmp | (tmp >> 12);

static void
redcode_ycbcr2rgb(int ** planes, int width,
                  int height, uint16_t * out, int out_stride)
  {
  int x,y;
  int tmp;
  const int pix_max = 4096;
  const int mask = pix_max - 1;
  out_stride /= 2;
  for (y = 0; y < height; y++)
    {
    uint16_t *o = out + y * out_stride;
    for (x = 0; x < width; x++)
      {
      int i = y*width + x;
      int y1  = (planes[0][i] & mask);
      int cb  = (planes[1][i] & mask)  - pix_max/2;
      int cr  = (planes[2][i] & mask)  - pix_max/2;
      int y2  = (planes[3][i] & mask);
      
      int b_ = cb << 4;
      int r_ = cr << 4;

      /* 12 -> 16 bits */
      int y = ((y1 + y2)>>1)<<4;
      
      CLIP(r_ + y, *o++);
      CLIP(   + y, *o++);
      CLIP(b_ + y, *o++);
      //			*o++ = 1.0;
      }
    }
  }

/* Decode function */

static gavl_source_status_t decode_openjpeg(bgav_stream_t * s, gavl_video_frame_t * f)
  {
  openjpeg_priv_t * priv;
  bgav_packet_t * p = NULL;
  opj_cio_t *cio;
  gavl_source_status_t st;
  
  priv = s->decoder_priv;

  if(!(s->flags & STREAM_HAVE_FRAME))
    {
    if((st = bgav_stream_get_packet_read(s, &p)) != GAVL_SOURCE_OK)
      return st;
    }
  
  if(f || priv->need_format)
    {
    if(!(s->flags & STREAM_HAVE_FRAME))
      {
      /* open a byte stream */
      cio = opj_cio_open((opj_common_ptr)priv->dinfo, 
                         p->data, p->data_size);
      
      priv->img = opj_decode(priv->dinfo, cio);

      /* close the byte stream */
      opj_cio_close(cio);
            
      if(priv->need_format)
        {
        s->data.video.format->image_width  = priv->img->x1 - priv->img->x0; 
        s->data.video.format->image_height = priv->img->y1 - priv->img->y0; 
        s->data.video.format->image_width  /= (1 << s->opt->shrink);
        s->data.video.format->image_height /= (1 << s->opt->shrink);
        
        s->data.video.format->frame_width  = s->data.video.format->image_width; 
        s->data.video.format->frame_height = s->data.video.format->image_height; 
        

        if(s->fourcc == BGAV_MK_FOURCC('R', '3', 'D', '1'))
          {
          s->data.video.format->pixelformat = GAVL_RGB_48;
          }
        }
      s->flags |= STREAM_HAVE_FRAME;
      }
    if(f)
      {
      if(s->fourcc == BGAV_MK_FOURCC('R', '3', 'D', '1'))
        {
        int i;
        int* planes[4];
        for(i = 0; i < 4; i++)
          planes[i] = priv->img->comps[i].data;

        
        redcode_ycbcr2rgb(planes, priv->img->comps[0].w,
                          priv->img->comps[0].h,
                          (uint16_t*)f->planes[0], f->strides[0]);

        bgav_set_video_frame_from_packet(p, f);
        }
      else
        {
        
        }
      opj_image_destroy(priv->img);
      }
    
    }
  if(p)
    bgav_stream_done_packet_read(s, p);
  
  return 1;
  }

static int init_openjpeg(bgav_stream_t * s)
  {
  openjpeg_priv_t * priv;
  priv = calloc(1, sizeof(*priv));
  s->decoder_priv = priv;
  s->ci.flags &= ~GAVL_COMPRESSION_HAS_P_FRAMES;
  
  priv->event_mgr.error_handler = error_callback;
  priv->event_mgr.warning_handler = warning_callback;
  priv->event_mgr.info_handler = info_callback;

  opj_set_default_decoder_parameters(&priv->parameters);
  priv->parameters.decod_format = JP2_CFMT;

  priv->parameters.cp_reduce = s->opt->shrink;
  
#if 0  
  if(scale == 2)
    {
    priv->parameters.cp_reduce = 1;
    }
  else if (scale == 4)
    {
    priv->parameters.cp_reduce = 2;
    }
  else if (scale == 8)
    {
    priv->parameters.cp_reduce = 3;
    }
#endif

  /* get a decoder handle */
  priv->dinfo = opj_create_decompress(CODEC_JP2);

  /* catch events using our callbacks and give a local context */
  opj_set_event_mgr((opj_common_ptr)priv->dinfo, &priv->event_mgr,
                    (bgav_options_t*)s->opt);
  
  /* setup the decoder decoding parameters using the current image 
     and user parameters */
  opj_setup_decoder(priv->dinfo, &priv->parameters);

  priv->need_format = 1;
  decode_openjpeg(s, NULL);
  priv->need_format = 0;
  
  gavl_dictionary_set_string(s->m, GAVL_META_FORMAT, "JPEG-2000");
  return 1;
  }

static void close_openjpeg(bgav_stream_t * s)
  {
  openjpeg_priv_t * priv;
  priv = s->decoder_priv;
  
  /* free remaining structures */
  if(priv->dinfo)
    opj_destroy_decompress(priv->dinfo);
  
  free(priv);
  }

#if 0
static void resync_openjpeg(bgav_stream_t * s)
  {
  openjpeg_priv_t * priv;
  priv = s->decoder_priv;
  }
#endif

static bgav_video_decoder_t decoder =
  {
    .name =   "OPENJPEG video decoder",
    .fourccs =  (uint32_t[]){ BGAV_MK_FOURCC('R', '3', 'D', '1'),
                              0x00  },
    .init =   init_openjpeg,
    .decode = decode_openjpeg,
    //    .resync = resync_openjpeg,
    .close =  close_openjpeg,
    .resync = NULL,
  };

void bgav_init_video_decoders_openjpeg()
  {
  bgav_video_decoder_register(&decoder);
  }
