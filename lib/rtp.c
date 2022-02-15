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
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif


#include <avdec_private.h>
// #include <inttypes.h>

#ifdef HAVE_POLL
#include <poll.h>
#endif

#include <rtp.h>

#include <bitstream.h>

#ifdef HAVE_OGG
#include <ogg/ogg.h>
#endif

// #define FOURCC_UNKNOWN BGAV_MK_FOURCC('?','?','?','?')
#define LOG_DOMAIN "rtp"

/* Stream specific init and parse functions are at the end of the file */

/* mpeg4-generic */
static int init_mpeg4_generic_audio(bgav_stream_t * s);

/* H264 */
static int init_h264(bgav_stream_t * s);

static int init_mpa(bgav_stream_t * s);
static int init_mpv(bgav_stream_t * s);


static int init_mp4v_es(bgav_stream_t * s);

#ifdef HAVE_OGG
static int init_ogg(bgav_stream_t * s);
#endif

static int init_h263_1998(bgav_stream_t * s);

static int rtp_header_read(bgav_input_context_t * ctx,
                           rtp_header_t * ret)
  {
  int i;
  uint32_t h;
  if(!bgav_input_read_32_be(ctx, &h))
    return 0;
  ret->version         = (h >> 30) & 0x03;
  ret->padding         = (h >> 29) & 0x01;
  ret->extension       = (h >> 28) & 0x01;
  ret->csrc_count      = (h >> 24) & 0x0f;
  ret->marker          = (h >> 23) & 0x01;
  ret->payload_type    = (h >> 16) & 0x7f;
  ret->sequence_number = (h)       & 0xffff;

  if(!bgav_input_read_32_be(ctx, &h))
    return 0;
  ret->timestamp = h;
  if(!bgav_input_read_32_be(ctx, &ret->ssrc))
    return 0;
  for(i = 0; i < ret->csrc_count; i++)
    {
    if(!bgav_input_read_32_be(ctx, &ret->csrc_list[i]))
      return 0;
    }
  return 1;
  }

#if 0
static void rtp_header_dump(rtp_header_t * h)
  {
  int i;
  bgav_dprintf("RTP Header\n");
  bgav_dprintf("  version:      %d\n", h->version);
  bgav_dprintf("  padding:      %d\n", h->padding);
  bgav_dprintf("  extension:    %d\n", h->extension);
  bgav_dprintf("  csrc_count:   %d\n", h->csrc_count);
  bgav_dprintf("  marker:       %d\n", h->marker);
  bgav_dprintf("  payload_type: %d\n", h->payload_type);
  bgav_dprintf("  seq:          %d\n", h->sequence_number);
  bgav_dprintf("  timestamp:    %"PRId64"\n", h->timestamp);
  for(i = 0; i < h->csrc_count; i++)
    {
    bgav_dprintf("  csrc[%d]:    %d\n", i, h->csrc_list[i]);
    }
  }
#endif

/* */

typedef struct
  {
  const char * name;
  const uint32_t fourcc;
  int (*init)(bgav_stream_t * s);
  } dynamic_payload_t;

typedef struct
  {
  int format;
  const uint32_t fourcc;
  int bits;
  int timescale;
  int (*init)(bgav_stream_t * s);
  } static_payload_t;

static const static_payload_t static_payloads[] =
  {
    // 0          PCMU          A              8000          1     [RFC3551]
    { 0, BGAV_MK_FOURCC('u','l','a','w'), 8, 8000 },
    // 1          Reserved	  
    // 2          Reserved
    // 3          GSM           A              8000          1     [RFC3551]
    { 3, BGAV_MK_FOURCC('G', 'S', 'M', ' '), 0, 8000 },
    // 4          G723          A              8000          1     [Kumar]
    // 5          DVI4          A              8000          1     [RFC3551]
    // 6          DVI4          A              16000         1     [RFC3551]
    // 7          LPC           A              8000          1     [RFC3551]
    // 8          PCMA          A              8000          1     [RFC3551]
    { 8, BGAV_MK_FOURCC('a','l','a','w'), 8, 8000 },
    // 9          G722          A              8000          1     [RFC3551]
    // 10         L16           A              44100         2     [RFC3551]
    { 10, BGAV_MK_FOURCC('t','w','o','s'), 16, 44100 },
    // 11         L16           A              44100         1     [RFC3551]
    { 11, BGAV_MK_FOURCC('t','w','o','s'), 16, 44100 },
    // 12         QCELP         A              8000          1 
    { 11, BGAV_MK_FOURCC('q','c','l','p'), 0, 8000 },
    // 13         CN            A              8000          1     [RFC3389]
    // 14         MPA           A              90000               [RFC3551][RFC2250]
    { 14, BGAV_MK_FOURCC('.','m','p','3'), 0, 90000, init_mpa },
    // 15         G728          A              8000          1     [RFC3551]
    // 16         DVI4          A              11025         1     [DiPol]
    // 17         DVI4          A              22050         1     [DiPol]
    // 18         G729          A              8000          1
    // 19         reserved      A
    // 20         unassigned    A
    // 21         unassigned    A
    // 22         unassigned    A
    // 23         unassigned    A
    // 24         unassigned    V
    // 25         CelB          V              90000               [RFC2029]
    // 26         JPEG          V              90000               [RFC2435]
    { 26, BGAV_MK_FOURCC('j','p','e','g'), 0, 90000 },
    // 27         unassigned    V
    // 28         nv            V              90000               [RFC3551]
    // 29         unassigned    V
    // 30         unassigned    V
    // 31         H261          V              90000               [RFC2032]
    { 31, BGAV_MK_FOURCC('h','2','6','1'), 0, 90000 },
    // 32         MPV           V              90000               [RFC2250]
    { 32, BGAV_MK_FOURCC('m','p','g','v'), 0, 90000, init_mpv },
    // 33         MP2T          AV             90000               [RFC2250]
    // 34         H263          V              90000               [Zhu]
    { 34, BGAV_MK_FOURCC('h','2','6','3'), 0, 90000 },
    { -1, },
  };

static const dynamic_payload_t dynamic_video_payloads[] =
  {
    { "H264",      BGAV_MK_FOURCC('h', '2', '6', '4'), init_h264 },
    { "MP4V-ES",   BGAV_MK_FOURCC('m', 'p', '4', 'v'), init_mp4v_es },
    { "H263-1998", BGAV_MK_FOURCC('h', '2', '6', '3'), init_h263_1998 },
    { "H263-2000", BGAV_MK_FOURCC('h', '2', '6', '3'), init_h263_1998 },
#ifdef HAVE_OGG
    { "theora",    BGAV_MK_FOURCC('T', 'H', 'R', 'A'), init_ogg },
#endif
    { },
  };

static const dynamic_payload_t dynamic_audio_payloads[] =
  {
    { "mpeg4-generic", BGAV_MK_FOURCC('m', 'p', '4', 'a'), init_mpeg4_generic_audio },
#ifdef HAVE_OGG
    { "vorbis",        BGAV_MK_FOURCC('V','B','I','S'),    init_ogg },
#endif
    { },
  };

struct rtp_priv_s
  {
  pthread_mutex_t mutex;

  struct pollfd * pollfds;
  int num_pollfds;
  
  /* The core is not fully initialized at the time
     the input thread is started. Especially unsupported
     streams can be deleted. This must happen without
     disturbing the input thread */

  struct
    {
    bgav_rtp_packet_buffer_t * buf;
    bgav_stream_t * s;
    int interleave_base;
    uint32_t lsr;
    uint32_t server_ssrc;
    uint32_t client_ssrc;
    int sr_count;
    
    int rtp_fd;
    int rtcp_fd;
    } * streams;
  int num_streams;
  
  bgav_input_context_t * input_mem;
  int tcp;
  
  pthread_t read_thread;
  };

#if 0
static void lock_rtp(rtp_priv_t*p)
  {
  pthread_mutex_lock(&p->mutex);
  }

static void unlock_rtp(rtp_priv_t* p)
  {
  pthread_mutex_unlock(&p->mutex);
  }
#endif

static void cleanup_stream_rtp(bgav_stream_t * s)
  {
  rtp_stream_priv_t * priv = s->priv;
  int i;

  if(!priv) return;

  if(s->demuxer && s->demuxer->priv)
    {
    for(i = 0; i < priv->rtp_priv->num_streams; i++)
      {
      if(priv->rtp_priv->streams[i].s == s)
        {
        priv->rtp_priv->streams[i].s = NULL;
        bgav_rtp_packet_buffer_set_eof(priv->rtp_priv->streams[i].buf);
        break;
        }
      }
    }
  
  if(!priv) return;
  
  if(priv->control_url) free(priv->control_url);
  if(priv->fmtp) gavl_strbreak_free(priv->fmtp);
  if(priv->free_priv)
    priv->free_priv(priv);
  free(priv);
  }

static void check_dynamic(bgav_stream_t * s,
                          const dynamic_payload_t * dynamic_payloads,
                          const char * rtpmap)
  {
  const char * pos1, *pos2;
  int i;
  pos1 = rtpmap;
  pos2 = strchr(pos1, '/');
  if(!pos2)
    return;
  
  i = 0;

  while(dynamic_payloads[i].name)
    {
    if((strlen(dynamic_payloads[i].name) == pos2 - pos1) &&
       !strncasecmp(dynamic_payloads[i].name, pos1, pos2 - pos1))
      {
      s->fourcc = dynamic_payloads[i].fourcc;
      if(dynamic_payloads[i].init)
        if(!dynamic_payloads[i].init(s))
          {
          /* Make the stream undecodable */
          s->fourcc = 0;
          return;
          }
      break;
      }
    i++;
    }
  }

static int find_codec(bgav_stream_t * s, bgav_sdp_media_desc_t * md,
                      int format_index)
  {
  int format = atoi(md->formats[0]);
  int i;
  char * fmtp;
  rtp_stream_priv_t * sp = s->priv;

  if(bgav_sdp_get_attr_fmtp(md->attributes, md->num_attributes,
                            format, &fmtp))
    sp->fmtp = gavl_strbreak(fmtp, ';');
  
  if(format >= 96) // Dynamic
    {
    char * rtpmap;
    char * pos;
    if(!bgav_sdp_get_attr_rtpmap(md->attributes, md->num_attributes,
                                 format, &rtpmap))
      {
      return 0;
      }

    pos = strchr(rtpmap, '/');

    if(pos)
      {
      pos++;
      s->timescale = atoi(pos);
      }

    if(s->type == GAVL_STREAM_AUDIO)
      {
      pos = strchr(pos, '/');
      if(pos)
        {
        pos++;
        s->data.audio.format->num_channels = atoi(pos);
        }
      }
    
    if(s->type == GAVL_STREAM_AUDIO)
      {
      check_dynamic(s, dynamic_audio_payloads, rtpmap);
      }
    else
      {
      check_dynamic(s, dynamic_video_payloads, rtpmap);
      }
    
    if(!s->fourcc)
      return 0;
    
    return 1;
    }
  else
    {
    i = 0;
    while(static_payloads[i].format >= 0)
      {
      if(static_payloads[i].format == format)
        {
        s->fourcc = static_payloads[i].fourcc;
        s->timescale = static_payloads[i].timescale;
        if(static_payloads[i].init)
          {
          if(!static_payloads[i].init(s))
            s->fourcc = 0;
          }
        return 1;
        }
      i++;
      }
    return 0;
    }
  }

static int
init_stream(bgav_demuxer_context_t * ctx,
            bgav_stream_t * s, bgav_sdp_media_desc_t * md,
            int format_index)
  {
  rtp_stream_priv_t * sp;
  char * control, *tmp_string;
  //  int i;
  s->cleanup = cleanup_stream_rtp;
  sp = calloc(1, sizeof(*sp));
  s->priv = sp;
  sp->client_ssrc = rand();
  sp->rtp_priv = ctx->priv;
  sp->interleave_base = -2;
  if(!find_codec(s, md, format_index))
    return 0;
  
  /* Get control URL */
  if(!bgav_sdp_get_attr_string(md->attributes, md->num_attributes,
                               "control", &control))
    {
    return 0;
    }
  tmp_string = NULL;
  if(bgav_url_split(control,
                    &tmp_string,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL))
    {
    if(tmp_string) /* Complete URL */
      sp->control_url = gavl_strdup(control);
    }
  if(!sp->control_url)
    sp->control_url = bgav_sprintf("%s/%s", ctx->input->url, control);
  return 1;
  }

static int read_rtp_packet(bgav_demuxer_context_t * ctx,
                           int fd, int len, bgav_rtp_packet_buffer_t * b)
  {
  rtp_packet_t * p;
  int bytes_read;
  rtp_priv_t * priv;
  priv = ctx->priv;

  if(bgav_rtp_packet_buffer_get_eof(b))
    {
    return 0;
    }
  /* Read packet */
  p = bgav_rtp_packet_buffer_lock_write(b);
  
  if(!len)
    {
    bytes_read = bgav_udp_read(fd,
                               p->buffer, RTP_MAX_PACKET_LENGTH);
    }
  else
    {
    if(len > RTP_MAX_PACKET_LENGTH)
      return 0;
    if(bgav_input_read_data(ctx->input, p->buffer, len) < len)
      return 0;
    bytes_read = len;
    }
  
  bgav_input_reopen_memory(priv->input_mem, p->buffer, bytes_read);
  
  if(!rtp_header_read(priv->input_mem, &p->h))
    {
    return 0;
    }
  p->buf = p->buffer + priv->input_mem->position;
  p->len = bytes_read - priv->input_mem->position;

  /* Handle padding */
  if(p->h.padding)
    p->len -= p->buf[p->len-1];
  
  bgav_rtp_packet_buffer_unlock_write(b);
  
  return 1;
  }

static int read_rtcp_packet(bgav_demuxer_context_t * ctx,
                            int fd, int len,
                            bgav_rtp_packet_buffer_t * b, int * sr_count,
                            uint32_t client_ssrc, uint32_t server_ssrc)
  {
  rtp_priv_t * priv;
  
  int bytes_read;
  rtcp_sr_t rr;
  uint8_t buf[RTP_MAX_PACKET_LENGTH];
  uint32_t  lsr;
  priv = ctx->priv;
  /* Read packet */

  if(!len)
    {
    bytes_read = bgav_udp_read(fd,
                               buf, RTP_MAX_PACKET_LENGTH);
    }
  else
    {
    if(len > RTP_MAX_PACKET_LENGTH)
      return 0;
    if(bgav_input_read_data(ctx->input, buf, len) < len)
      return 0;
    bytes_read = len;
    }
  
  if(buf[1] == 200)
    {
    bgav_input_reopen_memory(priv->input_mem, buf, bytes_read);
    if(!bgav_rtcp_sr_read(priv->input_mem, &rr))
      return 0;

    //    bgav_rtcp_sr_dump(&rr);
    lsr = (rr.ntp_time >> 16) && 0xFFFFFFFF;
    (*sr_count)++;
    
    /* TODO: The standard has more sophisticated formulas
     * for calculating the interval for receiver reports.
     * Here we assume a constant fraction of the number
     * (not bandwidth!) of the sender reports
     */
    
    if((*sr_count >= 5) && !len)
      {
      *sr_count = 0;
      memset(&rr, 0, sizeof(rr));
      /* Send seceiver report */
      bgav_rtcp_rr_setup(&rr, bgav_rtp_packet_buffer_get_stats(b), lsr,
                         client_ssrc, server_ssrc);
      bytes_read = bgav_rtcp_rr_write(&rr, buf);
      sendto(fd, buf, bytes_read, 0, NULL, 0);
      }
    }
#if 0
  else if(buf[1] == 203)
    {
    /* Goodbye */
    }
#endif
  return 1;
  }

static void init_pollfds(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s;
  rtp_priv_t * priv;
  int i, index;
  rtp_stream_priv_t * sp;
  priv = ctx->priv;

  if(!priv->pollfds)
    {
    priv->num_pollfds = priv->num_streams*2;
    priv->pollfds = calloc(priv->num_pollfds, sizeof(*priv->pollfds));
    }
  
  index = 0;
  for(i = 0; i < ctx->tt->cur->num_audio_streams; i++)
    {
    s = bgav_track_get_audio_stream(ctx->tt->cur, i);
    sp = s->priv;
    priv->pollfds[index].fd = sp->rtp_fd;
    priv->pollfds[index].events = POLLIN;
    index++;
    priv->pollfds[index].fd = sp->rtcp_fd;
    priv->pollfds[index].events = POLLIN;
    index++;
    }
  for(i = 0; i < ctx->tt->cur->num_video_streams; i++)
    {
    s = bgav_track_get_video_stream(ctx->tt->cur, i);
    sp = s->priv;
    priv->pollfds[index].fd = sp->rtp_fd;
    priv->pollfds[index].events = POLLIN;
    index++;
    priv->pollfds[index].fd = sp->rtcp_fd;
    priv->pollfds[index].events = POLLIN;
    index++;
    }
  
  }

static int handle_pollfds(bgav_demuxer_context_t * ctx)
  {
  //  int bytes_read;
  rtp_priv_t * priv;
  int i, index;
  int ret = 0;
  priv = ctx->priv;

  index = 0;
    
  for(i = 0; i < priv->num_streams; i++)
    {
    if(priv->pollfds[index].revents & POLLIN)
      {
      if(read_rtp_packet(ctx, priv->pollfds[index].fd, 0, priv->streams[i].buf))
        ret++;
      }
    index++;
    if(priv->pollfds[index].revents & POLLIN)
      {
      if(read_rtcp_packet(ctx, priv->pollfds[index].fd, 0, priv->streams[i].buf,
                          &priv->streams[i].sr_count,
                          priv->streams[i].client_ssrc, priv->streams[i].server_ssrc))
        ret++;
      }
    index++;
    }
  return ret;
  }

static int read_tcp_packet(bgav_demuxer_context_t * ctx,
                           int channel, int len)
  {
  rtp_priv_t * priv;
  int i;
  priv = ctx->priv;
  
  for(i = 0; i < priv->num_streams; i++)
    {
    if(priv->streams[i].interleave_base == channel)
      return read_rtp_packet(ctx, -1, len, priv->streams[i].buf);
    else if(priv->streams[i].interleave_base+1 == channel)
      return read_rtcp_packet(ctx, -1, len, priv->streams[i].buf,
                              &priv->streams[i].sr_count,
                              priv->streams[i].client_ssrc,
                              priv->streams[i].server_ssrc);
    }
  
  /* No stream found: Skip and return 1 */
  bgav_input_skip(ctx->input, len);
  return 1;
  }

/* Thread function */

static void set_eof(bgav_demuxer_context_t * ctx)
  {
  int i;
  rtp_priv_t * priv = ctx->priv;
  
  for(i = 0; i < priv->num_streams; i++)
    bgav_rtp_packet_buffer_set_eof(priv->streams[i].buf);
  }

static void * tcp_thread(void * data)
  {
  bgav_demuxer_context_t * ctx = data;
  uint8_t c = 0;
  uint16_t len;

  while(1)
    {
    while(c != '$')
      {
      if(!bgav_input_read_8(ctx->input, &c))
        break;
      }
    if(!bgav_input_read_8(ctx->input, &c) ||
       !bgav_input_read_16_be(ctx->input, &len))
      break;
    
    /* Process packet */
    if(!read_tcp_packet(ctx, c, len))
      break;
    }
  set_eof(ctx);
  return NULL;
  }

static void * udp_thread(void * data)
  {
  rtp_priv_t * priv;
  bgav_demuxer_context_t * ctx = data;
  priv = ctx->priv;

  while(1)
    {
    if(bgav_poll(priv->pollfds, priv->num_pollfds, ctx->opt->read_timeout) <= 0)
      {
      break;
      }
    if(!handle_pollfds(ctx))
      {
      break;
      }
    while(bgav_poll(priv->pollfds, priv->num_pollfds, 0) > 0)
      {
      if(!handle_pollfds(ctx))
        {
        break;
        }
      }
    }
  set_eof(ctx);
  return NULL;
  }

static int flush_stream(bgav_stream_t * s,
                        bgav_rtp_packet_buffer_t * buf, int * processed)
  {
  rtp_stream_priv_t * sp;
  rtp_packet_t * p;
  int num = 0;

  if(!s || (s->action != BGAV_STREAM_DECODE))
    {
    /* Just flush any packets */
    while(bgav_rtp_packet_buffer_try_lock_read(buf))
      {
      bgav_rtp_packet_buffer_unlock_read(buf);
      num++;
      }
    if(!num && bgav_rtp_packet_buffer_get_eof(buf))
      return 0;
    return 1;
    }
  sp = s->priv;
  
  /* Flush packet buffer */
  while((p = bgav_rtp_packet_buffer_try_lock_read(buf)))
    {
    /* We must do this in the output thread */
    p->h.timestamp -= sp->first_rtptime;
    
    if(sp->process)
      {
      if(!sp->process(s, &p->h, p->buf, p->len))
        {
        bgav_rtp_packet_buffer_unlock_read(buf);
        return 0;
        }
      (*processed)++;
      }
    bgav_rtp_packet_buffer_unlock_read(buf);
    num++;
    }
  if(!num && bgav_rtp_packet_buffer_get_eof(buf))
    return 0;
  return 1;
  }

static int next_packet_rtp(bgav_demuxer_context_t * ctx)
  {
  int ret = 0;
  int i;
  rtp_priv_t * priv;
  int processed = 0;
  
  priv = ctx->priv;

  for(i = 0; i < priv->num_streams; i++)
    {
    if(flush_stream(priv->streams[i].s, priv->streams[i].buf, &processed))
      ret = 1;
    }
  
  if(!processed)
    {
    /* HACK: Waiting a short time increases the chance for the
       input thread to lock the mutexes and write new data */
    gavl_time_t delay_time = GAVL_TIME_SCALE / 100;
    gavl_time_delay(&delay_time);
    }
  
  return ret;
  }

static void close_rtp(bgav_demuxer_context_t * ctx)
  {
  int i;
  rtp_priv_t * priv = ctx->priv;
  if(!priv)
    return;
  
  bgav_demuxer_rtp_stop(ctx);

  for(i = 0; i < priv->num_streams; i++)
    {
    if(priv->streams[i].buf)
      bgav_rtp_packet_buffer_destroy(priv->streams[i].buf);

    if(priv->streams[i].rtp_fd >= 0)
      closesocket(priv->streams[i].rtp_fd);
    if(priv->streams[i].rtcp_fd >= 0)
      closesocket(priv->streams[i].rtcp_fd);
    }
  if(priv->streams)
    free(priv->streams);
  
  if(priv->input_mem)
    {
    bgav_input_close(priv->input_mem);
    bgav_input_destroy(priv->input_mem);
    }
  if(priv->pollfds)
    free(priv->pollfds);

  pthread_mutex_destroy(&priv->mutex);
  
  free(priv);

  /* Hack: The cleanup routine for the streams might access free()d data,
     let's prevent this here */
  ctx->priv = NULL;
  }

void bgav_rtp_set_tcp(bgav_demuxer_context_t * ctx)
  {
  rtp_priv_t * priv = ctx->priv;
  if(!priv)
    return;
  priv->tcp = 1;
  }

static gavl_time_t get_duration(bgav_sdp_t * sdp)
  {
  char * str, *rest;
  double lower, upper;
  
  if(!bgav_sdp_get_attr_string(sdp->attributes, sdp->num_attributes,
                               "range", &str))
    return GAVL_TIME_UNDEFINED;
  
  if(strncasecmp(str, "npt=", 4))
    return GAVL_TIME_UNDEFINED;
  
  str+=4;

  lower = strtod(str, &rest);
  if(rest == str)
    return GAVL_TIME_UNDEFINED;

  if(*rest != '-')
    return GAVL_TIME_UNDEFINED;

  str = rest + 1;

  upper = strtod(str, &rest);
  if(rest == str)
    return GAVL_TIME_UNDEFINED;
  
  if(fabs(lower) > 0.001)
    return GAVL_TIME_UNDEFINED;
  
  return gavl_seconds_to_time(upper);
  }

int bgav_demuxer_rtp_open(bgav_demuxer_context_t * ctx,
                          bgav_sdp_t * sdp)
  {
  int i, j;
  bgav_stream_t * s;
  rtp_priv_t * priv;
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  pthread_mutex_init(&priv->mutex, NULL);
  
  priv->input_mem = bgav_input_open_memory(NULL, 0, ctx->opt);
  
  ctx->tt = bgav_track_table_create(1);

  for(i = 0; i < sdp->num_media; i++)
    {
    if(!strcmp(sdp->media[i].media, "audio"))
      {
      //for(j = 0; j < sdp->media[i].num_formats; j++)
      for(j = 0; j < 1; j++)
        {
        s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
        init_stream(ctx, s, &sdp->media[i], j);

        if(s->fourcc != BGAV_MK_FOURCC('.','m','p','3'))
          s->data.audio.format->samplerate = s->timescale;
        }
      }
    else if(!strcmp(sdp->media[i].media, "video"))
      {
      // for(j = 0; j < sdp->media[i].num_formats; j++)
      for(j = 0; j < 1; j++)
        {
        s = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
        init_stream(ctx, s, &sdp->media[i], j);
        }
      }
    }

  /* Check for duration */
  gavl_track_set_duration(ctx->tt->cur->info, get_duration(sdp));
  
  priv->num_streams = ctx->tt->cur->num_audio_streams +
    ctx->tt->cur->num_video_streams;
  /* Create the stream array */
  
  priv->streams = calloc(priv->num_streams, sizeof(*priv->streams));

  j = 0;
  for(i = 0; i < ctx->tt->cur->num_audio_streams; i++)
    {
    s = bgav_track_get_audio_stream(ctx->tt->cur, i);
    //    sp = ctx->tt->cur->audio_streams[i].priv;
    priv->streams[j].buf = bgav_rtp_packet_buffer_create(s->opt, s->timescale);
    priv->streams[j].s = s;
    j++;
    }
  for(i = 0; i < ctx->tt->cur->num_video_streams; i++)
    {
    s = bgav_track_get_video_stream(ctx->tt->cur, i);
    //    sp = ctx->tt->cur->video_streams[i].priv;
    priv->streams[j].buf = bgav_rtp_packet_buffer_create(s->opt, s->timescale);
    priv->streams[j].s = s;
    j++;
    }
  
  
  return 1;
  }

void bgav_demuxer_rtp_start(bgav_demuxer_context_t * ctx)
  {
  rtp_priv_t * priv;
  int i, j = 0;
  rtp_stream_priv_t * sp;
  bgav_stream_t * s;

  priv = ctx->priv;

  /* Copy stuff for the input thread */
  for(i = 0; i < ctx->tt->cur->num_audio_streams; i++)
    {
    s = bgav_track_get_audio_stream(ctx->tt->cur, i);
    
    sp = s->priv;
    priv->streams[j].interleave_base = sp->interleave_base;
    priv->streams[j].client_ssrc = sp->client_ssrc;
    priv->streams[j].server_ssrc = sp->server_ssrc;
    priv->streams[j].rtp_fd = sp->rtp_fd;
    priv->streams[j].rtcp_fd = sp->rtcp_fd;
    j++;
    }
  for(i = 0; i < ctx->tt->cur->num_video_streams; i++)
    {
    s = bgav_track_get_video_stream(ctx->tt->cur, i);

    sp = s->priv;
    priv->streams[j].interleave_base = sp->interleave_base;
    priv->streams[j].client_ssrc = sp->client_ssrc;
    priv->streams[j].server_ssrc = sp->server_ssrc;
    priv->streams[j].rtp_fd = sp->rtp_fd;
    priv->streams[j].rtcp_fd = sp->rtcp_fd;
    j++;
    }
  
  
  if(priv->tcp)
    {
    pthread_create(&priv->read_thread, NULL, tcp_thread, ctx);
    }
  else
    {
    init_pollfds(ctx);
    pthread_create(&priv->read_thread, NULL, udp_thread, ctx);
    }
  }

void bgav_demuxer_rtp_stop(bgav_demuxer_context_t * ctx)
  {
  rtp_priv_t * priv;
  priv = ctx->priv;
  set_eof(ctx);
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
           "Joining RTP thread...");

  pthread_join(priv->read_thread, NULL);
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
           "Joined RTP thread");
  }

bgav_demuxer_t bgav_demuxer_rtp =
  {
    .next_packet =  next_packet_rtp,
    .close =        close_rtp
  };


/* Format specific initialization and processing routines */

static uint8_t hex_to_byte(const char * s)
  {
  char c, ret;
  c = toupper(s[0]);

  if (c >= '0' && c <= '9')
    c = c - '0';
  else if (c >= 'A' && c <= 'F')
    c = c - 'A' + 10;

  ret = c << 4;

  c = toupper(s[1]);

  if (c >= '0' && c <= '9')
    c = c - '0';
  else if (c >= 'A' && c <= 'F')
    c = c - 'A' + 10;

  ret |= c;
  return ret;
  }

static char * find_fmtp(char ** fmtp, char * key)
  {
  int i = 0;
  char * pos;
  int len;
  while(fmtp[i])
    {
    pos = fmtp[i];
    while(isspace(*pos))
      pos++;
    len = strlen(key);
    if(!strncasecmp(key, pos, len) &&
       (pos[len] == '='))
      {
      pos += len+1;
      while(isspace(*pos))
        pos++;
      return pos;
      }
    i++;
    }
  return NULL;
  }


/* mpeg-4 AU parsing */

static int mpeg4_au_read(rtp_stream_priv_t * sp,
                         bgav_bitstream_t * b,
                         mpeg4_au_t * ret)
  {
  if(!bgav_bitstream_get(b, &ret->size, sp->priv.mpeg4_generic.sizelength) ||
     !bgav_bitstream_get(b, &ret->delta, sp->priv.mpeg4_generic.indexlength))
    {
    return 0;
    }
  return sp->priv.mpeg4_generic.sizelength +
    sp->priv.mpeg4_generic.indexlength;
  }

static int mpeg4_aus_read(bgav_stream_t * s,
                          uint8_t * data, int len)
  {
  bgav_bitstream_t bs;
  rtp_stream_priv_t * sp;
  int total_bits;

  total_bits = GAVL_PTR_2_16BE(data);data+=2;

  sp = s->priv;
  sp->priv.mpeg4_generic.num_aus = 0;
  bgav_bitstream_init(&bs, data, (total_bits+7)/8);
  while(bgav_bitstream_get_bits(&bs) >= 8)
    {
    if(sp->priv.mpeg4_generic.aus_alloc < sp->priv.mpeg4_generic.num_aus + 1)
      {
      sp->priv.mpeg4_generic.aus_alloc += 10;
      sp->priv.mpeg4_generic.aus =
        realloc(sp->priv.mpeg4_generic.aus,
                sp->priv.mpeg4_generic.aus_alloc *
                sizeof(*sp->priv.mpeg4_generic.aus));
      memset(sp->priv.mpeg4_generic.aus +
             sp->priv.mpeg4_generic.num_aus, 0,
             10 * sizeof(*sp->priv.mpeg4_generic.aus));
      }
    if(!mpeg4_au_read(sp, &bs,
                      &sp->priv.mpeg4_generic.aus[sp->priv.mpeg4_generic.num_aus]))
      return 0;
    sp->priv.mpeg4_generic.num_aus++;
    }
  return (total_bits+7)/8 + 2;
  }

#if 0
static void dump_aus(mpeg4_au_t * aus, int num)
  {
  int i;
  bgav_dprintf("Access units: %d\n", num);
  for(i = 0; i < num; i++)
    {
    bgav_dprintf("  AU %d, size: %d, delta: %d\n",
                 i, aus[i].size, aus[i].delta);
    }
  }
#endif

static int
process_aac(bgav_stream_t * s, rtp_header_t * h,
            uint8_t * data, int len)
  {
  rtp_stream_priv_t * sp;
  int aus_len, i;
  sp = s->priv;

  aus_len = mpeg4_aus_read(s, data, len);
  if(!aus_len)
    return 0;

  data += aus_len;
  //  dump_aus(sp->priv.mpeg4_generic.aus,
  //           sp->priv.mpeg4_generic.num_aus);

  if(sp->priv.mpeg4_generic.num_aus == 1)
    {
    /* Single AU: Can be one fragment or one packet */
    if(s->packet && (s->packet->pts != h->timestamp))
      {
      bgav_stream_done_packet_write(s, s->packet);
      s->packet = NULL;
      }
    if(!s->packet)
      {
      s->packet = bgav_stream_get_packet_write(s);
      s->packet->data_size = 0;
      s->packet->pts = h->timestamp;
      }
    bgav_packet_alloc(s->packet, s->packet->data_size +
                      sp->priv.mpeg4_generic.aus[0].size);
    memcpy(s->packet->data + s->packet->data_size,
           data, sp->priv.mpeg4_generic.aus[0].size);
    s->packet->data_size += sp->priv.mpeg4_generic.aus[0].size;
    }
  else /* Multiple AUs */
    {
    /* We send multiple AUs in one packet, because we don't
       want to calculate the intermediate timestamps.
       Doing so would be non-trivial if HE-AAC (with SBR) is used */
    int packet_size = 0;
    if(s->packet)
      {
      bgav_stream_done_packet_write(s, s->packet);
      s->packet = NULL;
      }
    for(i = 0; i < sp->priv.mpeg4_generic.num_aus; i++)
      {
      if(sp->priv.mpeg4_generic.aus[i].delta)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Interleaved MPEG-4 audio not supported yet");
        return 0;
        }
      packet_size += sp->priv.mpeg4_generic.aus[i].size;
      }
    s->packet = bgav_stream_get_packet_write(s);
    s->packet->pts = h->timestamp;
    bgav_packet_alloc(s->packet, packet_size);
    memcpy(s->packet->data, data, packet_size);
    s->packet->data_size = packet_size;
    bgav_stream_done_packet_write(s, s->packet);
    s->packet = NULL;
    }
  
  return 1;
  }

static void cleanup_mpeg4_generic_audio(rtp_stream_priv_t * priv)
  {
  if(priv->priv.mpeg4_generic.aus)
    free(priv->priv.mpeg4_generic.aus);
  }

static int init_mpeg4_generic_audio(bgav_stream_t * s)
  {
  rtp_stream_priv_t * sp;
  char * var;

  sp = s->priv;
  if(!sp->fmtp)
    return 0;
  sp->free_priv = cleanup_mpeg4_generic_audio;
  
  var = find_fmtp(sp->fmtp, "mode");
  if(!var)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "No audio mode for mpeg4-generic");
    return 0;
    }
  if(!strcasecmp(var, "AAC-hbr"))
    {
    sp->process = process_aac;
    }
  else if(!strcasecmp(var, "AAC-lbr"))
    {
    sp->process = process_aac;
    }
  else
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Unknown audio mode for mpeg4-generic: %s", var);
    return 0;
    }

  var = find_fmtp(sp->fmtp, "maxDisplacement");
  if(var && atoi(var))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Interleaved audio not yet supported for mpeg4-generic");
    return 0;
    }
  var = find_fmtp(sp->fmtp, "sizelength");
  if(!var)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "No sizelength for mpeg4-generic");
    return 0;
    }
  sp->priv.mpeg4_generic.sizelength = atoi(var);
  
  var = find_fmtp(sp->fmtp, "indexlength");
  if(!var)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "No indexlength for mpeg4-generic");
    return 0;
    }
  sp->priv.mpeg4_generic.indexlength = atoi(var);
  
  var = find_fmtp(sp->fmtp, "indexdeltalength");
  if(!var)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "No indexdeltalength for mpeg4-generic");
    return 0;
    }
  sp->priv.mpeg4_generic.indexdeltalength = atoi(var);
  
  var = find_fmtp(sp->fmtp, "config");
  if(var)
    {
    int i;
    s->ci->global_header_len = strlen(var)/2;
    s->ci->global_header = malloc(s->ci->global_header_len);
    i = 0;
    while(i < s->ci->global_header_len)
      {
      s->ci->global_header[i] = hex_to_byte(var);
      var += 2;
      i++;
      }
    }
  return 1;
  }

/* H.264 */

static void send_nal(bgav_stream_t * s, uint8_t * nal, int len,
                     int64_t time)
  {
  uint8_t start_sequence[]= { 0, 0, 1 };

  
  if(s->packet && (s->packet->pts != time))
    {
    bgav_stream_done_packet_write(s, s->packet);
    s->packet = NULL;
    }
  
  if(!s->packet)
    {
    s->packet = bgav_stream_get_packet_write(s);
    s->packet->data_size = 0;
    s->packet->pts = time;
    }
  
  bgav_packet_alloc(s->packet, s->packet->data_size + sizeof(start_sequence) + len);
  memcpy(s->packet->data + s->packet->data_size, start_sequence, sizeof(start_sequence));
  s->packet->data_size += sizeof(start_sequence);

  memcpy(s->packet->data + s->packet->data_size, nal, len);
  s->packet->data_size += len;
  
  }

static void append_nal(bgav_stream_t * s, uint8_t * nal, int len)
  {
  if(!s->packet)
    return;
  bgav_packet_alloc(s->packet, s->packet->data_size + len);
  memcpy(s->packet->data + s->packet->data_size, nal, len);
  s->packet->data_size += len;
  }

static int process_h264(bgav_stream_t * s, rtp_header_t * h, uint8_t * data, int len)
  {
  uint8_t global_nal = (*data & 0x1f);

  if(global_nal >= 1 && global_nal <= 23)
    {
    /* One packet one nal */
    send_nal(s, data, len, h->timestamp);
    return 1;
    }

  switch(global_nal)
    {
    case 24: // STAP-A (one packet, multiple nals)
      data++;
      len--;

      while(len > 2)
        {
        uint16_t nal_size;
        nal_size = GAVL_PTR_2_16BE(data);data+=2;
        send_nal(s, data, nal_size, h->timestamp);
        len -= (2 + nal_size);
        }
      break;
    case 28: // FU-A (fragmented nal)
      {
      uint8_t nal_header;
      
      nal_header = *data & 0xe0;
      data++;
      len--;                  // skip the fu_indicator
      if(*data >> 7) /* Start bit set */
        {
        nal_header |= (*data & 0x1f);
        /* Reconstruct */
        *data = nal_header;
        send_nal(s, data, len, h->timestamp);
        }
      else
        {
        data++;
        len--;                  // skip the fu_indicator
        append_nal(s, data, len);
        }
      }
      break;
    default:
      return 0;
      break;
    }
  
  return 1;
  }

static int init_h264(bgav_stream_t * s)
  {
  rtp_stream_priv_t * sp;
  char * value;
  uint8_t start_sequence[]= { 0, 0, 1 };
  sp = s->priv;

  /* Get extradata from the sprop-parameter-sets */
  value = find_fmtp(sp->fmtp, "sprop-parameter-sets");
  if(!value)
    return 0;

  // s->not_aligned = 1;
  
  while(*value)
    {
    char base64packet[1024];
    uint8_t decoded_packet[1024];
    uint32_t packet_size;
    char *dst = base64packet;
    int len = 0;
    
    while (*value && *value != ','
           && (dst - base64packet) < sizeof(base64packet) - 1)
      {
      *dst++ = *value++;
      len++;
      }
    *dst++ = '\0';
    
    if (*value == ',')
      value++;

    packet_size=bgav_base64decode(base64packet,
                                  len,
                                  decoded_packet, sizeof(decoded_packet));
    if(packet_size)
      {
      s->ci->global_header = realloc(s->ci->global_header,
                            s->ci->global_header_len + sizeof(start_sequence) + packet_size);
      memcpy(s->ci->global_header + s->ci->global_header_len, start_sequence, sizeof(start_sequence));
      s->ci->global_header_len += sizeof(start_sequence);
      memcpy(s->ci->global_header + s->ci->global_header_len, decoded_packet, packet_size);

      s->ci->global_header_len += packet_size;
      
      }
    }
  /* TODO: Get packetization-mode */
  s->data.video.format->framerate_mode = GAVL_FRAMERATE_VARIABLE;
  s->data.video.format->timescale = s->timescale;
  s->flags |= STREAM_NO_DURATIONS;
  sp->process = process_h264;
  return 1;
  }

/* MPEG Audio */

static int
process_mpa(bgav_stream_t * s, rtp_header_t * h, uint8_t * data, int len)
  {
  bgav_packet_t * p;
  
  p = bgav_stream_get_packet_write(s);
  bgav_packet_alloc(p, len - 4);

  PACKET_SET_KEYFRAME(p);
  p->pts      = h->timestamp;
  memcpy(p->data, data + 4, len - 4);
  p->data_size = len - 4;
  bgav_stream_done_packet_write(s, p);
  return 1;
  }

static int init_mpa(bgav_stream_t * s)
  {
  rtp_stream_priv_t * sp;
  sp = s->priv;
  sp->process = process_mpa;
  return 1;
  }

/* MPEG Video (rfc 2250) */

static int
process_mpv(bgav_stream_t * s, rtp_header_t * h, uint8_t * data, int len)
  {
  bgav_packet_t * p;
  uint32_t i;
  
  p = bgav_stream_get_packet_write(s);

  i = GAVL_PTR_2_32BE(data);
  if(i & (1 << 26)) /* MPEG-2 */
    {
    data += 8;
    len -= 8;
    }
  else
    {
    data += 4;
    len -= 4;
    }
  
  bgav_packet_alloc(p, len);
  
  p->pts      = h->timestamp;
  memcpy(p->data, data, len);
  p->data_size = len;
  bgav_stream_done_packet_write(s, p);
  return 1;
  }

static int init_mpv(bgav_stream_t * s)
  {
  rtp_stream_priv_t * sp;
  sp = s->priv;
  sp->process = process_mpv;
  s->flags |= STREAM_PARSE_FULL;
  return 1;
  }

/* MP4V-ES */

static int process_mp4v_es(bgav_stream_t * s,
                           rtp_header_t * h, uint8_t * data, int len)
  {
#if 1
  if(!h->marker) /* No frame ends here */
    {
    if(!s->packet)
      {
      s->packet = bgav_stream_get_packet_write(s);
      s->packet->data_size = 0;
      s->packet->pts = h->timestamp;
      }
    bgav_packet_alloc(s->packet, s->packet->data_size + len);
    memcpy(s->packet->data + s->packet->data_size, data, len);
    s->packet->data_size += len;
    }
  else
    {
    if(s->packet)
      {
      bgav_packet_alloc(s->packet, s->packet->data_size + len);
      memcpy(s->packet->data + s->packet->data_size, data, len);
      s->packet->data_size += len;
      //      gavl_hexdump(s->packet->data, 16, 16);
      bgav_stream_done_packet_write(s, s->packet);
      s->packet = NULL;
      }
    else
      {
      bgav_packet_t * p;
      p = bgav_stream_get_packet_write(s);
      bgav_packet_alloc(p, len);
      memcpy(p->data, data, len);
      //      gavl_hexdump(p->data, 16, 16);
      p->data_size = len;
      bgav_stream_done_packet_write(s, p);
      }
    }
#else
  bgav_packet_t * p;
  p = bgav_stream_get_packet_write(s);
  bgav_packet_alloc(p, len);
  memcpy(p->data, data, len);
  //  gavl_hexdump(p->data, 16, 16);
  p->data_size = len;
  p->pts = h->timestamp;
  bgav_stream_done_packet_write(s, p);
#endif
  
  return 1;
  }

static int init_mp4v_es(bgav_stream_t * s)
  {
  char * value;
  rtp_stream_priv_t * sp;
  int i;

  sp = s->priv;
  
  value = find_fmtp(sp->fmtp, "config");
  if(!value)
    return 0;

  s->ci->global_header_len = strlen(value)/2;
  s->ci->global_header = malloc(s->ci->global_header_len);
  //  s->not_aligned = 1;
  i = 0;
  while(i < s->ci->global_header_len)
    {
    s->ci->global_header[i] = hex_to_byte(value);
    value += 2;
    i++;
    }
  sp->process = process_mp4v_es;
  return 1;
  }

/* H263-1998 */

static int process_h263_1998(bgav_stream_t * s,
                             rtp_header_t * h, uint8_t * data, int len)
  {
  int p_bit;
  int skip = 2;


  p_bit = !!(data[0] & 0x4);
  
  if(!s->packet)
    {
    // incomplete packet without initial fragment
    if(!p_bit)
      return 1;
    s->packet = bgav_stream_get_packet_write(s);
    s->packet->data_size = 0;
    s->packet->pts = h->timestamp;
    }

  /* Get length */
  if(data[0]&0x2) // v bit - skip one more
    ++skip;

  skip += (data[1]>>3)|((data[0]&0x1)<<5); // plen - skip that many bytes

  data += skip;
  len -= skip;

  if(p_bit)
    {
    bgav_packet_alloc(s->packet, s->packet->data_size + len + 2);
    /* Make startcodes complete */
    s->packet->data[s->packet->data_size]   = 0;
    s->packet->data[s->packet->data_size+1] = 0;
    s->packet->data_size += 2;
    }
  else
    bgav_packet_alloc(s->packet, s->packet->data_size + len);
  
  memcpy(s->packet->data + s->packet->data_size, data, len);
  //  gavl_hexdump(p->data, 16, 16);
  s->packet->data_size += len;
  
  if(h->marker)
    {
    bgav_stream_done_packet_write(s, s->packet);
    s->packet = NULL;
    }
  return 1;
  }

static int init_h263_1998(bgav_stream_t * s)
  {
  rtp_stream_priv_t * sp;
  sp = s->priv;

  sp->process = process_h263_1998;
  return 1;
  }

/*
 *  Theora (draft-barbato-avt-rtp-theora-01)
 *  and Vorbis (RFC 5215) are practically identical
 */

#ifdef HAVE_OGG

static void start_packet_ogg(bgav_stream_t * s, int64_t pts)
  {
  s->packet = bgav_stream_get_packet_write(s);
  
  bgav_packet_alloc(s->packet, sizeof(ogg_packet));
  memset(s->packet->data, 0, sizeof(ogg_packet));
  s->packet->data_size = sizeof(ogg_packet);
  s->packet->pts = pts;
  }

static void append_packet_ogg(bgav_stream_t * s, uint8_t * data, int len)
  {
  ogg_packet * op;
  bgav_packet_alloc(s->packet, s->packet->data_size + len);
  op = (ogg_packet *)s->packet->data;
  op->packet = s->packet->data + sizeof(*op);
  op->bytes += len;
  memcpy(s->packet->data + s->packet->data_size, data, len);
  s->packet->data_size += len;
  }

static void end_packet_ogg(bgav_stream_t * s)
  {
  //  fprintf(stderr, "Done packet %d bytes\n", s->packet->data_size);
  bgav_stream_done_packet_write(s, s->packet);
  s->packet = NULL;
  }

static int process_ogg(bgav_stream_t * s,
                       rtp_header_t * h, uint8_t * data, int len)
  {
  uint32_t header;
  int size, i;
  int fragment_type, data_type, num_packets, ident;
  rtp_stream_priv_t * sp = s->priv;
  
  header = GAVL_PTR_2_32BE(data);
  data += 4;
  len -= 4;

  ident = header >> 8;
  fragment_type = (header & 0xff) >> 6;
  data_type = (header & 0x30) >> 4;
  num_packets = (header & 0x0f);

  if(sp->priv.xiph.ident != ident)
    return 0;
  
  if(data_type != 0)
    return 1;

  switch(fragment_type)
    {
    case 0: // Not fragmented
      for(i = 0; i < num_packets; i++)
        {
        size = GAVL_PTR_2_16BE(data); data += 2;
        start_packet_ogg(s, !i ? h->timestamp : GAVL_TIME_UNDEFINED);
        append_packet_ogg(s, data, size);
        end_packet_ogg(s);
        data += size;
        }
      break;
    case 1: // Start fragment
      size = GAVL_PTR_2_16BE(data); data += 2;
      start_packet_ogg(s, h->timestamp);
      append_packet_ogg(s, data, size);
      break;
    case 2: // Continuation fragment
      size = GAVL_PTR_2_16BE(data); data += 2;
      append_packet_ogg(s, data, size);
      break;
    case 3: // End fragment
      size = GAVL_PTR_2_16BE(data); data += 2;
      append_packet_ogg(s, data, size);
      end_packet_ogg(s);
      break;
    }
  
#if 0
  fprintf(stderr, "Ident: %d, fragment_type: %d, data_type: %d, packets: %d\n",
          header >> 8,
          (header & 0xff) >> 6,
          (header & 0x30) >> 4,
          (header & 0x0f));
#endif
  return 1;
  }

static int get_v_ogg(uint8_t * data1, int * ret)
  {
  uint8_t * data = data1;
  *ret = 0;

  while(*data & 0x80)
    {
    *ret <<= 7;
    *ret |= (*data & 0x7f);
    data++;
    }

  *ret <<= 7;
  *ret |= (*data & 0x7f);
  data++;
  return data - data1;
  }

static int extract_extradata_ogg(bgav_stream_t * s, uint8_t * data, int siz)
  {
  int i;
  int count_total, count;
  int sizes[3];
  uint8_t * pos;
  ogg_packet op;
  rtp_stream_priv_t * sp;
  uint8_t * end = data + siz;

  sp = s->priv;
  
  count_total = GAVL_PTR_2_32BE(data); data += 4;

  //  fprintf(stderr, "Extract extradata: %d\n", siz);
    
  /* TODO: handle count > 1 */
  if(count_total != 1)
    {
    //    fprintf(stderr, "Only exactly one configuration supported\n");
    return 0;
    }

  sp->priv.xiph.ident  = GAVL_PTR_2_24BE(data); data += 3;
  data += 2; // len = GAVL_PTR_2_16BE(data); data += 2;

  //  fprintf(stderr, "ID: %d, len: %d\n",
  //          sp->priv.xiph.ident, len);

  data += get_v_ogg(data, &count);
  //  fprintf(stderr, "count: %d\n", count);

  if(count != 2)
    {
    //    fprintf(stderr, "need 3 header packets\n");
    return 0;
    }
  
  for(i = 0; i < count; i++)
    {
    data += get_v_ogg(data, &sizes[i]);
    //    fprintf(stderr, "val: %d\n", sizes[i]);
    }
  sizes[2] = (end - data) - (sizes[0] + sizes[1]);
  
  /* Assemble 3 header packets */
  
  s->ci->global_header_len = sizes[0]+sizes[1]+sizes[2] + 3*sizeof(op);
  s->ci->global_header = malloc(s->ci->global_header_len);
  pos = s->ci->global_header;
  
  for(i = 0; i < 3; i++)
    {
    //    gavl_hexdump(data, 16, 16);
    memset(&op, 0, sizeof(op));
    if(!i)
      op.b_o_s = 1;
    op.bytes = sizes[i];
    op.packet = pos + sizeof(op);
    memcpy(pos, &op, sizeof(op));
    pos+= sizeof(op);
    memcpy(pos, data, sizes[i]);
    data += sizes[i];
    pos += sizes[i];
    }
  
  return 1;
  }

static int init_ogg(bgav_stream_t * s)
  {
  rtp_stream_priv_t * sp;
  char * value;
  int config_len_base64;
  int config_len_raw;
  uint8_t * config_buffer;
  
  sp = s->priv;

  value = find_fmtp(sp->fmtp, "configuration");
  if(!value)
    return 0;

  config_len_base64 = strlen(value);
  
  config_buffer = malloc(config_len_base64);
  config_len_raw = bgav_base64decode(value,
                                     config_len_base64,
                                     config_buffer,
                                     config_len_base64);

  //  gavl_hexdump(config_buffer, config_len_raw, 16);
  
  if(!extract_extradata_ogg(s, config_buffer, config_len_raw))
    {
    free(config_buffer);
    return 0;
    }
  free(config_buffer);

  if(s->type == GAVL_STREAM_VIDEO)
    {
    s->data.video.format->timescale = s->timescale;
    s->data.video.format->frame_duration = 0;
    s->data.video.format->framerate_mode = GAVL_FRAMERATE_VARIABLE;
    }
  
  sp->process = process_ogg;
  return 1;
  }

#endif

