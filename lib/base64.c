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



/* Imported file, see original copyright below */
   
/*
 * FILE:   base64.c
 * AUTHOR: Colin Perkins
 *
 * MIME base64 encoder/decoder described in rfc1521. This code is derived
 * from version 2.7 of the Bellcore metamail package.
 *
 * Copyright (c) 1998-2000 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (c) 1991 Bell Communications Research, Inc. (Bellcore)
 * 
 * Permission to use, copy, modify, and distribute this material 
 * for any purpose and without fee is hereby granted, provided 
 * that the above copyright notice and this permission notice 
 * appear in all copies, and that the name of Bellcore not be 
 * used in advertising or publicity pertaining to this 
 * material without the specific, prior written permission 
 * of an authorized representative of Bellcore.  BELLCORE 
 * MAKES NO REPRESENTATIONS ABOUT THE ACCURACY OR SUITABILITY 
 * OF THIS MATERIAL FOR ANY PURPOSE.  IT IS PROVIDED "AS IS", 
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES.
 * 
 */

/* Taken from mpeg4ip CVS and changed to fit into this library */

/* 2010-08-16 [IOhannes m zmoelnig]: rebased the code on
 * http://mediatools.cs.ucl.ac.uk/nets/mmedia/browser/common/trunk/src/base64.c
 * (revision:4412), which is released under the 3-clause BSD style license (with
 * explicit permission of the copyright holders) - thus it is now GPL compatible
 */

#define ASSERT(cond) \
if(!(cond)) return 0

#include <avdec_private.h>

static const unsigned char basis_64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int bgav_base64encode(const unsigned char *input, int input_length, unsigned char *output, int output_length)
  {
  int	i = 0, j = 0;
  int	pad;

  ASSERT(output_length >= (input_length * 4 / 3));

  while (i < input_length) {
  pad = 3 - (input_length - i);
  if (pad == 2) {
  output[j  ] = basis_64[input[i]>>2];
  output[j+1] = basis_64[(input[i] & 0x03) << 4];
  output[j+2] = '=';
  output[j+3] = '=';
  } else if (pad == 1) {
  output[j  ] = basis_64[input[i]>>2];
  output[j+1] = basis_64[((input[i] & 0x03) << 4) | ((input[i+1] & 0xf0) >> 4)];
  output[j+2] = basis_64[(input[i+1] & 0x0f) << 2];
  output[j+3] = '=';
  } else{
  output[j  ] = basis_64[input[i]>>2];
  output[j+1] = basis_64[((input[i] & 0x03) << 4) | ((input[i+1] & 0xf0) >> 4)];
  output[j+2] = basis_64[((input[i+1] & 0x0f) << 2) | ((input[i+2] & 0xc0) >> 6)];
  output[j+3] = basis_64[input[i+2] & 0x3f];
  }
  i += 3;
  j += 4;
  }
  return j;
  }


/* This assumes that an unsigned char is exactly 8 bits. Not portable code! :-) */
static const unsigned char index_64[128] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,   62, 0xff, 0xff, 0xff,   63,
  52,   53,   54,   55,   56,   57,   58,   59,   60,   61, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff,    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,
  15,   16,   17,   18,   19,   20,   21,   22,   23,   24,   25, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff,   26,   27,   28,   29,   30,   31,   32,   33,   34,   35,   36,   37,   38,   39,   40,
  41,   42,   43,   44,   45,   46,   47,   48,   49,   50,   51, 0xff, 0xff, 0xff, 0xff, 0xff
};

#define char64(c)  ((c > 127) ? 0xff : index_64[(c)])

int bgav_base64decode(const char *_input,
                      int input_length,
                      unsigned char *output, int output_length)
  {
  int		i = 0, j = 0, pad;
  unsigned char	c[4];
  const unsigned char * input = (const unsigned char *)_input;
  
  ASSERT(output_length >= (input_length * 3 / 4));
  ASSERT((input_length % 4) == 0);
  while ((i + 3) < input_length) {
  pad  = 0;
  c[0] = char64(input[i  ]); pad += (c[0] == 0xff);
  c[1] = char64(input[i+1]); pad += (c[1] == 0xff);
  c[2] = char64(input[i+2]); pad += (c[2] == 0xff);
  c[3] = char64(input[i+3]); pad += (c[3] == 0xff);
  if (pad == 2) {
  output[j++] = (c[0] << 2) | ((c[1] & 0x30) >> 4);
  output[j]   = (c[1] & 0x0f) << 4;
  } else if (pad == 1) {
  output[j++] = (c[0] << 2) | ((c[1] & 0x30) >> 4);
  output[j++] = ((c[1] & 0x0f) << 4) | ((c[2] & 0x3c) >> 2);
  output[j]   = (c[2] & 0x03) << 6;
  } else {
  output[j++] = (c[0] << 2) | ((c[1] & 0x30) >> 4);
  output[j++] = ((c[1] & 0x0f) << 4) | ((c[2] & 0x3c) >> 2);
  output[j++] = ((c[2] & 0x03) << 6) | (c[3] & 0x3f);
  }
  i += 4;
  }
  return j;
  }
