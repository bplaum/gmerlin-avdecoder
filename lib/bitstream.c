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
#include <bitstream.h>

//#define OLD_BITSTREAM

static inline void fill_cache(bgav_bitstream_t * b)
  {
  int i;
  int bytes = sizeof(b->c);
  
  if(b->end - b->pos < bytes)
    bytes = b->end - b->pos;

  b->c = 0;
  
  for(i = 0; i < bytes; i++)
    {
    b->c <<= 8;
    b->c |= *b->pos;
    b->pos++;
    }
  b->bit_cache = bytes * 8;
  }

void bgav_bitstream_init(bgav_bitstream_t * b, const uint8_t * pos, 
                         int len)
  {
  b->pos = pos;
  b->end = pos + len;
#ifdef OLD_BITSTREAM
  b->c = *pos;
  b->pos++;
  b->bit_cache = 8;
#else
  fill_cache(b);
#endif
  }

int bgav_bitstream_get_long(bgav_bitstream_t * b, int64_t * ret1,  int bits)
  {
  int bits_read = 0;
  int bits_to_copy;
  int64_t ret = 0;

  while(bits_read < bits)
    {
    if(!b->bit_cache)
      {
      if(b->pos >= b->end)
        return 0;
#ifdef OLD_BITSTREAM
      b->c = *b->pos;
      b->pos++;
      b->bit_cache = 8;
#else
      fill_cache(b);
#endif
      }
    bits_to_copy = bits - bits_read;
    if(bits_to_copy > b->bit_cache)
      bits_to_copy = b->bit_cache;
    
    ret <<= bits_to_copy;
    ret |= (b->c >> (b->bit_cache-bits_to_copy)) & (((1<<bits_to_copy)-1));
    bits_read += bits_to_copy;
    b->bit_cache -= bits_to_copy;
    }
  *ret1 = ret;
  return 1;
  }

int bgav_bitstream_get(bgav_bitstream_t * b, int * ret,  int bits)
  {
  int64_t tmp;
  if(!bgav_bitstream_get_long(b, &tmp, bits))
    return 0;
  *ret = tmp;
  return 1;
  }

int bgav_bitstream_get_bits(bgav_bitstream_t * b)
  {
  return b->bit_cache + 8 * (b->end - b->pos);
  }

int bgav_bitstream_peek(bgav_bitstream_t * b, int * ret, int bits)
  {
  int64_t tmp = 0;
  int result;
  
  /* State is saved here */
  const uint8_t * pos;
  int bit_cache;
  uint32_t c;

  /* Save state */
  pos       = b->pos;
  bit_cache = b->bit_cache;
  c         = b->c;
  
  result = bgav_bitstream_get_long(b, &tmp, bits);

  /* Restore state */
  b->pos       = pos;
  b->bit_cache = bit_cache;
  b->c         = c;

  *ret = tmp;
  
  return result;
  }

int bgav_bitstream_skip(bgav_bitstream_t * b, int bits)
  {
  int64_t tmp;
  return bgav_bitstream_get_long(b, &tmp, bits);
  }

/* golomb parsing */

int bgav_bitstream_get_golomb_ue(bgav_bitstream_t * b, int * ret)
  {
  int bits, num = 0;
  while(1)
    {
    if(!bgav_bitstream_get(b, &bits, 1))
      return 0;
    if(bits)
      break;
    else
      num++;
    }
  /* The variable codeNum is then assigned as follows:
     codeNum = 2^leadingZeroBits - 1 + read_bits( leadingZeroBits ) */
  
  if(!bgav_bitstream_get(b, &bits, num))
    return 0;
  
  *ret = (1 << num) - 1 + bits;
  return 1;
  }

int bgav_bitstream_get_golomb_se(bgav_bitstream_t * b, int * ret)
  {
  int ret1;
  if(!bgav_bitstream_get_golomb_ue(b, &ret1))
    return 0;

  if(ret1 & 1)
    *ret = ret1>>1;
  else
    *ret = -(ret1>>1);
  return 1;
  }

int bgav_bitstream_decode012(bgav_bitstream_t * b, int * ret)
  {
  int n;

  if(!bgav_bitstream_get(b, &n, 1))
    return 0;

  if(!n)
    {
    *ret = 0;
    return 1;
    }

  if(!bgav_bitstream_get(b, &n, 1))
    return 0;
  *ret = n + 1;
  return 1;
  }

int bgav_bitstream_get_unary(bgav_bitstream_t * b, int stop,
                             int len, int * ret)
  {
  int i = 0;
  int tmp;
  
  while(i < len)
    {
    if(!bgav_bitstream_get(b, &tmp, 1))
      return 0;
    if(tmp == stop)
      break;
    i++;
    }
  *ret = i;
  return 1;
  }
