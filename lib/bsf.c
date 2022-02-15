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

#include <stdlib.h>


#include <config.h>
#include <avdec_private.h>
#include <bsf.h>
#include <bsf_private.h>
// #include <utils.h>

#define LOG_DOMAIN "bsf"

typedef struct
  {
  uint32_t fourcc;
  int (*init_func)(bgav_bsf_t*);
  } filter_t;

static const filter_t filters[] =
  {
    { BGAV_MK_FOURCC('a', 'v', 'c', '1'), bgav_bsf_init_avcC },
#ifdef HAVE_AVCODEC
    { BGAV_MK_FOURCC('A', 'D', 'T', 'S'), bgav_bsf_init_adts },
#endif
  };

void bgav_bsf_run(bgav_bsf_t * bsf, bgav_packet_t * in, bgav_packet_t * out)
  {
  /* Set packet fields now, so the filter has a chance
     to overwrite them */
  
  out->flags = in->flags;
  out->pts = in->pts;
  out->dts = in->dts;
  out->duration = in->duration;
    
  bsf->filter(bsf, in, out);
  }

gavl_source_status_t
bgav_bsf_get_packet(void * bsf_p, bgav_packet_t ** ret)
  {
  bgav_packet_t * in_packet;
  gavl_source_status_t st;
  bgav_bsf_t * bsf = bsf_p;

  if(bsf->out_packet)
    {
    *ret = bsf->out_packet;
    bsf->out_packet = NULL;
    return GAVL_SOURCE_OK;
    }

  in_packet = NULL;
  if((st = bsf->src.get_func(bsf->src.data, &in_packet)) != GAVL_SOURCE_OK)
    return st;
  
  *ret = bgav_packet_pool_get(bsf->s->pp);
  bgav_bsf_run(bsf, in_packet, *ret);

  bgav_packet_pool_put(bsf->s->pp, in_packet);
  return GAVL_SOURCE_OK;
  }

gavl_source_status_t
bgav_bsf_peek_packet(void * bsf_p, bgav_packet_t ** ret, int force)
  {
  gavl_source_status_t st;
  bgav_bsf_t * bsf = bsf_p;
  bgav_packet_t * in_packet;

  if(bsf->out_packet)
    {
    if(ret)
      *ret = bsf->out_packet;
    return GAVL_SOURCE_OK;
    }

  if((st = bsf->src.peek_func(bsf->src.data, &in_packet, force)) !=
     GAVL_SOURCE_OK)
    return st;
  
  /* We are eating up this packet so we need to remove it from the
     packet buffer */
  bsf->src.get_func(bsf->src.data, &in_packet);
  
  if(!in_packet)
    return GAVL_SOURCE_EOF; // Impossible but who knows?
  
  bsf->out_packet = bgav_packet_pool_get(bsf->s->pp);
  bgav_bsf_run(bsf, in_packet, bsf->out_packet);
  bgav_packet_pool_put(bsf->s->pp, in_packet);

  if(ret)
    *ret = bsf->out_packet;
  return GAVL_SOURCE_OK;
  }

bgav_bsf_t * bgav_bsf_create(bgav_stream_t * s)
  {
  const filter_t * f = NULL;
  bgav_bsf_t * ret;
  int i;
  
  for(i = 0; i < sizeof(filters)/sizeof(filters[0]); i++)
    {
    if(s->fourcc == filters[i].fourcc)
      {
      f = &filters[i];
      break;
      }
    }
  if(!f)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "No bitstream filter found");
    return NULL;
    }
  ret = calloc(1, sizeof(*ret));
  ret->s = s;
  bgav_packet_source_copy(&ret->src, &s->src);
  
  s->src.get_func = bgav_bsf_get_packet;
  s->src.peek_func = bgav_bsf_peek_packet;
  s->src.data = ret;
  
  if(!filters[i].init_func(ret))
    {
    free(ret);
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Initialitzing bitstream filter failed");
    return NULL;
    }

  if(ret->ci.id != GAVL_CODEC_ID_NONE)
    {
    s->ci = &ret->ci;
    gavl_stream_set_compression_info(ret->s->info, s->ci);
    }
  return ret;
  }

void bgav_bsf_destroy(bgav_bsf_t * bsf)
  {
  if(bsf->cleanup)
    bsf->cleanup(bsf);

  /* Restore extradata */

  bsf->s->ci = &bsf->s->ci_orig;
  gavl_stream_set_compression_info(bsf->s->info, bsf->s->ci);
  
  /* Warning: This breaks when the bsf is *not* the last element
     in the processing chain */
  bgav_packet_source_copy(&bsf->s->src, &bsf->src);

  gavl_compression_info_free(&bsf->ci);
  
  free(bsf);
  }

