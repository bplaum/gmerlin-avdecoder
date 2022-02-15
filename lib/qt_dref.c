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
 * Data reference
 */

#if 0

typedef struct
  {
  int version;
  uint32_t flags;
  int table_size;

  struct
    {
    int64_t size;
    uint32_t type;
    int version;
    uint32_t flags;
    uint8_t * data_reference;
    } * table;
  
  } qt_dref_t;

#endif

static int read_dref_table(bgav_input_context_t * input,
                           qt_dref_table_t * ret)
  {
  uint8_t version;

  if(!bgav_input_read_32_be(input, &ret->size) ||
     !bgav_input_read_fourcc(input, &ret->type))
    return 0;

  if(!bgav_input_read_8(input, &version) || 
     !bgav_input_read_24_be(input, &ret->flags))
    return 0;
  ret->version = version;
  
  if(ret->size > 12)
    {
    ret->data_reference = malloc(ret->size - 11);
    if(bgav_input_read_data(input, ret->data_reference,
                            ret->size - 12) < ret->size - 12)
      return 0;
    ret->data_reference[ret->size-12] = '\0';
    }
  return 1;
  }

int bgav_qt_dref_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_dref_t * ret)
  {
  int i;
  READ_VERSION_AND_FLAGS;

  if(!bgav_input_read_32_be(input, &ret->table_size))
    return 0;

  ret->table = calloc(ret->table_size, sizeof(*ret->table));
    
  for(i = 0; i < ret->table_size; i++)
    {
    if(!read_dref_table(input, &ret->table[i]))
      return 0;
    }
  return 1;
  }

void bgav_qt_dref_free(qt_dref_t * dref)
  {
  int i;
  for(i = 0; i < dref->table_size; i++)
    {
    if(dref->table[i].data_reference)
      free(dref->table[i].data_reference);
    }
  if(dref->table)
    free(dref->table);
  }

void bgav_qt_dref_dump(int indent, qt_dref_t * c)
  {
  int i;
  bgav_diprintf(indent, "dref\n");
  bgav_diprintf(indent+2, "version:    %d\n", c->version);
  bgav_diprintf(indent+2, "flags:      %08x\n", c->flags);
  bgav_diprintf(indent+2, "table_size: %08x\n", c->table_size);
  for(i = 0; i < c->table_size; i++)
    {
    bgav_diprintf(indent+4, "Table    %d\n", i);
    bgav_diprintf(indent+4, "size:    %d\n", c->table[i].size);
    bgav_diprintf(indent+4, "type:    ");
    bgav_dump_fourcc(c->table[i].type);
    bgav_dprintf("\n");

    bgav_diprintf(indent+4, "version: %d\n", c->table[i].version);
    bgav_diprintf(indent+4, "flags:   %08x\n", c->table[i].flags);
    if(c->table[i].size > 12)
      {
      bgav_diprintf(indent+4, "data_reference:\n");
      gavl_hexdump(c->table[i].data_reference,
                   c->table[i].size-12, 16);
      }
    }
  bgav_diprintf(indent, "end of dref\n");
  }
