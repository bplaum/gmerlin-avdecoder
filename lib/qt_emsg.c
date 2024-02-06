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


#include <avdec_private.h>

#include <qt.h>

static char * read_string(bgav_input_context_t * input, qt_atom_header_t * h)
  {
  char buf[2];
  char * ret = NULL;

  buf[1] = '\0';

  while(input->position < h->start_position + h->size)
    {
    if(bgav_input_read_data(input, (uint8_t*)buf, 1) < 1)
      goto fail;

    if(buf[0] == '\0')
      break;
    
    ret = gavl_strcat(ret, buf);
    }
  return ret;
  fail:
  if(ret)
    free(ret);
  return NULL;
  }

int bgav_qt_emsg_read(qt_atom_header_t * h,
                      bgav_input_context_t * input,
                      qt_emsg_t * ret)
  {
  READ_VERSION_AND_FLAGS;

  if(ret->version == 0)
    {
    ret->scheme_id_uri = read_string(input, h);
    ret->value = read_string(input, h);
    if(!bgav_input_read_32_be(input, &ret->timescale) ||
       !bgav_input_read_32_be(input, &ret->presentation_time_delta) ||
       !bgav_input_read_32_be(input, &ret->event_duration) ||
       !bgav_input_read_32_be(input, &ret->id))
      return 0;
    }
  else if(ret->version == 1)
    {
    if(!bgav_input_read_32_be(input, &ret->timescale) ||
       !bgav_input_read_64_be(input, &ret->presentation_time) ||
       !bgav_input_read_32_be(input, &ret->event_duration) ||
       !bgav_input_read_32_be(input, &ret->id))
      return 0;
    ret->scheme_id_uri = read_string(input, h);
    ret->value = read_string(input, h);
    }
  if(h->start_position + h->size - input->position > 0)
    {
    gavl_buffer_alloc(&ret->message_data, h->start_position + h->size - input->position);
    ret->message_data.len = bgav_input_read_data(input, ret->message_data.buf,
                                                 h->start_position + h->size - input->position);
    }
  return 1;
  }

void bgav_qt_emsg_init(qt_emsg_t * emsg)
  {
  memset(emsg, 0, sizeof(*emsg));
  }
   

void bgav_qt_emsg_free(qt_emsg_t * emsg)
  {
  if(emsg->scheme_id_uri)
    free(emsg->scheme_id_uri);
  if(emsg->value)
    free(emsg->value);
  gavl_buffer_free(&emsg->message_data);
  }

void bgav_qt_emsg_dump(int indent, qt_emsg_t * emsg)
  {
  gavl_diprintf(indent, "emsg\n");
  gavl_diprintf(indent, "  Version: %d\n", emsg->version);
  gavl_diprintf(indent, "  Flags:   %d\n", emsg->flags);

  if(emsg->version == 0)
    {
    gavl_diprintf(indent, "  scheme_id_uri:           %s\n", emsg->scheme_id_uri);
    gavl_diprintf(indent, "  value:                   %s\n", emsg->value);
    gavl_diprintf(indent, "  timescale:               %d\n", emsg->timescale);
    gavl_diprintf(indent, "  presentation_time_delta: %d\n", emsg->presentation_time_delta);
    gavl_diprintf(indent, "  event_duration:          %d\n", emsg->event_duration);
    gavl_diprintf(indent, "  id:                      %d\n", emsg->id);
    }
  else if(emsg->version == 1)
    {
    gavl_diprintf(indent, "  timescale:               %d\n", emsg->timescale);
    gavl_diprintf(indent, "  presentation_time:       %"PRId64"\n", emsg->presentation_time);
    gavl_diprintf(indent, "  event_duration:          %d\n", emsg->event_duration);
    gavl_diprintf(indent, "  id:                      %d\n", emsg->id);
    gavl_diprintf(indent, "  scheme_id_uri:           %s\n", emsg->scheme_id_uri);
    gavl_diprintf(indent, "  value:                   %s\n", emsg->value);
    }
  
  gavl_hexdump(emsg->message_data.buf, emsg->message_data.len, 16);
  }
