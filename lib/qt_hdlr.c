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
#include <stdlib.h>

#include <avdec_private.h>
#include <stdio.h>

#include <qt.h>

/*

typedef struct
  {
  uint32_t component_type;
  uint32_t component_subtype;
  uint32_t component_manufacturer;
  uint32_t component_flags;
  uint32_t component_flag_mask;
  char * component_name;
  } qt_hdlr_t;
*/


void bgav_qt_hdlr_dump(int indent, qt_hdlr_t * ret)
  {
  gavl_diprintf(indent, "hdlr:\n");
  
  gavl_diprintf(indent+2, "component_type:         ");
  bgav_dump_fourcc(ret->component_type);
  
  gavl_dprintf("\n");
  gavl_diprintf(indent+2, "component_subtype:      ");
  bgav_dump_fourcc(ret->component_subtype);
  gavl_dprintf("\n");

  gavl_diprintf(indent+2, "component_manufacturer: ");
  bgav_dump_fourcc(ret->component_manufacturer);
  gavl_dprintf("\n");

  gavl_diprintf(indent+2, "component_flags:        0x%08x\n",
                ret->component_flags);
  gavl_diprintf(indent+2, "component_flag_mask:    0x%08x\n",
                ret->component_flag_mask);
  gavl_diprintf(indent+2, "component_name:         %s\n",
                ret->component_name);
  gavl_diprintf(indent, "end of hdlr\n");

  }


int bgav_qt_hdlr_read(qt_atom_header_t * h,
                      bgav_input_context_t * input,
                      qt_hdlr_t * ret)
  {
  int name_len;
  //  uint8_t tmp_8;
  READ_VERSION_AND_FLAGS;
  memcpy(&ret->h, h, sizeof(*h));
  if (!bgav_input_read_fourcc(input, &ret->component_type) ||
      !bgav_input_read_fourcc(input, &ret->component_subtype) ||
      !bgav_input_read_fourcc(input, &ret->component_manufacturer) ||
      !bgav_input_read_32_be(input, &ret->component_flags) ||
      !bgav_input_read_32_be(input, &ret->component_flag_mask))
    return 0;

  name_len = h->start_position + h->size - input->position;

  if(name_len)
    {
    ret->component_name = malloc(name_len + 1);
    if(bgav_input_read_data(input, (uint8_t*)(ret->component_name), name_len) <
       name_len)
      return 0;
    ret->component_name[name_len] = '\0';

    /* Dirty fix for Quicktime files: If the first byte is the length
       byte, remove it */
    if(ret->component_name[0] == name_len - 1)
      memmove(ret->component_name, ret->component_name + 1,
              strlen(ret->component_name + 1) + 1);
    }
  
  bgav_qt_atom_skip(input, h);
  
  return 1;
  }

void bgav_qt_hdlr_free(qt_hdlr_t * c)
  {
  if(c->component_name)
    free(c->component_name);
  }
