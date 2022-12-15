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
 *  MMS Protocol support rewritten from scratch
 *
 *  References: http://sdp.ppona.com/
 *              http://www.geocities.com/majormms/
 */

#include <avdec_private.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/select.h>
#endif

#include <mms.h>

#define LOG_DOMAIN "mms"

#define   PRINT_STRING(label, str) \
if(str)\
bgav_dprintf("%s%s\n", label, str);\
else \
bgav_dprintf("%sNULL\n", label);

#if 0
static void dump_data(uint8_t * data, int len, const char * filename)
  {
  FILE * out;

  out = fopen(filename, "wb");
  fwrite(data,  1, len, out);
  fclose(out);
  }
#endif

static int read_data(const bgav_options_t * opt, int fd, uint8_t * buffer, int len)
  {
  int result;
  fd_set rset;
  struct timeval timeout;
  int bytes_read = 0;

  while(bytes_read < len)
    {
    result = read(fd, buffer + bytes_read, len - bytes_read);
    if(result < 0)
      {
      if(errno == EAGAIN)
        {
        FD_ZERO (&rset);
        FD_SET  (fd, &rset);
        
        timeout.tv_sec  = opt->read_timeout / 1000;
        timeout.tv_usec = (opt->read_timeout % 1000) * 1000;
        
        if(select (fd+1, &rset, NULL, NULL, &timeout) <= 0)
          return bytes_read;
        }
      }
    else if(!result)
      return bytes_read;
    else
      bytes_read += result;
    }
  return bytes_read;
  }


#define BUFFER_SIZE 10240

struct bgav_mms_s
  {
  int fd;
  int seqnum;
  int got_data;
  /* Some infos */

  char * server_version;
  char * tool_version;
  char * update_url;
  char * password_encryption_type;

  int header_id;
  int data_id;
    
  /* Reading */
  struct
    {
    int command;
    int data_len;
    int padded_data_len;
    uint32_t seqnum;
    uint32_t prefix1;
    uint32_t prefix2;
    } command_header;

  uint8_t read_buffer[BUFFER_SIZE];
  int read_buffer_len;
  uint8_t * cmd_data_read;

  /* These are pointers to the read buffer */
    
  uint8_t * command;
  //  uint8_t * data;
  int data_size;

  uint8_t * packet_buffer;
  int packet_len; /* Packet len is fixed */
    
  /* The header is allocated */
  
  uint8_t * header;
  int header_size;
  int header_alloc;
  
  /* Writing */
    
  uint8_t write_buffer[BUFFER_SIZE];
  int write_buffer_len;
  uint8_t * cmd_data_write;
  
  const bgav_options_t * opt;
  };

#if 0
static void mms_dump(bgav_mms_t * mms)
  {
  PRINT_STRING("Server Version:           ", mms->server_version);
  PRINT_STRING("Tool Version:             ", mms->tool_version);
  PRINT_STRING("Update URL:               ", mms->update_url);
  PRINT_STRING("Password Encryption Type: ", mms->password_encryption_type);
  }
#endif
static void set_command_header(bgav_mms_t * mms, int command,
                               uint32_t switches,
                               uint32_t extra, int length)
  {
  uint8_t * ptr;
  int len8;
  
  len8 = (length + 7) / 8;
  ptr = mms->write_buffer;
  mms->write_buffer_len = len8*8 + 48;

  
  GAVL_32LE_2_PTR(0x00000001, ptr);ptr+=4; /* Start sequence */
  GAVL_32LE_2_PTR(0xB00BFACE, ptr);ptr+=4; /* :-) */
  GAVL_32LE_2_PTR(len8*8+32, ptr);ptr+=4;
  GAVL_32LE_2_PTR(0x20534d4d, ptr);ptr+=4; /* MMS */
  GAVL_32LE_2_PTR(len8 + 4, ptr);ptr+=4;
  GAVL_32LE_2_PTR(mms->seqnum, ptr);ptr+=4;
  GAVL_32LE_2_PTR(0x00000000, ptr);ptr+=4; /* Timestamp */
  GAVL_32LE_2_PTR(0x00000000, ptr);ptr+=4; /* Timestamp */
  
  GAVL_32LE_2_PTR(len8 + 2, ptr);ptr+=4;
  GAVL_32LE_2_PTR(0x00030000 | command, ptr);ptr+=4; /* Direction | command */
  GAVL_32LE_2_PTR(switches, ptr);ptr+=4;
  GAVL_32LE_2_PTR(extra, ptr);ptr+=4;

  mms->cmd_data_write = ptr;
  }

static int flush_command(bgav_mms_t * mms)
  {
  return bgav_tcp_send(mms->opt, mms->fd, mms->write_buffer, mms->write_buffer_len);
  }

static int read_command_header(bgav_mms_t * mms)
  {
  uint32_t  i_tmp;
  uint8_t * ptr;

  /* Read the data length */

  if(read_data(mms->opt, mms->fd, mms->read_buffer+8, 4) < 4)
    return 0;
  
  ptr = mms->command + 8;
  
  i_tmp = GAVL_PTR_2_32LE(ptr);ptr+=4;

  mms->command_header.data_len = i_tmp + 16 - 48;

  /* Read remaining command */

  if(read_data(mms->opt, mms->fd, mms->read_buffer+12,
               i_tmp + 4) < i_tmp + 4)
    return 0;
  
  i_tmp = BGAV_PTR_2_FOURCC(ptr);ptr+=4;
  
  i_tmp = GAVL_PTR_2_32LE(ptr);ptr+=4;

  
  i_tmp = GAVL_PTR_2_32LE(ptr);ptr+=4;

  mms->command_header.seqnum = i_tmp;
  
  ptr+=8; /* Timestamp */

  i_tmp = GAVL_PTR_2_32LE(ptr);ptr+=4;

  i_tmp = GAVL_PTR_2_16LE(ptr);ptr+=2;
  
  mms->command_header.command = i_tmp;
  
  i_tmp = GAVL_PTR_2_16LE(ptr);ptr+=2;

  i_tmp = GAVL_PTR_2_32LE(ptr);ptr+=4;
  mms->command_header.prefix1 = i_tmp;

  i_tmp = GAVL_PTR_2_32LE(ptr);ptr+=4;
  mms->command_header.prefix2 = i_tmp;

  mms->cmd_data_read = mms->read_buffer + 48;

  if(mms->command_header.seqnum != mms->seqnum)
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
             "Sequence number mismatch, expected %d, got %d",
            mms->seqnum, mms->command_header.seqnum);
    mms->seqnum = mms->command_header.seqnum+1;
    }
  else
    mms->seqnum++;
  return 1;
  }

#if 0
static void dump_command_header(bgav_mms_t * mms)
  {
  bgav_dprintf("Got command header:\n");
  bgav_dprintf("  Command:         0x%02x\n", mms->command_header.command);
  bgav_dprintf("  Data len:        %d\n", mms->command_header.data_len);
  bgav_dprintf("  Padded data len: %d\n", mms->command_header.padded_data_len);
  bgav_dprintf("  Sequence number: %d\n", mms->command_header.seqnum);
  bgav_dprintf("  Prefix1:         %d\n", mms->command_header.prefix1);
  bgav_dprintf("  Prefix2:         %d\n", mms->command_header.prefix2);
  gavl_hexdump(mms->cmd_data_read, mms->command_header.data_len, 16);
  }
#endif
static int next_packet(bgav_mms_t * mms, int block)
  {
  uint8_t * ptr;
  uint32_t i_tmp1;
  uint32_t i_tmp2;
  fd_set rset;
  struct timeval timeout;


  mms->command = NULL;
  //  mms->header  = NULL;

  if(!block)
    {
    FD_ZERO (&rset);
    FD_SET  (mms->fd, &rset);
    
    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;
    
    if(select (mms->fd+1, &rset, NULL, NULL, &timeout) <= 0)
      return 0;
    }
  
  while(1)
    {
    mms->read_buffer_len = read_data(mms->opt, mms->fd, mms->read_buffer,
                                     8);
    if(mms->read_buffer_len < 8)
      {
      return 0;
      }
    /* Data read, now see, what we got */
    
    i_tmp1 = GAVL_PTR_2_32LE(mms->read_buffer);
    i_tmp2 = GAVL_PTR_2_32LE(mms->read_buffer+4);
    
    if(((i_tmp1 & (0x00FFFFFF)) == 0x00000001) &&
       (i_tmp2 == 0xB00BFACE))
      {
      mms->command = mms->read_buffer;
      
      /* Read the remaining stuff */
      
      if(!read_command_header(mms))
        return 0;
      
      /* Check for commands sent during playback */
      
      if(mms->command_header.command == 0x1b)
        {
        set_command_header(mms, 0x1b, 0x00000001, 0x0001ffff, 0);
        if(!flush_command(mms))
          return 0;
        continue;
        }
      
      //      dump_command_header(mms);
      
      }
    else
      {
      if(mms->read_buffer[4] == mms->header_id)
        {
        ptr = mms->read_buffer;
        i_tmp1 = GAVL_PTR_2_32LE(ptr);ptr+=4;
        ptr++;
        ptr++;
        i_tmp1 = GAVL_PTR_2_16LE(ptr);ptr+=2;
      
        if(mms->header_size < mms->header_alloc)
          {
          if(read_data(mms->opt, mms->fd, mms->header + mms->header_size, i_tmp1-8) < i_tmp1-8)
            return 0;
          mms->header_size += (i_tmp1-8);
          }
        }
      else if(mms->read_buffer[4] == mms->data_id)
        {
        //      gavl_hexdump(mms->read_buffer, 8, 8);
        ptr = mms->read_buffer;
      
        i_tmp1 = GAVL_PTR_2_32LE(ptr);ptr+=4;
        ptr++;
        ptr++;
        i_tmp1 = GAVL_PTR_2_16LE(ptr);ptr+=2;

        if(read_data(mms->opt, mms->fd, mms->packet_buffer,
                     i_tmp1-8) < i_tmp1-8)
          return 0;

      
        mms->got_data = 1;
        }
      else
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "Unknown data: %02x %02x %02x %02x %02x %02x %02x %02x",
                 mms->read_buffer[0], mms->read_buffer[0],
                 mms->read_buffer[0], mms->read_buffer[0],
                 mms->read_buffer[0], mms->read_buffer[0],
                 mms->read_buffer[0], mms->read_buffer[0]);
        gavl_hexdump(mms->read_buffer, 8, 8);
        }
      }
    break;
    }
  return 1;
  }

static void mms_gen_guid(char guid[])
  {
  static char digit[16] = "0123456789ABCDEF";
  int i = 0;
  
  srand(time(NULL));
  for (i = 0; i < 36; i++)
    {
    guid[i] = digit[(int) ((16.0*rand())/(RAND_MAX+1.0))];
    }
  guid[8] = '-'; guid[13] = '-'; guid[18] = '-'; guid[23] = '-';
  guid[36] = '\0';
  }

#define NUM_ZEROS 8

bgav_mms_t * bgav_mms_open(const bgav_options_t * opt,
                           const char * url)
  {
  char * host     = NULL;
  char * protocol = NULL;
  char * path     = NULL;
  int port = -1;
  char guid[37];
  char * buf;
  char * utf16;
  uint32_t len_out;
  uint8_t * pos;
  uint32_t i_tmp;
  int server_version_len;
  int tool_version_len;
  int update_url_len;
  int password_encryption_type_len;

  bgav_charset_converter_t * ascii_2_utf16;
  bgav_charset_converter_t * utf16_2_utf8;
  bgav_mms_t * ret = NULL;

  ascii_2_utf16 = bgav_charset_converter_create("ASCII", "UTF-16LE");
  utf16_2_utf8  = bgav_charset_converter_create("UTF-16LE", BGAV_UTF8);
  
  
  if(!bgav_url_split(url,
                     &protocol,
                     NULL, /* User */
                     NULL, /* Pass */
                     &host,
                     &port,
                     &path))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Invalid URL: %s", url);
    goto fail;
    }
  ret = calloc(1, sizeof(*ret));

  /* Store options */

  ret->opt = opt;
    
  /* Connect */
  
  if(port < 0)
    port = 1755;

  ret->fd = bgav_tcp_connect(opt, host, port);
  
  if(ret->fd < 0)
    goto fail;

  /* Set nonblocking mode */

#ifdef _WIN32
  unsigned long flags = 1;
  if (ioctlsocket(ret->fd,FIONBIO, &flags) == SOCKET_ERROR)
#else
  if(fcntl(ret->fd, F_SETFL, O_NONBLOCK) < 0)
#endif   
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Cannot set nonblocking mode");
    goto fail;
    }
  
  /* Begin negotiations */

  mms_gen_guid(guid);

  /* C->S: 0x01 Send player, version, guid and hostname */

  //  buf = bgav_sprintf("\x1c\x03NSPlayer/7.0.0.1956; {%s}; Host: %s",
  //                     guid, host);
  buf = bgav_sprintf("\034\003NSPlayer/7.0.0.1956; {33715801-BAB3-9D85-24E9-03B90328270A}; Host: %s",
                     host);
  
  utf16 = bgav_convert_string(ascii_2_utf16, buf, -1, &len_out);
  
  
  set_command_header(ret, 0x01, 0, 0x0004000b, strlen(buf)*2+2);

  memcpy(ret->cmd_data_write, utf16, len_out);
  memset(ret->cmd_data_write + len_out, 0, 2);
  if(!flush_command(ret))
    {
    goto fail;
    }
  free(buf);
  free(utf16);
      
  if(!next_packet(ret, 1))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Cannot get software version number and stuff");
    goto fail;
    }
  /* S->C: 0x01 Software version number and stuff */
  
  if((!ret->command) ||
     (ret->command_header.prefix1) ||
     (ret->command_header.command != 0x01))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Invalid answer 1 %08x %08x",
             ret->command_header.prefix1,
             ret->command_header.command);
    goto fail;
    }
  pos = ret->cmd_data_read + 32;

  server_version_len           = GAVL_PTR_2_32LE(pos);pos+=4;
  tool_version_len             = GAVL_PTR_2_32LE(pos);pos+=4;
  update_url_len               = GAVL_PTR_2_32LE(pos);pos+=4;
  password_encryption_type_len = GAVL_PTR_2_32LE(pos);pos+=4;

  if(server_version_len)
    {
    ret->server_version = bgav_convert_string(utf16_2_utf8,
                                              (char*)pos, server_version_len*2,
                                              NULL);
    pos += server_version_len*2;
    }

  if(tool_version_len)
    {
    ret->tool_version = bgav_convert_string(utf16_2_utf8,
                                            (char*)pos, tool_version_len*2,
                                            NULL);
    pos += tool_version_len*2;
    }
  
  if(update_url_len)
    {
    ret->update_url = bgav_convert_string(utf16_2_utf8,
                                          (char*)pos, update_url_len*2,
                                          NULL);
    pos += update_url_len*2;
    }

  if(password_encryption_type_len)
    {
    ret->password_encryption_type =
      bgav_convert_string(utf16_2_utf8, (char*)pos,
                          password_encryption_type_len*2,
                          NULL);
    pos += password_encryption_type_len*2;
    }
  
  /* C->S 0x02: Send Protocol and stuff */
  
  utf16 = bgav_convert_string(ascii_2_utf16,
                              "\002\000\\\\192.168.0.129\\TCP\\1037\0000",
                              28, &len_out);
  
  set_command_header(ret, 0x02, 0, 0, len_out + 8 + 1);
  memset(ret->cmd_data_write, 0, 8);
  memcpy(ret->cmd_data_write + 8, utf16, len_out + 1);
  if(!flush_command(ret))
    {
    goto fail;
    }
  free(utf16);

  /* S->C: 0x03: Protocol not accepted OR 0x02: Protocol accepted */
  
  if(!next_packet(ret, 1))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Next packet failed");
    goto fail;
    }
  if((!ret->command) ||
     (ret->command_header.prefix1))
    goto fail;

  if(ret->command_header.command == 0x03)
    {
    /* Protocol not supported, dammit */
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Protocol not supported");
    goto fail;
    }
  else if(ret->command_header.command != 0x02)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Got answer: %d", ret->command_header.command);
    goto fail;
    }
  
  /* C->S: Request file */

  utf16 = bgav_convert_string(ascii_2_utf16, path, strlen(path),
                              &len_out);
  
  set_command_header(ret, 0x05, 1, 0xFFFFFFFF, len_out + 10);
  memset(ret->cmd_data_write, 0, 8);
  memcpy(ret->cmd_data_write + 8, utf16, len_out);
  memset(ret->cmd_data_write + 8 + len_out, 0, 2);
  if(!flush_command(ret))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Remote end closed connection");
    goto fail;
    }
  free(utf16);
  
  if(!next_packet(ret, 1))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Next packet failed 2");
    goto fail;
    }
  if((!ret->command) || (ret->command_header.prefix1))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Invalid answer 2 %08x", ret->command_header.prefix1);
    goto fail;
    }
  if(ret->command_header.command == 0x1a)
    {
    /* Passwords not supported, dammit */
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Passwords not supported");
    goto fail;
    }
  else if(ret->command_header.command != 0x06)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
             "Invalid answer 3: %d", ret->command_header.command);
    goto fail;
    }
  
  pos = ret->cmd_data_read;
  i_tmp = GAVL_PTR_2_32LE(pos);pos+=4;

  if(i_tmp & 0x80000000)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Request not accepted %08x", i_tmp);
    goto fail;
    }
  pos+=4; /* 00000000 */
  pos+=4; /* 00000000 */
  i_tmp = GAVL_PTR_2_32LE(pos);pos+=4;

  pos+=8; /* Time point */

  i_tmp = GAVL_PTR_2_32LE(pos);pos+=4;

  pos += 16; /* Zeros */

  i_tmp = GAVL_PTR_2_32LE(pos);pos+=4;

  ret->packet_buffer = malloc(i_tmp);
  ret->packet_len = i_tmp;
  
  i_tmp = GAVL_PTR_2_32LE(pos);pos+=4;
  
  pos+=4; /* Zeros */
  
  i_tmp = GAVL_PTR_2_32LE(pos);pos+=4;
  
  i_tmp = GAVL_PTR_2_32LE(pos);pos+=4;
  ret->header_alloc = i_tmp;
  ret->header = malloc(ret->header_alloc);
  
  /* C->S: 0x15: Request media header */
  
  set_command_header(ret, 0x15, 1, 0, 40);
  memset (ret->cmd_data_write, 0, 40);
  ret->cmd_data_write[32] = 2;
  if(!flush_command(ret))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Remote end closed connection");
    goto fail;
    }
  
  /* S->C: 0x11: Header comes */

  if(!next_packet(ret, 1))
    return 0;
  if((!ret->command) ||
     (ret->command_header.prefix1) ||
     (ret->command_header.command != 0x11))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Invalid answer 4");
    goto fail;
    }
  ret->header_id = ret->command_header.prefix2;
  
  /* Now, the header should come */

  while(ret->header_size < ret->header_alloc)
    {
    if(!next_packet(ret, 1))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Next packet failed");
      goto fail;
      }
    }
  //  dump_data(ret->read_buffer+8, ret->read_buffer_len-8, "header.dat");

  if(!ret->header)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Read header failed");
    goto fail;
    }
  //  mms_dump(ret);

  bgav_charset_converter_destroy(ascii_2_utf16);
  bgav_charset_converter_destroy(utf16_2_utf8);
  
  if(host)
    free(host);
  if(protocol)
    free(protocol);
  if(path)
    free(path);
  
  return ret;
  
  fail:
  bgav_charset_converter_destroy(ascii_2_utf16);
  bgav_charset_converter_destroy(utf16_2_utf8);
  if(host)
    free(host);
  if(protocol)
    free(protocol);
  if(path)
    free(path);
  if(ret)
    free(ret);
  return NULL;
  }

#define FREE(ptr) if(ptr)free(ptr);

void bgav_mms_close(bgav_mms_t * mms)
  {
  FREE(mms->server_version);
  FREE(mms->tool_version);
  FREE(mms->update_url);
  FREE(mms->password_encryption_type);
  FREE(mms->packet_buffer);
  FREE(mms->header);
  if(mms->fd >= 0)
    closesocket(mms->fd);
  FREE(mms);
  }

uint8_t * bgav_mms_get_header(bgav_mms_t * mms, int * len)
  {
  if(len)
    *len = mms->header_size;
  return mms->header;
  }

int bgav_mms_select_streams(bgav_mms_t * mms,
                            int * stream_ids, int num_streams)
  {
  uint8_t * ptr;
  int i;
  set_command_header(mms, 0x33, num_streams, 0, 6 * num_streams - 4);
  ptr = mms->cmd_data_write - 4;

  for(i = 0; i < num_streams; i++)
    {
    GAVL_16LE_2_PTR(0xffff, ptr);ptr+=2;        /* Flags        */
    GAVL_16LE_2_PTR(stream_ids[i], ptr);ptr+=2; /* Stream_id    */
    GAVL_16LE_2_PTR(0x0000, ptr);ptr+=2;        /* Switch it on */
    }
  if(!flush_command(mms))
    return 0;

  if(!next_packet(mms, 1))
    return 0;

  if((!mms->command) ||
     (mms->command_header.prefix1) ||
     (mms->command_header.command != 0x21))
    return 0;

  /* Request Media data */

  set_command_header(mms, 0x07, 1, 0xFFFF | stream_ids[0], 40);
  
  memset(mms->cmd_data_write, 0, 40);

  for (i=8; i<16; i++)
    mms->cmd_data_write[i] = 0xFF;

  /* The following 2 must be equal */
  mms->cmd_data_write[20] = 0x04;
  mms->data_id = 0x04;
  if(!flush_command(mms))
    return 0;
  
  /* Now we need 0x05 (media packets follow) */

  if(!next_packet(mms, 1))
    return 0;

  if((!mms->command) ||
     (mms->command_header.prefix1) ||
     (mms->command_header.command != 0x05))
    return 0;
  
  return 1;
  }

uint8_t * bgav_mms_read_data(bgav_mms_t * mms, int * len, int block)
  {
  mms->got_data = 0;
  if(!next_packet(mms, block))
    return NULL;

  if(mms->packet_buffer && mms->got_data)
    {
    *len = mms->packet_len;
    return mms->packet_buffer;
    }
  return NULL;
  }
