/*
 * Copyright © 2009-2010 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _ANDGLOBALS_H_
#define _ANDGLOBALS_H_

#include "vtabmgr.h"
#include "dictnry.h"
#include "game.h"

typedef struct _JNIState JNIState;

typedef struct _AndGameGlobals {
    VTableMgr* vtMgr;
    CurGameInfo* gi;
    DrawCtx* dctx;
    XW_UtilCtxt* util;
    struct JNIUtilCtxt* jniutil;
    TransportProcs* xportProcs;
    XW_DUtilCtxt* dutil;
    JNIState* state;
} AndGameGlobals;

typedef struct _EnvThreadInfo EnvThreadInfo;

JNIEnv* envForMe( EnvThreadInfo* ti, const char* caller );
#define ENVFORME( ti ) envForMe( (ti), __func__ )

#endif
