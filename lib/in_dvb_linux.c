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

#ifdef HAVE_LINUXDVB
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include <mpegts_common.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <poll.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <dvb_channels.h>

#include <stdio.h>

#define LOG_DOMAIN "dvb"

// #define DUMP_PMT

extern bgav_demuxer_t bgav_demuxer_mpegts;

#define FILTER_AUDIO 0
#define FILTER_PAT   0

#define FILTER_VIDEO 1

/* Limitation from demuxer API */
#define MAX_AUDIO_STREAMS 4
#define MAX_VIDEO_STREAMS 4
#define MAX_AC3_STREAMS 1

typedef struct
  {
  int fe_fd;
  int dvr_fd;

  int * filter_fds;
  int num_filter_fds;
  int filter_fds_alloc;
  int eit_filter;
  
  int eit_version;
  
  struct dvb_frontend_info fe_info;

  bgav_dvb_channel_info_t * channels;
  int num_channels;

  char * device_directory;
  char * filter_filename;
  char * dvr_filename;
  char * frontend_filename;

  char * channels_conf_file;
  
  int service_id;

  int name_changed;
  int metadata_changed;
  
  } dvb_priv_t;

static void set_num_filters(dvb_priv_t * priv, int num)
  {
  int i;
  for(i = 0; i < priv->num_filter_fds; i++)
    {
    if(priv->filter_fds[i] >= 0)
      close(priv->filter_fds[i]);
    priv->filter_fds[i] = -1;
    }
  if(priv->filter_fds_alloc < num)
    {
    priv->filter_fds_alloc = num;
    priv->filter_fds =
      realloc(priv->filter_fds,
              priv->filter_fds_alloc * sizeof(*priv->filter_fds));
    }
  priv->num_filter_fds = num;
  
  for(i = 0; i < num; i++)
    priv->filter_fds[i] = open(priv->filter_filename, O_RDWR);
  }

#if 0

static struct
  {
  enum fe_caps cap;
  char * name;
  }
dvb_caps[] =
  {
    { FE_CAN_INVERSION_AUTO, "INVERSION_AUTO" },
    { FE_CAN_FEC_1_2,        "FEC_1_2"        },
    { FE_CAN_FEC_2_3,        "FEC_2_3"        },
    { FE_CAN_FEC_3_4,        "FEC_3_4"        },
    { FE_CAN_FEC_4_5,        "FEC_4_5"        },
    { FE_CAN_FEC_5_6,        "FEC_5_6"        },
    { FE_CAN_FEC_6_7,        "FEC_6_7"        },
    { FE_CAN_FEC_7_8,        "FEC_7_8"        }, 
    { FE_CAN_FEC_8_9,        "FEC_8_9"        },
    { FE_CAN_FEC_AUTO,       "FEC_AUTO"       },
    { FE_CAN_QPSK,           "QPSK"           },
    { FE_CAN_QAM_16,         "QAM_16"         },
    { FE_CAN_QAM_32,         "QAM_32"         },
    { FE_CAN_QAM_64,         "QAM_64"         },
    { FE_CAN_QAM_128,        "QAM_128"        },
    { FE_CAN_QAM_256,        "QAM_256"        },
    { FE_CAN_QAM_AUTO,       "QAM_AUTO"       },
    { FE_CAN_TRANSMISSION_MODE_AUTO, "TRANSMISSION_MODE_AUTO" },
    { FE_CAN_BANDWIDTH_AUTO,         "BANDWIDTH_AUTO" },
    { FE_CAN_GUARD_INTERVAL_AUTO,    "GUARD_INTERVAL_AUTO" },
    { FE_CAN_HIERARCHY_AUTO,         "HIERARCHY_AUTO" },
    { FE_CAN_8VSB,                   "CAN_8VSB" },
    { FE_CAN_16VSB,                  "CAN_16VSB" },
    { FE_CAN_RECOVER,                "CAN_RECOVER" },
    { FE_CAN_MUTE_TS,                "CAN_MUTE_TS" },
  };

static void dump_frontend_info(struct dvb_frontend_info * info)
  {
  int i;
  bgav_dprintf("DVB frontend info\n");
  bgav_dprintf("  .name = %s\n", info->name);
  bgav_dprintf("  .type = ");
  
  switch(info->type)
    {
    case FE_QPSK:
      bgav_dprintf("QPSK (DVB-S)\n");
      break;
    case FE_QAM:
      bgav_dprintf("QAM (DVB-C)\n");
      break;
    case FE_OFDM:
      bgav_dprintf("QFDM (DVB-T)\n");
      break;
    case FE_ATSC:
      bgav_dprintf("ATSC (DBV-A)\n");
      break;
    }
  bgav_dprintf("  Frequency range: [%d..%d] (step: %d, tol: %d)\n",
               info->frequency_min,
               info->frequency_max,
               info->frequency_stepsize,
               info->frequency_tolerance);
  
  bgav_dprintf("  Symbol rate range: [%d..%d] (tol: %d)\n",
               info->symbol_rate_min,
               info->symbol_rate_max,
               info->symbol_rate_tolerance);
  
  bgav_dprintf("  capabilities: ");
  for(i = 0; i < sizeof(dvb_caps)/sizeof(dvb_caps[0]); i++)
    {
    if(info->caps & dvb_caps[i].cap)
      bgav_dprintf("%s ", dvb_caps[i].name);
    }
  bgav_dprintf("\n");
  }

#endif

static int set_diseqc(bgav_input_context_t * ctx, bgav_dvb_channel_info_t * c)
  {
  dvb_priv_t * priv;
  gavl_time_t delay_time = 15000;
  
  struct dvb_diseqc_master_cmd cmd =
    {{0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00}, 4};

  priv = ctx->priv;
  
  cmd.msg[3] = 0xf0 | ((c->sat_no * 4) & 0x0f) |
    (c->tone ? 1 : 0) | (c->pol ? 0 : 2);
  
  if (ioctl(priv->fe_fd, FE_SET_TONE, SEC_TONE_OFF) < 0)
    return 0;
  if (ioctl(priv->fe_fd, FE_SET_VOLTAGE,
            c->pol ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18) < 0)
    return 0;
  gavl_time_delay(&delay_time);
  if (ioctl(priv->fe_fd, FE_DISEQC_SEND_MASTER_CMD, &cmd) < 0)
    return 0;
  gavl_time_delay(&delay_time);
  if (ioctl(priv->fe_fd, FE_DISEQC_SEND_BURST,
            (c->sat_no / 4) % 2 ? SEC_MINI_B : SEC_MINI_A) < 0)
    return 0;
  gavl_time_delay(&delay_time);
  if (ioctl(priv->fe_fd, FE_SET_TONE,
            c->tone ? SEC_TONE_ON : SEC_TONE_OFF) < 0)
    return 0;
  
  return 1;
  }

static int tune_in(bgav_input_context_t * ctx,
                   bgav_dvb_channel_info_t * channel)
  {
  int try;
  gavl_time_t delay_time;
  fe_status_t status = 0;
  dvb_priv_t * priv;
  struct dvb_frontend_event event;
  struct pollfd pfd[1];
  
  priv = ctx->priv;

  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Tuning to %s", channel->name);
  
  //  dvb_channels_dump(channel, priv->fe_info.type, 1);
  
  /* set_diseqc for satellite tuners */
  if(priv->fe_info.type==FE_QPSK)
    {
    if(!(priv->fe_info.caps & FE_CAN_INVERSION_AUTO))
      channel->front_param.inversion = INVERSION_OFF;
    if (!set_diseqc(ctx, channel))
      return 0;
    }
  
  /* Flush events */
  while (ioctl(priv->fe_fd, FE_GET_EVENT, &event) != -1);
  
  /* Tune to the channel */
  if(ioctl(priv->fe_fd, FE_SET_FRONTEND, &channel->front_param) <0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Setting channel failed: %s",
             strerror(errno));
    
    return 0;
    }

  /* Wait until the frontend tuned to the channel */
  pfd[0].fd = priv->fe_fd;
  pfd[0].events = POLLIN;

  if(poll(pfd,1,3000))
    {
    if(pfd[0].revents & POLLIN)
      {
      if(ioctl(priv->fe_fd, FE_GET_EVENT, &event) == -EOVERFLOW)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Setting channel failed: Event queue overflow");
        return 0;
        }
      if(event.parameters.frequency <= 0)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Setting frequency failed");
        return 0;
        }
      }
    }

  /* Wait for frontend to be locked */

  delay_time = GAVL_TIME_SCALE / 10;

  try = 0;
  do{
    status = 0;
    try++;
    if(try > 40)
      {
      status |= FE_TIMEDOUT;
      break;
      }
    if(ioctl(priv->fe_fd, FE_READ_STATUS, &status) < 0)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Reading status failed: %s",
               strerror(errno));
      return 0;
      }
    if(status & FE_HAS_LOCK)
      break;
    
    gavl_time_delay(&delay_time);
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Waiting for lock");
    } while (!(status & FE_TIMEDOUT));
  
  if(status & FE_TIMEDOUT)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Locking timed out");
    return 0;
    }

  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Frontend locked successfully");
  return 1;
  }

static int get_streams(bgav_input_context_t * ctx,
                       bgav_dvb_channel_info_t * channel)
  {
  int i, j;
  int program_index;
  int bytes_read;
  uint8_t buffer[4096];
  pat_section_t pats;
  pmt_section_t pmts;
  struct dmx_sct_filter_params params;
  dvb_priv_t * priv;
  priv = ctx->priv;
    
  if(channel->initialized)
    return 1;
  
  if(!tune_in(ctx, channel))
    return 0;
  
  /* Get PAT */
  
  set_num_filters(priv, 1);
  
  memset(&params, 0, sizeof(params));

  params.pid              = 0;
  params.filter.filter[0] = 0x00;
  params.filter.mask[0]   = 0xFF;
  params.flags             = DMX_IMMEDIATE_START | DMX_ONESHOT;

  if(ioctl(priv->filter_fds[0],
           DMX_SET_FILTER, &params) < 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Setting PAT filter failed: %s", strerror(errno));
    
    return 0;
    }
  bytes_read = read(priv->filter_fds[0], buffer, 4096);
  
  if(!bgav_pat_section_read(buffer, bytes_read, &pats))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Parsing PAT failed");
    return 0;
    }
  

  /* For each program, get the PMT */
  
  for(i = 0; i < pats.num_programs; i++)
    {
    program_index = 0;

    if(pats.programs[i].program_number == 0)
      continue;
    
    while(program_index < priv->num_channels)
      {
      if(pats.programs[i].program_number ==
         priv->channels[program_index].service_id)
        break;
      program_index++;
      }
    
    if(program_index == priv->num_channels)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Cannot find service ID %d in PAT (recreate channels.conf)",
               priv->channels[program_index].service_id);
      continue;
      }
    /* Get PMT */
    
    params.pid              = pats.programs[i].program_map_pid;
    params.filter.filter[0] = 0x02;
    params.filter.mask[0]   = 0xFF;
    params.flags             = DMX_IMMEDIATE_START | DMX_ONESHOT;
  
    if(ioctl(priv->filter_fds[0],
             DMX_SET_FILTER, &params) < 0)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Setting PMT filter failed: %s", strerror(errno));
      return 0;
      }

    bytes_read = read(priv->filter_fds[0],
                      buffer, 4096);
  
    if(!bgav_pmt_section_read(buffer, bytes_read, &pmts))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Parsing PMT failed");
      return 0;
      }
    
    /* We assume, that all streams are present */
    for(j = 0; j < pmts.num_streams; j++)
      pmts.streams[j].present = 1;
#ifdef DUMP_PMT    
    bgav_pmt_section_dump(&pmts);  
#endif

    /* Setup streams from pmt */
    bgav_pmt_section_setup_track(&pmts,
                                 ctx->tt->tracks[program_index],
                                 ctx->opt,
                                 MAX_AUDIO_STREAMS,
                                 MAX_VIDEO_STREAMS,
                                 MAX_AC3_STREAMS,
                                 &priv->channels[program_index].num_ac3_streams,
                                 &priv->channels[program_index].extra_pcr_pid);
    priv->channels[program_index].pcr_pid = pmts.pcr_pid;
    priv->channels[program_index].initialized = 1;
    }
  
  set_num_filters(priv, 0);
  return 1;
  }

/* The channel.conf doesn't save all neccesary infos,
   so we cache them instead tuning to all transponders
   and parsing pat/pmt tables.
*/

static int load_channel_cache(bgav_input_context_t * ctx)
  {
  bgav_input_context_t * input;
  char * filename;
  bgav_yml_node_t * yml = NULL;
  bgav_yml_node_t * yml_root = NULL;
  bgav_yml_node_t * channel_node;
  bgav_yml_node_t * channel_child;
  bgav_yml_node_t * stream_node;
  bgav_yml_node_t * stream_child;
  int num_channels;
  bgav_stream_t * s;
  int channel_index;
  int ret = 0;
  dvb_priv_t * priv;
  struct stat st;
  char * path;
  const char * attr;
  long file_time;
  
  priv = ctx->priv;
  
  input = bgav_input_create(ctx->b, NULL);
  
  filename = strrchr(priv->device_directory, '/');
  filename++;
    
  path = bgav_search_file_read(ctx->opt,
                               "dvb_linux", filename);
  if(!path)
    return 0;

  if(!bgav_input_open(input, path))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Channel cache %s cannot be opened: %s",
             path, strerror(errno));
    goto fail;
    }

  yml_root = bgav_yml_parse(input);
  yml = bgav_yml_find_by_name(yml_root, "channels");
  
  if(!yml)
    goto fail;
  
  attr = bgav_yml_get_attribute(yml, "num");

  if(!attr)
    goto fail;

  num_channels = atoi(attr);
  
  ctx->tt = bgav_track_table_create(num_channels);
  
  channel_node = yml->children;
  
  channel_index = 0;

  while(channel_node)
    {
    if(!channel_node->name)
      {
      channel_node = channel_node->next;
      continue;
      }

    if(!strcmp(channel_node->name, "conf_time"))
      {
      if(stat(priv->channels_conf_file, &st))
        goto fail;
      file_time = strtol(channel_node->children->str, NULL, 10);
      if(file_time != st.st_mtime)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Channel cache older than channels.conf");
        goto fail;
        }
      }
    else if(!strcmp(channel_node->name, "channel"))
      {
      channel_child = channel_node->children;

      attr = bgav_yml_get_attribute(channel_node, "pcr_pid");
      if(!attr)
        goto fail;
      priv->channels[channel_index].pcr_pid = strtol(attr, NULL, 16);

      attr = bgav_yml_get_attribute(channel_node, "extra_pcr_pid");
      if(!attr)
        goto fail;
      priv->channels[channel_index].extra_pcr_pid = atoi(attr);

      gavl_dictionary_set_string(ctx->tt->tracks[channel_index]->metadata,
                        GAVL_META_LABEL, priv->channels[channel_index].name);
      
      while(channel_child)
        {
        if(!channel_child->name)
          {
          channel_child = channel_child->next;
          continue;
          }

        if(!strcmp(channel_child->name, "astreams"))
          {
          stream_node = channel_child->children;

          while(stream_node)
            {
            if(!stream_node->name)
              {
              stream_node = stream_node->next;
              continue;
              }

            if(!strcmp(stream_node->name, "astream"))
              {
              s =
                bgav_track_add_audio_stream(ctx->tt->tracks[channel_index],
                                            ctx->opt);
              s->timescale = 90000;
              s->flags |= STREAM_PARSE_FULL;
              s->stats.pts_start = GAVL_TIME_UNDEFINED;
              stream_child = stream_node->children;

              while(stream_child)
                {
                if(!stream_child->name)
                  {
                  stream_child = stream_child->next;
                  continue;
                  }

                if(!strcmp(stream_child->name, "pid"))
                  s->stream_id = strtol(stream_child->children->str, NULL, 16);
                else if(!strcmp(stream_child->name, "fourcc"))
                  s->fourcc = strtol(stream_child->children->str, NULL, 16);

                else if(!strcmp(stream_child->name, "language"))
                  {
                  char language[4];
                  sscanf(stream_child->children->str, "%3s", language);
                  gavl_dictionary_set_string(s->m, GAVL_META_LANGUAGE, language);
                  }
                stream_child = stream_child->next;
                }
              }
            
            stream_node = stream_node->next;
            }
          
          }
        if(!strcmp(channel_child->name, "vstreams"))
          {
          stream_node = channel_child->children;

          while(stream_node)
            {
            if(!stream_node->name)
              {
              stream_node = stream_node->next;
              continue;
              }

            if(!strcmp(stream_node->name, "vstream"))
              {
              s =
                bgav_track_add_video_stream(ctx->tt->tracks[channel_index], ctx->opt);

              s->timescale = 90000;
              s->flags |= STREAM_PARSE_FULL;
              s->stats.pts_start = GAVL_TIME_UNDEFINED;
              stream_child = stream_node->children;

              while(stream_child)
                {
                if(!stream_child->name)
                  {
                  stream_child = stream_child->next;
                  continue;
                  }

                if(!strcmp(stream_child->name, "pid"))
                  s->stream_id = strtol(stream_child->children->str, NULL, 16);
                else if(!strcmp(stream_child->name, "fourcc"))
                  s->fourcc = strtol(stream_child->children->str, NULL, 16);
                
                stream_child = stream_child->next;
                }
              }
            
            stream_node = stream_node->next;
            }
          
          }

        channel_child = channel_child->next;
        }
      channel_index++;
      }
    
    channel_node = channel_node->next;
    }

  
  ret = 1;

  fail:
  bgav_input_close(input);
  bgav_input_destroy(input);
  if(path) free(path);
  if(yml_root)
    bgav_yml_free(yml_root);
  return ret;
  }

static void save_channel_cache(bgav_input_context_t * ctx)
  {
  FILE * output = NULL;
  char * filename;
  char * path = NULL;
  const char * language;
  bgav_stream_t * s;
  dvb_priv_t * priv;
  int i, j;
  struct stat st;
  priv = ctx->priv;
  
  filename = strrchr(priv->device_directory, '/');
  filename++;
  
  path = bgav_search_file_write(ctx->opt,
                                "dvb_linux", filename);
  if(!path)
    return;

  output = fopen(path, "w");

  fprintf(output, "<channels num=\"%d\">\n", ctx->tt->num_tracks);
  
  if(stat(priv->channels_conf_file, &st))
    goto fail;
  fprintf(output, "  <conf_time>%ld</conf_time>\n", st.st_mtime);
    
  for(i = 0; i < ctx->tt->num_tracks; i++)
    {
    fprintf(output, "  <channel pcr_pid=\"%04x\" extra_pcr_pid=\"%d\">\n",
            priv->channels[i].pcr_pid, priv->channels[i].extra_pcr_pid);
    
    fprintf(output, "    <astreams num=\"%d\">\n", 
            ctx->tt->tracks[i]->num_audio_streams);
    for(j = 0; j < ctx->tt->tracks[i]->num_audio_streams; j++)
      {
      s = bgav_track_get_audio_stream(ctx->tt->tracks[i], j);
      fprintf(output, "      <astream>\n");
      fprintf(output, "        <pid>%04x</pid>\n",
              s->stream_id);
      fprintf(output, "        <fourcc>%08x</fourcc>\n",
              s->fourcc);

      language = gavl_dictionary_get_string(s->m, GAVL_META_LANGUAGE);
      if(language)
        fprintf(output, "        <language>%3s</language>\n",
                language);
      fprintf(output, "      </astream>\n");
      }
    fprintf(output, "    </astreams>\n");

    
    fprintf(output, "    <vstreams num=\"%d\">\n", 
            ctx->tt->tracks[i]->num_video_streams);
    for(j = 0; j < ctx->tt->tracks[i]->num_video_streams; j++)
      {
      s = bgav_track_get_video_stream(ctx->tt->tracks[i], j);
      
      fprintf(output, "      <vstream>\n");
      fprintf(output, "        <pid>%04x</pid>\n",
              s->stream_id);
      fprintf(output, "        <fourcc>%08x</fourcc>\n",
              s->fourcc);
      fprintf(output, "      </vstream>\n");
      }
    fprintf(output, "    </vstreams>\n");
    
    fprintf(output, "  </channel>\n");
    }
  
  fprintf(output, "</channels>\n");
  fail:
  if(path) free(path);
  if(output) fclose(output);
  return;
  }

static int open_dvb(bgav_input_context_t * ctx, const char * url, char ** redirect_url)
  {
  int i;
  fe_status_t status;
  dvb_priv_t * priv;
  char * filename;
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  if(!strncasecmp(url, "dvb://", 6))
    url += 6;
  
  priv->device_directory  = gavl_strdup(url);
  priv->filter_filename   = bgav_sprintf("%s/demux0", url);
  priv->dvr_filename      = bgav_sprintf("%s/dvr0", url);
  priv->frontend_filename = bgav_sprintf("%s/frontend0", url);
  
  /* Open frontend */
  priv->fe_fd = open(priv->frontend_filename, O_RDWR | O_NONBLOCK);
  if(priv->fe_fd < 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Cannot open frontend device %s: %s", priv->frontend_filename, strerror(errno));
    return 0;
    }
  /* Get frontend info */
  if((ioctl(priv->fe_fd, FE_GET_INFO, &priv->fe_info)) < 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Cannot get frontend info");
    return 0;
    }
  //  dump_frontend_info(&priv->fe_info);

  /* Get status */
  if((ioctl(priv->fe_fd, FE_READ_STATUS, &status)) < 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Cannot get frontend status");
    return 0;
    }

  //  bgav_dprintf("Frontend status: 0x%02x\n", status);
  
  filename = strrchr(priv->device_directory, '/');
  filename++;
  
  priv->channels_conf_file =
    bgav_dvb_channels_seek(ctx->opt,
                           priv->fe_info.type);
  
  if(!priv->channels_conf_file)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Found no channels.conf file");
    return 0;
    }
  
  /* Load channels file */
  priv->channels = bgav_dvb_channels_load(ctx->b, priv->fe_info.type,
                                          &priv->num_channels,
                                          priv->channels_conf_file);

  if(!load_channel_cache(ctx))
    {
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
             "Regenerating channel cache");
    
    // Add channels to the tracks
    
    ctx->tt = bgav_track_table_create(priv->num_channels);
    for(i = 0; i < priv->num_channels; i++)
      {
      gavl_dictionary_set_string(ctx->tt->tracks[i]->metadata,
                        GAVL_META_LABEL, priv->channels[i].name);
      if(!get_streams(ctx, &priv->channels[i]))
        return 0;
      }
    save_channel_cache(ctx);
    }
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
           "Using channel cache");
  
  /* Create demuxer */
  
  ctx->demuxer = bgav_demuxer_create(ctx->b, &bgav_demuxer_mpegts, NULL);

  ctx->demuxer->tt = ctx->tt;
  
  priv->dvr_fd = -1;
  
  return 1;
  }

static void close_dvb(bgav_input_context_t * ctx)
  {
  dvb_priv_t * priv;
  priv = ctx->priv;
  
  set_num_filters(priv, 0);
  if(priv->dvr_fd >= 0)
    close(priv->dvr_fd);
  if(priv->fe_fd >= 0)
    close(priv->fe_fd);
  
  if(priv->filter_fds)
    free(priv->filter_fds);
  
  if(priv->device_directory)
    free(priv->device_directory);
  if(priv->frontend_filename)
    free(priv->frontend_filename);
  if(priv->filter_filename)
    free(priv->filter_filename);
  if(priv->dvr_filename)
    free(priv->dvr_filename);
  
  if(priv->channels)
    dvb_channels_destroy(priv->channels, priv->num_channels);
  free(priv);
  }

static const struct
  {
  int id;
  const char * charset;
  }
eit_charsets[] =
  {
    { 0x01, "ISO8859-5" },
    { 0x02, "ISO8859-6" },
    { 0x03, "ISO8859-7" },
    { 0x04, "ISO8859-8" },
    { 0x05, "ISO8859-9" },
    { 0x06, "ISO8859-10" },
    { 0x07, "ISO8859-11" },
    { 0x08, "ISO8859-12" },
    { 0x09, "ISO8859-13" },
    { 0x0A, "ISO8859-14" },
    { 0x0B, "ISO8859-15" },
    { 0x11, "ISO-10646/UCS2" },
    //    { 0x12, NULL }
    { 0x13, "GB2312" },
    { 0x14, "BIG5" },
  };

static char * decode_eit_string(const bgav_options_t * opt,
                                uint8_t * pos,
                                int len)
  {
  char * ret;
  int i;
  const char * charset = NULL;
  bgav_charset_converter_t * cnv;
  if(!(*pos))
    return NULL;
  
  /* Detect charset */
  if(*pos >= 0x20)
    {
    charset = "ISO_6937";
    }
  else if(*pos == 0x10)
    {
    }
  else
    {
    for(i = 0; i < sizeof(eit_charsets)/sizeof(eit_charsets[0]); i++)
      {
      if(*pos == eit_charsets[i].id)
        {
        charset = eit_charsets[i].charset;
        pos++;
        len--;
        break;
        }
      }
    }
  if(!charset)
    {
    return NULL;
    }

  cnv = bgav_charset_converter_create(opt, charset, BGAV_UTF8);
  ret = bgav_convert_string(cnv, (char*)pos, len, NULL);
  bgav_charset_converter_destroy(cnv);
  return ret;
  }

#define bcdtoint(i) ((((i & 0xf0) >> 4) * 10) + (i & 0x0f))

/*
 *  Extract UTC time and date encoded in modified julian date format and
 *  return it as a time_t.
 */

static void dvb_mjdtime(uint8_t *buf, struct tm *tma)
  {
  int i;
  unsigned int year, month, day, hour, min, sec;
  unsigned long int mjd;
  time_t t;

  memset(tma, 0, sizeof(*tma));
  
  mjd = (unsigned int)(buf[0] & 0xff) << 8;
  mjd +=(unsigned int)(buf[1] & 0xff);
  hour =(unsigned char)bcdtoint(buf[2] & 0xff);
  min = (unsigned char)bcdtoint(buf[3] & 0xff);
  sec = (unsigned char)bcdtoint(buf[4] & 0xff);
  year =(unsigned long)((mjd - 15078.2)/365.25);
  month=(unsigned long)((mjd - 14956.1 - (unsigned long)(year * 365.25))/30.6001);
  day = mjd - 14956 - (unsigned long)(year * 365.25) - (unsigned long)(month * 30.6001);

  if (month == 14 || month == 15)
    i = 1;
  else
    i = 0;
  year += i;
  month = month - 1 - i * 12;

  tma->tm_sec=sec;
  tma->tm_min=min;
  tma->tm_hour=hour;
  tma->tm_mday=day;
  tma->tm_mon=month-1;
  tma->tm_year=year;
  t = timegm(tma);
  localtime_r(&t,tma);
  }

static void check_eit(bgav_input_context_t* ctx)
  {
  uint8_t eit[8192];
  int len;
  int bytes_read;
  dvb_priv_t * priv;
  uint8_t * pos, *start, *descriptors_end;
  uint64_t tmp;
  struct tm start_time;
  fd_set rset;
  int version;
  struct timeval timeout;
  //  char * tmp_string;
  priv = ctx->priv;
  
  FD_ZERO(&rset);
  FD_SET (priv->filter_fds[priv->eit_filter], &rset);
  timeout.tv_sec  = 0;
  timeout.tv_usec = 0;
  if(select(priv->filter_fds[priv->eit_filter]+1,
            &rset, 
            NULL, NULL, &timeout) <= 0)
    return;
  
  /* Read EIT */
  if(read(priv->filter_fds[priv->eit_filter],
          eit, 3) < 3)
    return;
  len = ((eit[1] & 0x0f) << 8) | eit[2];
  bytes_read  = read(priv->filter_fds[priv->eit_filter],
                     &eit[3], len);
  if(bytes_read < len)
    return;
  len += 3;

  start = eit;
  pos = start + 3;

  tmp = GAVL_PTR_2_16BE(pos); pos+=2;

  
  if(tmp != priv->service_id)
    return;

  version = (*pos & 0x7F) >> 2;
#if 1
  if(priv->eit_version == version)
    return;
#endif
  pos++;

  /*
   section_number               8
   last_section_number          8
   transport_stream_id         16
   original_network_id         16
   segment_last_section_number  8
   last_table_id                8
  */
  
  pos += 8;
  
  //  gavl_hexdump(pos, 16, 16);

  while(pos - start + 4 < len)
    {
    tmp = GAVL_PTR_2_16BE(pos); pos+=2;
    
    dvb_mjdtime(pos, &start_time);
    pos += 5;
    
    tmp = GAVL_PTR_2_24BE(pos); pos+=3;

    tmp = GAVL_PTR_2_16BE(pos); pos+=2;
    
    descriptors_end = pos + (tmp & 0x0fff);
    
    if(tmp >> 13 == 0x04)
      {
      int desc_tag;
      uint8_t * end_pos;
      gavl_dictionary_t * m;
      char time_string[GAVL_METADATA_DATE_TIME_STRING_LEN];
      const char * tmp_string;
      
      gavl_metadata_date_time_to_string(start_time.tm_year + 1900,
                                        start_time.tm_mon+1,
                                        start_time.tm_mday,
                                        start_time.tm_hour,
                                        start_time.tm_min,
                                        start_time.tm_sec,
                                        time_string);
      
      /* Parse descriptors */

      m = ctx->tt->cur->metadata;

      tmp_string = gavl_dictionary_get_string(m, GAVL_META_DATE);
      
      if(tmp_string && !strcmp(tmp_string, time_string))
        {
        /* Already read this */
        pos = descriptors_end;
        continue;
        }
      
      priv->name_changed = 1;
      priv->metadata_changed = 1;
      
      gavl_dictionary_free(m);
      
      while(1)
        {
        desc_tag = *pos; pos++;
        pos++; // desc_len = *pos; pos++;
        
        end_pos = pos + len;

        switch(desc_tag)
          {
          case 0x4d: // short_event_descriptor
            pos+=3; // ISO_639_language_code
            
            tmp = *pos; pos++;// event_name_length
            
            gavl_dictionary_set_string_nocopy(m, GAVL_META_TITLE, 
                                              decode_eit_string(ctx->opt, pos, tmp));
            pos += tmp;
            
            tmp = *pos; pos++;// text_length
            
            gavl_dictionary_set_string_nocopy(m, GAVL_META_COMMENT, 
                                              decode_eit_string(ctx->opt,
                                                                pos, tmp));
            pos += tmp;
            break;
          default:
            break;
          }
        
        gavl_dictionary_set_string(m, GAVL_META_DATE, time_string);
        bgav_metadata_changed(ctx->b, m, GAVL_TIME_UNDEFINED);
        
        pos = end_pos;
        if(pos >= descriptors_end)
          break;
        }
      }
    else
      {
      pos = descriptors_end;
      }
    
    }

  if(priv->metadata_changed)
    {
    bgav_metadata_changed(ctx->b, ctx->tt->cur->metadata, GAVL_TIME_UNDEFINED);
    priv->metadata_changed = 0;
    }
  }

static int read_dvb(bgav_input_context_t* ctx,
                    uint8_t * buffer, int len)
  {
  dvb_priv_t * priv;
  struct dvb_frontend_event event;
  int ret = 0;
  int result;
  struct pollfd pfd[1];
  
  priv = ctx->priv;
  
  /* Flush events */
  while (ioctl(priv->fe_fd, FE_GET_EVENT, &event) != -1);
  
  pfd[0].fd = priv->dvr_fd;
  pfd[0].events = POLLIN;

  while(ret < len)
    {
    check_eit(ctx);
    
    result = poll(pfd, 1, ctx->opt->read_timeout ? ctx->opt->read_timeout : -1);
    if(!result)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Reading timed out (check cable connections)");
      return 0;
      }
    else if(result < 0)
      {
      /* Signal caught: This sometimes happens for unclear reasons */
      if(errno == EINTR)
        continue;
      else
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "poll failed: %s", strerror(errno));
        return ret;
        }
      }

    if(!(pfd[0].revents & POLLIN))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Reading timed out (check cable connections)");
      return ret;
      }
    result = read(priv->dvr_fd, buffer + ret, len - ret);
    
    if(result <= 0)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "read failed: %s", strerror(errno));
      return ret;
      }
    ret += result;

    /* Check EIT */
    check_eit(ctx);
    }
  return ret;
  }

static int setup_pes_filter(const bgav_options_t * opt,
                            int fd, int pes_type, int stream_id)
  {
  struct dmx_pes_filter_params params;
  memset(&params, 0, sizeof(params));
  params.pid      = stream_id;
  params.input    = DMX_IN_FRONTEND;
  params.output   = DMX_OUT_TS_TAP;
  params.pes_type = pes_type;
  params.flags    = DMX_IMMEDIATE_START;
  
  if(ioctl(fd, DMX_SET_PES_FILTER, &params) < 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Setting pes filter failed: %s", strerror(errno));
    return 0;
    }
  return 1;
  }

static int setup_section_filter(const bgav_options_t * opt,
                                int fd, uint16_t pid, int pidtype,
                                uint8_t table, uint8_t mask)
  {
  struct dmx_sct_filter_params params;
  memset(&params, 0, sizeof(params)); 
  params.pid      = pid;
  params.filter.filter[0] = table; 
  params.filter.mask[0] = mask;
  params.flags = DMX_IMMEDIATE_START;
  
  if (ioctl(fd, DMX_SET_FILTER, &params) < 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Setting section filter failed: %s", strerror(errno));
    return 0;
    }
  return 1; 
  }

static const int audio_pes_filters[4] =
  {
    DMX_PES_AUDIO0,
    DMX_PES_AUDIO1,
    DMX_PES_AUDIO2,
    DMX_PES_AUDIO3,
  };

static const int video_pes_filters[4] =
  {
    DMX_PES_VIDEO0,
    DMX_PES_VIDEO1,
    DMX_PES_VIDEO2,
    DMX_PES_VIDEO3,
  };

static int setup_filters(bgav_input_context_t * ctx,
                         bgav_dvb_channel_info_t * channel,
                         bgav_track_t * track)
  {
  dvb_priv_t * priv;
  int filter_index;
  int stream_index;
  
  priv = ctx->priv;

  set_num_filters(priv, track->num_audio_streams +
                  track->num_video_streams + channel->extra_pcr_pid + 1);
  
  filter_index = 0;
  
  stream_index = 0;
  /* Audio filters */

  while(stream_index < track->num_streams)
    {
    if((track->streams[stream_index].type != GAVL_STREAM_AUDIO) ||
       (track->streams[stream_index].fourcc == BGAV_MK_FOURCC('.','a','c','3')))
      {
      stream_index++;
      }
    else
      {
      if(!setup_pes_filter(ctx->opt,
                           priv->filter_fds[filter_index],
                           audio_pes_filters[filter_index],
                           track->streams[stream_index].stream_id))
        return 0;
      filter_index++;
      stream_index++;
      }
    }
  
  /* AC3 filter */

  if(channel->num_ac3_streams)
    {
    stream_index = 0;

    while(stream_index < track->num_streams)
      {
      if((track->streams[stream_index].type != GAVL_STREAM_AUDIO) ||
         (track->streams[stream_index].fourcc != BGAV_MK_FOURCC('.','a','c','3')))
        {
        stream_index++;
        }
      else
        {
        if(!setup_pes_filter(ctx->opt,
                             priv->filter_fds[filter_index],
                             DMX_PES_OTHER,
                             track->streams[stream_index].stream_id))
          return 0;
        }
      }
    }
    
  /* Video filter */
  
  stream_index = 0;

  while(stream_index < track->num_streams)
    {
    if(track->streams[stream_index].type != GAVL_STREAM_VIDEO)
      {
      stream_index++;
      }
    else
      {
      if(!setup_pes_filter(ctx->opt,
                           priv->filter_fds[filter_index],
                           video_pes_filters[filter_index],
                           track->streams[stream_index].stream_id))
        return 0;
      filter_index++;
      stream_index++;
      }
    }
  
  /* PCR filter */
  if(channel->extra_pcr_pid)
    {
    if(!setup_pes_filter(ctx->opt,
                         priv->filter_fds[filter_index++],
                         DMX_PES_PCR0,
                         channel->pcr_pid))
      return 0;
    }

  /* EIT Filter */
  priv->eit_filter = filter_index++;
  priv->eit_version = -1;
  if(ioctl(priv->filter_fds[priv->eit_filter],
           DMX_SET_BUFFER_SIZE,8192*10) < 0)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Setting section filter buffer failed: %s", strerror(errno));
    return 0;
    }
  
  if(!setup_section_filter(ctx->opt,
                           priv->filter_fds[priv->eit_filter],
                           0x12,DMX_PES_OTHER,0x4e, 0xff))
    return 0;
  
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
           "Filters initialized successfully");
  
  return 1;
  
  }

static int select_track_dvb(bgav_input_context_t * ctx, int track)
  {
  dvb_priv_t * priv;
  fd_set rset;
  struct timeval timeout;
  
  priv = ctx->priv;
  
  if(priv->dvr_fd >= 0)
    {
    close(priv->dvr_fd);
    priv->dvr_fd = -1;
    }
  if(!tune_in(ctx, &priv->channels[track]))
    return 0;
  
  if(!setup_filters(ctx, &priv->channels[track], ctx->tt->cur))
    return 0;
  
  priv->dvr_fd = open(priv->dvr_filename, O_RDONLY | O_NONBLOCK);
  ctx->sync_id = priv->channels[track].pcr_pid;

  /* Wait here for some longer time until we can read data.
     This allows a really short timeout in the read function */
  
  FD_ZERO(&rset);
  FD_SET (priv->dvr_fd, &rset);
  timeout.tv_sec  = 2;
  timeout.tv_usec = 0;
  priv->service_id = priv->channels[track].service_id;
  select(priv->dvr_fd+1, &rset, NULL, NULL, &timeout);

  /* Clear metadata so they are reread */
  gavl_dictionary_free(ctx->tt->cur->metadata);
  return 1;
  }

const bgav_input_t bgav_input_dvb =
  {
    .name =          "dvb",
    .open =          open_dvb,
    .read =          read_dvb,
    .close =         close_dvb,
    .select_track =  select_track_dvb,
  };

static
bgav_input_context_t * bgav_input_open_dvb(const char * device,
                                           bgav_t * b)
  {
  bgav_input_context_t * ret = NULL;
  ret = bgav_input_create(b, NULL);
  ret->input = &bgav_input_dvb;

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

int bgav_open_dvb(bgav_t * b, const char * device)
  {
  bgav_codecs_init(&b->opt);
  b->input = bgav_input_open_dvb(device, b);
  if(!b->input)
    return 0;
  if(!bgav_init(b))
    goto fail;
  return 1;
  fail:
  return 0;
  }

int bgav_check_device_dvb(const char * device, char ** name)
  {
  struct dvb_frontend_info fe_info;
  int fd;
  char * tmp_string;

  tmp_string = bgav_sprintf("%s/frontend0", device);
  fd = open(tmp_string, O_RDONLY);
  free(tmp_string);
  
  if(fd < 0)
    {
    return 0;
    }
  /* Get frontend info */
  if((ioctl(fd, FE_GET_INFO, &fe_info)) < 0)
    {
    close(fd);
    return 0;
    }
  *name = gavl_strdup(fe_info.name);
  close(fd);
  return 1;
  }

bgav_device_info_t * bgav_find_devices_dvb()
  {
  int i;
  char * device_name;
  char * directory;
  
  bgav_device_info_t * ret = NULL;
  
  for(i = 0; i < 8; i++)
    {
    device_name = NULL;
    
    directory = bgav_sprintf("/dev/dvb/adapter%d", i);
    
    if(bgav_check_device_dvb(directory, &device_name))
      {
      ret = bgav_device_info_append(ret, directory, device_name);
      if(device_name)
        free(device_name);
      free(directory);
      }
    else
      {
      free(directory);
      break;
      }
    }
  return ret;
  }

#else // !HAVE_LINUXDVB

int bgav_check_device_dvb(const char * device, char ** name)
  {
  return 0;
  }

bgav_device_info_t * bgav_find_devices_dvb()
  {
  return NULL;
  }

int bgav_open_dvb(bgav_t * b, const char * device)
  {
  return 0;
  }

#endif // !HAVE_LINUXDVB
