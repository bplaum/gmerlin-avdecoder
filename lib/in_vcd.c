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

#include <avdec_private.h>
#include <stdio.h>

#define LOG_DOMAIN "vcd"

#ifdef HAVE_CDIO

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <cdio/cdio.h>
#include <cdio/device.h>
#include <cdio/cd_types.h>

#define SECTOR_ACCESS

extern bgav_demuxer_t bgav_demuxer_mpegps;

#define SECTOR_SIZE 2324

#define TRACK_OTHER 0
#define TRACK_VCD   1
#define TRACK_SVCD  2
#define TRACK_CVD   3

typedef struct
  {
  CdIo_t * cdio;

  int current_track;
  int next_sector; /* Next sector to be read */
  int last_sector; /* Sector currently in buffer */
  int num_tracks; 
  int num_vcd_tracks; 
  
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
  priv->next_sector = priv->tracks[priv->current_track].start_sector;
  priv->last_sector = -1;
  ctx->position = 0;
  ctx->total_bytes = SECTOR_SIZE *
    (priv->tracks[priv->current_track].end_sector -
     priv->tracks[priv->current_track].start_sector + 1);

  ctx->total_sectors = priv->tracks[priv->current_track].end_sector -
    priv->tracks[priv->current_track].start_sector + 1;
  ctx->sector_position = 0;
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

static void toc_2_tt(bgav_input_context_t * ctx, char * disk_name)
  {
  int index;
  bgav_stream_t * stream;
  bgav_track_t * track;
  gavl_dictionary_t * m;
  int i;
  vcd_priv * priv;

  const char * format = NULL;
  const char * mimetype = NULL;

  priv = ctx->priv;
  
  ctx->tt = bgav_track_table_create(priv->num_video_tracks);
  
  m = gavl_track_get_metadata_nc(&ctx->tt->info);
  gavl_dictionary_set_string_nocopy(m, GAVL_META_DISK_NAME, disk_name);    

  index = 0;
  for(i = 1; i < priv->num_tracks; i++)
    {
    if(priv->tracks[i].mode == TRACK_OTHER)
      continue;
    
    track = ctx->tt->tracks[index];

    m = track->metadata;
    
    stream =  bgav_track_add_audio_stream(track, ctx->opt);
    stream->fourcc = BGAV_MK_FOURCC('.', 'm', 'p', '2');
    stream->stream_id = 0xc0;
    stream->timescale = 90000;
        
    stream =  bgav_track_add_video_stream(track, ctx->opt);
    stream->fourcc = BGAV_MK_FOURCC('m', 'p', 'g', 'v');
    stream->stream_id = 0xe0;
    stream->timescale = 90000;
    if(priv->tracks[i].mode == TRACK_SVCD)
      {
      gavl_dictionary_set_string_nocopy(track->metadata, GAVL_META_LABEL,
                              bgav_sprintf("SVCD Track %d", i));
      format = "MPEG-2";
      mimetype = "video/MP2P";
      }
    else if(priv->tracks[i].mode == TRACK_CVD)
      {
      gavl_dictionary_set_string_nocopy(track->metadata, GAVL_META_LABEL,
                              bgav_sprintf("CVD Track %d", i));

      format = "MPEG-2";
      mimetype = "video/MP2P";
      }
    else
      {
      gavl_dictionary_set_string_nocopy(track->metadata, GAVL_META_LABEL,
                                        bgav_sprintf("VCD Track %d", i));
      format = "MPEG-1";
      mimetype = "video/MP1S";
      }
    
    gavl_dictionary_set_string(track->metadata, GAVL_META_MEDIA_CLASS,
                               GAVL_META_MEDIA_CLASS_VIDEO_DISK_TRACK);
    
    bgav_track_set_format(track, format, mimetype);
    
    index++;
    }
  }

static int open_vcd(bgav_input_context_t * ctx, const char * url, char ** r)
  {
  driver_return_code_t err;
  int i;
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



  /* Set up sector stuff */

#ifdef SECTOR_ACCESS
  ctx->sector_size        = 2324;
  ctx->sector_size_raw    = 2352;
  ctx->sector_header_size = 8;
#endif
  
  /* Create demuxer */
  
  ctx->demuxer = bgav_demuxer_create(ctx->b, &bgav_demuxer_mpegps, ctx);
  ctx->demuxer->tt = ctx->tt;

  /* Now, loop through all tracks and let the demuxer find the durations */

  for(i = 0; i < ctx->tt->num_tracks; i++)
    {
    select_track_vcd(ctx, i);
    bgav_track_table_select_track(ctx->tt, i);
    bgav_demuxer_start(ctx->demuxer);
    bgav_demuxer_stop(ctx->demuxer);
    }
  ctx->flags |= BGAV_INPUT_CAN_PAUSE;
  return 1;
  }

static int read_sector(bgav_input_context_t * ctx, uint8_t * data)
  {
  vcd_priv * priv;
  priv = ctx->priv;

  //  do
  //    {

  //  fprintf(stderr, "Read vcd sector %d...", priv->next_sector);

  if(priv->next_sector > priv->tracks[priv->current_track].end_sector)
    {
    //    fprintf(stderr, "failed (end of track)\n");
    
    return 0;
    }
  if(cdio_read_mode2_sector(priv->cdio, data, priv->next_sector, true)!=0)
    {
    //    fprintf(stderr, "failed (internal error)\n");
    return 0;
    }
  priv->next_sector++;

  priv->last_sector = priv->next_sector - 1;

  //  fprintf(stderr, "Read vcd sector done\n");
  
  
  //  gavl_hexdump(data, 16, 16);
  return 1;
  }

static int64_t seek_sector_vcd(bgav_input_context_t * ctx,
                               int64_t sector)
  {
  vcd_priv * priv;
  priv = ctx->priv;

  priv->next_sector = sector + priv->tracks[priv->current_track].start_sector;
  return priv->next_sector;
  }

static void    close_vcd(bgav_input_context_t * ctx)
  {
  vcd_priv * priv;
  priv = ctx->priv;
  if(priv->cdio)
    cdio_destroy(priv->cdio);
  if(priv->tracks)
    free(priv->tracks);
  free(priv);
  return;
  }

const bgav_input_t bgav_input_vcd =
  {
    .name =          "vcd",
    .open =          open_vcd,
    .read_sector =   read_sector,
    .seek_sector =   seek_sector_vcd,
    .close =         close_vcd,
    .select_track =  select_track_vcd,
  };

static char * get_device_name(CdIo_t * cdio,
                              cdio_drive_read_cap_t  read_cap,
                              cdio_drive_write_cap_t write_cap,
                              const char * device)
  {
  cdio_hwinfo_t driveid;

  if(cdio_get_hwinfo(cdio, &driveid) &&
     (driveid.psz_model[0] != '\0'))
    {
    return bgav_sprintf("%s %s", driveid.psz_vendor, driveid.psz_model);
    }
  
  if(write_cap & CDIO_DRIVE_CAP_WRITE_DVD_R)
    {
    return bgav_sprintf("DVD Writer (%s)", device);
    }
  else if(write_cap & CDIO_DRIVE_CAP_WRITE_CD_R)
    {
    return bgav_sprintf("CD Writer (%s)", device);
    }
  else if(read_cap & CDIO_DRIVE_CAP_READ_DVD_ROM)
    {
    return bgav_sprintf("DVD Drive (%s)", device);
    }
  return bgav_sprintf("CDrom Drive (%s)", device);
  }

int bgav_check_device_vcd(const char * device, char ** name)
  {
  CdIo_t * cdio;
  cdio_drive_read_cap_t  read_cap = 0;
  cdio_drive_write_cap_t write_cap = 0;
  cdio_drive_misc_cap_t  misc_cap = 0;

  cdio = cdio_open (device, DRIVER_DEVICE);
  if(!cdio)
    return 0;
  
  cdio_get_drive_cap(cdio, &read_cap, &write_cap, &misc_cap);

  if(!(read_cap & CDIO_DRIVE_CAP_READ_MODE2_FORM2))
    {
    cdio_destroy(cdio);
    return 0;
    }
  
  /* Seems the drive is ok */

  if(name)
    *name = get_device_name(cdio, read_cap, write_cap, device);
  cdio_destroy(cdio);
  return 1;
  }

bgav_device_info_t * bgav_find_devices_vcd()
  {
  int i;
  char * device_name;
  char ** devices;
  bgav_device_info_t * ret = NULL;

  devices = cdio_get_devices(DRIVER_DEVICE);

  if(!devices)
    return 0;
    
  i = 0;
  while(devices[i])
    {
    device_name = NULL;
    if(bgav_check_device_vcd(devices[i], &device_name))
      {
      ret = bgav_device_info_append(ret,
                                  devices[i],
                                  device_name);
      if(device_name)
        free(device_name);
      }
    i++;
    }
  cdio_free_device_list(devices);
  return ret;

  }

static
bgav_input_context_t * bgav_input_open_vcd(const char * device,
                                           bgav_t * b)
  {
  char * tmp_string;
  bgav_input_context_t * ret = NULL;
  ret = bgav_input_create(b, NULL);

  tmp_string = bgav_sprintf("vcd://%s", device);
  gavl_metadata_add_src(&ret->m, GAVL_META_SRC, NULL, tmp_string);
  free(tmp_string);
  
  ret->input = &bgav_input_vcd;

  if(!ret->input->open(ret, device, NULL))
    {
    goto fail;
    }
  return ret;
  fail:
  if(ret)
    free(ret);
  return NULL;
  }

int bgav_open_vcd(bgav_t * b, const char * device)
  {
  bgav_codecs_init(&b->opt);
  b->input = bgav_input_open_vcd(device, b);
  if(!b->input)
    return 0;
  if(!bgav_init(b))
    goto fail;
  return 1;
  fail:
  return 0;
  
  }

int bgav_eject_disc(const char * device)
  {
#if LIBCDIO_VERSION_NUM >= 78
  
  driver_return_code_t err;
  if((err = cdio_eject_media_drive(device)) != DRIVER_OP_SUCCESS)
    return 0;
  else
    return 1;
#else
  return 0;
#endif
  }

#else /* !HAVE_CDIO */


int bgav_check_device_vcd(const char * device, char ** name)
  {
  return 0;
  }

bgav_device_info_t * bgav_find_devices_vcd()
  {
  return NULL;
  }

int bgav_open_vcd(bgav_t * b, const char * device)
  {
  return 0;
  }

int bgav_eject_disc(const char * device)
  {
  return 0;
  }

#endif
