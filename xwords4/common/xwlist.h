/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2017 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _XWLIST_H_
#define _XWLIST_H_

#include "comtypes.h"
#include "mempool.h"

#include "xptypes.h"

typedef void* elem;
typedef struct XWList XWList;
typedef void (*destructor)(elem one, void* closure);

XWList* mk_list(MPFORMAL XP_U16 sizeHint);
void list_free(XWList* list, destructor proc, void* closure);

void list_append(XWList* list, elem one);
XP_U16 list_get_len(const XWList* list);
void list_remove_front(XWList* list, elem* here, XP_U16* count);
void list_remove_back(XWList* list, elem* here, XP_U16* count);

#ifdef DEBUG
void list_test_lists(MPFORMAL_NOCOMMA);
#endif

#endif
