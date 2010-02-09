

#ifndef _ANDDICT_H_
#define _ANDDICT_H_

#include "dictnry.h"
#include "comtypes.h"

void
dict_splitFaces( DictionaryCtxt* dict, const XP_U8* bytes,
                 XP_U16 nBytes, XP_U16 nFaces );

DictionaryCtxt* makeDict( MPFORMAL JNIEnv *env, XW_UtilCtxt* util, 
                          jbyteArray bytes );

DictionaryCtxt* and_dictionary_make_empty( MPFORMAL_NOCOMMA );

#endif
