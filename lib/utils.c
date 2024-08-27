/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
 *
 * Copyright (c) 2001 - 2024 Members of the Gmerlin project
 * http://github.com/bplaum
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



#define _GNU_SOURCE /* vasprintf */

#include <avdec_private.h>

#define LOG_DOMAIN "utils"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <regex.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <pthread.h>
#else
#include <sys/socket.h>
#include <sys/select.h>
#endif

#include <utils.h>

void bgav_dump_fourcc(uint32_t fourcc)
  {
  if((fourcc & 0xffff0000) || !(fourcc))
    gavl_dprintf( "%c%c%c%c (%08x)",
            (fourcc & 0xFF000000) >> 24,
            (fourcc & 0x00FF0000) >> 16,
            (fourcc & 0x0000FF00) >> 8,
            (fourcc & 0x000000FF),
            fourcc);
  else
    gavl_dprintf( "WavID: 0x%04x", fourcc);
    
  }

int bgav_check_fourcc(uint32_t fourcc, const uint32_t * fourccs)
  {
  int i = 0;
  while(fourccs[i])
    {
    if(fourccs[i] == fourcc)
      return 1;
    else
      i++;
    }
  return 0;
  }


/*
 *  Read a single line from a filedescriptor
 *
 *  ret will be reallocated if neccesary and ret_alloc will
 *  be updated then
 *
 *  The string will be 0 terminated, a trailing \r or \n will
 *  be removed
 */
#define BYTES_TO_ALLOC 1024

int bgav_read_line_fd(const bgav_options_t * opt,
                      int fd, char ** ret, int * ret_alloc, int milliseconds)
  {
  char * pos;
  char c;
  int bytes_read;
  bytes_read = 0;
  /* Allocate Memory for the case we have none */
  if(!(*ret_alloc))
    {
    *ret_alloc = BYTES_TO_ALLOC;
    *ret = realloc(*ret, *ret_alloc);
    **ret = '\0';
    }
  pos = *ret;
  while(1)
    {
    if(!bgav_read_data_fd(opt, fd, (uint8_t*)(&c), 1, milliseconds))
      {
      if(!bytes_read)
        return 0;
      break;
      }
    /*
     *  Line break sequence
     *  is starting, remove the rest from the stream
     */
    if(c == '\n')
      {
      break;
      }
    /* Reallocate buffer */
    else if(c != '\r')
      {
      if(bytes_read+2 >= *ret_alloc)
        {
        *ret_alloc += BYTES_TO_ALLOC;
        *ret = realloc(*ret, *ret_alloc);
        pos = &((*ret)[bytes_read]);
        }
      /* Put the byte and advance pointer */
      *pos = c;
      pos++;
      bytes_read++;
      }
    }
  *pos = '\0';
  return 1;
  }

int bgav_read_data_fd(const bgav_options_t * opt,
                      int fd, uint8_t * ret, int len, int milliseconds)
  {
  int bytes_read = 0;
  int result;
  fd_set rset;
  struct timeval timeout;

  int flags = 0;


  //  if(milliseconds < 0)
  //    flags = MSG_WAITALL;
  
  while(bytes_read < len)
    {
    if(milliseconds >= 0)
      { 
      FD_ZERO (&rset);
      FD_SET  (fd, &rset);
     
      timeout.tv_sec  = milliseconds / 1000;
      timeout.tv_usec = (milliseconds % 1000) * 1000;
    
      if((result = select (fd+1, &rset, NULL, NULL, &timeout)) <= 0)
        {
//        fprintf(stderr, "Read timed out %d\n", milliseconds);
        return bytes_read;
        }
      }

    result = recv(fd, ret + bytes_read, len - bytes_read, flags);
    if(result > 0)
      bytes_read += result;
    else if(result <= 0)
      {
      if(result <0)
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
                 "recv failed: %s", strerror(errno));
      return bytes_read;
      }
    }
  return bytes_read;
  }

int bgav_slurp_file(const char * location,
                    bgav_packet_t * p,
                    const bgav_options_t * opt)
  {
  bgav_input_context_t * input;
  input = bgav_input_create(NULL, opt);
  if(!bgav_input_open(input, location))
    {
    bgav_input_destroy(input);
    return 0;
    }
  if(!input->total_bytes)
    {
    bgav_input_destroy(input);
    return 0;
    }

  gavl_packet_alloc(p, input->total_bytes);
  
  if(bgav_input_read_data(input, p->buf.buf, input->total_bytes) <
     input->total_bytes)
    {
    bgav_input_destroy(input);
    return 0;
    }
  p->buf.len = input->total_bytes;
  bgav_input_destroy(input);
  return 1;
  }

int bgav_check_file_read(const char * filename)
  {
  if(access(filename, R_OK))
    return 0;
  return 1;
  }

/* Search file for writing */

char * bgav_search_file_write(const bgav_options_t * opt,
                              const char * directory, const char * file)
  {
  char * home_dir;
  char * testpath;
  char * pos1;
  char * pos2;
  FILE * testfile;

  if(!file)
    return NULL;
  
  testpath = malloc((PATH_MAX+1) * sizeof(char));
  
  home_dir = getenv("HOME");

  /* Try to open the file */

  snprintf(testpath, PATH_MAX,
           "%s/.%s/%s/%s", home_dir, PACKAGE, directory, file);
  testfile = fopen(testpath, "a");
  if(testfile)
    {
    fclose(testfile);
    return testpath;
    }
  
  if(errno != ENOENT)
    {
    free(testpath);
    return NULL;
    }

  /*
   *  No such file or directory can mean, that the directory 
   *  doesn't exist
   */
  
  pos1 = &testpath[strlen(home_dir)+1];
  
  while(1)
    {
    pos2 = strchr(pos1, '/');

    if(!pos2)
      break;

    *pos2 = '\0';

#ifdef _WIN32
    if(mkdir(testpath) == -1)
#else
    if(mkdir(testpath, S_IRUSR|S_IWUSR|S_IXUSR) == -1)
#endif
      {
      if(errno != EEXIST)
        {
        *pos2 = '/';
        break;
        }
      }
    else
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
               "Created directory %s", testpath);
    
    *pos2 = '/';
    pos1 = pos2;
    pos1++;
    }

  /* Try once more to open the file */

  testfile = fopen(testpath, "w");

  if(testfile)
    {
    fclose(testfile);
    return testpath;
    }
  free(testpath);
  return NULL;
  }

char * bgav_search_file_read(const bgav_options_t * opt,
                             const char * directory, const char * file)
  {
  char * home_dir;
  char * test_path;

  home_dir = getenv("HOME");

  test_path = malloc((PATH_MAX+1) * sizeof(*test_path));
  snprintf(test_path, PATH_MAX, "%s/.%s/%s/%s",
           home_dir, PACKAGE, directory, file);

  if(bgav_check_file_read(test_path))
    return test_path;

  free(test_path);
  return NULL;
  }

int bgav_match_regexp(const char * str, const char * regexp)
  {
  int ret = 0;
  regex_t exp;
  memset(&exp, 0, sizeof(exp));

  /*
    `REG_EXTENDED'
     Treat the pattern as an extended regular expression, rather than
     as a basic regular expression.

     `REG_ICASE'
     Ignore case when matching letters.

     `REG_NOSUB'
     Don't bother storing the contents of the MATCHES-PTR array.

     `REG_NEWLINE'
     Treat a newline in STRING as dividing STRING into multiple lines,
     so that `$' can match before the newline and `^' can match after.
     Also, don't permit `.' to match a newline, and don't permit
     `[^...]' to match a newline.
  */
  
  regcomp(&exp, regexp, REG_NOSUB|REG_EXTENDED);
  if(!regexec(&exp, str, 0, NULL, 0))
    ret = 1;
  regfree(&exp);
  return ret;
  }


static const struct
  {
  gavl_codec_id_t id;
  uint32_t fourcc;
  }
fourccs[] =
  {
    { GAVL_CODEC_ID_NONE,      BGAV_MK_FOURCC('g','a','v','f') },
    /* Audio */
    { GAVL_CODEC_ID_ALAW,      BGAV_MK_FOURCC('a','l','a','w') }, 
    { GAVL_CODEC_ID_ULAW,      BGAV_MK_FOURCC('u','l','a','w') }, 
    { GAVL_CODEC_ID_MP2,       BGAV_MK_FOURCC('.','m','p','2') }, 
    { GAVL_CODEC_ID_MP3,       BGAV_MK_FOURCC('.','m','p','3') }, 
    { GAVL_CODEC_ID_AC3,       BGAV_MK_FOURCC('.','a','c','3') }, 
    { GAVL_CODEC_ID_AAC,       BGAV_MK_FOURCC('m','p','4','a') }, 
    { GAVL_CODEC_ID_VORBIS,    BGAV_MK_FOURCC('V','B','I','S') }, 
    { GAVL_CODEC_ID_FLAC,      BGAV_MK_FOURCC('F','L','A','C') }, 
    { GAVL_CODEC_ID_OPUS,      BGAV_MK_FOURCC('O','P','U','S') }, 
    { GAVL_CODEC_ID_SPEEX,     BGAV_MK_FOURCC('S','P','E','X') }, 
    { GAVL_CODEC_ID_DTS,       BGAV_MK_FOURCC('d','t','s',' ') }, 
    
    /* Video */
    { GAVL_CODEC_ID_JPEG,      BGAV_MK_FOURCC('j','p','e','g') }, 
    { GAVL_CODEC_ID_PNG,       BGAV_MK_FOURCC('p','n','g',' ') }, 
    { GAVL_CODEC_ID_TIFF,      BGAV_MK_FOURCC('t','i','f','f') }, 
    { GAVL_CODEC_ID_TGA,       BGAV_MK_FOURCC('t','g','a',' ') }, 
    { GAVL_CODEC_ID_MPEG1,     BGAV_MK_FOURCC('m','p','v','1') }, 
    { GAVL_CODEC_ID_MPEG2,     BGAV_MK_FOURCC('m','p','v','2') },
    { GAVL_CODEC_ID_MPEG4_ASP, BGAV_MK_FOURCC('m','p','4','v') },
    { GAVL_CODEC_ID_H264,      BGAV_MK_FOURCC('H','2','6','4') },
    { GAVL_CODEC_ID_THEORA,    BGAV_MK_FOURCC('T','H','R','A') },
    { GAVL_CODEC_ID_DIRAC,     BGAV_MK_FOURCC('d','r','a','c') },
    { GAVL_CODEC_ID_DV,        BGAV_MK_FOURCC('D','V',' ',' ') },
    { GAVL_CODEC_ID_VP8,       BGAV_MK_FOURCC('V','P','8','0') },
    { GAVL_CODEC_ID_DIV3,      BGAV_MK_FOURCC('D','I','V','3') },

    /* Overlay */
    { GAVL_CODEC_ID_DVDSUB,    BGAV_MK_FOURCC('D','V','D','S') },
    { /* End */                                                },
  };

uint32_t bgav_compression_id_2_fourcc(gavl_codec_id_t id)
  {
  int i = 0;
  while(fourccs[i].fourcc)
    {
    if(fourccs[i].id == id)
      return fourccs[i].fourcc;
    i++;
    }
  return 0;
  }

uint32_t * bgav_get_vobsub_palette(const char * str)
  {
  int index;
  uint32_t * pal = NULL;
  float r, g, b;
  int y, u, v;
        
  char ** colors = gavl_strbreak(str, ',');

  index = 0;
  while(colors[index])
    index++;
  if(index == 16)
    {
    index = 0;
    pal = malloc(16 * sizeof(*pal));

    for(index = 0; index < 16; index++)
      {
      pal[index] = strtol(colors[index], NULL, 16);

      /* Now it gets insane: The vobsub program
         converts the YCbCr palette from the IFO file to RGB,
         so we need to convert it back

         http://guliverkli.svn.sourceforge.net/viewvc/guliverkli/
         trunk/guliverkli/src/subtitles/
         VobSubFile.cpp?revision=605&view=markup
         (line 821)
      */
            
      r = (pal[index] >> 16) & 0xff;
      g = (pal[index] >>  8) & 0xff;
      b = pal[index] & 0xff;

      y =  (int)((0.257 * r) + (0.504 * g) + (0.098 * b) + 16);
      u =  (int)(-(0.148 * r) - (0.291 * g) + (0.439 * b) + 128);
      v =  (int)((0.439 * r) - (0.368 * g) - (0.071 * b) + 128);
      pal[index] = y << 16 | u << 8 | v;
      }
    }
  gavl_strbreak_free(colors);
  return pal;  
  }

