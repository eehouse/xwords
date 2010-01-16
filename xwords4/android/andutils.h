

#ifndef _ANDUTILS_H_
#define _ANDUTILS_H_

#include <jni.h>

#include "comtypes.h"
#include "mempool.h"

XP_U32 and_ntohl(XP_U32 l);
XP_U16 and_ntohs(XP_U16 l);
XP_U32 and_htonl(XP_U32 l);
XP_U16 and_htons(XP_U16 l);

bool getInt( JNIEnv* env, jobject obj, const char* name, int* result );
bool setInt( JNIEnv* env, jobject obj, const char* name, int value );
bool getBool( JNIEnv* env, jobject obj, const char* name, XP_Bool* result );
bool setBool( JNIEnv* env, jobject obj, const char* name, bool value );
bool setString( JNIEnv* env, jobject obj, const char* name, const XP_UCHAR* value );
bool getString( JNIEnv* env, jobject jlp, const char* name, XP_UCHAR* buf,
                int bufLen );
bool getObject( JNIEnv* env, jobject obj, const char* name, const char* sig, 
                jobject* ret );

jintArray makeIntArray( JNIEnv *env, int size, const jint* vals );
int getIntFromArray( JNIEnv* env, jintArray arr, bool del );

jobjectArray makeStringArray( JNIEnv *env, int size, const XP_UCHAR** vals );
jstring streamToJString( MPFORMAL JNIEnv* env, XWStreamCtxt* stream );

/* Note: jmethodID can be cached.  Should not look up more than once. */
jmethodID getMethodID( JNIEnv* env, jobject obj, const char* proc,
                       const char* sig );

#endif
