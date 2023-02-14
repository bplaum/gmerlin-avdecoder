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
#include <string.h>

#include <dvdnav/dvdnav.h>

#include <config.h>
#include <avdec_private.h>

#define LOG_DOMAIN "dvd"

#define META_TITLE "title"
#define META_ANGLE "angle"

typedef struct
  {
  dvdnav_t * d;
  uint8_t block[2048];
  int blocks_read;

  int current_title;
  int current_angle;
  
  } dvd_priv_t;


static void log_func(void * priv, dvdnav_logger_level_t level, const char * fmt, va_list list)
  {
  gavl_log_level_t gavl_level;

  switch(level)
    {
    case DVDNAV_LOGGER_LEVEL_ERROR:
      gavl_level = GAVL_LOG_ERROR;
      break;
    case DVDNAV_LOGGER_LEVEL_WARN:
      gavl_level = GAVL_LOG_WARNING;
      break;
    case DVDNAV_LOGGER_LEVEL_DEBUG:
      gavl_level = GAVL_LOG_DEBUG;
      break;
    default:
      gavl_level = GAVL_LOG_INFO;
      break;
    }
  gavl_logv(gavl_level, "libdvdnav", fmt, list);
  }
  
static const dvdnav_logger_cb log_cb =
  {
    .pf_log = log_func,
  };

static const struct
  {
  uint32_t image_width;
  uint32_t image_height;
  uint32_t pixel_width_4_3;
  uint32_t pixel_height_4_3;
  uint32_t pixel_width_16_9;
  uint32_t pixel_height_16_9;
  }
frame_sizes[] =
  {
    { 720, 576, 59, 54, 118, 81 }, /* PAL  */
    { 720, 480, 10, 11, 40,  33 }, /* NTSC */
    { 352, 576, 59, 27,  0,   0 }, /* PAL CVD */
    { 352, 480, 20, 11,  0,   0 }, /* NTSC CVD */
    { 352, 288, 59, 54,  0,   0 }, /* PAL VCD */
    { 352, 480, 10, 11,  0,   0 }, /* NTSC VCD */
  };

static void guess_pixel_aspect(int width, int height, int aspect,
                               uint32_t * pixel_width,
                               uint32_t * pixel_height)
  {
  int i;
  for(i = 0; i < sizeof(frame_sizes)/sizeof(frame_sizes[0]); i++)
    {
    if((frame_sizes[i].image_width == width) &&
       (frame_sizes[i].image_height == height))
      {
      if(aspect == 0) /* 4:3 */
        {
        *pixel_width  = frame_sizes[i].pixel_width_4_3;
        *pixel_height = frame_sizes[i].pixel_height_4_3;
        }
      else if(aspect == 2) /* 16:9 */
        {
        *pixel_width  = frame_sizes[i].pixel_width_16_9;
        *pixel_height = frame_sizes[i].pixel_height_16_9;
        }
      return;
      }
    }
  }


static int open_dvd(bgav_input_context_t * ctx, const char * url, char ** r)
  {
  int i;
  int ret = 0;
  dvd_priv_t * priv;
  int32_t num_titles;
  int32_t num_chapters;
  int32_t num_angles;
  gavl_dictionary_t * src;
  const char * disk_name = NULL;
  
  if(gavl_string_starts_with(url, "dvd://"))
    url += 6;
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;
  ctx->block_size = 2048;
  
  if(dvdnav_open2(&priv->d, NULL, &log_cb, url) != DVDNAV_STATUS_OK)
    goto fail;

  /* Read track table */
  if(dvdnav_get_number_of_titles(priv->d, &num_titles) != DVDNAV_STATUS_OK)
    goto fail;

  ctx->tt = bgav_track_table_create(0);

  if(dvdnav_get_title_string(priv->d, &disk_name) == DVDNAV_STATUS_OK)
    {
    gavl_dictionary_t * m;
    m = gavl_track_get_metadata_nc(&ctx->tt->info);
    gavl_dictionary_set_string(m, GAVL_META_DISK_NAME, disk_name);
    }
  
  fprintf(stderr, "Got %d titles\n", num_titles);

  for(i = 1; i <= num_titles; i++)
    {
    int j;
    uint64_t * times;
    uint64_t duration;
    char * label;
    bgav_track_t * new_track;
    int32_t event, event_len;
    uint32_t palette[16];
    int done;
    
    num_chapters = 0;
    num_angles = 0;
    dvdnav_get_number_of_parts(priv->d, i, &num_chapters);
    dvdnav_get_number_of_angles(priv->d, i, &num_angles);
    
    fprintf(stderr, "Title %d: %d chapters, %d angles\n", i, num_chapters, num_angles);

    if(dvdnav_describe_title_chapters(priv->d, i, &times, &duration) != num_chapters)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Chapter count mismatch");
      goto fail;
      }

    fprintf(stderr, "  Duration: %"PRId64"\n", duration);
        
    for(j = 0; j < num_chapters; j++)
      {
      fprintf(stderr, "  Chapter time %d: %"PRId64" (%f)\n", j+1, times[j], (double)times[j] / 90000.0);
      }

    /* Ignore titles < 10 sec */
    if(duration < (uint64_t)10*90000)
      {
      free(times);
      continue;
      }
    
    for(j = 0; j < num_angles; j++)
      {
      bgav_stream_t * s;
      int k;
      uint32_t width;
      uint32_t height;
      uint16_t lang;
      int stream_position;
      char language_2cc[3];
      int aspect;
      gavl_dictionary_t * dict;
      const gavl_video_format_t * vfmt; // Video stream format
      
      /* Select track and angle and start vm */
      dvdnav_title_play(priv->d, i);
      dvdnav_angle_change(priv->d, j);
      
      done = 0;
      
      while(dvdnav_get_next_block(priv->d, priv->block, &event, &event_len) == DVDNAV_STATUS_OK)
        {
        switch(event)
          {
          case DVDNAV_STOP:
          case DVDNAV_BLOCK_OK:
            done = 1;
            break;
          case DVDNAV_SPU_CLUT_CHANGE:
            /*
             * DVDNAV_SPU_CLUT_CHANGE
             *
             * Inform the SPU decoder/overlaying engine to update its colour lookup table.
             * The CLUT is given as 16 uint32_t's in the buffer.
             */
            fprintf(stderr, "DVDNAV_SPU_CLUT_CHANGE\n");
            memcpy(palette, priv->block, 16*4);
            break;
          case DVDNAV_WAIT:
            dvdnav_wait_skip(priv->d);
            break;
          }
        if(done)
          break;
        }
  
      
      new_track = bgav_track_table_append_track(ctx->tt);

      dict = bgav_track_get_priv_nc(new_track);
      gavl_dictionary_set_int(dict, META_TITLE, i);
      gavl_dictionary_set_int(dict, META_ANGLE, j);
      
      if(num_angles > 1)
        label = gavl_sprintf("Title %d angle %d", i+1, j+1);
      else
        label = gavl_sprintf("Title %d", i+1);

      gavl_dictionary_set_string_nocopy(new_track->metadata, GAVL_META_LABEL, label);
      gavl_dictionary_set_long(new_track->metadata, GAVL_META_APPROX_DURATION, gavl_time_unscale(90000, duration));

      gavl_dictionary_set_string(new_track->metadata, GAVL_META_MEDIA_CLASS,
                                 GAVL_META_MEDIA_CLASS_VIDEO_DISK_TRACK);

      dvdnav_title_play(priv->d, i);

      /* Video format */
      s = bgav_track_add_video_stream(new_track, &ctx->opt);
      dvdnav_get_video_resolution(priv->d, &width, &height);

      s->data.video.format->image_width = width;
      s->data.video.format->image_height = height;

      vfmt = s->data.video.format;
      aspect = dvdnav_get_video_aspect(priv->d);

      guess_pixel_aspect(width, height, aspect,
                         &s->data.video.format->pixel_width,
                         &s->data.video.format->pixel_height);
      
      s->fourcc = BGAV_MK_FOURCC('m', 'p', 'g', 'v');
      s->stream_id = 0xE0;
      s->timescale = 90000;
      
      k = 0;
      while((lang = dvdnav_audio_stream_to_lang(priv->d, k)) != 0xffff)
        {
        audio_attr_t attr;
        const char * audio_codec;
        
        /* It seems that dvdnav_get_audio_logical_stream() does the opposite of what
           the name suggests */
        stream_position = dvdnav_get_audio_logical_stream(priv->d, k);
        
        s = bgav_track_add_audio_stream(new_track, &ctx->opt);
        s->timescale = 90000;

        dvdnav_get_audio_attr(priv->d, k, &attr);

        switch(attr.audio_format)
          {
          case 0:
            //        printf("ac3 ");
            s->fourcc = BGAV_MK_FOURCC('.', 'a', 'c', '3');
            audio_codec = "AC3";
            s->stream_id = 0xbd80 + stream_position;
            break;
          case 2:
            //        printf("mpeg1 ");
            s->fourcc = BGAV_MK_FOURCC('.', 'm', 'p', '2');
            s->stream_id = 0xc0 + stream_position;
            audio_codec = "MPA";
            break;
          case 3:
            //        printf("mpeg2ext ");
            /* Unsupported */
            s->fourcc = BGAV_MK_FOURCC('m', 'p', 'a', 'e');
            audio_codec = "MPAext";
            break;
          case 4:
            //        printf("lpcm ");
            s->fourcc = BGAV_MK_FOURCC('L', 'P', 'C', 'M');
            s->stream_id = 0xbda0 + stream_position;
            audio_codec = "LPCM";
            break;
          case 6:
            //        printf("dts ");
            s->fourcc = BGAV_MK_FOURCC('d', 't', 's', ' ');
            s->stream_id = 0xbd88 + stream_position;
            audio_codec = "DTS";
            break;
          default:
            //        printf("(please send a bug report) ");
            break;
          }

        language_2cc[0] = attr.lang_code >> 8;
        language_2cc[1] = attr.lang_code & 0xff;
        language_2cc[2] = '\0';
        gavl_dictionary_set_string(s->m, GAVL_META_LANGUAGE,
                                   gavl_language_get_iso639_2_b_from_code(language_2cc));

        switch(attr.code_extension)
          {
          case 0:
            gavl_dictionary_set_string_nocopy(s->m, GAVL_META_LABEL,
                                              bgav_sprintf("Unspecified (%s, %dch)",
                                                           audio_codec, attr.channels+1));
            break;
          case 1:
            gavl_dictionary_set_string_nocopy(s->m, GAVL_META_LABEL,
                                              bgav_sprintf("Audio stream (%s, %dch)",
                                                           audio_codec, attr.channels+1));
            break;
          case 2:
            gavl_dictionary_set_string_nocopy(s->m, GAVL_META_LABEL,
                                              bgav_sprintf("Audio for visually impaired (%s, %dch)",
                                                           audio_codec, attr.channels+1));
            break;
          case 3:
            gavl_dictionary_set_string_nocopy(s->m, GAVL_META_LABEL,
                                              bgav_sprintf("Director's comments 1 (%s, %dch)",
                                                           audio_codec, attr.channels+1));
            break;
          case 4:
            gavl_dictionary_set_string_nocopy(s->m, GAVL_META_LABEL,
                                              bgav_sprintf("Director's comments 2 (%s, %dch)",
                                                           audio_codec, attr.channels+1));
            break;
          }
        
        k++;
        }
      k = 0;
      /* Subtitles */
      while((lang = dvdnav_spu_stream_to_lang(priv->d, k)) != 0xffff)
        {
        subp_attr_t attr;
        s = bgav_track_add_overlay_stream(new_track, &ctx->opt);
        s->timescale = 90000;

        dvdnav_get_spu_attr(priv->d, k, &attr);

        stream_position = dvdnav_get_spu_logical_stream(priv->d, k);
        s->fourcc = BGAV_MK_FOURCC('D', 'V', 'D', 'S');
        s->stream_id = 0xbd20 + stream_position;
        s->timescale = 90000;

        dvdnav_get_spu_attr(priv->d, k, &attr);

        if(attr.type == 1)
          {
          language_2cc[0] = attr.lang_code >> 8;
          language_2cc[1] = attr.lang_code & 0xff;
          language_2cc[2] = '\0';

          gavl_dictionary_set_string(s->m, GAVL_META_LANGUAGE,
                                     gavl_language_get_iso639_2_b_from_code(language_2cc));
          }
        
        switch(attr.code_extension)
          {
          case 0:
            //        printf("Not specified ");
            break;
          case 1:
            gavl_dictionary_set_string(s->m, GAVL_META_LABEL,
                                       "Caption");
            //        printf("Caption with normal size character ");
            break;
          case 2:
            gavl_dictionary_set_string(s->m, GAVL_META_LABEL,
                                       "Caption, big");
            break;
          case 3:
            gavl_dictionary_set_string(s->m, GAVL_META_LABEL,
                                       "Caption for children");
            break;
          case 4:
            //        printf("reserved ");
            break;
          case 5:
            gavl_dictionary_set_string(s->m, GAVL_META_LABEL,
                                       "Closed caption");
            //        printf("Closed Caption with normal size character ");
            break;
          case 6:
            gavl_dictionary_set_string(s->m, GAVL_META_LABEL,
                                       "Closed caption, big");
            //        printf("Closed Caption with bigger size character ");
            break;
          case 7:
            gavl_dictionary_set_string(s->m, GAVL_META_LABEL,
                                       "Closed caption for children");
            //        printf("Closed Caption for children ");
            break;
          case 8:
            //        printf("reserved ");
            break;
          case 9:
            gavl_dictionary_set_string(s->m, GAVL_META_LABEL,
                                       "Forced caption");
            break;
          case 10:
            //        printf("reserved ");
            break;
          case 11:
            //        printf("reserved ");
            break;
          case 12:
            //        printf("reserved ");
            break;
          case 13:
            gavl_dictionary_set_string(s->m, GAVL_META_LABEL,
                                       "Directors comments");
            break;
          case 14:
            gavl_dictionary_set_string(s->m, GAVL_META_LABEL,
                                       "Directors comments, big");
            break;
          case 15:
            gavl_dictionary_set_string(s->m, GAVL_META_LABEL,
                                       "Directors comments for children");
            break;
          default:
            //        printf("(please send a bug report) ");
            break;
          }
        
        s->data.subtitle.video.format->image_width = vfmt->image_width;
        s->data.subtitle.video.format->image_height = vfmt->image_height;
        s->data.subtitle.video.format->pixel_width = vfmt->pixel_width;
        s->data.subtitle.video.format->pixel_height = vfmt->pixel_height;
        
        bgav_stream_set_extradata(s, (uint8_t*)palette, sizeof(palette));
        k++;
        }
      
      }
    
    free(times);
    }

  if((src = gavl_metadata_get_src_nc(&ctx->m, GAVL_META_SRC, 0)))
    gavl_dictionary_set_string(src, GAVL_META_MIMETYPE, "video/MP2P");
  
  ret = 1;
  fail:

  if(!ret)
    {
    
    }
  return ret;
  }

static int read_block_dvd(bgav_input_context_t * ctx)
  {
  int ret = 0;
  int done = 0;
  int32_t event, event_len;
  dvd_priv_t * priv = ctx->priv;

  while(dvdnav_get_next_block(priv->d, priv->block, &event, &event_len) == DVDNAV_STATUS_OK)
    {
    switch(event)
      {
      case DVDNAV_STOP:
        done = 1;
        fprintf(stderr, "DVDNAV_STOP\n");
        break;
      case DVDNAV_BLOCK_OK:
        ret = 1;
        done = 1;
        // fprintf(stderr, "DVDNAV_OK\n");
        priv->blocks_read++;
        break;
      case DVDNAV_WAIT:
        fprintf(stderr, "DVDNAV_WAIT\n");
        dvdnav_wait_skip(priv->d);
        fprintf(stderr, "DVDNAV_WAIT done\n");
        break;
      case DVDNAV_STILL_FRAME:
        fprintf(stderr, "DVDNAV_STILL_FRAME\n");
        dvdnav_still_skip(priv->d);
        fprintf(stderr, "DVDNAV_STILL_FRAME done\n");
        break;
      case DVDNAV_NAV_PACKET:
        fprintf(stderr, "DVD_NAV_PACKET\n");
        break;
      case DVDNAV_CELL_CHANGE:
        {
        int32_t title;
        int32_t part;
        
        fprintf(stderr, "DVDNAV_CELL_CHANGE\n");
        dvdnav_current_title_info(priv->d, &title,
                                  &part);
        if(title != priv->current_title)
          {
          fprintf(stderr, "Detected track change\n");
          done = 1;
          }
        }
        break;
      case DVDNAV_HOP_CHANNEL:
      case DVDNAV_SPU_CLUT_CHANGE:
      case DVDNAV_SPU_STREAM_CHANGE:
      case DVDNAV_AUDIO_STREAM_CHANGE:
      case DVDNAV_VTS_CHANGE:
        if(priv->blocks_read)
          done = 1;
        break;
      default:
        fprintf(stderr, "Unhandled event %d\n", event);
        break;
      }
    if(done)
      break;
    }
  ctx->block = priv->block;
  
  return ret;
  }

static int select_track_dvd(bgav_input_context_t * ctx, int track)
  {
  dvd_priv_t * priv = ctx->priv;
  
  const gavl_dictionary_t * dict = bgav_track_get_priv(ctx->tt->cur);

  gavl_dictionary_get_int(dict, META_TITLE, &priv->current_title);
  gavl_dictionary_get_int(dict, META_ANGLE, &priv->current_angle);
  priv->blocks_read = 0;
  
  /* Select track and angle and start vm */
  dvdnav_title_play(priv->d, priv->current_title);
  dvdnav_angle_change(priv->d, priv->current_angle);
  return 1;
  }

static void close_dvd(bgav_input_context_t * ctx)
  {
  dvd_priv_t * priv = ctx->priv;
  
  if(priv->d)
    dvdnav_close(priv->d);
  free(priv);
  }

const bgav_input_t bgav_input_dvd =
  {
    .name =          "dvd",
    .open =          open_dvd,
    .read_block =    read_block_dvd,
    //    .seek_time =     seek_time_dvd,
    .close =         close_dvd,
    .select_track =  select_track_dvd,
  };


