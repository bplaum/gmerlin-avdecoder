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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <rtsp.h>
#include <rmff.h>
#include <bswap.h>
#include <rtp.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif





#define LOG_DOMAIN "in_rtsp"

#define USER_AGENT "RealMedia Player Version 6.0.9.1235 (linux-2.0-libc6-i386-gcc2.95)"
//#define USER_AGENT "QuickTime/7.4.1 (qtver=7.4.1;os=Windows NT 5.1Service Pack 2)"
/* We support multiple server types */

#define SERVER_TYPE_GENERIC   0
#define SERVER_TYPE_REAL      1
//#define SERVER_TYPE_QTSS      2
//#define SERVER_TYPE_DARWIN    3

extern bgav_demuxer_t bgav_demuxer_rmff;

/* See bottom of this file */

static void
real_calc_response_and_checksum (char *response,
                                 char *chksum, char *challenge);


typedef struct rtsp_priv_s
  {
  int has_smil; /* Nonzero if we want to fetch a smil file */
  int eof;      /* EOF detected? */
  
  int type;
  char * challenge1;
  bgav_rtsp_t * r;
  bgav_rmff_header_t * rmff_header;
  
  /* Packet */
    
  uint8_t * packet;
  uint8_t * packet_ptr;
  int       packet_len;
  int packet_alloc;
  
  /* Function for getting the next packet */
  
  int (*next_packet)(bgav_input_context_t * ctx, int block);

  /* RDT Specific */
  uint32_t prev_ts;
  uint32_t prev_stream_number;

  int tcp;
  } rtsp_priv_t;

static void packet_alloc(rtsp_priv_t * priv, int size)
  {
  if(priv->packet_alloc < size)
    {
    priv->packet_alloc = size + 64;
    priv->packet = realloc(priv->packet, priv->packet_alloc);
    }
  priv->packet_ptr = priv->packet;
  priv->packet_len = size;
  }

/* Get one RDT packet */

static int next_packet_rdt(bgav_input_context_t * ctx, int block)
  {
  int size;
  int flags1, flags2;
  
  uint32_t timestamp;
  bgav_rmff_packet_header_t ph;
  //  int unknown1;
  uint8_t * pos;
  char * buf;
  int bytes_read;
  int fd;
  uint8_t header[8];
  int seq;
  rtsp_priv_t * priv = ctx->priv;

  if(priv->eof)
    return 0;
  
  fd = bgav_rtsp_get_fd(priv->r);
  
  while(1)
    {
    if(block)
      {
      if(bgav_read_data_fd(ctx->opt, fd, header, 8, ctx->opt->read_timeout) < 8)
        return 0;
      }
    else
      {
      bytes_read = bgav_read_data_fd(ctx->opt, fd, header, 8, 0);
      if(!bytes_read)
        return 0;
      else if(bytes_read < 8)
        {
        if(bgav_read_data_fd(ctx->opt, fd, &header[bytes_read], 8 - bytes_read,
                             ctx->opt->read_timeout) < 8 - bytes_read)
          return 0;
        
        }
      }
    
    /* Check for a ping request */
    
    if(!strncmp((char*)header, "SET_PARA", 8))
      {
    
      size = 0;

      seq = -1;
      while(1)
        {
        char * ptr = (char*)priv->packet;
        if(!bgav_read_line_fd(ctx->opt, fd, &ptr,
                              &priv->packet_alloc,
                              ctx->opt->read_timeout))
          return 0;
        priv->packet = (uint8_t*)ptr;
        size = strlen((char*)(priv->packet));
        if(!size)
          break;

        if (!strncmp((char*)(priv->packet),"Cseq:",5))
          sscanf((char*)(priv->packet),"Cseq: %u",&seq);
        }
      if(seq < 0)
        seq = 1;
      /* Make the server happy */

      bgav_tcp_send(ctx->opt, fd, (uint8_t*)"RTSP/1.0 451 Parameter Not Understood\r\n",
                    strlen("RTSP/1.0 451 Parameter Not Understood\r\n"));
      buf = gavl_sprintf("CSeq: %u\r\n\r\n", seq);
      bgav_tcp_send(ctx->opt, fd, (uint8_t*)buf, strlen(buf));
      free(buf);
      }
    else if(header[0] == '$')
      {
      size = GAVL_PTR_2_24BE(&header[1]);
    
      flags1 = header[4];

      if ((flags1!=0x40) && (flags1!=0x42))
        {
        if(header[6] == 0x06)
          {
          priv->eof = 1;
          return 0; /* End of stream */
          }
        header[0]=header[5];
        header[1]=header[6];
        header[2]=header[7];

        if(bgav_read_data_fd(ctx->opt, fd, &header[3], 5, ctx->opt->read_timeout) < 5)
          return 0;

        if(bgav_read_data_fd(ctx->opt, fd, &header[4], 4, ctx->opt->read_timeout) < 4)
          return 0;
        flags1 = header[4];
        size-=9;
        }

      flags2=header[7];
      // unknown1 = (header[5]<<16)+(header[6]<<8)+(header[7]);
      if(bgav_read_data_fd(ctx->opt, fd, header, 6, ctx->opt->read_timeout) < 6)
        return 0;

      timestamp = GAVL_PTR_2_32BE(header);
      size+=2;

      
      ph.object_version=0;
      ph.length=size;
      ph.stream_number=(flags1>>1)&1;
      ph.timestamp=timestamp;
      ph.packet_group=0;
      if((flags2&1) == 0 && (priv->prev_ts != timestamp ||
                             priv->prev_stream_number != ph.stream_number))
        {
        priv->prev_ts = timestamp;
        priv->prev_stream_number = ph.stream_number;
        ph.flags=2;
        }
      else
        ph.flags=0;

      //    bgav_rmff_packet_header_dump(&ph);

      packet_alloc(priv, size);
    
      size -= 12;
      bgav_rmff_packet_header_to_pointer(&ph, priv->packet);
      if(bgav_read_data_fd(ctx->opt, fd, priv->packet + 12, size,
                           ctx->opt->read_timeout) < size)
        return 0;

      if(priv->has_smil)
        {
        pos = (uint8_t*)strstr((char*)(priv->packet + 12), "<smil>");
        if(pos)
          {
          priv->packet_len -= (pos - priv->packet);
          memmove(priv->packet, pos, priv->packet_len);
          }

        pos = (uint8_t*)strstr((char*)(priv->packet + 12), "</smil>");
        if(pos)
          {
          pos += strlen("</smil>");
          *pos = '\0';
          priv->packet_len = (pos - priv->packet) + 1;
          }


        }
      return 1;
      }
    else
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
              "Unknown RDT chunk %02x %02x %02x %02x %02x %02x %02x %02x",
              header[0], header[1], header[2], header[3],
              header[4], header[5], header[6], header[7]);
      return 0;
      }
    }
  return 0;
  }


static const char * get_answer_var(const char * str, const char * name,
                                   int * len)
  {
  const char * pos1, *pos2;
  
  pos1 = strstr(str, name);
  if(!pos1)
    return NULL;
  pos2 = strchr(pos1, ';');
  if(!pos2)
    pos2 = pos1 + strlen(pos1);
  pos1 += strlen(name);
  *len = pos2 - pos1;
  return pos1;
  }

static int open_and_describe(bgav_input_context_t * ctx,
                             const char * url, int * got_redirected)
  {
  const char * var;
  rtsp_priv_t * priv = ctx->priv;
  
  /* Open URL */

  bgav_rtsp_schedule_field(priv->r,
                           "User-Agent: "USER_AGENT);
#if 1
  bgav_rtsp_schedule_field(priv->r,
                           "ClientChallenge: 9e26d33f2984236010ef6253fb1887f7");
  bgav_rtsp_schedule_field(priv->r,
                           "PlayerStarttime: [28/03/2003:22:50:23 00:00]");
  bgav_rtsp_schedule_field(priv->r,
                           "CompanyID: KnKV4M4I/B2FjJ1TToLycw==");
  bgav_rtsp_schedule_field(priv->r,
                           "GUID: 00000000-0000-0000-0000-000000000000");
  bgav_rtsp_schedule_field(priv->r,
                           "RegionData: 0");
  bgav_rtsp_schedule_field(priv->r,
                           "ClientID: Linux_2.4_6.0.9.1235_play32_RN01_EN_586");
  bgav_rtsp_schedule_field(priv->r,
                           "Pragma: initiate-session");
#endif
  if(!bgav_rtsp_open(priv->r, url, got_redirected))
    return 0;

  if(*got_redirected)
    return 1;
  
  /* Check wether this is a Real Server */

  var = bgav_rtsp_get_answer(priv->r, "RealChallenge1");
  if(var)
    {
    priv->challenge1 = gavl_strdup(var);
    priv->type = SERVER_TYPE_REAL;
    gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN,
             "Real Server, challenge %s", var);
    }
#if 0
  else
    {
    if(!bgav_rtsp_reopen(priv->r))
      return 0;
    }
#endif
#if 0
  else
    {
    var = bgav_rtsp_get_answer(priv->r, "Server");
    if(var)
      {
      if(!strncmp(var, "QTSS", 4))
        {
        priv->type = SERVER_TYPE_QTSS;
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "QTSS Server");
        }
      }
    }
#endif
  
#if 1
  if(priv->type == SERVER_TYPE_GENERIC)
    {
    gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Generic RTSP code\n");
    }
#endif

  /* The describe request looks a bit different depending on what server type we have */

  switch(priv->type)
    {
    case SERVER_TYPE_REAL:
      bgav_rtsp_schedule_field(priv->r,
                               "Accept: application/sdp");
      bgav_rtsp_schedule_field(priv->r,
                               "Bandwidth: 10485800");
      bgav_rtsp_schedule_field(priv->r,
                               "GUID: 00000000-0000-0000-0000-000000000000");
      bgav_rtsp_schedule_field(priv->r,
                               "RegionData: 0");
      bgav_rtsp_schedule_field(priv->r,
                               "ClientID: Linux_2.4_6.0.9.1235_play32_RN01_EN_586");
      bgav_rtsp_schedule_field(priv->r,
                               "SupportsMaximumASMBandwidth: 1");
      bgav_rtsp_schedule_field(priv->r,
                               "Language: en-US");
      bgav_rtsp_schedule_field(priv->r,
                               "Require: com.real.retain-entity-for-setup");
      break;
    default:
      bgav_rtsp_schedule_field(priv->r,
                               "Accept: application/sdp");
      bgav_rtsp_schedule_field(priv->r,
                               "Bandwidth: 384000");
      bgav_rtsp_schedule_field(priv->r,
                               "Accept-Language: en-US");
      bgav_rtsp_schedule_field(priv->r,
                               "User-Agent: "USER_AGENT);

    }
    
  if(!bgav_rtsp_request_describe(priv->r, got_redirected))
    return 0;
  
  return 1;
  }


static int is_real_smil(bgav_sdp_t * s)
  {
  if((s->num_media == 1) && !strcmp(s->media[0].media, "data"))
    return 1;
  return 0;
  }

static int init_real(bgav_input_context_t * ctx, bgav_sdp_t * sdp, char * session_id)
  {
  rtsp_priv_t * priv;
  char * stream_rules = NULL;
  char challenge2[64];
  char checksum[34];
  char * field;
  int ret = 0;
  priv = ctx->priv;

  priv->next_packet = next_packet_rdt;
  
  if(is_real_smil(sdp))
    {
    gavl_log(GAVL_LOG_DEBUG, LOG_DOMAIN, "Got smil redirector");
    priv->has_smil = 1;
    }
  else
    {
    priv->rmff_header =
      bgav_rmff_header_create_from_sdp(ctx->opt, sdp,
                                       &stream_rules);
    if(!priv->rmff_header)
      goto fail;
    ctx->demuxer = bgav_demuxer_create(ctx->b, &bgav_demuxer_rmff, ctx);
    if(!bgav_demux_rm_open_with_header(ctx->demuxer,
                                       priv->rmff_header))
      return 0;
    }
  
  /* Setup */
  
  real_calc_response_and_checksum(challenge2, checksum, priv->challenge1);

  field = gavl_sprintf("RealChallenge2: %s, sd=%s", challenge2, checksum);
  bgav_rtsp_schedule_field(priv->r, field);free(field);
  
  field = gavl_sprintf("If-Match: %s", session_id);
  bgav_rtsp_schedule_field(priv->r, field);free(field);
  bgav_rtsp_schedule_field(priv->r, "Transport: x-pn-tng/tcp;mode=play,rtp/avp/tcp;unicast;mode=play");
  field = gavl_sprintf("%s/streamid=0", ctx->url);

  if(!bgav_rtsp_request_setup(priv->r,field))
    {
    free(field);
    goto fail;
    }
  free(field);
  
  if(priv->rmff_header && (priv->rmff_header->prop.num_streams > 1))
    {
    field = gavl_sprintf("If-Match: %s", session_id);
    bgav_rtsp_schedule_field(priv->r, field);free(field);
    bgav_rtsp_schedule_field(priv->r, "Transport: x-pn-tng/tcp;mode=play,rtp/avp/tcp;unicast;mode=play");
    field = gavl_sprintf("%s/streamid=1", ctx->url);
    if(!bgav_rtsp_request_setup(priv->r,field))
      {
      free(field);
      goto fail;
      }
    free(field);
    }

  /* Set Parameter */

  field = gavl_sprintf("Subscribe: %s", stream_rules);
  bgav_rtsp_schedule_field(priv->r, field);free(field);
  if(!bgav_rtsp_request_setparameter(priv->r))
    goto fail;
  /* Play */

  bgav_rtsp_schedule_field(priv->r, "Range: npt=0-");
  if(!bgav_rtsp_request_play(priv->r))
    goto fail;

  ret = 1;
  priv->tcp = 1;
  
  fail:
  if(stream_rules)
    free(stream_rules);
  return ret;
  }

static int rtp_parse_range(const char * range, int * ret)
  {
  const char * pos;
  char * rest;
  
  ret[0] = strtol(range, &rest, 10);
  if(range == rest)
    return 0;
  pos = rest;
  while(isspace(*pos))
    pos++;

  if(*pos != '-')
    return 0;
  pos++;

  while(isspace(*pos))
    pos++;
  
  ret[1] = strtol(pos, &rest, 10);
  if(pos == rest)
    return 0;
  return 1;
  }

static int handle_stream_transport(bgav_stream_t * s,
                                   const char * transport, int tcp)
  {
  const char * var;
  int var_len = 0;
  struct addrinfo * addr;
  int server_ports[2] = { 0, 0 };
  int client_ports[2] = { 0, 0 };
  //  int i;
  rtp_stream_priv_t * sp = s->priv;
  
  if(!tcp)
    {
    if(!(var = get_answer_var(transport, "client_port=", &var_len)) ||
       !rtp_parse_range(var, client_ports))
      {
      return 0;
      }
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Client ports: %d %d\n",
             client_ports[0], client_ports[1]);
    
    if(!(var = get_answer_var(transport, "server_port=", &var_len)) ||
       !rtp_parse_range(var, server_ports))
    
      return 0;

    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Server ports: %d %d\n",
             server_ports[0], server_ports[1]);
  
    if((var = get_answer_var(transport, "source=", &var_len)))
      {
      char * ip = gavl_strndup(var, var + var_len);
      addr = bgav_hostbyname(s->opt, ip, server_ports[0], SOCK_DGRAM, 0);
      if(!addr)
        {
        free(ip);
        return 0;
        }
      if(connect(sp->rtp_fd, addr->ai_addr, addr->ai_addrlen)<0)
        {
        free(ip);
        return 0;
        }
      freeaddrinfo(addr);
      
      addr = bgav_hostbyname(s->opt, ip, server_ports[1], SOCK_DGRAM, 0);
      if(!addr)
        {
        free(ip);
        return 0;
        }

      if(connect(sp->rtcp_fd, addr->ai_addr, addr->ai_addrlen)<0)
        {
        free(ip);
        return 0;
        }

      freeaddrinfo(addr);
      
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Server address: %s\n", ip);
      free(ip);
      }
    else
      return 0;

    }
  

  if((var = get_answer_var(transport, "ssrc=", &var_len)))
    sp->server_ssrc = strtoul(var, NULL, 16);

  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "ssrc: %08x\n",
           sp->server_ssrc);
  
  return 1;
  }

static int init_stream_generic(bgav_input_context_t * ctx,
                               bgav_stream_t * s, int * port,
                               char ** session_id, int tcp)
  {
  rtsp_priv_t * priv = ctx->priv;
  rtp_stream_priv_t * sp = s->priv;
  char * field;
  const char * var;

  if(!s->fourcc)
    return 1;
  
  if(!sp || !sp->control_url)
    return 0;

  if(!tcp)
    {
    /* Open ports */
    sp->rtp_fd = bgav_udp_open(ctx->opt, *port);
    if(sp->rtp_fd < 0)
      return 0;
  
    sp->rtcp_fd = bgav_udp_open(ctx->opt, (*port)+1);
    if(sp->rtcp_fd < 0)
      return 0;
    field = gavl_sprintf("Transport: RTP/AVP/UDP;unicast;client_port=%d-%d",
                         *port, (*port)+1);
    }
  else
    {
    field = gavl_sprintf("Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d",
                         *port, (*port)+1);
    
    sp->interleave_base = *port;
    sp->rtp_fd = -1;
    sp->rtcp_fd = -1;
    }
  
  /* Send setup request */
  bgav_rtsp_schedule_field(priv->r, field);free(field);
  //  bgav_rtsp_schedule_field(priv->r, "Range: npt=0-");
  //  bgav_rtsp_schedule_field(priv->r,
  //                           "User-Agent: "USER_AGENT);
  //  bgav_rtsp_schedule_field(priv->r,
  //                           "Accept-Language: en-US");

  if(!bgav_rtsp_request_setup(priv->r,sp->control_url))
    return 0;
  
  if(!(*session_id))
    {
    var = bgav_rtsp_get_answer(priv->r, "Session");
    if(var)
      *session_id = gavl_strdup(var);
    }
  
  var = bgav_rtsp_get_answer(priv->r, "Transport");
  if(!var || !handle_stream_transport(s, var, tcp))
    return 0;

  *port += 2;
  
  return 1;
  }

/*
 * Handle things like
 * url=rtsp://live.polito.it/accademia/2007/accademia-2007-02-28.mov/TrackID=0;seq=37253;rtptime=2299148613,url=rtsp://live.polito.it/accademia/2007/accademia-2007-02-28.mov/TrackID=1;seq=18653;rtptime=4265967293
 */

static int handle_rtpinfo(bgav_input_context_t * ctx,
                          const char * rtpinfo)
  {
  char ** streams;
  int i, j;

  bgav_stream_t * s;
  bgav_stream_t * s_tmp;
  rtp_stream_priv_t * sp;
  const char * var, *pos1, *pos2;
  int var_len;
  
  streams = gavl_strbreak(rtpinfo, ',');

  i = 0;
  while(streams[i])
    {
    var = get_answer_var(streams[i], "url=", &var_len);
    if(!var)
      return 0;
    pos1 = strrchr(var, '/');
    
    if(pos1)
      var_len -= (int)(pos1 - var);
    else
      pos1 = var;
    
    s = NULL;
    /* Search for the bgav stream */
    for(j = 0; j < ctx->demuxer->tt->cur->num_video_streams; j++)
      {
      s_tmp = bgav_track_get_video_stream(ctx->demuxer->tt->cur, j);
      
      if(!s_tmp->fourcc)
        continue;
      sp = s_tmp->priv;

      pos2 = strrchr(sp->control_url, '/');
      if(!pos2)
        pos2 = sp->control_url;

      if(!strncmp(pos1, pos2, var_len))
        {
        s = s_tmp;
        break;
        }
      }
    if(!s)
      {
      for(j = 0; j < ctx->demuxer->tt->cur->num_audio_streams; j++)
        {
        s_tmp = bgav_track_get_audio_stream(ctx->demuxer->tt->cur, j);

        if(!s_tmp->fourcc)
          continue;
        sp = s_tmp->priv;
        pos2 = strrchr(sp->control_url, '/');
        if(!pos2)
          pos2 = sp->control_url;
        
        if(!strncmp(pos1, pos2, var_len))
          {
          s = s_tmp;
          break;
          }
        }
      }
    if(s && sp)
      {
      var = get_answer_var(streams[i], "rtptime=", &var_len);
      if(!var)
        {
        return 0;
        }
      sp->first_rtptime = strtoul(var, NULL, 10);

      
      var = get_answer_var(streams[i], "seq=", &var_len);
      if(!var)
        return 0;
      
      sp->first_seq = strtoul(var, NULL, 10);
      }
    
    i++;
    }
  gavl_strbreak_free(streams);
  return 1;
  }

static int init_generic(bgav_input_context_t * ctx, bgav_sdp_t * sdp, int tcp)
  {
  int ret = 0;
  rtsp_priv_t * priv;
  bgav_stream_t * s;
  int i;
  int port;
  char * session_id = NULL;
  char * field;
  const char * var;
  priv = ctx->priv;
  
  ctx->demuxer = bgav_demuxer_create(ctx->b, &bgav_demuxer_rtp, ctx);
  if(!bgav_demuxer_rtp_open(ctx->demuxer, sdp))
    goto fail;

  if(tcp)
    port = 0; /* Interleave channel */
  else
    {
    if(ctx->opt->rtp_port_base <= 1024)
      port = rand() % (65530 - 10000) + 10000;
    else
      port = ctx->opt->rtp_port_base;
    if(port & 1)
      port++;
    }
  
  /* Transport negotiation */
  for(i = 0; i < ctx->demuxer->tt->cur->num_video_streams; i++)
    {
    s = bgav_track_get_video_stream(ctx->demuxer->tt->cur, i);
    if(!init_stream_generic(ctx, s, &port, &session_id, tcp))
      goto fail;
    }
  for(i = 0; i < ctx->demuxer->tt->cur->num_audio_streams; i++)
    {
    s = bgav_track_get_audio_stream(ctx->demuxer->tt->cur, i);

    if(!init_stream_generic(ctx, s, &port, &session_id, tcp))
      goto fail;
    }
  if(tcp)
    bgav_rtp_set_tcp(ctx->demuxer);
  else
    {
    /* Start read thread before play request */
    bgav_demuxer_rtp_start(ctx->demuxer);
    }
  /* Play */
  if(session_id)
    {
    field = gavl_sprintf("Session: %s", session_id);
    bgav_rtsp_schedule_field(priv->r, field);free(field);
    }

  bgav_rtsp_schedule_field(priv->r, "Range: npt=0-");
  
  if(!bgav_rtsp_request_play(priv->r))
    goto fail;
    
  var = bgav_rtsp_get_answer(priv->r, "RTP-Info");
  if(!var)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Got no RTP-Info from server");
    return 0;
    }
  
  handle_rtpinfo(ctx, var);

  if(tcp)
    bgav_demuxer_rtp_start(ctx->demuxer);
  
  ret = 1;
  fail:
  if(session_id)
    free(session_id);
  
  return ret;
  }

static int open_rtsp(bgav_input_context_t * ctx, const char * url, char ** r)
  {
  rtsp_priv_t * priv;
  bgav_sdp_t * sdp;
  const char * var;
  char * session_id = NULL;
  int num_redirections = 0;
  int got_redirected = 0;
  priv = calloc(1, sizeof(*priv));
  priv->r = bgav_rtsp_create(ctx->opt);

  ctx->priv = priv;
  ctx->url = gavl_strdup(url);
  
  while(num_redirections < 5)
    {
    got_redirected = 0;

    if(num_redirections)
      {
      if(!open_and_describe(ctx, NULL, &got_redirected))
        return 0;
      }
    else
      {
      if(!open_and_describe(ctx, url, &got_redirected))
        return 0;
      }
    if(got_redirected)
      {
      // gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Got redirected to: %s\n", bgav_rtsp_get_url(priv->r));
      num_redirections++;
      }
    else
      break;
    }
  
  if(num_redirections == 5)
    goto fail;

  if(priv->type == SERVER_TYPE_REAL)
    {
    var = bgav_rtsp_get_answer(priv->r,"ETag");
    if(!var)
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Got no ETag");
      goto fail;
      }
    else
      session_id=gavl_strdup(var);
    }
  
  sdp = bgav_rtsp_get_sdp(priv->r);

  bgav_sdp_dump(sdp);
  
  /* Set up input metadata from sdp */
  gavl_dictionary_set_string(&ctx->m, GAVL_META_TITLE, sdp->session_name);
  gavl_dictionary_set_string(&ctx->m, GAVL_META_COMMENT, sdp->session_description);
    
  switch(priv->type)
    {
    case SERVER_TYPE_REAL:
      if(!init_real(ctx, sdp, session_id))
        goto fail;
      ctx->flags |= BGAV_INPUT_DO_BUFFER;
      break;
    case SERVER_TYPE_GENERIC:
      if(ctx->opt->rtp_try_tcp && init_generic(ctx, sdp, 1))
        {
        priv->tcp = 1;
        break;
        }
      if(!init_generic(ctx, sdp, 0))
        goto fail;
      break;
    }
  
  if(session_id)
    free(session_id);
  
  
  return 1;

  fail:
  if(priv->challenge1)
    {
    free(priv->challenge1);
    priv->challenge1 = NULL;
    }
  if(session_id)
    free(session_id);
  free(priv);
  ctx->priv = NULL;
  return 0;
  }

static void close_rtsp(bgav_input_context_t * ctx)
  {
  rtsp_priv_t * priv;
  priv = ctx->priv;
  if(!priv)
    return;
  
  if(priv->type == SERVER_TYPE_GENERIC)
    {
    /* Stop read thread */
    //    bgav_demuxer_rtp_stop(ctx->demuxer);
    }
  if(priv->r)
    bgav_rtsp_close(priv->r, !priv->tcp);
  
  /* Header is always destroyed by the demuxer */
  //  if(priv->rmff_header)
  //    bgav_rmff_header_destroy(priv->rmff_header);

  /* Send a teardown request */
  
  if(priv->packet)
    free(priv->packet);
  
  free(priv);
  }

static int do_read(bgav_input_context_t* ctx,
                   uint8_t * buffer, int len, int block)
  {
  int bytes_read;
  rtsp_priv_t * priv;
  int bytes_to_copy;
  int fd;
  priv = ctx->priv;
  bytes_read = 0;
  if(priv->next_packet)
    {
    while(bytes_read < len)
      {
      if(!priv->packet_len)
        {
        if(!priv->next_packet(ctx, block))
          return bytes_read;
        }

      bytes_to_copy = (priv->packet_len < (len - bytes_read)) ?
        priv->packet_len : (len - bytes_read);
      memcpy(buffer + bytes_read, priv->packet_ptr, bytes_to_copy);

      bytes_read += bytes_to_copy;
      priv->packet_ptr += bytes_to_copy;
      priv->packet_len -= bytes_to_copy;
      }
    return bytes_read;
    }

  fd = bgav_rtsp_get_fd(priv->r);

  if(block)
    return bgav_read_data_fd(ctx->opt, fd, buffer, len, ctx->opt->read_timeout);
  else
    return bgav_read_data_fd(ctx->opt, fd, buffer, len, 0);
  }

static int read_rtsp(bgav_input_context_t* ctx,
                     uint8_t * buffer, int len)
  {
  return do_read(ctx, buffer, len, 1);
  }

static int read_nonblock_rtsp(bgav_input_context_t * ctx,
                              uint8_t * buffer, int len)
  {
  return do_read(ctx, buffer, len, 0);
  }


const bgav_input_t bgav_input_rtsp =
  {
    .name =          "rtsp",
    .open =          open_rtsp,
    .read =          read_rtsp,
    .read_nonblock = read_nonblock_rtsp,
    .close =         close_rtsp,
  };

/* The following is ported from MPlayer */

/* This is some really ugly code moved here not to disturb the reader */

static const unsigned char xor_table[] = {
    0x05, 0x18, 0x74, 0xd0, 0x0d, 0x09, 0x02, 0x53,
    0xc0, 0x01, 0x05, 0x05, 0x67, 0x03, 0x19, 0x70,
    0x08, 0x27, 0x66, 0x10, 0x10, 0x72, 0x08, 0x09,
    0x63, 0x11, 0x03, 0x71, 0x08, 0x08, 0x70, 0x02,
    0x10, 0x57, 0x05, 0x18, 0x54, 0x00, 0x00, 0x00 };


#define BE_32C(x,y) (*((uint32_t*)(x))=be2me_32(y))

static void hash(char *field, char *param) {

  uint32_t a, b, c, d;
 

  /* fill variables */
  a= le2me_32(*(uint32_t*)(field));
  b= le2me_32(*(uint32_t*)(field+4));
  c= le2me_32(*(uint32_t*)(field+8));
  d= le2me_32(*(uint32_t*)(field+12));

  
  a = ((b & c) | (~b & d)) + le2me_32(*((uint32_t*)(param+0x00))) + a - 0x28955B88;
  a = ((a << 0x07) | (a >> 0x19)) + b;
  d = ((a & b) | (~a & c)) + le2me_32(*((uint32_t*)(param+0x04))) + d - 0x173848AA;
  d = ((d << 0x0c) | (d >> 0x14)) + a;
  c = ((d & a) | (~d & b)) + le2me_32(*((uint32_t*)(param+0x08))) + c + 0x242070DB;
  c = ((c << 0x11) | (c >> 0x0f)) + d;
  b = ((c & d) | (~c & a)) + le2me_32(*((uint32_t*)(param+0x0c))) + b - 0x3E423112;
  b = ((b << 0x16) | (b >> 0x0a)) + c;
  a = ((b & c) | (~b & d)) + le2me_32(*((uint32_t*)(param+0x10))) + a - 0x0A83F051;
  a = ((a << 0x07) | (a >> 0x19)) + b;
  d = ((a & b) | (~a & c)) + le2me_32(*((uint32_t*)(param+0x14))) + d + 0x4787C62A;
  d = ((d << 0x0c) | (d >> 0x14)) + a;
  c = ((d & a) | (~d & b)) + le2me_32(*((uint32_t*)(param+0x18))) + c - 0x57CFB9ED;
  c = ((c << 0x11) | (c >> 0x0f)) + d;
  b = ((c & d) | (~c & a)) + le2me_32(*((uint32_t*)(param+0x1c))) + b - 0x02B96AFF;
  b = ((b << 0x16) | (b >> 0x0a)) + c;
  a = ((b & c) | (~b & d)) + le2me_32(*((uint32_t*)(param+0x20))) + a + 0x698098D8;
  a = ((a << 0x07) | (a >> 0x19)) + b;
  d = ((a & b) | (~a & c)) + le2me_32(*((uint32_t*)(param+0x24))) + d - 0x74BB0851;
  d = ((d << 0x0c) | (d >> 0x14)) + a;
  c = ((d & a) | (~d & b)) + le2me_32(*((uint32_t*)(param+0x28))) + c - 0x0000A44F;
  c = ((c << 0x11) | (c >> 0x0f)) + d;
  b = ((c & d) | (~c & a)) + le2me_32(*((uint32_t*)(param+0x2C))) + b - 0x76A32842;
  b = ((b << 0x16) | (b >> 0x0a)) + c;
  a = ((b & c) | (~b & d)) + le2me_32(*((uint32_t*)(param+0x30))) + a + 0x6B901122;
  a = ((a << 0x07) | (a >> 0x19)) + b;
  d = ((a & b) | (~a & c)) + le2me_32(*((uint32_t*)(param+0x34))) + d - 0x02678E6D;
  d = ((d << 0x0c) | (d >> 0x14)) + a;
  c = ((d & a) | (~d & b)) + le2me_32(*((uint32_t*)(param+0x38))) + c - 0x5986BC72;
  c = ((c << 0x11) | (c >> 0x0f)) + d;
  b = ((c & d) | (~c & a)) + le2me_32(*((uint32_t*)(param+0x3c))) + b + 0x49B40821;
  b = ((b << 0x16) | (b >> 0x0a)) + c;
  
  a = ((b & d) | (~d & c)) + le2me_32(*((uint32_t*)(param+0x04))) + a - 0x09E1DA9E;
  a = ((a << 0x05) | (a >> 0x1b)) + b;
  d = ((a & c) | (~c & b)) + le2me_32(*((uint32_t*)(param+0x18))) + d - 0x3FBF4CC0;
  d = ((d << 0x09) | (d >> 0x17)) + a;
  c = ((d & b) | (~b & a)) + le2me_32(*((uint32_t*)(param+0x2c))) + c + 0x265E5A51;
  c = ((c << 0x0e) | (c >> 0x12)) + d;
  b = ((c & a) | (~a & d)) + le2me_32(*((uint32_t*)(param+0x00))) + b - 0x16493856;
  b = ((b << 0x14) | (b >> 0x0c)) + c;
  a = ((b & d) | (~d & c)) + le2me_32(*((uint32_t*)(param+0x14))) + a - 0x29D0EFA3;
  a = ((a << 0x05) | (a >> 0x1b)) + b;
  d = ((a & c) | (~c & b)) + le2me_32(*((uint32_t*)(param+0x28))) + d + 0x02441453;
  d = ((d << 0x09) | (d >> 0x17)) + a;
  c = ((d & b) | (~b & a)) + le2me_32(*((uint32_t*)(param+0x3c))) + c - 0x275E197F;
  c = ((c << 0x0e) | (c >> 0x12)) + d;
  b = ((c & a) | (~a & d)) + le2me_32(*((uint32_t*)(param+0x10))) + b - 0x182C0438;
  b = ((b << 0x14) | (b >> 0x0c)) + c;
  a = ((b & d) | (~d & c)) + le2me_32(*((uint32_t*)(param+0x24))) + a + 0x21E1CDE6;
  a = ((a << 0x05) | (a >> 0x1b)) + b;
  d = ((a & c) | (~c & b)) + le2me_32(*((uint32_t*)(param+0x38))) + d - 0x3CC8F82A;
  d = ((d << 0x09) | (d >> 0x17)) + a;
  c = ((d & b) | (~b & a)) + le2me_32(*((uint32_t*)(param+0x0c))) + c - 0x0B2AF279;
  c = ((c << 0x0e) | (c >> 0x12)) + d;
  b = ((c & a) | (~a & d)) + le2me_32(*((uint32_t*)(param+0x20))) + b + 0x455A14ED;
  b = ((b << 0x14) | (b >> 0x0c)) + c;
  a = ((b & d) | (~d & c)) + le2me_32(*((uint32_t*)(param+0x34))) + a - 0x561C16FB;
  a = ((a << 0x05) | (a >> 0x1b)) + b;
  d = ((a & c) | (~c & b)) + le2me_32(*((uint32_t*)(param+0x08))) + d - 0x03105C08;
  d = ((d << 0x09) | (d >> 0x17)) + a;
  c = ((d & b) | (~b & a)) + le2me_32(*((uint32_t*)(param+0x1c))) + c + 0x676F02D9;
  c = ((c << 0x0e) | (c >> 0x12)) + d;
  b = ((c & a) | (~a & d)) + le2me_32(*((uint32_t*)(param+0x30))) + b - 0x72D5B376;
  b = ((b << 0x14) | (b >> 0x0c)) + c;
  
  a = (b ^ c ^ d) + le2me_32(*((uint32_t*)(param+0x14))) + a - 0x0005C6BE;
  a = ((a << 0x04) | (a >> 0x1c)) + b;
  d = (a ^ b ^ c) + le2me_32(*((uint32_t*)(param+0x20))) + d - 0x788E097F;
  d = ((d << 0x0b) | (d >> 0x15)) + a;
  c = (d ^ a ^ b) + le2me_32(*((uint32_t*)(param+0x2c))) + c + 0x6D9D6122;
  c = ((c << 0x10) | (c >> 0x10)) + d;
  b = (c ^ d ^ a) + le2me_32(*((uint32_t*)(param+0x38))) + b - 0x021AC7F4;
  b = ((b << 0x17) | (b >> 0x09)) + c;
  a = (b ^ c ^ d) + le2me_32(*((uint32_t*)(param+0x04))) + a - 0x5B4115BC;
  a = ((a << 0x04) | (a >> 0x1c)) + b;
  d = (a ^ b ^ c) + le2me_32(*((uint32_t*)(param+0x10))) + d + 0x4BDECFA9;
  d = ((d << 0x0b) | (d >> 0x15)) + a;
  c = (d ^ a ^ b) + le2me_32(*((uint32_t*)(param+0x1c))) + c - 0x0944B4A0;
  c = ((c << 0x10) | (c >> 0x10)) + d;
  b = (c ^ d ^ a) + le2me_32(*((uint32_t*)(param+0x28))) + b - 0x41404390;
  b = ((b << 0x17) | (b >> 0x09)) + c;
  a = (b ^ c ^ d) + le2me_32(*((uint32_t*)(param+0x34))) + a + 0x289B7EC6;
  a = ((a << 0x04) | (a >> 0x1c)) + b;
  d = (a ^ b ^ c) + le2me_32(*((uint32_t*)(param+0x00))) + d - 0x155ED806;
  d = ((d << 0x0b) | (d >> 0x15)) + a;
  c = (d ^ a ^ b) + le2me_32(*((uint32_t*)(param+0x0c))) + c - 0x2B10CF7B;
  c = ((c << 0x10) | (c >> 0x10)) + d;
  b = (c ^ d ^ a) + le2me_32(*((uint32_t*)(param+0x18))) + b + 0x04881D05;
  b = ((b << 0x17) | (b >> 0x09)) + c;
  a = (b ^ c ^ d) + le2me_32(*((uint32_t*)(param+0x24))) + a - 0x262B2FC7;
  a = ((a << 0x04) | (a >> 0x1c)) + b;
  d = (a ^ b ^ c) + le2me_32(*((uint32_t*)(param+0x30))) + d - 0x1924661B;
  d = ((d << 0x0b) | (d >> 0x15)) + a;
  c = (d ^ a ^ b) + le2me_32(*((uint32_t*)(param+0x3c))) + c + 0x1fa27cf8;
  c = ((c << 0x10) | (c >> 0x10)) + d;
  b = (c ^ d ^ a) + le2me_32(*((uint32_t*)(param+0x08))) + b - 0x3B53A99B;
  b = ((b << 0x17) | (b >> 0x09)) + c;
  
  a = ((~d | b) ^ c)  + le2me_32(*((uint32_t*)(param+0x00))) + a - 0x0BD6DDBC;
  a = ((a << 0x06) | (a >> 0x1a)) + b; 
  d = ((~c | a) ^ b)  + le2me_32(*((uint32_t*)(param+0x1c))) + d + 0x432AFF97;
  d = ((d << 0x0a) | (d >> 0x16)) + a; 
  c = ((~b | d) ^ a)  + le2me_32(*((uint32_t*)(param+0x38))) + c - 0x546BDC59;
  c = ((c << 0x0f) | (c >> 0x11)) + d; 
  b = ((~a | c) ^ d)  + le2me_32(*((uint32_t*)(param+0x14))) + b - 0x036C5FC7;
  b = ((b << 0x15) | (b >> 0x0b)) + c; 
  a = ((~d | b) ^ c)  + le2me_32(*((uint32_t*)(param+0x30))) + a + 0x655B59C3;
  a = ((a << 0x06) | (a >> 0x1a)) + b; 
  d = ((~c | a) ^ b)  + le2me_32(*((uint32_t*)(param+0x0C))) + d - 0x70F3336E;
  d = ((d << 0x0a) | (d >> 0x16)) + a; 
  c = ((~b | d) ^ a)  + le2me_32(*((uint32_t*)(param+0x28))) + c - 0x00100B83;
  c = ((c << 0x0f) | (c >> 0x11)) + d; 
  b = ((~a | c) ^ d)  + le2me_32(*((uint32_t*)(param+0x04))) + b - 0x7A7BA22F;
  b = ((b << 0x15) | (b >> 0x0b)) + c; 
  a = ((~d | b) ^ c)  + le2me_32(*((uint32_t*)(param+0x20))) + a + 0x6FA87E4F;
  a = ((a << 0x06) | (a >> 0x1a)) + b; 
  d = ((~c | a) ^ b)  + le2me_32(*((uint32_t*)(param+0x3c))) + d - 0x01D31920;
  d = ((d << 0x0a) | (d >> 0x16)) + a; 
  c = ((~b | d) ^ a)  + le2me_32(*((uint32_t*)(param+0x18))) + c - 0x5CFEBCEC;
  c = ((c << 0x0f) | (c >> 0x11)) + d; 
  b = ((~a | c) ^ d)  + le2me_32(*((uint32_t*)(param+0x34))) + b + 0x4E0811A1;
  b = ((b << 0x15) | (b >> 0x0b)) + c; 
  a = ((~d | b) ^ c)  + le2me_32(*((uint32_t*)(param+0x10))) + a - 0x08AC817E;
  a = ((a << 0x06) | (a >> 0x1a)) + b; 
  d = ((~c | a) ^ b)  + le2me_32(*((uint32_t*)(param+0x2c))) + d - 0x42C50DCB;
  d = ((d << 0x0a) | (d >> 0x16)) + a; 
  c = ((~b | d) ^ a)  + le2me_32(*((uint32_t*)(param+0x08))) + c + 0x2AD7D2BB;
  c = ((c << 0x0f) | (c >> 0x11)) + d; 
  b = ((~a | c) ^ d)  + le2me_32(*((uint32_t*)(param+0x24))) + b - 0x14792C6F;
  b = ((b << 0x15) | (b >> 0x0b)) + c; 

#ifdef LOG
  printf("real: hash output: %x %x %x %x\n", a, b, c, d);
#endif
  
  a += le2me_32(*((uint32_t *)(field+0)));
  *((uint32_t *)(field+0)) = le2me_32(a);
  b += le2me_32(*((uint32_t *)(field+4)));
  *((uint32_t *)(field+4)) = le2me_32(b);
  c += le2me_32(*((uint32_t *)(field+8)));
  *((uint32_t *)(field+8)) = le2me_32(c);
  d += le2me_32(*((uint32_t *)(field+12)));
  *((uint32_t *)(field+12)) = le2me_32(d);

#ifdef LOG
  printf("real: hash field:\n");
  hexdump(field, 64+24);
#endif
}



static void call_hash (char *key, char *challenge, int len)
  {

  uint32_t *ptr1, *ptr2;
  uint32_t a, b, c, d;
  uint32_t tmp;

  ptr1=(uint32_t*)(key+16);
  ptr2=(uint32_t*)(key+20);
  
  a = le2me_32(*ptr1);
  b = (a >> 3) & 0x3f;
  a += len * 8;
  *ptr1 = le2me_32(a);
  
  if (a < (len << 3))
  {
#ifdef LOG
    printf("not verified: (len << 3) > a true\n");
#endif
    ptr2 += 4;
  }

  tmp = le2me_32(*ptr2);
  tmp += (len >> 0x1d);
  *ptr2 = le2me_32(tmp);
  a = 64 - b;
  c = 0;  
  if (a <= len)
  {

    memcpy(key+b+24, challenge, a);
    hash(key, key+24);
    c = a;
    d = c + 0x3f;
    
    while ( d < len ) {

#ifdef LOG
      printf("not verified:  while ( d < len )\n");
#endif
      hash(key, challenge+d-0x3f);
      d += 64;
      c += 64;
    }
    b = 0;
  }
  
  memcpy(key+b+24, challenge+c, len-c);
}



static void calc_response (char *result, char *field)
  {

  char buf1[128];
  char buf2[128];
  int i;

  memset (buf1, 0, 64);
  *buf1 = 128;
  
  memcpy (buf2, field+16, 8);
  
  i = ( le2me_32(*((uint32_t*)(buf2))) >> 3 ) & 0x3f;
 
  if (i < 56) {
    i = 56 - i;
  } else {
#ifdef LOG
    printf("not verified: ! (i < 56)\n");
#endif
    i = 120 - i;
  }

  call_hash (field, buf1, i);
  call_hash (field, buf2, 8);

  memcpy (result, field, 16);

  }


static void calc_response_string (char *result, char *challenge)
  {
  char field[128];
  char zres[20];
  int  i;
      
  /* initialize our field */
  BE_32C (field,      0x01234567);
  BE_32C ((field+4),  0x89ABCDEF);
  BE_32C ((field+8),  0xFEDCBA98);
  BE_32C ((field+12), 0x76543210);
  BE_32C ((field+16), 0x00000000);
  BE_32C ((field+20), 0x00000000);

  /* calculate response */
  call_hash(field, challenge, 64);
  calc_response(zres,field);
 
  /* convert zres to ascii string */
  for (i=0; i<16; i++ ) {
    char a, b;
    
    a = (zres[i] >> 4) & 15;
    b = zres[i] & 15;

    result[i*2]   = ((a<10) ? (a+48) : (a+87)) & 255;
    result[i*2+1] = ((b<10) ? (b+48) : (b+87)) & 255;
  }
}

static void
real_calc_response_and_checksum (char *response, char *chksum, char *challenge)
  {
  int   ch_len, table_len, resp_len;
  int   i;
  char *ptr;
  char  buf[128];

  /* initialize return values */
  memset(response, 0, 64);
  memset(chksum, 0, 34);
  
  /* initialize buffer */
  memset(buf, 0, 128);
  ptr=buf;
  BE_32C(ptr, 0xa1e9149d);
  ptr+=4;
  BE_32C(ptr, 0x0e6b3b59);
  ptr+=4;

  /* some (length) checks */
  if (challenge != NULL)
    {
    ch_len = strlen (challenge);
    
    if (ch_len == 40) /* what a hack... */
      {
      challenge[32]=0;
      ch_len=32;
      }
    if ( ch_len > 56 )
      ch_len=56;
    
    /* copy challenge to buf */
    memcpy(ptr, challenge, ch_len);
    }
  
  if (xor_table != NULL)
    {
    table_len = strlen((char*)xor_table);
    
    if (table_len > 56) table_len=56;
    
    /* xor challenge bytewise with xor_table */
    for (i=0; i<table_len; i++)
      ptr[i] = ptr[i] ^ xor_table[i];
    }
  
  calc_response_string (response, buf);
  
  /* add tail */
  resp_len = strlen (response);
  strcpy (&response[resp_len], "01d0a8e3");
  
  /* calculate checksum */
  for (i=0; i<resp_len/4; i++)
    chksum[i] = response[i*4];
  }


