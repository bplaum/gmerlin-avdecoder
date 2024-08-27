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

#include	"config.h"

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>



#include "gsm.h"
#include "gsm610_priv.h"

gsm gsm_create (void)
{
	gsm  r;

	r = malloc (sizeof(struct gsm_state));
	if (!r) return r;
	
	memset((char *)r, 0, sizeof (struct gsm_state));
	r->nrp = 40;

	return r;
}

/* Added for libsndfile : May 6, 2002. Not sure if it works. */
void gsm_init (gsm state)
{
	memset (state, 0, sizeof (struct gsm_state)) ;
	state->nrp = 40 ;
} 
/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch 
** revision control system.
**
** arch-tag: 9fedb6b3-ed99-40c2-aac1-484c536261fe
*/

