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
#include "dutil.h"
#include "andglobals.h"
#include "jniutlswrapper.h"

XW_DUtilCtxt* makeDUtil( MPFORMAL JNIEnv* env,
#ifdef MAP_THREAD_TO_ENV
                         EnvThreadInfo* ti,
#endif
                         jobject j_dutil, VTableMgr* vtMgr,
                         JNIUtilCtxt* jniutil, void* closure );
void destroyDUtil( XW_DUtilCtxt** dutilp, JNIEnv* env );

XW_UtilCtxt* makeUtil( MPFORMAL JNIEnv* env, jobject jutil,
                       const CurGameInfo* gi, XW_DUtilCtxt* dutil, GameRef gr );

#endif
