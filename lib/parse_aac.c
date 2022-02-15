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

#include <config.h>
#include <avdec_private.h>
#include <parser.h>
#include <audioparser_priv.h>
#include <aac_frame.h>

typedef struct
  {
  int samples_per_frame;
  int scan_pos;

  bgav_aac_frame_t * frame;
  } aac_priv_t;

static int parse_aac(bgav_audio_parser_t * parser)
  {
  int bytes, samples = 0, result;
  aac_priv_t * priv = parser->priv;

  if(!priv->frame)
    priv->frame = bgav_aac_frame_create(parser->s->opt,
                                        parser->s->ext_data,
                                        parser->s->ext_size);

  if(!priv->samples_per_frame) // No non-silent frame yet
    {
    while(1)
      {
      if(priv->scan_pos >= parser->buf.size)
        return PARSER_NEED_DATA;

      //      fprintf(stderr, "Scan %d\n", priv->scan_pos);
      
      result = bgav_aac_frame_parse(priv->frame,
                                    parser->buf.buffer + priv->scan_pos,
                                    parser->buf.size - priv->scan_pos,
                                    &bytes, &samples);

      if(!result)
        return PARSER_NEED_DATA;

      else if(result > 0)
        {
        if(samples)
          {
          if(samples == 2048)
            parser->s->ci.flags |= GAVL_COMPRESSION_SBR;
          
          priv->samples_per_frame = samples;
          priv->scan_pos = 0;
          break;
          }
        else
          {
          priv->scan_pos += bytes;
          }
        }
      else // result < 0
        {
        return PARSER_ERROR;
        }
      }
    }

  if(parser->buf.size - priv->scan_pos <= 0)
    return PARSER_NEED_DATA;
  
  result = bgav_aac_frame_parse(priv->frame,
                                parser->buf.buffer,
                                parser->buf.size,
                                &bytes, &samples);
  
  if(!result)
    return PARSER_NEED_DATA;

  else if(result < 0) 
    return PARSER_ERROR;
  
  if(!parser->have_format)
    {
    parser->have_format = 1;
    bgav_aac_frame_get_audio_format(priv->frame, parser->s->data.audio.format);
    return PARSER_HAVE_FORMAT;
    }
  
  if(result > 0)
    {
    //    fprintf(stderr, "Got frame %d %d\n", bytes, priv->samples_per_frame);
    bgav_audio_parser_set_frame(parser,
                                0, bytes, priv->samples_per_frame);
    return PARSER_HAVE_FRAME;
    }
  
  return PARSER_ERROR;
  }

static void cleanup_aac(bgav_audio_parser_t * parser)
  {
  aac_priv_t * priv = parser->priv;
  if(priv->frame)
    bgav_aac_frame_destroy(priv->frame);
  free(priv);
  }

static void reset_aac(bgav_audio_parser_t * parser)
  {
  aac_priv_t * priv = parser->priv;
  priv->scan_pos = 0;
  }

void bgav_audio_parser_init_aac(bgav_audio_parser_t * parser)
  {
  aac_priv_t * priv = calloc(1, sizeof(*priv));
  parser->priv = priv;

  priv->frame = bgav_aac_frame_create(parser->s->opt,
                                      parser->s->ext_data, parser->s->ext_size);

  
  parser->parse = parse_aac;
  parser->cleanup = cleanup_aac;
  parser->reset = reset_aac;
  }
