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

#define LOG_DOMAIN "qt_udta"

#if 0

typedef struct
  {
  /*
   *   We support all strings, which begin with the
   *   (C)-Character, from the Quicktime Spec
   */

  char * cpy; /* Copyright                        */
  char * day; /* Date                             */
  char * dir; /* Director                         */
  char * ed1; /* 1st edit date and description    */
  char * ed2; /* 2nd edit date and description    */
  char * ed3; /* 3rd edit date and description    */
  char * ed4; /* 4th edit date and description    */
  char * ed5; /* 5th edit date and description    */
  char * ed6; /* 6th edit date and description    */
  char * ed7; /* 7th edit date and description    */
  char * ed8; /* 8th edit date and description    */
  char * ed9; /* 9th edit date and description    */
  char * fmt; /* Format (Computer generated etc.) */  
  char * inf; /* Information about the movie      */
  char * prd; /* Producer                         */
  char * prf; /* Performers                       */
  char * rec; /* Hard- software requirements      */
  char * src; /* Credits for movie Source         */
  char * wrt; /* Writer                           */

  /* Additional stuff from OpenQuicktime (not in the QT spec I have) */

  char * nam; /* Name            */
  char * ART; /* Artist          */
  char * alb; /* Album           */
  char * enc; /* Encoded by      */
  char * trk; /* Track           */
  char * cmt; /* Comment         */
  char * aut; /* Author          */
  char * com; /* Composer        */
  char * des; /* Description     */
  char * dis; /* Disclaimer      */
  char * gen; /* Genre           */
  char * hst; /* Host computer   */
  char * mak; /* Make (??)       */
  char * mod; /* Model (??)      */
  char * ope; /* Original Artist */
  char * PRD; /* Product         */
  char * swr; /* Software        */
  char * wrn; /* Warning         */
  char * url; /* URL link        */

  /* iTunes track number */
  uint16_t trkn;
  
  } qt_udta_t;

#endif

/* Read a newly allocated string from a string list */
/* We take only the first language                  */

static char * read_string(qt_atom_header_t * h,
                          bgav_input_context_t * ctx, int have_ilst)
  {
  uint16_t size;
  uint16_t lang;
  uint32_t tmp_32;
  char * ret;
  qt_atom_header_t data_atom;
  if(!have_ilst)
    {
    if(!bgav_input_read_16_be(ctx, &size) ||
       !bgav_input_read_16_be(ctx, &lang))
      return NULL;

    if(ctx->position + size > h->start_position + h->size)
      {
      gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN,
               "Skipping garbage in udta atom");
      bgav_qt_atom_skip(ctx, h);
      return NULL;
      }
    
    ret = malloc(size + 1);
    if(bgav_input_read_data(ctx, (uint8_t*)ret, size) < size)
      {
      free(ret);
      return NULL;
      }
    ret[size] = '\0';
    }
  else
    {
    if(!bgav_qt_atom_read_header(ctx, &data_atom) ||
       (data_atom.fourcc != BGAV_MK_FOURCC('d','a','t','a')) ||
       !bgav_input_read_32_be(ctx, &tmp_32) || /* Version (0) and flags 0x000001 */
       !(tmp_32 & 0x000001))
      return NULL;
    bgav_input_skip(ctx, 4); /* Reserved (0) */
    tmp_32 = data_atom.start_position  + data_atom.size - ctx->position;
    ret = malloc(tmp_32+1);
    if(bgav_input_read_data(ctx, (uint8_t*)ret, tmp_32) < tmp_32)
      {
      free(ret);
      return NULL;
      }
    ret[tmp_32] = '\0';
    }
  return ret;
  }

static uint16_t read_trkn(qt_atom_header_t * h,
                          bgav_input_context_t * ctx, int have_ilst)
  {
  uint16_t ret;
  qt_atom_header_t data_atom;

  if(!bgav_qt_atom_read_header(ctx, &data_atom) ||
     (data_atom.fourcc != BGAV_MK_FOURCC('d','a','t','a')))
    return 0;

  bgav_input_skip(ctx, 10); /* Version, flags & 6 bytes unused */
  if(!bgav_input_read_16_be(ctx, &ret))
    return 0;
  bgav_input_skip(ctx, 2); /* 2 bytes unused */
  return ret;
  }

int bgav_qt_udta_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_udta_t * ret)
  {
  qt_atom_header_t ch;
  memcpy(&ret->h, h, sizeof(*h));


  while(input->position + 8 < h->start_position + h->size)
    {
    
    if(!bgav_qt_atom_read_header(input, &ch))
      return 0;

    /* iTunes Metadata are somewhat different */

    if(ch.fourcc == BGAV_MK_FOURCC('m','e','t','a'))
      {
      /* Skip version and flags */
      bgav_input_skip(input, 4);
      continue;
      }
    if(ch.fourcc == BGAV_MK_FOURCC('i','l','s','t'))
      {
      ret->have_ilst = 1;
      continue;
      }
    
    switch(ch.fourcc)
      {
      case BGAV_MK_FOURCC(0xa9, 'c','p','y'): /* Copyright                        */
        ret->cpy = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'd','a','y'): /* Date                             */
        ret->day = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'd','i','r'): /* Director                         */
        ret->dir = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'e','d','1'): /* 1st edit date and description    */
        ret->ed1 = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'e','d','2'): /* 2nd edit date and description    */
        ret->ed2 = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'e','d','3'): /* 3rd edit date and description    */
        ret->ed3 = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'e','d','4'): /* 4th edit date and description    */
        ret->ed4 = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'e','d','5'): /* 5th edit date and description    */
        ret->ed5 = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'e','d','6'): /* 6th edit date and description    */
        ret->ed6 = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'e','d','7'): /* 7th edit date and description    */
        ret->ed7 = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'e','d','8'): /* 8th edit date and description    */
        ret->ed8 = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'e','d','9'): /* 9th edit date and description    */
        ret->ed9 = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'f','m','t'): /* Format (Computer generated etc.) */  
        ret->fmt = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'i','n','f'): /* Information about the movie      */
        ret->inf = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'p','r','d'): /* Producer                         */
        ret->prd = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'p','r','f'): /* Performers                       */
        ret->prf = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'r','e','c'): /* Hard- software requirements      */
        ret->rec = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 's','r','c'): /* Credits for movie Source         */
        ret->src = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'w','r','t'): /* Writer                           */
        ret->wrt = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'n','a','m'): /* Name            */
        ret->nam = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'A','R','T'): /* Artist          */
        ret->ART = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'a','l','b'): /* Album           */
        ret->alb = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'e','n','c'): /* Encoded by      */
        ret->enc = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 't','r','k'): /* Track           */
        ret->trk = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'c','m','t'): /* Comment         */
        ret->cmt = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'a','u','t'): /* Author          */
        ret->aut = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'c','o','m'): /* Composer        */
        ret->com = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'd','e','s'): /* Description     */
        ret->des = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'd','i','s'): /* Disclaimer      */
        ret->dis = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'g','e','n'): /* Genre           */
        ret->gen = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'h','s','t'): /* Host computer   */
        ret->hst = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'm','a','k'): /* Make (??)       */
        ret->mak = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'm','o','d'): /* Model (??)      */
        ret->mod = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'o','p','e'): /* Original Artist */
        ret->ope = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'P','R','D'): /* Product         */
        ret->PRD = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 's','w','r'): /* Software        */
        ret->swr = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'w','r','n'): /* Warning         */
        ret->wrn = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC(0xa9, 'u','r','l'): /* URL link        */
        ret->url = read_string(&ch, input, ret->have_ilst);
        break;
      case BGAV_MK_FOURCC('t', 'r','k', 'n'): /* Track number     */
        ret->trkn = read_trkn(&ch, input, ret->have_ilst);
        break;
      default:
        bgav_qt_atom_skip_unknown(input, &ch, h->fourcc);
        break;
      }
    bgav_qt_atom_skip(input, &ch);
    }
  bgav_qt_atom_skip(input, h);
//  bgav_qt_udta_dump(ret);
  return 1;
  }

#define FREE(s) if(s){free(s);s=NULL;}

void bgav_qt_udta_free(qt_udta_t * udta)
  {
  FREE(udta->cpy); /* Copyright                        */
  FREE(udta->day); /* Date                             */
  FREE(udta->dir); /* Director                         */
  FREE(udta->ed1); /* 1st edit date and description    */
  FREE(udta->ed2); /* 2nd edit date and description    */
  FREE(udta->ed3); /* 3rd edit date and description    */
  FREE(udta->ed4); /* 4th edit date and description    */
  FREE(udta->ed5); /* 5th edit date and description    */
  FREE(udta->ed6); /* 6th edit date and description    */
  FREE(udta->ed7); /* 7th edit date and description    */
  FREE(udta->ed8); /* 8th edit date and description    */
  FREE(udta->ed9); /* 9th edit date and description    */
  FREE(udta->fmt); /* Format (Computer generated etc.) */  
  FREE(udta->inf); /* Information about the movie      */
  FREE(udta->prd); /* Producer                         */
  FREE(udta->prf); /* Performers                       */
  FREE(udta->rec); /* Hard- software requirements      */
  FREE(udta->src); /* Credits for movie Source         */
  FREE(udta->wrt); /* Writer                           */

  FREE(udta->nam); /* Name            */
  FREE(udta->ART); /* Artist          */
  FREE(udta->alb); /* Album           */
  FREE(udta->enc); /* Encoded by      */
  FREE(udta->trk); /* Track           */
  FREE(udta->cmt); /* Comment         */
  FREE(udta->aut); /* Author          */
  FREE(udta->com); /* Composer        */
  FREE(udta->des); /* Description     */
  FREE(udta->dis); /* Disclaimer      */
  FREE(udta->gen); /* Genre           */
  FREE(udta->hst); /* Host computer   */
  FREE(udta->mak); /* Make (??)       */
  FREE(udta->mod); /* Model (??)      */
  FREE(udta->ope); /* Original Artist */
  FREE(udta->PRD); /* Product         */
  FREE(udta->swr); /* Software        */
  FREE(udta->wrn); /* Warning         */
  FREE(udta->url); /* URL link        */

  
  }

#define PRINT(e) bgav_diprintf(indent+2, "%s: %s\n", #e, (udta->e ? udta->e : "(null)"));

void bgav_qt_udta_dump(int indent, qt_udta_t * udta)
  {
  bgav_diprintf(indent, "udta\n");
  PRINT(cpy); /* Copyright                        */
  PRINT(day); /* Date                             */
  PRINT(dir); /* Director                         */
  PRINT(ed1); /* 1st edit date and description    */
  PRINT(ed2); /* 2nd edit date and description    */
  PRINT(ed3); /* 3rd edit date and description    */
  PRINT(ed4); /* 4th edit date and description    */
  PRINT(ed5); /* 5th edit date and description    */
  PRINT(ed6); /* 6th edit date and description    */
  PRINT(ed7); /* 7th edit date and description    */
  PRINT(ed8); /* 8th edit date and description    */
  PRINT(ed9); /* 9th edit date and description    */
  PRINT(fmt); /* Format (Computer generated etc.) */  
  PRINT(inf); /* Information about the movie      */
  PRINT(prd); /* Producer                         */
  PRINT(prf); /* Performers                       */
  PRINT(rec); /* Hard- software requirements      */
  PRINT(src); /* Credits for movie Source         */
  PRINT(wrt); /* Writer                           */
  
  PRINT(nam); /* Name            */
  PRINT(ART); /* Artist          */
  PRINT(alb); /* Album           */
  PRINT(enc); /* Encoded by      */
  PRINT(trk); /* Track           */
  PRINT(cmt); /* Comment         */
  PRINT(aut); /* Author          */
  PRINT(com); /* Composer        */
  PRINT(des); /* Description     */
  PRINT(dis); /* Disclaimer      */
  PRINT(gen); /* Genre           */
  PRINT(hst); /* Host computer   */
  PRINT(mak); /* Make (??)       */
  PRINT(mod); /* Model (??)      */
  PRINT(ope); /* Original Artist */
  PRINT(PRD); /* Product         */
  PRINT(swr); /* Software        */
  PRINT(wrn); /* Warning         */
  PRINT(url); /* URL link        */
  bgav_diprintf(indent+2, "trkn: %d\n", udta->trkn);
  
  }
