

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
bool getObject( JNIEnv* env, jobject obj, const char* name, const char* sig, 
                jobject* ret );

jintArray makeIntArray( JNIEnv *env, int size, const jint* vals );
int getIntFromArray( JNIEnv* env, jintArray arr, bool del );

jbyteArray makeByteArray( JNIEnv* env, int size, const jbyte* vals );

jbooleanArray makeBooleanArray( JNIEnv* env, int size, const jboolean* vals );

jobjectArray makeStringArray( JNIEnv *env, int size, const XP_UCHAR** vals );
jstring streamToJString( MPFORMAL JNIEnv* env, XWStreamCtxt* stream );

jobjectArray makeBitmapsArray( JNIEnv* env, const XP_Bitmaps* bitmaps );

/* Note: jmethodID can be cached.  Should not look up more than once. */
jmethodID getMethodID( JNIEnv* env, jobject obj, const char* proc,
                       const char* sig );

void setJAddrRec( JNIEnv* env, jobject jaddr, const CommsAddrRec* addr );
void getJAddrRec( JNIEnv* env, CommsAddrRec* addr, jobject jaddr );
jint jenumFieldToInt( JNIEnv* env, jobject j_gi, const char* field, 
                      const char* fieldSig );
void intToJenumField( JNIEnv* env, jobject j_gi, int val, const char* field, 
                      const char* fieldSig );
jobject intToJEnum( JNIEnv* env, int val, const char* enumSig );
#endif
