/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
 *
 * Copyright (c) 2001 - 2008 Members of the Gmerlin project
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


#include <config.h>

#ifdef HAVE_DTS_H
#include <dts.h>
#else
# include <dca.h>

typedef struct dca_state_s dts_state_t;
#define DTS_MONO DCA_MONO
#define DTS_CHANNEL DCA_CHANNEL
#define DTS_STEREO DCA_STEREO
#define DTS_STEREO_SUMDIFF DCA_STEREO_SUMDIFF
#define DTS_STEREO_TOTAL DCA_STEREO_TOTAL
#define DTS_3F DCA_3F
#define DTS_2F1R DCA_2F1R
#define DTS_3F1R DCA_3F1R
#define DTS_2F2R DCA_2F2R
#define DTS_3F2R DCA_3F2R
#define DTS_4F2R DCA_4F2R
#define DTS_DOLBY DCA_DOLBY
#define DTS_CHANNEL_MAX DCA_CHANNEL_MAX
#define DTS_CHANNEL_BITS DCA_CHANNEL_BITS
#define DTS_CHANNEL_MASK DCA_CHANNEL_MASK
#define DTS_LFE DCA_LFE
#define DTS_ADJUST_LEVEL DCA_ADJUST_LEVEL

#define dts_init dca_init
#define dts_syncinfo dca_syncinfo
#define dts_frame dca_frame
#define dts_dynrng dca_dynrng
#define dts_blocks_num dca_blocks_num
#define dts_block dca_block
#define dts_samples dca_samples
#define dts_free dca_free
#endif // !HAVE_DTS_H

