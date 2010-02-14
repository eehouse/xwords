/* -*-mode: C; fill-column: 76; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001-2010 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

#ifndef _UTILWRAPPER_H_
#define _UTILWRAPPER_H_

#include <jni.h>

#include "game.h"
#include "util.h"
#include "andglobals.h"

XW_UtilCtxt* makeUtil( MPFORMAL JNIEnv** env, jobject j_util,
                       CurGameInfo* gi, AndGlobals* globals );
void destroyUtil( XW_UtilCtxt** util );

bool utilTimerFired( XW_UtilCtxt* util, XWTimerReason why, int handle );

XP_U32 and_util_getCurSeconds( XW_UtilCtxt* uc ); /* uc can be NULL */

#endif
