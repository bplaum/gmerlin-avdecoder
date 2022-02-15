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


/*
 *  Ported from mplayer, only 3ivx is supported,
 *  the other xanim codecs are already there via libavcodec or Win32 dlls
 */

#include <dlfcn.h> /* dlsym, dlopen, dlclose */
#include <stdarg.h> /* va_alist, va_start, va_end */
#include <errno.h> /* strerror, errno */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <avdec_private.h>
#include <codecs.h>
#define LOG_DOMAIN "video_xadll"

char * bgav_dll_path_xanim = NULL;

typedef struct
{
  unsigned int          what;
  unsigned int          id;
  int                   (*iq_func)();        /* init/query function */
  unsigned int          (*dec_func)();  /* opt decode function */
} XAVID_FUNC_HDR;

typedef struct
{
    unsigned int        file_num;
    unsigned int        anim_type;
    unsigned int        imagex;
    unsigned int        imagey;
    unsigned int        imagec;
    unsigned int        imaged;
} XA_ANIM_HDR;


#define XAVID_WHAT_NO_MORE      0x0000
#define XAVID_AVI_QUERY         0x0001
#define XAVID_QT_QUERY          0x0002
#define XAVID_DEC_FUNC          0x0100

#define XAVID_API_REV           0x0003

typedef struct
{
  unsigned int          api_rev;
  char                  *desc;
  char                  *rev;
  char                  *copyright;
  char                  *mod_author;
  char                  *authors;
  unsigned int          num_funcs;
  XAVID_FUNC_HDR        *funcs;
} XAVID_MOD_HDR;

/* XA CODEC .. */
typedef struct
{
  void                  *anim_hdr;
  unsigned int          compression;
  unsigned int          x, y;
  unsigned int          depth;
  void                  *extra;
  unsigned int          xapi_rev;
  unsigned int          (*decoder)();
  char                  *description;
  unsigned int          avi_ctab_flag;
  unsigned int          (*avi_read_ext)();
} XA_CODEC_HDR;

#define CODEC_SUPPORTED 1
#define CODEC_UNKNOWN 0
#define CODEC_UNSUPPORTED -1

/* fuckin colormap structures for xanim */
typedef struct
{
  unsigned short        red;
  unsigned short        green;
  unsigned short        blue;
  unsigned short        gray;
} ColorReg;


typedef struct XA_ACTION_STRUCT
{
  int                   type;
  int                   cmap_rev;
  unsigned char         *data;
  struct XA_ACTION_STRUCT *next;
  struct XA_CHDR_STRUCT *chdr;
  ColorReg              *h_cmap;
  unsigned int          *map;
  struct XA_ACTION_STRUCT *next_same_chdr;
} XA_ACTION;


typedef struct XA_CHDR_STRUCT
{
  unsigned int          rev;
  ColorReg              *cmap;
  unsigned int          csize, coff;
  unsigned int          *map;
  unsigned int          msize, moff;
  struct XA_CHDR_STRUCT *next;
  XA_ACTION             *acts;
  struct XA_CHDR_STRUCT *new_chdr;
} XA_CHDR;



typedef struct
{
  unsigned int          cmd;
  unsigned int          skip_flag;
  unsigned int          imagex, imagey;        /* image buffer size */
  unsigned int          imaged;                /* image depth */
  XA_CHDR               *chdr;                /* color map header */
  unsigned int          map_flag;
  unsigned int          *map;
  unsigned int          xs, ys;
  unsigned int          xe, ye;
  unsigned int          special;
  void                  *extra;
} XA_DEC_INFO;

#define xaFALSE 0
#define xaTRUE 1

#define ACT_DLTA_NORM   0x00000000
#define ACT_DLTA_BODY   0x00000001
#define ACT_DLTA_XOR    0x00000002
#define ACT_DLTA_NOP    0x00000004
#define ACT_DLTA_MAPD   0x00000008
#define ACT_DLTA_DROP   0x00000010
#define ACT_DLTA_BAD    0x80000000

typedef struct
  {
  XA_DEC_INFO decinfo;
  void *dll_handle;
  long (*iq_func)(XA_CODEC_HDR *codec_hdr);
  unsigned int (*dec_func)(unsigned char *image, unsigned char *delta,
                           unsigned int dsize, XA_DEC_INFO *dec_info);
  gavl_video_frame_t * frame;
  } xanim_priv_t;

/* Exported for the codec */

typedef struct
{
    unsigned long Uskip_mask;
    long        *YUV_Y_tab;
    long        *YUV_UB_tab;
    long        *YUV_VR_tab;
    long        *YUV_UG_tab;
    long        *YUV_VG_tab;
} YUVTabs;

YUVTabs def_yuv_tabs;

typedef struct
{
    unsigned char *Ybuf;
    unsigned char *Ubuf;
    unsigned char *Vbuf;
    unsigned char *the_buf;
    unsigned int the_buf_size;
    unsigned short y_w, y_h;
    unsigned short uv_w, uv_h;
} YUVBufs;


static void XA_YUV221111_Convert(unsigned char *image_p,
                                 unsigned int imagex,
                                 unsigned int imagey,
                                 unsigned int i_x,
                                 unsigned int i_y,
                                 YUVBufs *yuv,
                                 YUVTabs *yuv_tabs,
                                 unsigned int map_flag,
                                 unsigned int *map, XA_CHDR *chdr)
  {
  bgav_stream_t * stream;
  xanim_priv_t * priv;
  int i;
  uint8_t * src;
  uint8_t * dst;
  // note: 3ivX codec doesn't set y_w, uv_w, they are random junk :(
  int ystride; 
  int uvstride;

  
  stream = (bgav_stream_t *)image_p;

  ystride=stream->data.video.format.image_width; //(yuv->y_w)?yuv->y_w:imagex;
  uvstride=stream->data.video.format.image_width/2; //(yuv->uv_w)?yuv->uv_w:(imagex/2);

  priv = stream->decoder_priv;

  if(!priv->frame)
    return;
  
  src = yuv->Ybuf;
  dst = priv->frame->planes[0];
  for(i = 0; i < stream->data.video.format.image_height; i++)
    {
    memcpy(dst, src, ystride);
    dst += ystride;
    src += priv->frame->strides[0];
    }

  src = yuv->Ubuf;
  dst = priv->frame->planes[1];
  for(i = 0; i < stream->data.video.format.image_height/2; i++)
    {
    memcpy(dst, src, uvstride);
    dst += uvstride;
    src += priv->frame->strides[1];
    }
  src = yuv->Vbuf;
  dst = priv->frame->planes[2];
  for(i = 0; i < stream->data.video.format.image_height/2; i++)
    {
    memcpy(dst, src, uvstride);
    dst += uvstride;
    src += priv->frame->strides[1];
    }
  }

void *XA_YUV221111_Func(unsigned int image_type);


void *XA_YUV221111_Func(unsigned int image_type)
  {
  return((void *)XA_YUV221111_Convert);
  }

void XA_Print(char *fmt, ...);

void XA_Print(char *fmt, ...)
{
    va_list vallist;

    va_start(vallist, fmt);
    vfprintf(stderr, fmt, vallist);
    va_end(vallist);

    return;
}

/* 0 is no debug (needed by 3ivX) */
long xa_debug = 0;

void XA_Gen_YUV_Tabs(XA_ANIM_HDR *anim_hdr);

void XA_Gen_YUV_Tabs(XA_ANIM_HDR *anim_hdr)
  {
  //  XA_Print("XA_Gen_YUV_Tabs('anim_hdr: %08x')\n", anim_hdr);
  return;
  }

void JPG_Setup_Samp_Limit_Table(XA_ANIM_HDR *anim_hdr);
void JPG_Setup_Samp_Limit_Table(XA_ANIM_HDR *anim_hdr)
  {
  //  XA_Print("JPG_Setup_Samp_Limit_Table('anim_hdr: %08x')\n", anim_hdr);
  return;
  }

/* load, init and query */

static int init_xadll(bgav_stream_t * s)
  {
  void *(*what_the)();
  XAVID_MOD_HDR *mod_hdr;
  XAVID_FUNC_HDR *func;
  XA_CODEC_HDR codec_hdr;
  int i;
  xanim_priv_t * priv;
  char dll_filename[PATH_MAX];
  
  priv = calloc(1, sizeof(*priv));
           
  sprintf(dll_filename, "%s/%s", bgav_dll_path_xanim, "vid_3ivX.xa");
  
  priv->dll_handle = dlopen(dll_filename,
                              RTLD_NOW);
  if(!priv->dll_handle)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Cannot open dll %s: %s", dll_filename, dlerror());
    goto fail;
    }
  
  what_the = dlsym(priv->dll_handle, "What_The");

  if(!what_the)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "failed to init %s", dlerror());
    goto fail;
    }
        
  mod_hdr = what_the();
  if(!mod_hdr)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "initializer function failed");
    goto fail;
    }
  if (mod_hdr->api_rev > XAVID_API_REV)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Unsupported api revision (%d)", mod_hdr->api_rev);
    goto fail;
    }
  
  func = mod_hdr->funcs;
  if (!func)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "function table error");
    goto fail;
    }
  
  for (i = 0; i < (int)mod_hdr->num_funcs; i++)
    {
    if (func[i].what & XAVID_AVI_QUERY)
      {
      priv->iq_func = (void *)func[i].iq_func;
      }
    if (func[i].what & XAVID_QT_QUERY)
      {
      priv->iq_func = (void *)func[i].iq_func;
      }
    if (func[i].what & XAVID_DEC_FUNC)
      {
      priv->dec_func = (void *)func[i].dec_func;
      }
    }
  
  codec_hdr.xapi_rev = XAVID_API_REV;
  codec_hdr.anim_hdr = malloc(4096);
  codec_hdr.description = "Description";
  codec_hdr.compression = s->fourcc;
  codec_hdr.decoder = NULL;
  codec_hdr.x = s->data.video.format.image_width; /* ->disp_w */
  codec_hdr.y = s->data.video.format.image_height; /* ->disp_h */
  /* extra fields to store palette */
  codec_hdr.avi_ctab_flag = 0;
  codec_hdr.avi_read_ext = NULL;
  codec_hdr.extra = NULL;
  codec_hdr.depth = 12;

  if(priv->iq_func(&codec_hdr) != CODEC_SUPPORTED)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Codec not supported");
    goto fail;
    }
  priv->dec_func = (void *)codec_hdr.decoder;
  
  priv->decinfo.cmd = 0;
  priv->decinfo.skip_flag = 0;
  priv->decinfo.imagex = priv->decinfo.xe = codec_hdr.x;
  priv->decinfo.imagey = priv->decinfo.ye = codec_hdr.y;
  priv->decinfo.imaged = codec_hdr.depth;
  priv->decinfo.chdr = NULL;
  priv->decinfo.map_flag = 0; /* xaFALSE */
  priv->decinfo.map = NULL;
  priv->decinfo.xs = priv->decinfo.ys = 0;
  priv->decinfo.special = 0;
  priv->decinfo.extra = codec_hdr.extra;
  s->decoder_priv = priv;
  s->data.video.format.pixelformat = GAVL_YUV_420_P;

  gavl_dictionary_set_string(&s->m, GAVL_META_FORMAT,
                    "3ivx");
  free(codec_hdr.anim_hdr);
  
  return 1;
  
  fail:
  if(codec_hdr.anim_hdr)
    free(codec_hdr.anim_hdr);
  return 0;
  }

static gavl_source_status_t 
decode_xadll(bgav_stream_t * s, gavl_video_frame_t * frame)
  {
  int ret;
  bgav_packet_t * p;
  xanim_priv_t * priv;
  gavl_source_status_t st;

  priv = s->decoder_priv;
 
  if((st = bgav_stream_get_packet_read(s, &p)) != GAVL_SOURCE_OK)
    return st;
    
  priv->frame = frame;
  ret = priv->dec_func((uint8_t*)s, p->data, p->data_size, &priv->decinfo);

  if(frame)
    {
    frame->timestamp = p->pts;
    frame->duration = p->duration;
    }

  bgav_stream_done_packet_read(s, p);
  return GAVL_SOURCE_OK;
  }

static void close_xadll(bgav_stream_t * s)
  {
  xanim_priv_t * priv;
  priv = s->decoder_priv;

  dlclose(priv->dll_handle);

  free(priv);
  }
                        
static bgav_video_decoder_t decoder =
  {
    .name =   "Xanim dll decoder",
    .fourccs = (uint32_t[]){ BGAV_MK_FOURCC('3','I','V','1'),
                      BGAV_MK_FOURCC('3','i','v','1'),
                      0x0 },
    .init =   init_xadll,
    .decode = decode_xadll,
    .close =  close_xadll,
  };

int bgav_init_video_decoders_xadll(bgav_options_t * opt)
  {
  struct stat stat_buf;
  char dll_filename[PATH_MAX];
  sprintf(dll_filename, "%s/%s", bgav_dll_path_xanim, "vid_3ivX.xa");

  if(!stat(dll_filename, &stat_buf))
    {
    bgav_video_decoder_register(&decoder);
    return 1;
    }
  else
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "Cannot find file %s, disabling %s",
             dll_filename, decoder.name);
    return 0;
    }
  }

