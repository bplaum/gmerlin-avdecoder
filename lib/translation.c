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

#include <pthread.h>
#include <avdec_private.h>
#include <libintl.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int translation_initialized = 0;

void bgav_translation_init()
  {
  pthread_mutex_lock(&mutex);

  if(!translation_initialized)
    {
    bindtextdomain (PACKAGE, LOCALE_DIR);
    translation_initialized = 1;
    }
  pthread_mutex_unlock(&mutex);
  }
