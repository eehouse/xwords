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
void and_send_on_close( XWStreamCtxt* stream, XWEnv xwe, void* closure );
XWStreamCtxt* and_empty_stream( MPFORMAL AndGameGlobals* globals );

typedef struct _SetInfo {
    const char* name; 
    int offset; 
    int siz; 
} SetInfo;
#define ARR_MEMBER(obj, fld) { .name = #fld, \
            .offset = OFFSET_OF(obj, fld),   \
            .siz = sizeof(((obj *)0)->fld)   \
            }
jfieldID getFieldID( JNIEnv* env, jobject obj, const char* fieldName,
                     const char* fieldSig );
int getInt( JNIEnv* env, jobject obj, const char* name );
void setInt( JNIEnv* env, jobject obj, const char* name, int value );
void setInts( JNIEnv* env, jobject jobj, void* cobj, 
              const SetInfo* sis, XP_U16 nSis );
void getInts( JNIEnv* env, void* cobj, jobject jobj, 
              const SetInfo* sis, XP_U16 nSis );
bool getBool( JNIEnv* env, jobject obj, const char* name );
void getBools( JNIEnv* env, void* cobj, jobject jobj, 
               const SetInfo* sis, XP_U16 nSis );
bool setBool( JNIEnv* env, jobject obj, const char* name, bool value );
void setBools( JNIEnv* env, jobject jobj, void* cobj, 
               const SetInfo* sis, XP_U16 nSis );
bool setString( JNIEnv* env, jobject obj, const char* name, const XP_UCHAR* value );
void getString( JNIEnv* env, jobject jlp, const char* name, XP_UCHAR* buf,
                int bufLen );
void getStrings( JNIEnv* env, void* cobj, jobject jobj, 
                 const SetInfo* sis, XP_U16 nSis );
void setStrings( JNIEnv* env, jobject jobj, void* cobj, 
                 const SetInfo* sis, XP_U16 nSis );
XP_UCHAR* getStringCopy( MPFORMAL JNIEnv* env, jstring jname );
void setObjectField( JNIEnv* env, jobject container, const char* fieldName,
                     const char* fieldSig, jobject val );
jobject getObjectField( JNIEnv* env, jobject obj, const char* fieldName,
                        const char* fieldClassSig );
jintArray makeIntArray( JNIEnv* env, int size, const void* vals, size_t elemSize );
void setIntArray( JNIEnv* env, jobject jowner, const char* ownerField,
                  int count, const void* vals, size_t elemSize );
void getIntsFromArray( JNIEnv* env, int dest[], jintArray arr, int count, bool del );
void setIntInArray( JNIEnv* env, jintArray arr, int index, int val );

jbyteArray makeByteArray( JNIEnv* env, int size, const jbyte* vals );
jobjectArray makeByteArrayArray( JNIEnv* env, int siz );

jbooleanArray makeBooleanArray( JNIEnv* env, int size, const jboolean* vals );
void setBoolArray( JNIEnv* env, jbooleanArray jarr, int count, 
                   const jboolean* vals );

jobjectArray makeStringArray( JNIEnv* env, int size, const XP_UCHAR** vals );
void setStringArray( JNIEnv* env, jobject jowner, const char* ownerField,
                     int count, const XP_UCHAR** vals );

jstring streamToJString( JNIEnv* env, XWStreamCtxt* stream );
jbyteArray streamToBArray( JNIEnv* env, XWStreamCtxt* stream );

/* Note: jmethodID can be cached.  Should not look up more than once. */
jmethodID getMethodID( JNIEnv* env, jobject obj, const char* proc,
                       const char* sig );

jobject makeObject( JNIEnv* env, const char* className, const char* initSig, ... );
jobject makeObjectEmptyConst( JNIEnv* env, const char* className );

jobject makeJAddr( JNIEnv* env, const CommsAddrRec* addr );
jobject setJAddrRec( JNIEnv* env, jobject jaddr, const CommsAddrRec* addr );
void getJAddrRec( JNIEnv* env, CommsAddrRec* addr, jobject jaddr );
void setTypeSetFieldIn( JNIEnv* env, const CommsAddrRec* addr, jobject jTarget, 
                        const char* fldName );
jobject addrTypesToJ( JNIEnv* env, const CommsAddrRec* addr );
jint jenumFieldToInt( JNIEnv* env, jobject jobj, const char* field, 
                      const char* fieldSig );
void intToJenumField( JNIEnv* env, jobject jobj, int val, const char* field, 
                      const char* fieldSig );
jobject intToJEnum( JNIEnv* env, int val, const char* enumSig );
jint jEnumToInt( JNIEnv* env, jobject jenum );

#define AANDS(a) (a), VSIZE(a)
void loadNLI( JNIEnv* env, NetLaunchInfo* nli, jobject jnli );
void setNLI( JNIEnv* env, jobject jnli, const NetLaunchInfo* nli );

XP_U32 getCurSeconds( JNIEnv* env );

void deleteLocalRef( JNIEnv* env, jobject jobj );
void deleteLocalRefs( JNIEnv* env, ... );

JNIEnv* waitEnvFromGlobals();
void releaseEnvFromGlobals( JNIEnv* env );

void raw_log( const char* func, const char* fmt, ... );
#ifdef DEBUG
# define RAW_LOG(...) raw_log(  __func__, __VA_ARGS__ )
#else
# define RAW_LOG(...)
#endif

# define DELETE_NO_REF ((jobject)-1)    /* terminates above varargs list */
#endif
