// as copied from https://github.com/Zunawe/md5-c

#ifndef MD5_H
#define MD5_H

#ifdef NO_NATIVE_MD5SDUM

#include <stdint.h>
#include <stdlib.h>

typedef struct _MD5Result {
    char output[33];
} MD5Result;

void calcMD5Sum( MD5Result* out, uint8_t* data, size_t len );
#endif

#endif
