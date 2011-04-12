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

#ifndef _ANDUTILS_H_
#define _ANDUTILS_H_

#include <jni.h>

#include "comtypes.h"
#include "comms.h"
#include "dictnry.h"

#include "andglobals.h"


/* callback for streams */
void and_send_on_close( XWStreamCtxt* stream, void* closure );
XWStreamCtxt* and_empty_stream( MPFORMAL AndGlobals* globals );

int getInt( JNIEnv* env, jobject obj, const char* name );
void setInt( JNIEnv* env, jobject obj, const char* name, int value );
bool getBool( JNIEnv* env, jobject obj, const char* name );
bool setBool( JNIEnv* env, jobject obj, const char* name, bool value );
bool setString( JNIEnv* env, jobject obj, const char* name, const XP_UCHAR* value );
void getString( JNIEnv* env, jobject jlp, const char* name, XP_UCHAR* buf,
                int bufLen );
XP_UCHAR* getStringCopy( MPFORMAL JNIEnv* env, jstring jname );
void setObject( JNIEnv* env, jobject obj, const char* name, const char* sig,
                jobject val );
bool getObject( JNIEnv* env, jobject obj, const char* name, const char* sig, 
                jobject* ret );

jintArray makeIntArray( JNIEnv *env, int size, const jint* vals );
int getIntFromArray( JNIEnv* env, jintArray arr, bool del );

jbyteArray makeByteArray( JNIEnv* env, int size, const jbyte* vals );

jbooleanArray makeBooleanArray( JNIEnv* env, int size, const jboolean* vals );
void setBoolArray( JNIEnv* env, jbooleanArray jarr, int count, 
                   const jboolean* vals );

jobjectArray makeStringArray( JNIEnv *env, int size, const XP_UCHAR** vals );
jstring streamToJString( MPFORMAL JNIEnv* env, XWStreamCtxt* stream );

/* Note: jmethodID can be cached.  Should not look up more than once. */
jmethodID getMethodID( JNIEnv* env, jobject obj, const char* proc,
                       const char* sig );

void setJAddrRec( JNIEnv* env, jobject jaddr, const CommsAddrRec* addr );
void getJAddrRec( JNIEnv* env, CommsAddrRec* addr, jobject jaddr );
jint jenumFieldToInt( JNIEnv* env, jobject jobj, const char* field, 
                      const char* fieldSig );
void intToJenumField( JNIEnv* env, jobject jobj, int val, const char* field, 
                      const char* fieldSig );
jobject intToJEnum( JNIEnv* env, int val, const char* enumSig );
jint jEnumToInt( JNIEnv* env, jobject jenum );
#endif
