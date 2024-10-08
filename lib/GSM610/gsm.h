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

#ifndef	GSM_H
#define	GSM_H

#include	<stdio.h>		/* for FILE * 	*/

/*
 *	Interface
 */

typedef struct gsm_state * 	gsm;
typedef short		   		gsm_signal;		/* signed 16 bit */
typedef unsigned char		gsm_byte;
typedef gsm_byte 			gsm_frame[33];		/* 33 * 8 bits	 */

#define	GSM_MAGIC		0xD		  	/* 13 kbit/s RPE-LTP */

#define	GSM_PATCHLEVEL		10
#define	GSM_MINOR			0
#define	GSM_MAJOR			1

#define	GSM_OPT_VERBOSE		1
#define	GSM_OPT_FAST		2
#define	GSM_OPT_LTP_CUT		3
#define	GSM_OPT_WAV49		4
#define	GSM_OPT_FRAME_INDEX	5
#define	GSM_OPT_FRAME_CHAIN	6

gsm  gsm_create 	(void);

/* Added for libsndfile : May 6, 2002 */
void gsm_init (gsm);	

void gsm_destroy (gsm);	

int  gsm_print   (FILE *, gsm, gsm_byte  *);
int  gsm_option  (gsm, int, int *);

void gsm_encode  (gsm, gsm_signal *, gsm_byte  *);
int  gsm_decode  (gsm, gsm_byte   *, gsm_signal *);

int  gsm_explode (gsm, gsm_byte   *, gsm_signal *);
void gsm_implode (gsm, gsm_signal *, gsm_byte   *);

#endif	/* GSM_H */
/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch 
** revision control system.
**
** arch-tag: 8cfc7698-5433-4b6f-aeca-967c6fda4dec
*/

