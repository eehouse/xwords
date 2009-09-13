/* -*- fill-column: 77; compile-command: "make TARGET_OS=wince DEBUG=TRUE" -*- */
/* 
 * Copyright 1997-2008 by Eric House (xwords@eehouse.org).   All rights reserved.
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

#ifndef STUBBED_DICT

#include <stdio.h>              /* _snwprintf */
#include <string.h>              /* _snwprintf */

#include "stdafx.h" 
/* #include <accctrl.h> */
#include <commdlg.h>
#include "dictnryp.h"
#include "strutils.h"
#include "cedict.h"
#include "debhacks.h"
#include "cedebug.h"
#include "ceutil.h"

typedef struct CEDictionaryCtxt {
    DictionaryCtxt super;
    HANDLE mappedFile;
    void* mappedBase;
} CEDictionaryCtxt;

static void ce_dict_destroy( DictionaryCtxt* dict );
static const XP_UCHAR* ce_dict_getShortName( const DictionaryCtxt* dict );
static void ceLoadSpecialData( CEDictionaryCtxt* ctxt, XP_U8** ptrp );
static XP_U16 ceCountSpecials( CEDictionaryCtxt* ctxt );
static XP_Bitmap ceMakeBitmap( CEDictionaryCtxt* ctxt, XP_U8** ptrp );

static XP_U32 n_ptr_tohl( XP_U8** in );
static XP_U16 n_ptr_tohs( XP_U8** in );
static XP_U8* openMappedFile( MPFORMAL const wchar_t* name, 
                              HANDLE* mappedFileP, HANDLE* hFileP, 
                              XP_U32* sizep );
static void closeMappedFile( MPFORMAL XP_U8* base, HANDLE mappedFile );
static XP_Bool checkIfDictAndLegal( MPFORMAL wchar_t* path, XP_U16 pathLen, 
                                    wchar_t* name );
static XP_Bool findAlternateDict( CEAppGlobals* globals, wchar_t* dictName );

#define ALIGN_COUNT 2

DictionaryCtxt*
ce_dictionary_make( CEAppGlobals* globals, const char* dictName )
{
    CEDictionaryCtxt* ctxt = (CEDictionaryCtxt*)NULL;
	HANDLE mappedFile = NULL;

    wchar_t nameBuf[MAX_PATH+1];
    HANDLE hFile;
    XP_U8* ptr = NULL;
    XP_U32 dictLength;
    XP_UCHAR buf[CE_MAX_PATH_LEN+1]; /* in case we have to look */
    UINT codePages[] = { CP_ACP, CP_UTF8 };
    XP_U16 ii;

    XP_ASSERT( !!dictName );

    /* Try both old-iso and UTF-8 to convert file name */
    for ( ii = 0; ii < VSIZE(codePages); ++ii ) {
        if ( 0 != MultiByteToWideChar( codePages[ii], 0, dictName, -1,
                                       nameBuf, VSIZE(nameBuf) ) ) {
            ptr = openMappedFile( MPPARM(globals->mpool) nameBuf, &mappedFile, 
                                  &hFile, &dictLength );
            break;
        }
    }
    
    if ( !ptr ) {
        if ( findAlternateDict( globals, nameBuf ) ) {
            (void)WideCharToMultiByte( CP_ACP, 0, nameBuf, -1,
                                       buf, sizeof(buf), NULL, NULL );
            ptr = openMappedFile( MPPARM(globals->mpool) nameBuf, &mappedFile, 
                                  &hFile, &dictLength );
            if ( !!ptr ) {
                dictName = buf;
            }
        }
    }

    while( !!ptr ) {           /* lets us break.... */
        XP_U32 offset;
        XP_U16 numFaces, numFaceBytes = 0;
        XP_U16 i;
        XP_U16 flags;
        void* mappedBase = (void*)ptr;
        XP_U8 nodeSize;
        XP_Bool isUTF8 = XP_FALSE;

        flags = n_ptr_tohs( &ptr );

#ifdef NODE_CAN_4
        if ( flags == 0x0002 ) {
            nodeSize = 3;
        } else if ( flags == 0x0003 ) {
            nodeSize = 4;
        } else if ( flags == 0x0004 ) {
            isUTF8 = XP_TRUE;
            nodeSize = 3;
        } else if ( flags == 0x0005 ) {
            isUTF8 = XP_TRUE;
            nodeSize = 4;
        } else {
            break;          /* we want to return NULL */
        }
#else
        if( flags != 0x0001 ) {
            break;
        }
#endif
        if ( isUTF8 ) {
            numFaceBytes = (XP_U16)(*ptr++);
        }
        numFaces = (XP_U16)(*ptr++);
        if ( numFaces > 64 ) {
            break;
        }

        ctxt = (CEDictionaryCtxt*)ce_dictionary_make_empty( globals );

        ctxt->mappedFile = mappedFile;
        ctxt->mappedBase = mappedBase;
        ctxt->super.nodeSize = nodeSize;
        ctxt->super.destructor = ce_dict_destroy;
        ctxt->super.func_dict_getShortName = ce_dict_getShortName;

        if ( !isUTF8 ) {
            numFaceBytes = numFaces * 2;
        }

        ctxt->super.nFaces = (XP_U8)numFaces;
        ctxt->super.isUTF8 = isUTF8;

        if ( isUTF8 ) {
            dict_splitFaces( &ctxt->super, ptr, numFaceBytes, numFaces );
            ptr += numFaceBytes;
        } else {
            XP_U8 tmp[numFaces*4]; /* should be enough... */
            XP_U16 nBytes = 0;
            XP_U16 ii;
            /* Need to translate from iso-8859-n to utf8 */
            for ( ii = 0; ii < numFaces; ++ii ) {
                XP_UCHAR ch = ptr[1];
                wchar_t wch;

                ptr += 2;
                MultiByteToWideChar( CP_ACP, 0, &ch, 1, &wch, 1 );
                XP_UCHAR utfs[8];
                int len = WideCharToMultiByte( CP_UTF8, 0, &wch, 1,
                                               utfs, VSIZE(utfs), NULL, NULL );
                XP_MEMCPY( &tmp[nBytes], utfs, len );
                nBytes += len;
            }
            dict_splitFaces( &ctxt->super, tmp, nBytes, numFaces );
        }

        ctxt->super.is_4_byte = (ctxt->super.nodeSize == 4);

        ctxt->super.countsAndValues = 
            (XP_U8*)XP_MALLOC(globals->mpool, numFaces*2);

        ptr += 2;		/* skip xloc header */
        for ( i = 0; i < numFaces*2; i += 2 ) {
            ctxt->super.countsAndValues[i] = *ptr++;
            ctxt->super.countsAndValues[i+1] = *ptr++;
        }

        ceLoadSpecialData( ctxt, &ptr );

        dictLength -= ptr - (XP_U8*)ctxt->mappedBase;
        if ( dictLength > sizeof(XP_U32) ) {
            offset = n_ptr_tohl( &ptr );
            dictLength -= sizeof(offset);
#ifdef NODE_CAN_4
            XP_ASSERT( dictLength % ctxt->super.nodeSize == 0 );
# ifdef DEBUG
            ctxt->super.numEdges = dictLength / ctxt->super.nodeSize;
# endif
#else
            XP_ASSERT( dictLength % 3 == 0 );
# ifdef DEBUG
            ctxt->super.numEdges = dictLength / 3;
# endif
#endif
        } else {
            offset = 0;
        }

        if ( dictLength > 0 ) {
            ctxt->super.base = (array_edge*)ptr;
#ifdef NODE_CAN_4
            ctxt->super.topEdge = ctxt->super.base 
                + (offset * ctxt->super.nodeSize);
#else
            ctxt->super.topEdge = ctxt->super.base + (offset * 3);
#endif
        } else {
            ctxt->super.topEdge = (array_edge*)NULL;
            ctxt->super.base = (array_edge*)NULL;
        }

        setBlankTile( &ctxt->super );

        ctxt->super.name = copyString(globals->mpool, dictName);
        break;              /* exit phony while loop */
    }
    return (DictionaryCtxt*)ctxt;
} /* ce_dictionary_make */

DictionaryCtxt*
ce_dictionary_make_empty( CEAppGlobals* XP_UNUSED_DBG(globals) )
{
    CEDictionaryCtxt* ctxt = (CEDictionaryCtxt*)XP_MALLOC(globals->mpool,
                                                          sizeof(*ctxt));
    XP_MEMSET( ctxt, 0, sizeof(*ctxt) );

    dict_super_init( (DictionaryCtxt*)ctxt );
    MPASSIGN( ctxt->super.mpool, globals->mpool );
    return (DictionaryCtxt*)ctxt;
} /* ce_dictionary_make_empty */

void
dict_splitFaces( DictionaryCtxt* dict, const XP_U8* utf8, 
                 XP_U16 nBytes, XP_U16 nFaces )
{
    XP_UCHAR* faces = XP_MALLOC( dict->mpool, nBytes + nFaces );
    XP_UCHAR** ptrs = XP_MALLOC( dict->mpool, nFaces * sizeof(ptrs[0]));
    XP_U16 ii;
    XP_UCHAR* next = faces;

    wchar_t widebuf[nFaces];
    int nRead = MultiByteToWideChar( CP_UTF8, 0, (char*)utf8, nBytes,
                                     widebuf, VSIZE(widebuf) );
    if ( 0 == nRead ) {
        logLastError( "MultiByteToWideChar" );
    }
    XP_ASSERT( nRead == nFaces );

    /* now split */
    for ( ii = 0; ii < nFaces; ++ii ) {
        ptrs[ii] = next;
        int nWritten = WideCharToMultiByte( CP_UTF8, 0, &widebuf[ii], 1,
                                            next, 100, NULL, NULL );
        next += nWritten;
        *next++ = 0;
    }

    XP_ASSERT( next == faces + nFaces + nBytes );
    XP_ASSERT( !dict->faces );
    dict->faces = faces;
    XP_ASSERT( !dict->facePtrs );
    dict->facePtrs = ptrs;
} /* dict_splitFaces */

static void
ceLoadSpecialData( CEDictionaryCtxt* ctxt, XP_U8** ptrp )
{
    XP_U16 nSpecials = ceCountSpecials( ctxt );
    XP_U8* ptr = *ptrp;
    Tile ii;
    XP_UCHAR** texts;
    SpecialBitmaps* bitmaps;

    texts = (XP_UCHAR**)XP_MALLOC( ctxt->super.mpool, 
                                   nSpecials * sizeof(*texts) );
    bitmaps = (SpecialBitmaps*)
        XP_MALLOC( ctxt->super.mpool, nSpecials * sizeof(*bitmaps) );

    for ( ii = 0; ii < ctxt->super.nFaces; ++ii ) {
	
        const XP_UCHAR* facep = ctxt->super.facePtrs[(short)ii];
        if ( IS_SPECIAL(*facep) ) {
            /* get the string */
            XP_U8 txtlen = *ptr++;
            XP_UCHAR* text = (XP_UCHAR*)XP_MALLOC(ctxt->super.mpool, txtlen+1);
            XP_MEMCPY( text, ptr, txtlen );
            ptr += txtlen;
            text[txtlen] = '\0';
            XP_ASSERT( *facep < nSpecials );
            texts[(int)*facep] = text;

            bitmaps[(int)*facep].largeBM = ceMakeBitmap( ctxt, &ptr );
            bitmaps[(int)*facep].smallBM = ceMakeBitmap( ctxt, &ptr );
        }
    }

    ctxt->super.chars = texts;
    ctxt->super.bitmaps = bitmaps;

    *ptrp = ptr;
} /* ceLoadSpecialData */

static XP_U16
ceCountSpecials( CEDictionaryCtxt* ctxt )
{
    XP_U16 result = 0;
    XP_U16 ii;

    for ( ii = 0; ii < ctxt->super.nFaces; ++ii ) {
        if ( IS_SPECIAL( ctxt->super.facePtrs[ii][0] ) ) {
            ++result;
        }
    }

    return result;
} /* ceCountSpecials */

#if 0
static void
printBitmapData1( XP_U16 nCols, XP_U16 nRows, XP_U8* data )
{
    char strs[20];
    XP_U16 rowBytes;
    XP_U16 row, col;

    rowBytes = (nCols + 7) / 8;
    while ( (rowBytes % 2) != 0 ) {
        ++rowBytes;
    }

    for ( row = 0; row < nRows; ++row ) {
        for ( col = 0; col < nCols; ++col ) {
            XP_UCHAR byt = data[col / 8];
            XP_Bool set = (byt & (0x80 >> (col % 8))) != 0;

            strs[col] = set? '#': '.';
        }
        data += rowBytes;

        strs[nCols] = '\0';
        XP_DEBUGF( strs );
    }
} /* printBitmapData1 */

static void
printBitmapData2( XP_U16 nCols, XP_U16 nRows, XP_U8* data )
{
    while ( nRows-- ) {
        XP_UCHAR buf[100];
        XP_UCHAR* ptr = buf;
        XP_U16 rowBytes = (nCols + 7) / 8;
        while ( (rowBytes % ALIGN_COUNT) != 0 ) {
            ++rowBytes;
        }

        while ( rowBytes-- ) {
            ptr += XP_SNPRINTF( ptr, sizeof(buf), "0x%.2x ", *data++ );
        }
        XP_DEBUGF( buf );
    }
} /* printBitmapData2 */

static void
longSwapData( XP_U8* destBase, XP_U16 nRows, XP_U16 rowBytes )
{
    XP_U32* longBase = (XP_U32*)destBase;
    rowBytes /= 4;

    while ( nRows-- ) {
        XP_U16 i;
        for ( i = 0; i < rowBytes; ++i ) {
            XP_U32 n = *longBase;
            XP_U32 tmp = 0;
            tmp |= (n >> 24) & 0x000000FF;
            tmp |= (n >> 16) & 0x0000FF00;
            tmp |= (n >> 8 ) & 0x00FF0000;
            tmp |= (n >> 0 ) & 0xFF000000;
            *longBase = tmp;

            ++longBase;
        }
    }
} /* longSwapData */
#endif

static XP_Bitmap
ceMakeBitmap( CEDictionaryCtxt* XP_UNUSED_DBG(ctxt), XP_U8** ptrp )
{
    XP_U8* ptr = *ptrp;
    XP_U8 nCols = *ptr++;
    CEBitmapInfo* bitmap = (CEBitmapInfo*)NULL;

    if ( nCols > 0 ) {
        XP_U8* dest;
        XP_U8* savedDest;
        XP_U8 nRows = *ptr++;
        XP_U16 rowBytes = (nCols+7) / 8;
        XP_U8 srcByte = 0;
        XP_U8 destByte = 0;
        XP_U8 nBits;
        XP_U16 i;

        bitmap = (CEBitmapInfo*)XP_MALLOC( ctxt->super.mpool, 
                                           sizeof(bitmap) );
        bitmap->nCols = nCols;
        bitmap->nRows = nRows;
        dest = XP_MALLOC( ctxt->super.mpool, rowBytes * nRows );
        bitmap->bits = savedDest = dest;

        nBits = nRows * nCols;
        for ( i = 0; i < nBits; ++i ) {
            XP_U8 srcBitIndex = i % 8;
            XP_U8 destBitIndex = (i % nCols) % 8;
            XP_U8 srcMask, bit;

            if ( srcBitIndex == 0 ) {
                srcByte = *ptr++;
            }

            srcMask = 1 << (7 - srcBitIndex);
            bit = (srcByte & srcMask) != 0;
            destByte |= bit << (7 - destBitIndex);

            /* we need to put the byte if we've filled it or if we're done
               with the row */
            if ( (destBitIndex==7) || ((i%nCols) == (nCols-1)) ) {
                *dest++ = destByte;
                destByte = 0;
            }
        }

/*         printBitmapData1( nCols, nRows, savedDest ); */
/*         printBitmapData2( nCols, nRows, savedDest ); */
    }

    *ptrp = ptr;
    return (XP_Bitmap)bitmap;
} /* ceMakeBitmap */

static void
ceDeleteBitmap( const CEDictionaryCtxt* XP_UNUSED_DBG(ctxt),
                XP_Bitmap* bitmap )
{
    if ( !!bitmap ) {
        CEBitmapInfo* bmi = (CEBitmapInfo*)bitmap;
        XP_FREE( ctxt->super.mpool, bmi->bits );
        XP_FREE( ctxt->super.mpool, bmi );
    }
}

static void
ce_dict_destroy( DictionaryCtxt* dict )
{
    CEDictionaryCtxt* ctxt = (CEDictionaryCtxt*)dict;
    XP_U16 nSpecials = ceCountSpecials( ctxt );
    XP_U16 i;

    if ( !!ctxt->super.chars ) {
        for ( i = 0; i < nSpecials; ++i ) {
            XP_UCHAR* text = ctxt->super.chars[i];
            if ( !!text ) {
                XP_FREE( ctxt->super.mpool, text );
            }
        }
        XP_FREE( ctxt->super.mpool, ctxt->super.chars );
    }
    if ( !!ctxt->super.bitmaps ) {
        for ( i = 0; i < nSpecials; ++i ) {
            ceDeleteBitmap( ctxt, ctxt->super.bitmaps[i].largeBM );
            ceDeleteBitmap( ctxt, ctxt->super.bitmaps[i].smallBM );
        }
        XP_FREE( ctxt->super.mpool, ctxt->super.bitmaps );
    }

    XP_FREE( ctxt->super.mpool, ctxt->super.faces );
    XP_FREE( ctxt->super.mpool, ctxt->super.facePtrs );
    XP_FREE( ctxt->super.mpool, ctxt->super.countsAndValues );
    XP_FREE( ctxt->super.mpool, ctxt->super.name );

    closeMappedFile( MPPARM(ctxt->super.mpool) ctxt->mappedBase, 
                     ctxt->mappedFile );
    XP_FREE( ctxt->super.mpool, ctxt );
} // ce_dict_destroy

static const XP_UCHAR* 
ce_dict_getShortName( const DictionaryCtxt* dict )
{
    const XP_UCHAR* name = dict_getName( dict );
    return bname( name );
} /* ce_dict_getShortName */

static XP_U8*
openMappedFile( MPFORMAL const wchar_t* name, HANDLE* mappedFileP, 
                HANDLE* hFileP, XP_U32* sizep )
{
    XP_U8* ptr = NULL;
    HANDLE hFile;

#if defined _WIN32_WCE
    hFile = CreateFileForMapping( name,
                                  GENERIC_READ,
                                  FILE_SHARE_READ, /* (was 0: no sharing) */
                                  NULL, /* security */
                                  OPEN_EXISTING,
                                  FILE_FLAG_RANDOM_ACCESS,
                                  NULL );

    if ( hFile == INVALID_HANDLE_VALUE ) {
        logLastError( "CreateFileForMapping" );
    } else {
        HANDLE mappedFile;

        mappedFile = CreateFileMapping( hFile,
                                        NULL,
                                        PAGE_READONLY,
                                        0,
                                        0,
                                        NULL );


        if ( mappedFile != INVALID_HANDLE_VALUE ) {
            void* mappedBase = MapViewOfFile( mappedFile, 
                                              FILE_MAP_READ, 
                                              0, 0, 0 );
            ptr = (XP_U8*)mappedBase;
            *mappedFileP = mappedFile;
            *hFileP = hFile;
            if ( sizep != NULL ) {
                *sizep = GetFileSize( hFile, NULL );
            }
        }
    }
#else
    hFile = CreateFile( name, 
                        GENERIC_READ, 
                        FILE_SHARE_READ,
                        NULL,   /* security */
                        OPEN_EXISTING,
                        FILE_FLAG_RANDOM_ACCESS,
                        NULL );
    if ( hFile != INVALID_HANDLE_VALUE ) {

        DWORD size = GetFileSize( hFile, NULL );

        ptr = XP_MALLOC( mpool, size );
        if ( ptr != NULL ) {
            DWORD nRead;
            if ( ReadFile( hFile, ptr, size, &nRead, NULL ) ) {
                XP_ASSERT( nRead == size );
            } else {
                XP_FREE( mpool, ptr );
                ptr = NULL;
            }
        }

        CloseHandle( hFile );

        *hFileP = NULL;             /* nothing to close later */
        if ( sizep != NULL ) {
            *sizep = size;
        }
        *mappedFileP = (HANDLE)ptr;
    }
#endif
    return ptr;
} /* openMappedFile */

static void
closeMappedFile( MPFORMAL XP_U8* base, 
                 HANDLE XP_UNUSED_32(mappedFile) )
{
#ifdef _WIN32_WCE
    UnmapViewOfFile( base );
    CloseHandle( mappedFile );
#else
    XP_FREE( mpool, base );
#endif
}

static XP_Bool
checkIfDictAndLegal( MPFORMAL wchar_t* path, XP_U16 pathLen, 
                     wchar_t* name )
{
    XP_Bool result = XP_FALSE;
    XP_U16 len;

    len = wcslen(name);

    /* are the last four bytes ".xwd"? */
    if ( 0 == lstrcmp( name + len - 4, L".xwd" ) ) {
        XP_U16 flags;
        HANDLE mappedFile, hFile;
        XP_U8* base;
        wchar_t pathBuf[CE_MAX_PATH_LEN+1];

        wcscpy( pathBuf, path );
        pathBuf[pathLen] = 0;
        wcscat( pathBuf, name );

#ifdef DEBUG
        {
            char narrowName[CE_MAX_PATH_LEN+1];
            int len = wcslen( pathBuf );
            len = WideCharToMultiByte( CP_UTF8, 0, pathBuf, len + 1,
                                       narrowName, len + 1, NULL, NULL );
        }
#endif

        base = openMappedFile( MPPARM(mpool) pathBuf, &mappedFile, 
                               &hFile, NULL );
        if ( !!base ) {
            XP_U8* ptr = base;
        
            flags = n_ptr_tohs( &ptr );
            closeMappedFile( MPPARM(mpool) base, mappedFile );
#ifdef NODE_CAN_4
            /* are the flags what we expect */
            result = flags >= 0x0002 && flags <= 0x0005;
#else
            result = flags == 0x0001;
#endif
        }
    }

    return result;
} /* checkIfDictAndLegal */

static XP_Bool
locateOneDir( MPFORMAL wchar_t* path, OnePathCB cb, void* ctxt, XP_U16 nSought,
              XP_U16* nFoundP )
{
    WIN32_FIND_DATA data;
    HANDLE fileH;
    XP_Bool done = XP_FALSE;
    XP_U16 startLen;

    lstrcat( path, L"\\" );
    startLen = wcslen(path);    /* record where we were so can back up */
    lstrcat( path, L"*" );

    XP_MEMSET( &data, 0, sizeof(data) );

    /* Looks like I need to look at every file.  If it's a directory I search
       it recursively.  If it's an .xwd file I check whether it's got the
       right flags and if so I return its name. */

    fileH = FindFirstFile( path, &data );

    if ( fileH != INVALID_HANDLE_VALUE ) {
        for ( ; ; ) {

            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0){

                if ( ( data.cFileName[0] == '.' ) 
                     && ( (data.cFileName[1] == '.') 
                          || (data.cFileName[1] == '\0' ) ) ) {
                    /* skip . and .. */
                } else {
                    lstrcpy( path+startLen, data.cFileName );
                    done = locateOneDir( MPPARM(mpool) path, cb, ctxt, 
                                         nSought, nFoundP );
                    XP_ASSERT( done || *nFoundP < nSought );
                    if ( done ) {
                        break;
                    }
                }
            } else if ( checkIfDictAndLegal( MPPARM(mpool) path, startLen,
                                             data.cFileName ) ) {
                XP_ASSERT( *nFoundP < nSought );

                lstrcpy( path+startLen, data.cFileName );
                done = (*cb)( path, (*nFoundP)++, ctxt )
                    || *nFoundP == nSought;
                if ( done ) {
                    break;
                }
            }

            if ( !FindNextFile( fileH, &data ) ) {
                XP_ASSERT( GetLastError() == ERROR_NO_MORE_FILES );
                break;
            }
            path[startLen] = 0;
        }

        (void)FindClose( fileH );
    }
    return done;
} /* locateOneDir */

XP_U16
ceLocateNDicts( CEAppGlobals* globals,  XP_U16 nSought, OnePathCB cb, 
                void* ctxt )
{
    XP_U16 nFound = 0;
    wchar_t path[CE_MAX_PATH_LEN+1];

    if ( ceGetExeDir( path, VSIZE(path) ) ) {
        locateOneDir( MPPARM(globals->mpool) path, cb, ctxt, nSought, &nFound );
    }

    if ( nFound < nSought ) {
        ceGetPath( globals, PROGFILES_PATH, path, VSIZE(path) );
        locateOneDir( MPPARM(globals->mpool) path, cb, ctxt, nSought, &nFound );
    }

    if ( nFound < nSought ) {
        WIN32_FIND_DATA data;
        HANDLE fileH;

        XP_MEMSET( &data, 0, sizeof(data) );

        fileH = FindFirstFile( L"\\*", &data );
        while ( fileH != INVALID_HANDLE_VALUE ) {
            if ( ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                 && (((data.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY) != 0) ) ) { 
                wsprintf( path, L"\\%s\\Crosswords", data.cFileName );

                XP_LOGW( "looking in:", path );
                locateOneDir( MPPARM(globals->mpool) path, cb, ctxt, 
                              nSought, &nFound );
            }
            if ( nFound >= nSought ) {
                break;
            }

            if ( !FindNextFile( fileH, &data ) ) {
                break;
            }
        }
    }

    return nFound;
} /* ceLocateNDicts */

typedef struct FindOneData {
    wchar_t* result;
    const wchar_t* sought;
    XP_Bool found;
} FindOneData;

static XP_Bool 
matchShortName( const wchar_t* wPath, XP_U16 XP_UNUSED(index), void* ctxt )
{
    FindOneData* datap = (FindOneData*)ctxt;
    wchar_t buf[CE_MAX_PATH_LEN+1];
    wchar_t* name;

    XP_ASSERT( !datap->found );

    name = wbname( buf, VSIZE(buf), wPath );
    if ( 0 == wcscmp( name, datap->sought ) ) {
        wcscpy( datap->result, wPath );
        datap->found = XP_TRUE;
    }
    return datap->found;
} /* matchShortName */

/* Users sometimes move dicts.  Given a path to a dict that doesn't exist, See
 * if another with the same short name exists somewhere else we're willing to
 * look.
 */
static XP_Bool
findAlternateDict( CEAppGlobals* globals, wchar_t* path )
{
    wchar_t shortPath[CE_MAX_PATH_LEN+1];
    FindOneData data;

    XP_MEMSET( &data, 0, sizeof(data) );
    data.sought = wbname( shortPath, VSIZE(shortPath), path );
    data.result = path;

    (void)ceLocateNDicts( globals, CE_MAXDICTS, matchShortName, 
                          &data );
    return data.found;
} /* findAlternateDict */

static XP_U32
n_ptr_tohl( XP_U8** inp )
{
    XP_U32 t;
    XP_MEMCPY( &t, *inp, sizeof(t) );

    *inp += sizeof(t);

    return XP_NTOHL(t);
} /* n_ptr_tohl */

static XP_U16
n_ptr_tohs( XP_U8** inp )
{
    XP_U16 t;
    XP_MEMCPY( &t, *inp, sizeof(t) );

    *inp += sizeof(t);

    return XP_NTOHS(t);
} /* n_ptr_tohs */

const XP_UCHAR*
bname( const XP_UCHAR* in )
{
    XP_U16 len = (XP_U16)XP_STRLEN(in);
    const XP_UCHAR* out = in + len - 1;

    while ( *out != '\\' && out >= in ) {
        --out;
    }
    return out + 1;
} /* bname */

wchar_t*
wbname( wchar_t* buf, XP_U16 buflen, const wchar_t* in )
{
    const wchar_t* slash = wcsrchr( in, L'\\' );
    if ( !!slash ) {
        ++slash;
    } else {
        slash = in;
    }

    const wchar_t* dot = wcsrchr( slash, L'.' );
    if ( !dot ) {
        dot = slash + wcslen( slash );
    }

    _snwprintf( buf, buflen, L"%s", slash );
    XP_ASSERT( dot >= slash );
    buf[dot-slash] = 0;
    return buf;
} /* wbname */

#endif /* ifndef STUBBED_DICT */
