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

#ifndef _ANDDICT_H_
#define _ANDDICT_H_

#include "dictnry.h"
#include "comtypes.h"
#include "jniutlswrapper.h"

void
dict_splitFaces( DictionaryCtxt* dict, const XP_U8* bytes,
                 XP_U16 nBytes, XP_U16 nFaces );

DictionaryCtxt* makeDict( MPFORMAL JNIEnv *env, JNIUtilCtxt* jniutil, 
                          jbyteArray bytes, jstring jname );

DictionaryCtxt* and_dictionary_make_empty( MPFORMAL JNIEnv *env,
                                           JNIUtilCtxt* jniutil );

jobject and_dictionary_getChars( JNIEnv* env, DictionaryCtxt* dict );

#endif
