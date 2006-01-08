/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999-2002 by Eric House (xwords@eehouse.org).  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _FRANKDICT_H_
#define _FRANKDICT_H_

#include <ebm_object.h>

#ifdef CPLUS
extern "C" {
#endif

typedef enum {
    LOC_UNKNOWN,
    IN_RAM,
    ON_MMC
} FileLoc ;

#define FRANK_DICT_FLAGS_ERROR 0xFFFF

U16 GetDictFlags( ebo_enumerator_t* eboe, FileLoc loc );

DictionaryCtxt* frank_dictionary_make( MPFORMAL XP_UCHAR* name );

void debugf( char* format, ... );

#ifdef CPLUS
}
#endif

#endif /* _FRANKDICT_H_ */
