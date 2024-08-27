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
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

#include "gsm610_priv.h"

#include "gsm.h"

int gsm_option (gsm r, int opt, int * val)
{
	int 	result = -1;

	switch (opt) {
	case GSM_OPT_LTP_CUT:
#ifdef 	LTP_CUT
		result = r->ltp_cut;
		if (val) r->ltp_cut = *val;
#endif
		break;

	case GSM_OPT_VERBOSE:
#ifndef	NDEBUG
		result = r->verbose;
		if (val) r->verbose = *val;
#endif
		break;

	case GSM_OPT_FAST:

#if	defined(FAST) && defined(USE_FLOAT_MUL)
		result = r->fast;
		if (val) r->fast = !!*val;
#endif
		break;

	case GSM_OPT_FRAME_CHAIN:

#ifdef WAV49
		result = r->frame_chain;
		if (val) r->frame_chain = *val;
#endif
		break;

	case GSM_OPT_FRAME_INDEX:

#ifdef WAV49
		result = r->frame_index;
		if (val) r->frame_index = *val;
#endif
		break;

	case GSM_OPT_WAV49:

#ifdef WAV49 
		result = r->wav_fmt;
		if (val) r->wav_fmt = !!*val;
#endif
		break;

	default:
		break;
	}
	return result;
}
/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch 
** revision control system.
**
** arch-tag: 963ff156-506f-4359-9145-371e9060b030
*/

