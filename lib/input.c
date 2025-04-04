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

#undef HAVE_LINUXDVB

static int do_read(bgav_input_context_t * ctx, uint8_t * buffer, int len)
  {
  if(ctx->input->read)
    {
    return ctx->input->read(ctx, buffer, len);
    }
  else if(ctx->input->read_block)
    {
    int bytes_read = 0;
    int bytes_to_copy;
    
    while(bytes_read < len)
      {
      if(!ctx->block_ptr || (ctx->block_ptr - ctx->block >= ctx->block_size))
        {
        if(!ctx->input->read_block(ctx))
          return bytes_read;
        ctx->block_ptr = ctx->block;
        }

      bytes_to_copy = len - bytes_read;

      if(bytes_to_copy > ctx->block_size - (int)(ctx->block_ptr - ctx->block))
        bytes_to_copy = ctx->block_size - (int)(ctx->block_ptr - ctx->block);
      
      memcpy(buffer + bytes_read, ctx->block_ptr, bytes_to_copy);
      ctx->block_ptr += bytes_to_copy;
      bytes_read += bytes_to_copy;
      }
    return bytes_read;
    }
  else // Never get here
    return 0;
    
  }

static void add_char_16(gavl_buffer_t * buf,
                        uint16_t c)
  {
  uint16_t * ptr;

  gavl_buffer_alloc(buf, buf->len + 2);
  
  ptr = (uint16_t*)(buf->buf + buf->len);
  *ptr = c;
  }

static int
read_line_utf16(bgav_input_context_t * ctx,
                int (*read_char)(bgav_input_context_t*,uint16_t*),
                gavl_buffer_t * ret)
  {
  uint16_t c;
  while(1)
    {
    if(!read_char(ctx, &c))
      {
      add_char_16(ret, 0);
      return ret->len;
      break;
      }
    if(c == 0x000a) /* \n' */
      break;
    else if((c != 0x000d) && (c != 0xfeff)) /* Skip '\r' and endian marker */
      {
      add_char_16(ret, c);
      ret->len += 2;
      }
    }

  add_char_16(ret, 0);
  
  return ret->len;
  }

static void add_char(gavl_buffer_t * ret, char c)
  {
  gavl_buffer_alloc(ret, ret->len + 1);
  ret->buf[ret->len] = c;
  }

int bgav_input_read_line(bgav_input_context_t* input,
                         gavl_buffer_t * ret)
  {
  char c;
  int chars_read = 0;

  gavl_buffer_reset(ret);
  
  if(input->charset)
    {
    if(!strcmp(input->charset, "UTF-16LE"))
      return read_line_utf16(input, bgav_input_read_16_le,
                             ret);
    else if(!strcmp(input->charset, "UTF-16BE"))
      return read_line_utf16(input, bgav_input_read_16_be,
                             ret);
    }
  
  while(1)
    {
    if(!bgav_input_read_data(input, (uint8_t*)(&c), 1))
      {
      //      return 0;
      add_char(ret, '\0');
      return ret->len;
      break;
      }
    chars_read++;
    
    if(c == '\n')
      break;
    else if(c != '\r')
      {
      add_char(ret, c);
      ret->len++;
      }
    }
  add_char(ret, '\0');
  return chars_read;
  }

int bgav_input_read_convert_line(bgav_input_context_t * input,
                                 gavl_buffer_t * ret)
  {
  if(!input->charset || !strcmp(input->charset, GAVL_UTF8))
    return bgav_input_read_line(input, ret);
  else
    {
    gavl_buffer_t line_buf;
    
    if(!input->cnv)
      input->cnv = gavl_charset_converter_create(input->charset, GAVL_UTF8);
    
    gavl_buffer_init(&line_buf);
    
    if(!strcmp(input->charset, "UTF-16LE"))
      {
      if(!read_line_utf16(input, bgav_input_read_16_le,
                          &line_buf))
        return 0;
      }
    else if(!strcmp(input->charset, "UTF-16BE"))
      {
      if(!read_line_utf16(input, bgav_input_read_16_be,
                          &line_buf))
        return 0;
      }
    else
      {
      if(!bgav_input_read_line(input, &line_buf))
        return 0;
      }
    
    gavl_convert_string_to_buffer(input->cnv,
                                (char*)line_buf.buf,
                                line_buf.len,
                                ret);
    gavl_buffer_free(&line_buf);
    return ret->len;
    }
  }


static int input_read_data(bgav_input_context_t * ctx, uint8_t * buffer, int len, int block)
  {
  int bytes_to_copy = 0;
  int result;

  int bytes_read = 0;

  if(ctx->flags & BGAV_INPUT_PAUSED)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "input_read_data failed: Input paused");
    return 0;
    }
  
  if(ctx->total_bytes)
    {
    if(ctx->position + len > ctx->total_bytes)
      len = ctx->total_bytes - ctx->position;
    if(len <= 0)
      return 0;
    }
  
  if(ctx->buf.pos < ctx->buf.len)
    {
    if(len > ctx->buf.len - ctx->buf.pos)
      bytes_to_copy = ctx->buf.len - ctx->buf.pos;
    else
      bytes_to_copy = len;

    memcpy(buffer, ctx->buf.buf + ctx->buf.pos, bytes_to_copy);
    
    // gavl_buffer_flush(&ctx->buf, bytes_to_copy);
    
    ctx->buf.pos += bytes_to_copy;

    if(ctx->buf.pos == ctx->buf.len)
      gavl_buffer_reset(&ctx->buf);
    
    bytes_read += bytes_to_copy;
    }

  if(len > bytes_read)
    {
    if(!block && ctx->input->read_nonblock)
      result =
        ctx->input->read_nonblock(ctx, buffer + bytes_read, len - bytes_read);
    else
      result = do_read(ctx, buffer + bytes_read, len - bytes_read);
    
    if(result < 0)
      result = 0;
    
    bytes_read += result;
    }
  
  ctx->position += bytes_read;
  
  return bytes_read;
  }

int bgav_input_read_data(bgav_input_context_t * ctx, uint8_t * buffer, int len)
  {
  return input_read_data(ctx, buffer, len, 1);
  }


void bgav_input_ensure_buffer_size(bgav_input_context_t * ctx, int len)
  {
  int result;

  if(ctx->flags & BGAV_INPUT_PAUSED)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "bgav_input_ensure_buffer_size: Input paused");
    return;
    }

  if(ctx->buf.len - ctx->buf.pos >= len)
    return;
  
  if(ctx->buf.pos > 0)
    gavl_buffer_flush(&ctx->buf, ctx->buf.pos);
  
  if(ctx->buf.len < len)
    {
    gavl_buffer_alloc(&ctx->buf, len);
    
    result = do_read(ctx, ctx->buf.buf + ctx->buf.len,
                     len - ctx->buf.len);
    if(result < 0)
      result = 0;
    ctx->buf.len += result;
    }
  }

int bgav_input_get_data(bgav_input_context_t * ctx, uint8_t * buffer, int len)
  {
  int bytes_gotten;

  if(ctx->flags & BGAV_INPUT_PAUSED)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "bgav_input_get_data failed: Input paused");
    return 0;
    }

  
  bgav_input_ensure_buffer_size(ctx, len);
  
  bytes_gotten = (len > ctx->buf.len) ? ctx->buf.len : len;
  
  if(bytes_gotten)
    memcpy(buffer, ctx->buf.buf + ctx->buf.pos, bytes_gotten);
  
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

int bgav_input_get_24_be(bgav_input_context_t * ctx,uint32_t * ret)
  {
  uint8_t data[3];
  if(bgav_input_get_data(ctx, data, 3) < 3)
    return 0;
  *ret = GAVL_PTR_2_24BE(data);
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
extern const bgav_input_t bgav_input_mms;
extern const bgav_input_t bgav_input_http;
extern const bgav_input_t bgav_input_hls;
//extern const bgav_input_t bgav_input_mmsh;

#ifdef HAVE_CDIO
extern const bgav_input_t bgav_input_vcd;
#endif // HAVE_CDIO

#ifdef HAVE_DVDNAV
extern const bgav_input_t bgav_input_dvd;
#endif

#ifdef HAVE_LINUXDVB
extern const bgav_input_t bgav_input_dvb;
#endif

void bgav_inputs_dump()
  {
  gavl_dprintf( "<h2>Input modules</h2>\n");
  gavl_dprintf( "<ul>\n");
  gavl_dprintf( "<li>%s\n", bgav_input_file.name);
  gavl_dprintf( "<li>%s\n", bgav_input_stdin.name);
  //  gavl_dprintf( "<li>%s\n", bgav_input_rtsp.name);
  gavl_dprintf( "<li>%s\n", bgav_input_mms.name);
  //  gavl_dprintf( "<li>%s\n", bgav_input_mmsh.name);
  gavl_dprintf( "<li>%s\n", bgav_input_http.name);
  gavl_dprintf( "<li>%s\n", bgav_input_hls.name);

#ifdef HAVE_CDIO

  gavl_dprintf( "<li>%s\n", bgav_input_vcd.name);

#ifdef HAVE_DVDREAD
  //  gavl_dprintf( "<li>%s\n", bgav_input_dvd.name);
#endif

#endif // HAVE_CDIO


#ifdef HAVE_LINUXDVB
  gavl_dprintf( "<li>%s\n", bgav_input_dvb.name);
#endif

  gavl_dprintf( "</ul>\n");
  }

#define DVD_PATH "/video_ts/video_ts.ifo"
#define DVD_PATH_LEN strlen(DVD_PATH)

#ifdef HAVE_DVDNAV
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
#endif // HAVE_DVDREAD

static int input_open(bgav_input_context_t * ctx,
                      const char *url, char ** redir)
  {
  int ret = 0;
  //  const char * pos;
  char * protocol = NULL;
  char * tmp_url;
  
  tmp_url = gavl_strdup(url);
  
  if(gavl_url_split(tmp_url,
                    &protocol,
                    NULL, /* User */
                    NULL, /* Pass */
                    NULL,
                    NULL,
                    NULL))
    {
    //    if(!strcasecmp(protocol, "rtsp"))
    //      ctx->input = &bgav_input_rtsp;
    if(!strcasecmp(protocol, "mms") ||
       !strcasecmp(protocol, "mmst")
       //            || !strcasecmp(protocol, "mmsu")
       )
      ctx->input = &bgav_input_mms;
    else if(!strcasecmp(protocol, "http") ||
            !strcasecmp(protocol, "icyx") ||
            !strcasecmp(protocol, "https"))
      ctx->input = &bgav_input_http;
    else if(!strcasecmp(protocol, "hls") ||
            !strcasecmp(protocol, "hlss"))
      ctx->input = &bgav_input_hls;
    //    else if(!strcasecmp(protocol, "mmsh"))
    //      ctx->input = &bgav_input_mmsh;
    else if(!strcasecmp(protocol, "file"))
      ctx->input = &bgav_input_file;
    else if(!strcasecmp(protocol, "stdin") || !strcmp(url, "-"))
      ctx->input = &bgav_input_stdin;
#ifdef HAVE_DVDNAV
    else if(!strcmp(protocol, "dvd"))
      ctx->input = &bgav_input_dvd;
#endif
#ifdef HAVE_CDIO
    else if(!strcmp(protocol, "vcd"))
      ctx->input = &bgav_input_vcd;
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
    /* Check dvd image */
      
#ifdef HAVE_DVDNAV
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

  //  if(ctx->input->seek_time)
  //    ctx->flags |= BGAV_INPUT_CAN_SEEK_TIME;
  
  if(!ctx->input->open(ctx, tmp_url, redir))
    {
    goto fail;
    }
  
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
      new_url = gavl_sprintf("http%s", pos);
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN,
               "mms connection failed, trying http");
      ret = do_open(ctx, new_url);
      free(new_url);
      }
    }

  /* Signal fast seeking support */
  if((ctx->flags & (BGAV_INPUT_CAN_SEEK_BYTE|BGAV_INPUT_SEEK_SLOW)) == 
     BGAV_INPUT_CAN_SEEK_BYTE)
    ctx->flags |= BGAV_INPUT_CAN_SEEK_BYTE_FAST;    
  
  return ret;
  }

void bgav_input_close(bgav_input_context_t * ctx)
  {
  bgav_options_t opt;
  if(ctx->input && ctx->priv)
    {
    ctx->input->close(ctx);
    ctx->priv = NULL;
    }

  gavl_buffer_free(&ctx->buf);

  if(ctx->location)
    free(ctx->location);
  if(ctx->id3v2)
    bgav_id3v2_destroy(ctx->id3v2);
  if(ctx->charset)
    free(ctx->charset);
  if(ctx->cnv)
    gavl_charset_converter_destroy(ctx->cnv);

  if(ctx->tt)
    bgav_track_table_unref(ctx->tt);
    
  gavl_dictionary_reset(&ctx->m);
  //  free(ctx);

  if(ctx->yml)
    bgav_yml_free(ctx->yml);
  
  memcpy(&opt, &ctx->opt, sizeof(opt));
  memset(ctx, 0, sizeof(*ctx));
  memcpy(&ctx->opt, &opt, sizeof(opt));
  

  return;
  }

void bgav_input_destroy(bgav_input_context_t * ctx)
  {
  bgav_input_close(ctx);
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

  if(ctx->buf.len > ctx->buf.pos)
    {
    if(ctx->buf.len - ctx->buf.pos >= bytes_to_skip)
      {
      ctx->buf.pos += bytes_to_skip;
      ctx->position += bytes_to_skip;

      if(ctx->buf.pos == ctx->buf.len)
        gavl_buffer_reset(&ctx->buf);
      return;
      }
    else
      {
      int bytes_skipped = ctx->buf.len - ctx->buf.pos;
      bytes_to_skip -= bytes_skipped;
      ctx->position += bytes_skipped;
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
  gavl_dprintf( "Skipping %d bytes:\n", bytes);
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

  if(ctx->input->seek_byte)
    ctx->input->seek_byte(ctx, position, whence);
  else if(ctx->input->seek_block)
    {
    if(!ctx->input->seek_block(ctx, ctx->position / ctx->block_size))
      return;
    ctx->block_ptr = ctx->block + (ctx->position % ctx->block_size);
    }
  gavl_buffer_reset(&ctx->buf);
  }

int bgav_input_seek_time(bgav_input_context_t * ctx,
                          gavl_time_t time)
  {
  gavl_buffer_reset(&ctx->buf);
  return ctx->input->seek_time(ctx, &time);
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



bgav_input_context_t * bgav_input_create(bgav_t * b, const bgav_options_t * opt)
  {
  bgav_input_context_t * ret;
  
  ret = calloc(1, sizeof(*ret));
  
  ret->b = b;
  ret->input_pts = GAVL_TIME_UNDEFINED;
  ret->clock_time = GAVL_TIME_UNDEFINED;
  if(b)
    bgav_options_copy(&ret->opt, &b->opt);
  else if(opt)
    bgav_options_copy(&ret->opt, opt);
  else
    bgav_options_set_defaults(&ret->opt);

  return ret;
  }

/* Reopen  the input. Not all inputs can do this */
int bgav_input_reopen(bgav_input_context_t * ctx)
  {
  gavl_time_t delay_time = GAVL_TIME_SCALE / 2;
  const bgav_input_t * input;
  char * location = NULL;
  int ret = 0;
  char * redir = NULL;
  bgav_options_t opt;
  bgav_track_table_t * tt = NULL;

  bgav_t * b;
  
  if(ctx->location)
    {
    location = ctx->location;
    input = ctx->input;

    bgav_options_copy(&opt, &ctx->opt);
    

    b = ctx->b;
    
    tt = ctx->tt;
    ctx->tt = NULL;
    
    ctx->location = NULL;
    
    bgav_input_close(ctx);

    gavl_metadata_add_src(&ctx->m, GAVL_META_SRC, NULL, location);
    
    /* Give the server time to recreate */
    gavl_time_delay(&delay_time);

    ctx->input = input;

    bgav_options_copy(&ctx->opt, &opt);
    
    ctx->b = b;
    
    if(!ctx->input->open(ctx, location, &redir))
      {
      if(redir) free(redir);
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN,
               "Reopening %s failed", location);
      goto fail;
      }
    //    init_buffering(ctx);

    ret = 1;

    ctx->tt = tt;
    }
  fail:
  if(location)
    free(location);
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

char * bgav_input_absolute_url(bgav_input_context_t * ctx, const char * location)
  {
  if(ctx->location)
    return gavl_get_absolute_uri(location, ctx->location);
  return NULL;
  }

int bgav_input_can_read(bgav_input_context_t * ctx, int milliseconds)
  {
  if(ctx->input->can_read)
    return ctx->input->can_read(ctx, milliseconds);
  else
    return 1;
  }

int bgav_input_read_nonblock(bgav_input_context_t * ctx, uint8_t * data, int len)
  {
  return input_read_data(ctx, data, len, 0);
  }

/* Legacy functions */

static int open_legacy(bgav_t * bgav, const char * location, const char * prefix)
  {
  if(!gavl_string_starts_with(location, "prefix"))
    {
    int ret;
    char * real_location = gavl_sprintf("%s%s", prefix, location);
    ret = bgav_open(bgav, real_location);
    free(real_location);
    return ret;
    }
  else
    return bgav_open(bgav, location);
  }

int bgav_open_vcd(bgav_t * bgav, const char * location)
  {
  return open_legacy(bgav, location, "vcd://");
  }

int bgav_open_dvd(bgav_t * bgav, const char * location)
  {
  return open_legacy(bgav, location, "vcd://");

  }

int bgav_open_dvb(bgav_t * bgav, const char * location)
  {
  return open_legacy(bgav, location, "dvb://");

  }

int bgav_check_device_dvd(const char * device, char ** name)
  {
  gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Called deprecated disabled function bgav_check_device_dvd");
  return 0;
  }

bgav_device_info_t * bgav_find_devices_dvd()
  {
  gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Called deprecated disabled function bgav_find_devices_dvd");
  return NULL;
  }

int bgav_check_device_vcd(const char * device, char ** name)
  {
  gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Called deprecated disabled function bgav_check_device_vcd");
  return 0;

  }

bgav_device_info_t * bgav_find_devices_vcd()
  {
  gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Called deprecated disabled function bgav_find_devices_vcd");
  return NULL;
  }

int bgav_check_device_dvb(const char * device, char ** name)
  {
  gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Called deprecated disabled function bgav_check_device_dvb");
  return 0;
  }

bgav_device_info_t * bgav_find_devices_dvb()
  {
  gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Called deprecated disabled function bgav_find_devices_dvb");
  return NULL;
  }

int bgav_eject_disc(const char * device)
  {
  gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Called deprecated disabled function bgav_eject_disc");
  return 0;
  }

void bgav_input_set_demuxer_pts(bgav_input_context_t * ctx, int64_t pts, int scale)
  {
  if(!ctx->demuxer_scale)
    {
    ctx->demuxer_pts = pts;
    ctx->demuxer_scale = scale;
    }
  }
