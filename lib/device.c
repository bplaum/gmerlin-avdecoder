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

/* Handling of input devices */
#include <avdec_private.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <utils.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

bgav_device_info_t * bgav_device_info_append(bgav_device_info_t * arr,
                                             const char * device,
                                             const char * name)
  {
  int size = 0;
    
  if(arr)
    {
    while(arr[size].device)
      size++;
    }
  
  size++;

  arr = realloc(arr, (size+1) * sizeof(*arr));
  
  arr[size-1].device = gavl_strdup(device);
  arr[size-1].name = gavl_strdup(name);

  /* Zero terminate */
  
  memset(&arr[size], 0, sizeof(arr[size]));

  return arr;
  }

void bgav_device_info_destroy(bgav_device_info_t * arr)
  {
  int i = 0;

  if(!arr)
    return;
  
  while(arr[i].device)
    {
    free(arr[i].device);
    if(arr[i].name)
      free(arr[i].name);
    i++;
    }
  free(arr);
  }

void bgav_device_info_dump(bgav_device_info_t * arr)
  {
  int i = 0;

  if(!arr || !arr->device)
    {
    bgav_dprintf( "No devices\n");
    return;
    }
  while(arr[i].device)
    {
    bgav_dprintf( "Name:   %s\n", (arr[i].name ? arr[i].name : "Unknown"));
    bgav_dprintf( "Device: %s\n", arr[i].device);
    i++;
    }

  }
