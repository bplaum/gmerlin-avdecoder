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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
//#include <ctype.h>

#include <avdec_private.h>

#ifdef HAVE_LIBUDF
#include <cdio/udf.h>
#endif

#define LOG_DOMAIN "input"

#define GET_LINE_SIZE 8
#define ALLOC_SIZE    128
#define MAX_REDIRECTIONS 5



static void add_char_16(char ** buffer, uint32_t * buffer_alloc,
                        int pos, uint16_t c)
  {
  uint16_t * ptr;
  if(pos + 2 > *buffer_alloc)
    {
    while(pos+2 > *buffer_alloc)
      (*buffer_alloc) += ALLOC_SIZE;
    *buffer = realloc(*buffer, *buffer_alloc);
    }
  ptr = (uint16_t*)(*buffer + pos);
  *ptr = c;
  }

static int
read_line_utf16(bgav_input_context_t * ctx,
                int (*read_char)(bgav_input_context_t*,uint16_t*),
                char ** buffer, uint32_t * buffer_alloc, int buffer_offset,
                uint32_t * ret_len)
  {
  uint16_t c;
  int pos = buffer_offset;
  int64_t old_pos = ctx->position;

  
  
  while(1)
    {
    if(!read_char(ctx, &c))
      {
      add_char_16(buffer, buffer_alloc, pos, 0);
      return pos - buffer_offset;
      break;
      }
    if(c == 0x000a) /* \n' */
      break;
    else if((c != 0x000d) && (c != 0xfeff)) /* Skip '\r' and endian marker */
      {
      add_char_16(buffer, buffer_alloc, pos, c);
      pos+=2;
      }
    }

  add_char_16(buffer, buffer_alloc, pos, 0);
  
  if(ret_len)
    *ret_len = pos - buffer_offset;
  
  return ctx->position - old_pos;
  }

static void add_char(char ** buffer, uint32_t * buffer_alloc,
                     int pos, char c)
  {
  if(pos + 1 > *buffer_alloc)
    {
    while(pos + 1 > *buffer_alloc)
      (*buffer_alloc) += ALLOC_SIZE;
    *buffer = realloc(*buffer, *buffer_alloc);
    }
  (*buffer)[pos] = c;
  }

int bgav_input_read_line(bgav_input_context_t* input,
                         char ** buffer, uint32_t * buffer_alloc,
                         int buffer_offset, uint32_t * len)
  {
  char c;
  int pos = buffer_offset;
  int64_t old_pos = input->position;
  
  if(input->charset)
    {
    if(!strcmp(input->charset, "UTF-16LE"))
      return read_line_utf16(input, bgav_input_read_16_le,
                             buffer, buffer_alloc, buffer_offset, len);
    else if(!strcmp(input->charset, "UTF-16BE"))
      return read_line_utf16(input, bgav_input_read_16_be,
                             buffer, buffer_alloc, buffer_offset, len);
    }
  
  while(1)
    {
    if(!bgav_input_read_data(input, (uint8_t*)(&c), 1))
      {
      //      return 0;
      add_char(buffer, buffer_alloc, pos, '\0');
      return pos - buffer_offset;
      break;
      }
    else if(c == '\n')
      break;
    else if(c != '\r')
      {
      add_char(buffer, buffer_alloc, pos, c);
      pos++;
      }
    }
  add_char(buffer, buffer_alloc, pos, '\0');
  if(len)
    *len = pos - buffer_offset;
  return input->position - old_pos;
  }

int bgav_input_read_convert_line(bgav_input_context_t * input,
                                 char ** buffer,
                                 uint32_t * buffer_alloc,
                                 uint32_t * len)
  {
  uint32_t line_alloc = 0;
  char * line = NULL;
  uint32_t in_len;
  uint32_t out_len;
  int64_t old_pos = input->position;
  
  if(!input->charset || !strcmp(input->charset, BGAV_UTF8))
    return bgav_input_read_line(input, buffer, buffer_alloc, 0,
                                len);
  else
    {
    if(!input->cnv)
      input->cnv = bgav_charset_converter_create(input->opt,
                                                 input->charset, BGAV_UTF8);

    if(!bgav_input_read_line(input, &line, &line_alloc, 0, &in_len))
      return 0;
    
    bgav_convert_string_realloc(input->cnv,
                                line, in_len,
                                &out_len,
                                buffer, buffer_alloc);
    if(len) *len = out_len;
    if(line) free(line);

    return input->position - old_pos;
    }
  }


int bgav_input_read_data(bgav_input_context_t * ctx, uint8_t * buffer, int len)
  {
  int bytes_to_copy = 0;
  int result;

  int bytes_read = 0;
  
  if(ctx->total_bytes)
    {
    if(ctx->position + len > ctx->total_bytes)
      len = ctx->total_bytes - ctx->position;
    if(len <= 0)
      return 0;
    }
  
  if(ctx->buf.len)
    {
    if(len > ctx->buf.len)
      bytes_to_copy = ctx->buf.len;
    else
      bytes_to_copy = len;

    memcpy(buffer, ctx->buf.buf, bytes_to_copy);
    
    gavl_buffer_flush(&ctx->buf, bytes_to_copy);
    bytes_read += bytes_to_copy;
    }

  if(len > bytes_read)
    {
    result =
      ctx->input->read(ctx, buffer + bytes_read, len - bytes_read);

    if(result < 0)
      result = 0;
    
    bytes_read += result;
    }
  
  ctx->position += bytes_read;
  
  return bytes_read;
  }

void bgav_input_ensure_buffer_size(bgav_input_context_t * ctx, int len)
  {
  int result;
  if(ctx->buf.len < len)
    {
    gavl_buffer_alloc(&ctx->buf, len);
    
    result =
      ctx->input->read(ctx, ctx->buf.buf + ctx->buf.len,
                      len - ctx->buf.len);
    if(result < 0)
      result = 0;
    ctx->buf.len += result;
    
    }
  }

int bgav_input_get_data(bgav_input_context_t * ctx, uint8_t * buffer, int len)
  {
  int bytes_gotten;
  bgav_input_ensure_buffer_size(ctx, len);
  
  bytes_gotten = (len > ctx->buf.len) ? ctx->buf.len : len;
  
  if(bytes_gotten)
    memcpy(buffer, ctx->buf.buf, bytes_gotten);
  
  return bytes_gotten;
  }

int bgav_input_read_8(bgav_input_context_t * ctx, uint8_t * ret)
  {
  if(bgav_input_read_data(ctx, ret, 1) < 1)
    return 0;
  return 1;
  }

int bgav_input_read_16_le(bgav_input_context_t * ctx,uint16_t * ret)
  {
  uint8_t data[2];
  if(bgav_input_read_data(ctx, data, 2) < 2)
    return 0;
  *ret = GAVL_PTR_2_16LE(data);
  
  return 1;
  }

int bgav_input_read_32_le(bgav_input_context_t * ctx,uint32_t * ret)
  {
  uint8_t data[4];
  if(bgav_input_read_data(ctx, data, 4) < 4)
    return 0;
  *ret = GAVL_PTR_2_32LE(data);
  return 1;
  }

int bgav_input_read_24_le(bgav_input_context_t * ctx,uint32_t * ret)
  {
  uint8_t data[3];
  if(bgav_input_read_data(ctx, data, 3) < 3)
    return 0;
  *ret = GAVL_PTR_2_24LE(data);
  return 1;
  }

int bgav_input_read_64_le(bgav_input_context_t * ctx,uint64_t * ret)
  {
  uint8_t data[8];
  if(bgav_input_read_data(ctx, data, 8) < 8)
    return 0;
  *ret = GAVL_PTR_2_64LE(data);
  return 1;
  }

int bgav_input_read_16_be(bgav_input_context_t * ctx,uint16_t * ret)
  {
  uint8_t data[2];
  if(bgav_input_read_data(ctx, data, 2) < 2)
    return 0;

  *ret = GAVL_PTR_2_16BE(data);
  return 1;
  }

int bgav_input_read_24_be(bgav_input_context_t * ctx,uint32_t * ret)
  {
  uint8_t data[3];
  if(bgav_input_read_data(ctx, data, 3) < 3)
    return 0;
  *ret = GAVL_PTR_2_24BE(data);
  return 1;
  }


int bgav_input_read_32_be(bgav_input_context_t * ctx,uint32_t * ret)
  {
  uint8_t data[4];
  if(bgav_input_read_data(ctx, data, 4) < 4)
    return 0;

  *ret = GAVL_PTR_2_32BE(data);
  return 1;
  }
    
int bgav_input_read_64_be(bgav_input_context_t * ctx, uint64_t * ret)
  {
  uint8_t data[8];
  if(bgav_input_read_data(ctx, data, 8) < 8)
    return 0;
  
  *ret = GAVL_PTR_2_64BE(data);
  return 1;
  }

int bgav_input_get_8(bgav_input_context_t * ctx, uint8_t * ret)
  {
  if(bgav_input_get_data(ctx, ret, 1) < 1)
    return 0;
  return 1;
  }

int bgav_input_get_16_le(bgav_input_context_t * ctx,uint16_t * ret)
  {
  uint8_t data[2];
  if(bgav_input_get_data(ctx, data, 2) < 2)
    return 0;
  *ret = GAVL_PTR_2_16LE(data);
  return 1;
  }

int bgav_input_get_32_le(bgav_input_context_t * ctx,uint32_t * ret)
  {
  uint8_t data[4];
  if(bgav_input_get_data(ctx, data, 4) < 4)
    return 0;
  *ret = GAVL_PTR_2_32LE(data);
  return 1;
  }

int bgav_input_get_24_le(bgav_input_context_t * ctx,uint32_t * ret)
  {
  uint8_t data[3];
  if(bgav_input_get_data(ctx, data, 3) < 3)
    return 0;
  *ret = GAVL_PTR_2_24LE(data);
  return 1;
  }

int bgav_input_get_64_le(bgav_input_context_t * ctx,uint64_t * ret)
  {
  uint8_t data[8];
  if(bgav_input_get_data(ctx, data, 8) < 8)
    return 0;
  *ret = GAVL_PTR_2_64LE(data);
  return 1;
  }

int bgav_input_get_16_be(bgav_input_context_t * ctx,uint16_t * ret)
  {
  uint8_t data[2];
  if(bgav_input_get_data(ctx, data, 2) < 2)
    return 0;
  *ret = GAVL_PTR_2_16BE(data);
  return 1;
  }

int bgav_input_get_32_be(bgav_input_context_t * ctx,uint32_t * ret)
  {
  uint8_t data[4];
  if(bgav_input_get_data(ctx, data, 4) < 4)
    return 0;
  *ret = GAVL_PTR_2_32BE(data);
  return 1;
  }
    
int bgav_input_get_64_be(bgav_input_context_t * ctx, uint64_t * ret)
  {
  uint8_t data[8];
  if(bgav_input_get_data(ctx, data, 8) < 8)
    return 0;
  *ret = GAVL_PTR_2_64BE(data);
  return 1;
  }

static float
float32_be_read (unsigned char *cptr)
{       int             exponent, mantissa, negative ;
        float   fvalue ;

        negative = cptr [0] & 0x80 ;
        exponent = ((cptr [0] & 0x7F) << 1) | ((cptr [1] & 0x80) ? 1 : 0) ;
        mantissa = ((cptr [1] & 0x7F) << 16) | (cptr [2] << 8) | (cptr [3]) ;

        if (! (exponent || mantissa))
                return 0.0 ;

        mantissa |= 0x800000 ;
        exponent = exponent ? exponent - 127 : 0 ;

        fvalue = mantissa ? ((float) mantissa) / ((float) 0x800000) : 0.0 ;

        if (negative)
                fvalue *= -1 ;

        if (exponent > 0)
                fvalue *= (1 << exponent) ;
        else if (exponent < 0)
                fvalue /= (1 << abs (exponent)) ;

        return fvalue ;
} /* float32_be_read */

static float
float32_le_read (unsigned char *cptr)
{       int             exponent, mantissa, negative ;
        float   fvalue ;

        negative = cptr [3] & 0x80 ;
        exponent = ((cptr [3] & 0x7F) << 1) | ((cptr [2] & 0x80) ? 1 : 0) ;
        mantissa = ((cptr [2] & 0x7F) << 16) | (cptr [1] << 8) | (cptr [0]) ;

        if (! (exponent || mantissa))
                return 0.0 ;

        mantissa |= 0x800000 ;
        exponent = exponent ? exponent - 127 : 0 ;

        fvalue = mantissa ? ((float) mantissa) / ((float) 0x800000) : 0.0 ;

        if (negative)
                fvalue *= -1 ;

        if (exponent > 0)
                fvalue *= (1 << exponent) ;
        else if (exponent < 0)
                fvalue /= (1 << abs (exponent)) ;

        return fvalue ;
} /* float32_le_read */

int bgav_input_read_float_32_be(bgav_input_context_t * ctx, float * ret)
  {
  uint8_t data[4];
  if(bgav_input_read_data(ctx, data, 4) < 4)
    return 0;
  *ret = float32_be_read(data);
  return 1;
  }

int bgav_input_read_float_32_le(bgav_input_context_t * ctx, float * ret)
  {
  uint8_t data[4];
  if(bgav_input_read_data(ctx, data, 4) < 4)
    return 0;
  *ret = float32_le_read(data);
  return 1;
  }

int bgav_input_get_float_32_be(bgav_input_context_t * ctx, float * ret)
  {
  uint8_t data[4];
  if(bgav_input_get_data(ctx, data, 4) < 4)
    return 0;
  *ret = float32_be_read(data);
  return 1;
  }

int bgav_input_get_float_32_le(bgav_input_context_t * ctx, float * ret)
  {
  uint8_t data[4];
  if(bgav_input_get_data(ctx, data, 4) < 4)
    return 0;
  *ret = float32_le_read(data);
  return 1;
  }

/* 64 bit double */

static double
double64_be_read (unsigned char *cptr)
{       int             exponent, negative ;
        double  dvalue ;

        negative = (cptr [0] & 0x80) ? 1 : 0 ;
        exponent = ((cptr [0] & 0x7F) << 4) | ((cptr [1] >> 4) & 0xF) ;

        /* Might not have a 64 bit long, so load the mantissa into a double. */
        dvalue = (((cptr [1] & 0xF) << 24) | (cptr [2] << 16) | (cptr [3] << 8) | cptr [4]) ;
        dvalue += ((cptr [5] << 16) | (cptr [6] << 8) | cptr [7]) / ((double) 0x1000000) ;

        if (exponent == 0 && dvalue == 0.0)
                return 0.0 ;

        dvalue += 0x10000000 ;

        exponent = exponent - 0x3FF ;

        dvalue = dvalue / ((double) 0x10000000) ;

        if (negative)
                dvalue *= -1 ;

        if (exponent > 0)
                dvalue *= (1 << exponent) ;
        else if (exponent < 0)
                dvalue /= (1 << abs (exponent)) ;

        return dvalue ;
} /* double64_be_read */

static double
double64_le_read (unsigned char *cptr)
{       int             exponent, negative ;
        double  dvalue ;

        negative = (cptr [7] & 0x80) ? 1 : 0 ;
        exponent = ((cptr [7] & 0x7F) << 4) | ((cptr [6] >> 4) & 0xF) ;

        /* Might not have a 64 bit long, so load the mantissa into a double. */
        dvalue = (((cptr [6] & 0xF) << 24) | (cptr [5] << 16) | (cptr [4] << 8) | cptr [3]) ;
        dvalue += ((cptr [2] << 16) | (cptr [1] << 8) | cptr [0]) / ((double) 0x1000000) ;

        if (exponent == 0 && dvalue == 0.0)
                return 0.0 ;

        dvalue += 0x10000000 ;

        exponent = exponent - 0x3FF ;

        dvalue = dvalue / ((double) 0x10000000) ;

        if (negative)
                dvalue *= -1 ;

        if (exponent > 0)
                dvalue *= (1 << exponent) ;
        else if (exponent < 0)
                dvalue /= (1 << abs (exponent)) ;

        return dvalue ;
} /* double64_le_read */

int bgav_input_read_double_64_be(bgav_input_context_t * ctx, double * ret)
  {
  uint8_t data[8];
  if(bgav_input_read_data(ctx, data, 8) < 8)
    return 0;
  *ret = double64_be_read(data);
  return 1;
  }

int bgav_input_read_double_64_le(bgav_input_context_t * ctx, double * ret)
  {
  uint8_t data[8];
  if(bgav_input_read_data(ctx, data, 8) < 8)
    return 0;
  *ret = double64_le_read(data);
  return 1;
  }

int bgav_input_get_double_64_be(bgav_input_context_t * ctx, double * ret)
  {
  uint8_t data[8];
  if(bgav_input_get_data(ctx, data, 8) < 8)
    return 0;
  *ret = double64_be_read(data);
  return 1;
  }

int bgav_input_get_double_64_le(bgav_input_context_t * ctx, double * ret)
  {
  uint8_t data[8];
  if(bgav_input_get_data(ctx, data, 8) < 8)
    return 0;
  *ret = double64_le_read(data);
  return 1;
  }


/* Open input */

extern const bgav_input_t bgav_input_file;
extern const bgav_input_t bgav_input_stdin;
// extern const bgav_input_t bgav_input_rtsp;
extern const bgav_input_t bgav_input_pnm;
extern const bgav_input_t bgav_input_mms;
extern const bgav_input_t bgav_input_http;
extern const bgav_input_t bgav_input_ftp;
//extern const bgav_input_t bgav_input_mmsh;

#ifdef HAVE_CDIO
extern const bgav_input_t bgav_input_vcd;
#endif // HAVE_CDIO

#ifdef HAVE_DVDREAD
extern const bgav_input_t bgav_input_dvd;
#endif


#ifdef HAVE_LINUXDVB
extern const bgav_input_t bgav_input_dvb;
#endif


#ifdef HAVE_SAMBA
extern const bgav_input_t bgav_input_smb;
#endif

void bgav_inputs_dump()
  {
  bgav_dprintf( "<h2>Input modules</h2>\n");
  bgav_dprintf( "<ul>\n");
  bgav_dprintf( "<li>%s\n", bgav_input_file.name);
  bgav_dprintf( "<li>%s\n", bgav_input_stdin.name);
  //  bgav_dprintf( "<li>%s\n", bgav_input_rtsp.name);
  bgav_dprintf( "<li>%s\n", bgav_input_pnm.name);
  bgav_dprintf( "<li>%s\n", bgav_input_mms.name);
  //  bgav_dprintf( "<li>%s\n", bgav_input_mmsh.name);
  bgav_dprintf( "<li>%s\n", bgav_input_http.name);
  bgav_dprintf( "<li>%s\n", bgav_input_ftp.name);

#ifdef HAVE_CDIO

  bgav_dprintf( "<li>%s\n", bgav_input_vcd.name);

#ifdef HAVE_DVDREAD
  bgav_dprintf( "<li>%s\n", bgav_input_dvd.name);
#endif

#endif // HAVE_CDIO

#ifdef HAVE_SAMBA
  bgav_dprintf( "<li>%s\n", bgav_input_smb.name);
#endif

#ifdef HAVE_LINUXDVB
  bgav_dprintf( "<li>%s\n", bgav_input_dvb.name);
#endif

  bgav_dprintf( "</ul>\n");
  }

#define DVD_PATH "/video_ts/video_ts.ifo"
#define DVD_PATH_LEN strlen(DVD_PATH)


static int is_dvd_iso(const char * path)
  {
#ifdef HAVE_LIBUDF
  int ret = 0;
  const char * pos;
  udf_t *udf = NULL;
  udf_dirent_t *udf_root_dir = NULL;
  
  /* Check .iso extension */
  pos = strrchr(path, '.');
  if(!pos || strcasecmp(pos, ".iso"))
    return 0;

  /* Open UDF structure and check for a video_ts directory */
  if(!(udf = udf_open(path)))
    return 0;

  udf_root_dir = udf_get_root(udf, true, 0);
  if(!udf_root_dir)
    goto fail;
  
  while(1)
    {
    if(udf_is_dir(udf_root_dir) &&
       !strcasecmp(udf_get_filename(udf_root_dir), "VIDEO_TS"))
      {
      ret = 1;
      break;
      }
    
    udf_root_dir = udf_readdir(udf_root_dir);
    if(!udf_root_dir)
      break;
    }
  
  fail:

  if(udf)
    udf_close(udf);
  if(udf_root_dir)
    udf_dirent_free(udf_root_dir);
  return ret;
#else
  return 0;
#endif
  }

static int input_open(bgav_input_context_t * ctx,
                      const char *url, char ** redir)
  {
  int ret = 0;
  //  const char * pos;
  char * protocol = NULL;
  char * tmp_url;
  
  tmp_url = gavl_strdup(url);
  
  if(bgav_url_split(tmp_url,
                    &protocol,
                    NULL, /* User */
                    NULL, /* Pass */
                    NULL,
                    NULL,
                    NULL))
    {
    //    if(!strcasecmp(protocol, "rtsp"))
    //      ctx->input = &bgav_input_rtsp;
    if(!strcasecmp(protocol, "pnm"))
      ctx->input = &bgav_input_pnm;
    else if(!strcasecmp(protocol, "mms") ||
            !strcasecmp(protocol, "mmst")
            //            || !strcasecmp(protocol, "mmsu")
            )
      ctx->input = &bgav_input_mms;
    else if(!strcasecmp(protocol, "http") ||
            !strcasecmp(protocol, "icyx") ||
            !strcasecmp(protocol, "https"))
      ctx->input = &bgav_input_http;
    else if(!strcasecmp(protocol, "ftp"))
      ctx->input = &bgav_input_ftp;
    //    else if(!strcasecmp(protocol, "mmsh"))
    //      ctx->input = &bgav_input_mmsh;
    else if(!strcasecmp(protocol, "file"))
      ctx->input = &bgav_input_file;
    else if(!strcasecmp(protocol, "stdin") || !strcmp(url, "-"))
      ctx->input = &bgav_input_stdin;
#ifdef HAVE_SAMBA
    else if(!strcmp(protocol, "smb"))
      ctx->input = &bgav_input_smb;
#endif
    else
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Unknown protocol: %s", protocol);
      goto fail;
      }
    }
  else
    {
    /* Check vcd image */
      
#ifdef HAVE_DVDREAD
    if(strlen(url) >= DVD_PATH_LEN)
      {
      char * tmp_pos;
      const char * pos;
      
      pos = url + strlen(url) - DVD_PATH_LEN;
      if(!strcasecmp(pos, DVD_PATH))
        {
        ctx->input = &bgav_input_dvd;
        /* Libdvdread wants just the directory */
        tmp_pos = strrchr(tmp_url, '/');
        if(tmp_pos)
          *tmp_pos = '\0';
        }
      }
    if(!ctx->input && is_dvd_iso(url))
      {
      ctx->input = &bgav_input_dvd;
      }
#endif
    if(!ctx->input && !strcmp(url, "-"))
      ctx->input = &bgav_input_stdin;
    
    if(!ctx->input)
      ctx->input = &bgav_input_file;
    }

  /* Set default flags */
  ctx->flags = 0;

  if(ctx->input->seek_byte)
    ctx->flags |= BGAV_INPUT_CAN_SEEK_BYTE;
  if(ctx->input->seek_time)
    ctx->flags |= BGAV_INPUT_CAN_SEEK_TIME;
  
  if(!ctx->input->open(ctx, tmp_url, redir))
    {
    goto fail;
    }

  if(ctx->input->finalize && 
     !ctx->input->finalize(ctx))
    goto fail;
     
  ret = 1;

  fail:
  if(protocol)
    free(protocol);
  if(tmp_url)
    free(tmp_url);
  
  return ret;
  }

static int do_open(bgav_input_context_t * ctx,
                    const char *url)
  {
  int ret = 0;
  char * r = NULL;
  int i;
  char * tmp_url;
  tmp_url = gavl_strdup(url);
  for(i = 0; i < MAX_REDIRECTIONS; i++)
    {
    if(input_open(ctx, tmp_url, &r))
      {
      ret = 1;
      break;
      }
    if(!r)
      break;
    free(tmp_url);
    tmp_url = r;
    gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got redirected to %s", r);
    r = NULL;
    }
  
  free(tmp_url);
  return ret;
  }


int bgav_input_open(bgav_input_context_t * ctx,
                    const char *url)
  {
  int ret;

  gavl_metadata_add_src(&ctx->m, GAVL_META_SRC, NULL, url);

  ret = do_open(ctx, url);
  
  
  if(!ret)
    {
    if(ctx->input == &bgav_input_mms)
      {
      char * new_url;
      char * pos;
      if(ctx->priv)
        {
        ctx->input->close(ctx);
        ctx->priv = NULL;
        }
      pos = strstr(url, "://");
      if(!pos)
        return 0;
      new_url = bgav_sprintf("http%s", pos);
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
               "mms connection failed, trying http");
      ret = do_open(ctx, new_url);
      free(new_url);
      }
    }
  
  return ret;
  }

void bgav_input_close(bgav_input_context_t * ctx)
  {
  const bgav_options_t * opt;
  if(ctx->input && ctx->priv)
    {
    ctx->input->close(ctx);
    ctx->priv = NULL;
    }

  gavl_buffer_free(&ctx->buf);

  if(ctx->filename)
    free(ctx->filename);
  if(ctx->index_file)
    free(ctx->index_file);
  if(ctx->url)
    free(ctx->url);
  if(ctx->id3v2)
    bgav_id3v2_destroy(ctx->id3v2);
  if(ctx->charset)
    free(ctx->charset);
  if(ctx->cnv)
    bgav_charset_converter_destroy(ctx->cnv);

  if(ctx->tt)
    bgav_track_table_unref(ctx->tt);
    
  gavl_dictionary_free(&ctx->m);
  //  free(ctx);
  
  opt = ctx->opt;
  memset(ctx, 0, sizeof(*ctx));
  ctx->opt = opt;

  if(ctx->yml)
    bgav_yml_free(ctx->yml);

  return;
  }

void bgav_input_destroy(bgav_input_context_t * ctx)
  {
  bgav_input_close(ctx);
  bgav_options_free(&ctx->opt_priv);
  free(ctx);
  }

void bgav_input_skip(bgav_input_context_t * ctx, int64_t bytes)
  {
  int i;
  //  int64_t old_pos;
  int64_t bytes_to_skip = bytes;
  uint8_t buf;

  //  ctx->position += bytes;
  //  old_pos = ctx->position;

  if(bytes < 0)
    fprintf(stderr, "Bytes < 0 in bgav_input_skip. That's most likely a bug\n");

  if(ctx->buf.len)
    {
    if(ctx->buf.len >= bytes_to_skip)
      {
      gavl_buffer_flush(&ctx->buf, bytes_to_skip);
      ctx->position += bytes;
      return;
      }
    else
      {
      bytes_to_skip -= ctx->buf.len;
      ctx->position += ctx->buf.len;
      gavl_buffer_reset(&ctx->buf);
      }
    }
  if((ctx->flags & (BGAV_INPUT_CAN_SEEK_BYTE|BGAV_INPUT_SEEK_SLOW)) ==
     BGAV_INPUT_CAN_SEEK_BYTE)
    bgav_input_seek(ctx, bytes_to_skip, SEEK_CUR);
  else if(((ctx->flags & (BGAV_INPUT_CAN_SEEK_BYTE|BGAV_INPUT_SEEK_SLOW)) ==
           (BGAV_INPUT_CAN_SEEK_BYTE|BGAV_INPUT_SEEK_SLOW)) && (bytes_to_skip >= 10 * 1024))
    bgav_input_seek(ctx, bytes_to_skip, SEEK_CUR);
  else /* Only small amounts of data should be skipped like this */
    {
    for(i = 0; i < bytes_to_skip; i++)
      bgav_input_read_8(ctx, &buf);
    }
  //  do_buffer(ctx);
  }

void bgav_input_skip_dump(bgav_input_context_t * ctx, int bytes)
  {
  uint8_t * buf;
  buf = malloc(bytes);
  if(bgav_input_read_data(ctx, buf, bytes) < bytes)
    {
    free(buf);
    return;
    }
  bgav_dprintf( "Skipping %d bytes:\n", bytes);
  gavl_hexdump(buf, bytes, 16);
  free(buf);
  }

void bgav_input_get_dump(bgav_input_context_t * ctx, int bytes)
  {
  uint8_t * buf;
  int bytes_read;
  
  buf = malloc(bytes);
  bytes_read = bgav_input_get_data(ctx, buf, bytes);

  gavl_hexdump(buf, bytes_read, 16);
  free(buf);
  }


void bgav_input_seek(bgav_input_context_t * ctx,
                     int64_t position,
                     int whence)
  {
  /*
   *  ctx->position MUST be set before seeking takes place
   *  because some seek() methods might use the position value
   */
  
  switch(whence)
    {
    case SEEK_SET:
      ctx->position = position;
      break;
    case SEEK_CUR:
      ctx->position += position;
      break;
    case SEEK_END:
      ctx->position = ctx->total_bytes + position;
      break;
    }
  ctx->input->seek_byte(ctx, position, whence);

  gavl_buffer_reset(&ctx->buf);
  }

int bgav_input_read_string_pascal(bgav_input_context_t * ctx,
                                  char * ret)
  {
  uint8_t len;
  if(!bgav_input_read_8(ctx, &len) ||
     (bgav_input_read_data(ctx, (uint8_t*)ret, len) < len))
    return 0;
  ret[len] = '\0';
  return 1;
  }

#if 0
void bgav_input_buffer(bgav_input_context_t * ctx)
  {
  int bytes_to_read;
  int result;
  
  if(!(ctx->flags & BGAV_INPUT_DO_BUFFER))
    return;
  
  while(ctx->buffer_size < ctx->buffer_alloc)
    {
    bytes_to_read = ctx->buffer_alloc / 20;
    if(bytes_to_read > ctx->buffer_alloc - ctx->buffer_size)
      bytes_to_read = ctx->buffer_alloc - ctx->buffer_size;
    result = ctx->input->read(ctx, ctx->buffer + ctx->buffer_size, bytes_to_read);

    if(result < bytes_to_read)
      return;

    ctx->buffer_size += result;
    
    if(ctx->opt->buffer_callback)
      {
      ctx->opt->buffer_callback(ctx->opt->buffer_callback_data,
                                (float)ctx->buffer_size / (float)ctx->buffer_alloc);
      }
    }
  }
#endif

int bgav_input_read_sector(bgav_input_context_t * ctx, uint8_t * buf)
  {
  if(!ctx->input->read_sector)
    return 0;
  return ctx->input->read_sector(ctx, buf);
  }

void bgav_input_seek_sector(bgav_input_context_t * ctx,
                            int64_t sector)
  {
  if(ctx->input->seek_sector)
    ctx->input->seek_sector(ctx, sector);
  }

bgav_input_context_t * bgav_input_create(bgav_t * b, const bgav_options_t * opt)
  {
  bgav_input_context_t * ret;
  
  ret = calloc(1, sizeof(*ret));
  
  ret->b = b;

  if(b)
    ret->opt = &b->opt;
  else
    ret->opt = opt;
  
  return ret;
  }

/* Reopen  the input. Not all inputs can do this */
int bgav_input_reopen(bgav_input_context_t * ctx)
  {
  gavl_time_t delay_time = GAVL_TIME_SCALE / 2;
  const bgav_input_t * input;
  char * url = NULL;
  int ret = 0;
  char * redir;
  const bgav_options_t * opt;
  bgav_track_table_t * tt = NULL;

  bgav_t * b;
  
  if(ctx->url)
    {
    url = ctx->url;
    input = ctx->input;
    opt = ctx->opt;

    b = ctx->b;
    
    tt = ctx->tt;
    ctx->tt = NULL;
    
    ctx->url = NULL;
    
    bgav_input_close(ctx);

    gavl_metadata_add_src(&ctx->m, GAVL_META_SRC, NULL, url);
    
    /* Give the server time to recreate */
    gavl_time_delay(&delay_time);

    ctx->input = input;
    ctx->opt = opt;
    
    if(!ctx->input->open(ctx, url, &redir))
      {
      if(redir) free(redir);
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Reopening %s failed", url);
      goto fail;
      }
    //    init_buffering(ctx);

    if(ctx->input->finalize && 
       !ctx->input->finalize(ctx))
      goto fail;
    ret = 1;

    ctx->tt = tt;
    ctx->b = b;
    }
  fail:
  if(url)
    free(url);
  return ret;
  }

bgav_yml_node_t * bgav_input_get_yml(bgav_input_context_t * ctx)
  {
  if(ctx->yml)
    return ctx->yml;
  else if(bgav_yml_probe(ctx))
    {
    ctx->yml = bgav_yml_parse(ctx);
    return ctx->yml;
    }
  return NULL;
  }

char * bgav_input_absolute_url(bgav_input_context_t * ctx, const char * url)
  {
  char * ret = NULL;
  char * base;
  const char * pos1;
  const char * pos2;

  // Return early for complete URL
  if(strstr(url, "://"))
    return gavl_strdup(url);
  
  /* Filename */
  if(ctx->filename)
    {
    if(*url == '/')            // Absolute path
      return gavl_strdup(url);
    else                       // Relative path
      {
      pos1 = strrchr(ctx->filename, '/');
      base = gavl_strndup(ctx->filename, pos1);
      ret = bgav_sprintf("%s/%s", base, url);
      free(base);
      return ret;
      }
    }
  
  /* URL */
  else if(*url == '/') // URL relative to host
    {
    pos1 = strstr(ctx->url, "://");

    if(!pos1)
      pos1 = ctx->url;
    else
      pos1 += 3;

    pos2 = strchr(pos1, '/');
    if(!pos2)
      pos2 = pos1 + strlen(pos1);
    
    base = gavl_strndup(pos1, pos2);
    ret = bgav_sprintf("%s/%s", base, url);
    free(base);
    }
  else // Relative to file
    {
    pos1 = strrchr(ctx->url, '/');
    base = gavl_strndup(ctx->url, pos1);
    ret = bgav_sprintf("%s/%s", base, url);
    free(base);
    return ret;
    }
  return ret;
  }
