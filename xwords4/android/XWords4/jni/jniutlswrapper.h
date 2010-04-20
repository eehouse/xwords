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

#ifndef _JNIUTLSWRAPPER_H_
#define _JNIUTLSWRAPPER_H_

#include <jni.h>

#include "util.h"
#include "andglobals.h"

typedef struct JNIUtilCtxt JNIUtilCtxt;

JNIUtilCtxt* makeJNIUtil( MPFORMAL JNIEnv** env, jobject jniutls );
void destroyJNIUtil( JNIUtilCtxt** jniu );

jobject and_util_makeJBitmap( JNIUtilCtxt* jniu, int nCols, int nRows, 
                              const jboolean* colors );
jobject and_util_splitFaces( JNIUtilCtxt* jniu, const XP_U8* bytes, int len,
                             XP_Bool isUTF8 );

#endif
