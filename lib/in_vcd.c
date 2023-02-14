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

#include <stdio.h>

#define LOG_DOMAIN "vcd"

#include <config.h>
#ifdef HAVE_CDIO

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <cdio/cdio.h>
#include <cdio/device.h>
#include <cdio/cd_types.h>

#include <avdec_private.h>
#include <pes_header.h>

#define SECTOR_ACCESS


#define SECTOR_SIZE     2324
#define SECTOR_SIZE_RAW 2352
#define HEADER_SIZE     8


#define TRACK_OTHER 0
#define TRACK_VCD   1
#define TRACK_SVCD  2
#define TRACK_CVD   3

typedef struct
  {
  CdIo_t * cdio;

  int current_track;

  //  int next_sector; /* Next sector to be read */
  int current_sector;
  
  int num_tracks; 
  int num_vcd_tracks; 
  uint8_t * sector_buffer;
  
  struct
    {
    uint32_t start_sector;
    uint32_t end_sector;
    int mode; /* A TRACK_* define from above */
    } * tracks;
  
  int num_video_tracks;
    
  } vcd_priv;

static int select_track_vcd(bgav_input_context_t * ctx, int track)
  {
  vcd_priv * priv;
  priv = ctx->priv;
  
  priv->current_track = track+1;
  priv->current_sector = -1;
  ctx->position = priv->tracks[priv->current_track].start_sector * SECTOR_SIZE;
  
  return 1;
  }

static int read_toc(vcd_priv * priv, char ** iso_label)
  {
  int i, j;
  cdio_iso_analysis_t iso;
  cdio_fs_anal_t fs;
  int first_track;
  
  priv->num_tracks = cdio_get_last_track_num(priv->cdio);
  if(priv->num_tracks == CDIO_INVALID_TRACK)
    {
    return 0;
    }
  /* VCD needs at least 2 tracks */
  if(priv->num_tracks < 2)
    return 0;
  
  priv->tracks = calloc(priv->num_tracks, sizeof(*(priv->tracks)));

  priv->num_video_tracks = 0;
  first_track = cdio_get_first_track_num(priv->cdio);
  
  if(iso_label)
    {
    fs = cdio_guess_cd_type(priv->cdio, 0, first_track,
                            &iso);
    
    /* Remove trailing spaces */
    j = strlen(iso.iso_label)-1;
    while(j)
      {
      if(!isspace(iso.iso_label[j]))
        break;
      j--;
      }
    if(!j && isspace(iso.iso_label[j]))
      iso.iso_label[j] = '\0';
    else
      iso.iso_label[j+1] = '\0';
    
    *iso_label = gavl_strdup(iso.iso_label);
    
    priv->tracks[first_track - 1].mode = TRACK_OTHER;
    }
  /* Actually it's (first_track - 1) + 1 */
  for(i = first_track; i < priv->num_tracks; i++)
    {
    priv->tracks[i].start_sector = cdio_get_track_lsn(priv->cdio, i+1);
    priv->tracks[i].end_sector = cdio_get_track_last_lsn(priv->cdio, i+1);

    fs = cdio_guess_cd_type(priv->cdio, 0, i+1, &iso);

    if(fs & CDIO_FS_ANAL_VIDEOCD)
      {
      priv->num_video_tracks++;
      priv->tracks[i].mode = TRACK_VCD;
      }
    else if(fs & CDIO_FS_ANAL_SVCD)
      {
      priv->num_video_tracks++;
      priv->tracks[i].mode = TRACK_SVCD;
      }
    else if(fs & CDIO_FS_ANAL_CVD)
      {
      priv->tracks[i].mode = TRACK_CVD;
      priv->num_video_tracks++;
      }
    else if(fs & CDIO_FS_ANAL_ISO9660_ANY)
      {
      priv->tracks[i].mode = TRACK_VCD;
      priv->num_video_tracks++;
      }

    }
  if(!priv->num_video_tracks)
    {
    free(priv->tracks);
    priv->tracks = NULL;
    return 0;
    }
  return 1;
  }

#if 0
static int64_t read_scr(bgav_input_context_t * ctx, int sector)
  {
  vcd_priv * priv;
  int64_t ret = GAVL_TIME_UNDEFINED;
  uint8_t buf[SECTOR_SIZE_RAW];
  uint8_t * ptr;
  bgav_input_context_t * in_mem;
  bgav_pack_header_t     pack_header;
  uint32_t header;
  
  priv = ctx->priv;

  memset(&pack_header, 0, sizeof(pack_header));
  
  if(cdio_read_mode2_sector(priv->cdio, buf, sector, true) !=0)
    {
    //    fprintf(stderr, "failed (internal error)\n");
    return GAVL_TIME_UNDEFINED;
    }

  ptr = &buf[HEADER_SIZE];

  header = GAVL_PTR_2_32BE(ptr);
  if(header != START_CODE_PACK_HEADER)
    {
    fprintf(stderr, "No pack header in sector %d\n", sector);
    gavl_hexdump(ptr, 16, 16);
    return GAVL_TIME_UNDEFINED;
    }
  in_mem = bgav_input_open_memory(ptr, SECTOR_SIZE);

  gavl_hexdump(&buf[HEADER_SIZE], 16, 16);
  
  if(bgav_pack_header_read(in_mem, &pack_header))
    ret = pack_header.scr;
  
  bgav_input_destroy(in_mem);
  return ret;
  }
#endif

static void toc_2_tt(bgav_input_context_t * ctx, char * disk_name)
  {
  int index;
  bgav_stream_t * as;
  bgav_stream_t * vs;
  bgav_track_t * track;
  gavl_dictionary_t * m;
  gavl_dictionary_t * src;
  int i;
  vcd_priv * priv;

  gavl_time_t duration;
  
  const char * format = NULL;
  const char * mimetype = NULL;

  priv = ctx->priv;
  
  ctx->tt = bgav_track_table_create(priv->num_video_tracks);
  
  m = gavl_track_get_metadata_nc(&ctx->tt->info);
  gavl_dictionary_set_string_nocopy(m, GAVL_META_DISK_NAME, disk_name);    

  index = 0;
  for(i = 1; i < priv->num_tracks; i++)
    {
    duration = GAVL_TIME_UNDEFINED;
    
    if(priv->tracks[i].mode == TRACK_OTHER)
      continue;
    
    track = ctx->tt->tracks[index];

    track->data_start = priv->tracks[i].start_sector   * SECTOR_SIZE;
    track->data_end   = (priv->tracks[i].end_sector+1) * SECTOR_SIZE;
    ctx->total_bytes = track->data_end;
    
    m = track->metadata;
    
    as =  bgav_track_add_audio_stream(track, &ctx->opt);
    as->fourcc = BGAV_MK_FOURCC('.', 'm', 'p', '2');
    as->stream_id = 0xc0;
    as->timescale = 90000;
        
    vs =  bgav_track_add_video_stream(track, &ctx->opt);
    vs->fourcc = BGAV_MK_FOURCC('m', 'p', 'g', 'v');
    vs->stream_id = 0xe0;
    vs->timescale = 90000;
    
    if(priv->tracks[i].mode == TRACK_SVCD)
      {
      gavl_dictionary_set_string_nocopy(track->metadata, GAVL_META_LABEL,
                              bgav_sprintf("SVCD Track %d", i));
      format = "MPEG-2";

      if(!mimetype)
        mimetype = "video/MP2P";
      }
    else if(priv->tracks[i].mode == TRACK_CVD)
      {
      gavl_dictionary_set_string_nocopy(track->metadata, GAVL_META_LABEL,
                              bgav_sprintf("CVD Track %d", i));

      format = "MPEG-2";
      
      if(!mimetype)
        mimetype = "video/MP2P";
      }
    else
      {
      gavl_dictionary_set_string_nocopy(track->metadata, GAVL_META_LABEL,
                                        bgav_sprintf("VCD Track %d", i));
      format = "MPEG-1";
      if(!mimetype)
        mimetype = "video/MP1S";

      /* VCD timing is the same as Audio CD timing */
      duration = gavl_time_unscale(75, (track->data_end - track->data_start) / SECTOR_SIZE);
      //      fprintf(stderr, "Got VCD duration\n");
      }
    
    gavl_dictionary_set_string(track->metadata, GAVL_META_MEDIA_CLASS,
                               GAVL_META_MEDIA_CLASS_VIDEO_DISK_TRACK);
    
    bgav_track_set_format(track, format, mimetype);
    index++;

    if(duration != GAVL_TIME_UNDEFINED)
      gavl_track_set_duration(track->info, duration);
    }

  if(mimetype && (src = gavl_metadata_get_src_nc(&ctx->m, GAVL_META_SRC, 0)))
    gavl_dictionary_set_string(src, GAVL_META_MIMETYPE, mimetype);

  //  fprintf(stderr, "Got track table");
  //  gavl_dictionary_dump(&ctx->tt->info, 2);

  
  }

static int open_vcd(bgav_input_context_t * ctx, const char * url, char ** r)
  {
  driver_return_code_t err;
  vcd_priv * priv;
  const char * pos;
  char * disk_name = NULL;
  //  bgav_find_devices_vcd();

  priv = calloc(1, sizeof(*priv));
  
  ctx->priv = priv;


  pos = strrchr(url, '.');
  if(pos && !strcasecmp(pos, ".cue"))
    priv->cdio = cdio_open (url, DRIVER_BINCUE);
  else
    {
    if((err = cdio_close_tray(url, NULL)))
#if LIBCDIO_VERSION_NUM >= 77
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "cdio_close_tray failed: %s",
              cdio_driver_errmsg(err));
#else
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "cdio_close_tray failed");
#endif    
    
    priv->cdio = cdio_open (url, DRIVER_DEVICE);

    /* Close tray, hope this won't be harmful if the
       tray is already closed */

    }
  if(!priv->cdio)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "cdio_open failed for %s", url);
    return 0;
    }
  /* Get some infos */

  //  dump_cdrom(priv->fd);
  
  /* Read TOC */
  
  if(!read_toc(priv, &disk_name))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "read_toc failed for %s", url);
    return 0;
    }

  toc_2_tt(ctx, disk_name);
  
  ctx->block_size = SECTOR_SIZE;
  
  ctx->flags |= BGAV_INPUT_CAN_PAUSE;
  return 1;
  }

static int read_block(bgav_input_context_t * ctx)
  {
  vcd_priv * priv;
  priv = ctx->priv;

  if(!priv->sector_buffer)
    priv->sector_buffer = malloc(SECTOR_SIZE_RAW);
  
  //  do
  //    {

  //  fprintf(stderr, "Read vcd sector %d...", priv->next_sector);

  if(priv->current_sector < 0)
    priv->current_sector = priv->tracks[priv->current_track].start_sector;
  else
    priv->current_sector++;
  
  if(priv->current_sector > priv->tracks[priv->current_track].end_sector)
    {
    //    fprintf(stderr, "failed (end of track)\n");
    
    return 0;
    }
  if(cdio_read_mode2_sector(priv->cdio, priv->sector_buffer, priv->current_sector, true)!=0)
    {
    //    fprintf(stderr, "failed (internal error)\n");
    return 0;
    }
  
  //  fprintf(stderr, "Read vcd sector done\n");

  ctx->block = priv->sector_buffer + HEADER_SIZE;
  
  //  fprintf(stderr, "Read vcd sector %d\n", priv->current_sector);
  //  gavl_hexdump(ctx->block, 16, 16);
  
  //  gavl_hexdump(data, 16, 16);
  return 1;
  }

static int seek_block_vcd(bgav_input_context_t * ctx,
                          int64_t sector)
  {
  vcd_priv * priv;
  priv = ctx->priv;

  if(priv->current_sector == sector)
    return 1;
  
  if((sector < 0) && (sector * SECTOR_SIZE >= (ctx->total_bytes)))
    return 0;
  
  priv->current_sector = sector;
  if(cdio_read_mode2_sector(priv->cdio, priv->sector_buffer, priv->current_sector, true)!=0)
    {
    //    fprintf(stderr, "failed (internal error)\n");
    return 0;
    }
  ctx->block = priv->sector_buffer + HEADER_SIZE;
  return 1;
  }

static void close_vcd(bgav_input_context_t * ctx)
  {
  vcd_priv * priv;
  priv = ctx->priv;
  if(priv->cdio)
    cdio_destroy(priv->cdio);
  if(priv->tracks)
    free(priv->tracks);
  if(priv->sector_buffer)
    free(priv->sector_buffer);

  free(priv);
  return;
  }

const bgav_input_t bgav_input_vcd =
  {
    .name =          "vcd",
    .open =          open_vcd,
    .read_block =    read_block,
    .seek_block =    seek_block_vcd,
    .close =         close_vcd,
    .select_track =  select_track_vcd,
  };

#endif
