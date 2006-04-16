/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */
/* 
 * Copyright 1997-2001 by Eric House (xwords@eehouse.org).   All rights reserved.
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

#include "stdafx.h" 
#include <commdlg.h>
#include "dictnryp.h"
#include "strutils.h"
#include "cedict.h"
#include "debhacks.h"

typedef struct CEDictionaryCtxt {
    DictionaryCtxt super;
    HANDLE mappedFile;
    void* mappedBase;
    /*     size_t dictSize; */
} CEDictionaryCtxt;

static void ce_dict_destroy( DictionaryCtxt* dict );
static XP_UCHAR* ce_dict_getShortName( DictionaryCtxt* dict );
static void ceLoadSpecialData( CEDictionaryCtxt* ctxt, XP_U8** ptrp );
static XP_U16 ceCountSpecials( CEDictionaryCtxt* ctxt );
static XP_Bitmap* ceMakeBitmap( CEDictionaryCtxt* ctxt, XP_U8** ptrp );

static XP_U32 n_ptr_tohl( XP_U8** in );
static XP_U16 n_ptr_tohs( XP_U8** in );
static XP_U8* openMappedFile( MPFORMAL const wchar_t* name, 
                              HANDLE* mappedFileP, HANDLE* hFileP, 
                              XP_U32* sizep );
static void closeMappedFile( MPFORMAL XP_U8* base, HANDLE mappedFile );
static XP_Bool checkIfDictAndLegal( MPFORMAL wchar_t* path, XP_U16 pathLen, 
                                    wchar_t* name );

#define ALIGN_COUNT 2

DictionaryCtxt*
ce_dictionary_make( CEAppGlobals* globals, XP_UCHAR* dictName )
{
    CEDictionaryCtxt* ctxt = (CEDictionaryCtxt*)NULL;
	HANDLE mappedFile = NULL;

    wchar_t nameBuf[MAX_PATH+1];
    HANDLE hFile;
    XP_U8* ptr;
    XP_U32 dictLength;

    XP_ASSERT( !!dictName );
    XP_DEBUGF( "looking for dict %s", dictName );

    MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, dictName, -1,
                         nameBuf, sizeof(nameBuf)/sizeof(nameBuf[0]) );

    ptr = openMappedFile( MPPARM(globals->mpool) nameBuf, &mappedFile, 
                          &hFile, &dictLength );

    while( !!ptr ) {           /* lets us break.... */
        XP_U32 offset;
        XP_U16 numFaces;
        XP_U16 i;
        XP_U16 flags;
        void* mappedBase = (void*)ptr;
        XP_U8 nodeSize;

        flags = n_ptr_tohs( &ptr );
        XP_DEBUGF( "%s: flags=0x%x", __FUNCTION__, flags );

#ifdef NODE_CAN_4
        if ( flags == 0x0002 ) {
            nodeSize = 3;
        } else if ( flags == 0x0003 ) {
            nodeSize = 4;
        } else {
            break;          /* we want to return NULL */
        }
#else
        if( flags != 0x0001 ) {
            break;
        }
#endif
        ctxt = (CEDictionaryCtxt*)ce_dictionary_make_empty( globals );

        ctxt->mappedFile = mappedFile;
        ctxt->mappedBase = mappedBase;
        ctxt->super.nodeSize = nodeSize;
        ctxt->super.destructor = ce_dict_destroy;
        ctxt->super.func_dict_getShortName = ce_dict_getShortName;

        XP_DEBUGF( "ptr starting at 0x%lx", ptr );
		
        numFaces = (XP_U16)(*ptr++);
        ctxt->super.nFaces = (XP_U8)numFaces;
        XP_DEBUGF( "read %d faces from dict", (short)numFaces );
        ctxt->super.faces16 = 
            XP_MALLOC( globals->mpool, 
                       numFaces * sizeof(ctxt->super.faces16[0]) );

#ifdef NODE_CAN_4
        ctxt->super.is_4_byte = (ctxt->super.nodeSize == 4);

        for ( i = 0; i < numFaces; ++i ) {
            ctxt->super.faces16[i] = n_ptr_tohs(&ptr);
        }
#else
        for ( i = 0; i < numFaces; ++i ) {
            ctxt->super.faces16[i] = (XP_CHAR16)*ptr++;
        }
#endif

        ctxt->super.countsAndValues = 
            (XP_U8*)XP_MALLOC(globals->mpool, numFaces*2);

        ptr += 2;		/* skip xloc header */
        XP_DEBUGF( "pre values: ptr now 0x%lx", ptr );
        for ( i = 0; i < numFaces*2; i += 2 ) {
            ctxt->super.countsAndValues[i] = *ptr++;
            ctxt->super.countsAndValues[i+1] = *ptr++;
        }
        XP_DEBUGF( "post values: ptr now 0x%lx", ptr );

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
        }

        if ( dictLength > 0 ) {
            XP_DEBUGF( "setting topEdge; offset = %ld", offset );
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

        setBlankTile( (DictionaryCtxt*)ctxt );

        ctxt->super.name = copyString(MPPARM(globals->mpool) dictName);
        break;              /* exit phony while loop */
    }
    return (DictionaryCtxt*)ctxt;
} /* ce_dictionary_make */

DictionaryCtxt*
ce_dictionary_make_empty( CEAppGlobals* globals )
{
    CEDictionaryCtxt* ctxt = (CEDictionaryCtxt*)XP_MALLOC(globals->mpool,
                                                          sizeof(*ctxt));
    XP_MEMSET( ctxt, 0, sizeof(*ctxt) );

    dict_super_init( (DictionaryCtxt*)ctxt );
    MPASSIGN( ctxt->super.mpool, globals->mpool );
    return (DictionaryCtxt*)ctxt;
} /* ce_dictionary_make_empty */

static void
ceLoadSpecialData( CEDictionaryCtxt* ctxt, XP_U8** ptrp )
{
    XP_U16 nSpecials = ceCountSpecials( ctxt );
    XP_U8* ptr = *ptrp;
    Tile i;
    XP_UCHAR** texts;
    SpecialBitmaps* bitmaps;

    XP_DEBUGF( "loadSpecialData: there are %d specials", nSpecials );

    texts = (XP_UCHAR**)XP_MALLOC( ctxt->super.mpool, 
                                   nSpecials * sizeof(*texts) );
    bitmaps = (SpecialBitmaps*)
        XP_MALLOC( ctxt->super.mpool, nSpecials * sizeof(*bitmaps) );

    for ( i = 0; i < ctxt->super.nFaces; ++i ) {
	
        XP_CHAR16 face = ctxt->super.faces16[(short)i];
        if ( IS_SPECIAL(face) ) {

            /* get the string */
            XP_U8 txtlen = *ptr++;
            XP_UCHAR* text = (XP_UCHAR*)XP_MALLOC(ctxt->super.mpool, txtlen+1);
            XP_MEMCPY( text, ptr, txtlen );
            ptr += txtlen;
            text[txtlen] = '\0';
            XP_ASSERT( face < nSpecials );
            texts[face] = text;

            XP_DEBUGF( "making bitmaps for %s", texts[face] );
            bitmaps[face].largeBM = ceMakeBitmap( ctxt, &ptr );
            bitmaps[face].smallBM = ceMakeBitmap( ctxt, &ptr );
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
    XP_U16 i;

    for ( i = 0; i < ctxt->super.nFaces; ++i ) {
        if ( IS_SPECIAL(ctxt->super.faces16[i] ) ) {
            ++result;
        }
    }

    return result;
} /* ceCountSpecials */

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

    XP_DEBUGF( "   printBitmapData (%dx%d):", nCols, nRows );

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
    XP_DEBUGF( "   printBitmapData done" );
} /* printBitmapData1 */

static void
printBitmapData2( XP_U16 nCols, XP_U16 nRows, XP_U8* data )
{
    XP_DEBUGF( "   printBitmapData (%dx%d):", nCols, nRows );
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
    XP_DEBUGF( "   printBitmapData done" );
} /* printBitmapData2 */

#if 0
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

static XP_Bitmap*
ceMakeBitmap( CEDictionaryCtxt* ctxt, XP_U8** ptrp )
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

        printBitmapData1( nCols, nRows, savedDest );
        printBitmapData2( nCols, nRows, savedDest );
    }

    *ptrp = ptr;
    return (XP_Bitmap*)bitmap;
} /* ceMakeBitmap */

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
            HBITMAP bitmap = (HBITMAP)ctxt->super.bitmaps[i].largeBM;
            if ( !!bitmap ) {
                DeleteObject( bitmap );
            }
            bitmap = (HBITMAP)ctxt->super.bitmaps[i].smallBM;
            if ( !!bitmap ) {
                DeleteObject( bitmap );
            }
        }
        XP_FREE( ctxt->super.mpool, ctxt->super.bitmaps );
    }

    XP_FREE( ctxt->super.mpool, ctxt->super.faces16 );

    closeMappedFile( MPPARM(ctxt->super.mpool) ctxt->mappedBase, 
                     ctxt->mappedFile );
    XP_FREE( ctxt->super.mpool, ctxt );
} // ce_dict_destroy

static XP_UCHAR* 
ce_dict_getShortName( DictionaryCtxt* dict )
{
    XP_UCHAR* name = dict_getName( dict );
    return bname( name );
} /* ce_dict_getShortName */

XP_Bool
ce_pickDictFile( CEAppGlobals* globals, XP_UCHAR* buf, XP_U16 bufLen )
{
    XP_Bool result = XP_FALSE;
    wchar_t nameBuf[256];
    OPENFILENAME openFileStruct;

    XP_MEMSET( &openFileStruct, 0, sizeof(openFileStruct) );
    XP_MEMSET( nameBuf, 0, sizeof(nameBuf) );

    openFileStruct.lStructSize = sizeof(openFileStruct);
    openFileStruct.hwndOwner = globals->hWnd;
    openFileStruct.lpstrFilter = L"Crosswords dictionaries" L"\0"
        L"*.xwd" L"\0\0";
    openFileStruct.Flags = OFN_FILEMUSTEXIST
        | OFN_HIDEREADONLY
        | OFN_PATHMUSTEXIST;

    openFileStruct.lpstrFile = nameBuf;
    openFileStruct.nMaxFile = sizeof(nameBuf)/sizeof(nameBuf[0]);

    while ( GetOpenFileName( &openFileStruct ) ) {
        XP_U16 len;
        XP_UCHAR multiBuf[256];

        if ( !checkIfDictAndLegal( MPPARM(globals->mpool) nameBuf, 
                                   openFileStruct.nFileOffset, 
                                   nameBuf + openFileStruct.nFileOffset ) ) {
            wchar_t errBuf[128];
            (void)swprintf( errBuf,
                            L"\"%s\" is not a valid Crosswords dictionary.",
                            nameBuf + openFileStruct.nFileOffset );
            MessageBox( globals->hWnd, errBuf, NULL, MB_OK );
            continue;
        }

        len = WideCharToMultiByte( CP_ACP, 0, nameBuf, wcslen(nameBuf),
                                   multiBuf,
                                   sizeof(multiBuf), NULL, NULL );

        if ( len > 0 && len < bufLen ) {
            multiBuf[len] = '\0';
            len = (XP_U16)XP_STRLEN( multiBuf );
            XP_MEMCPY( buf, multiBuf, len );
            buf[len] = '\0';
            result = XP_TRUE;
            break;
        }
    }

    return result;
} /* ce_pickDictFile */

static XP_U8*
openMappedFile( MPFORMAL const wchar_t* name, HANDLE* mappedFileP, 
                HANDLE* hFileP, XP_U32* sizep )
{
    XP_U8* ptr = NULL;
    HANDLE mappedFile = NULL;
    HANDLE hFile;

#if defined TARGET_OS_WINCE
    hFile = CreateFileForMapping( name,
                                  GENERIC_READ,
                                  FILE_SHARE_READ, /* (was 0: no sharing) */
                                  NULL, /* security */
                                  OPEN_EXISTING,
                                  FILE_FLAG_RANDOM_ACCESS,
                                  NULL );

    if ( hFile == INVALID_HANDLE_VALUE ) {
        XP_DEBUGF( "open file failed: %ld", GetLastError() );
    } else {
        HANDLE mappedFile;
        XP_DEBUGF( "open file succeeded!!!!" );

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
#elif defined TARGET_OS_WIN32
    hFile = CreateFile( name, 
                        GENERIC_READ, 
                        FILE_SHARE_READ,
                        NULL,   /* security */
                        OPEN_EXISTING,
                        FILE_FLAG_RANDOM_ACCESS,
                        NULL );
    XP_ASSERT( hFile != INVALID_HANDLE_VALUE );

    DWORD size = GetFileSize( hFile, NULL );
    XP_LOGF( "file size: %d", size );

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
        *sizep = GetFileSize( hFile, NULL );
    }
    *mappedFileP = (HANDLE)ptr;
#endif
    return ptr;
} /* openMappedFile */

static void
closeMappedFile( MPFORMAL XP_U8* base, HANDLE mappedFile )
{
#if defined TARGET_OS_WINCE
    UnmapViewOfFile( base );
    CloseHandle( mappedFile );
#elif defined TARGET_OS_WIN32
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
        wchar_t pathBuf[257];

        wcscpy( pathBuf, path );
        pathBuf[pathLen] = 0;
        wcscat( pathBuf, name );

#ifdef DEBUG
        {
            char narrowName[257];
            int len = wcslen( pathBuf );
            len = WideCharToMultiByte( CP_ACP, 0, pathBuf, len + 1,
                                       narrowName, len + 1, NULL, NULL );
            XP_LOGF( "%s ends in .xwd", narrowName );
        }
#endif

        base = openMappedFile( MPPARM(mpool) pathBuf, &mappedFile, 
                               &hFile, NULL );
        if ( !!base ) {
            XP_U8* ptr = base;
        
            flags = n_ptr_tohs( &ptr );
            XP_LOGF( "checkIfDictAndLegal: flags=0x%x", flags );
            closeMappedFile( MPPARM(mpool) base, mappedFile );
#ifdef NODE_CAN_4
            /* are the flags what we expect */
            result = flags == 0x0002 || flags == 0x0003;
#else
            result = flags == 0x0001;
#endif
        }
    }

    return result;
} /* checkIfDictAndLegal */

static void
locateOneDir( MPFORMAL wchar_t* path, XP_UCHAR** bufs, XP_U16 nSought,
              XP_U16* nFoundP )
{
    WIN32_FIND_DATA data;
    HANDLE fileH;
    XP_Bool result = XP_FALSE;
    XP_U16 startLen;

#if defined TARGET_OS_WINCE
    lstrcat( path, L"\\" );
#elif defined TARGET_OS_WIN32
    lstrcat( path, L".\\" );
#endif
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
#if defined TARGET_OS_WINCE
                /* We don't do recursive search on Win32!!! */
                lstrcpy( path+startLen, data.cFileName );
                locateOneDir( MPPARM(mpool) path, bufs, nSought, nFoundP );
                if ( *nFoundP == nSought ) {
                    break;
                }
                path[startLen] = 0;
#endif
            } else if ( checkIfDictAndLegal( MPPARM(mpool) path, startLen,
                                             data.cFileName ) ) {
                XP_U16 len;
                XP_UCHAR* str;
                XP_ASSERT( *nFoundP < nSought );

                len = startLen + wcslen( data.cFileName ) + 1;
                str = XP_MALLOC( mpool, len );
                WideCharToMultiByte( CP_ACP, 0, path, startLen, 
                                     str, startLen, NULL, NULL ); 
                WideCharToMultiByte( CP_ACP, 0, data.cFileName, -1, 
                                     str + startLen, len - startLen, 
                                     NULL, NULL ); 
                XP_LOGF( "%s: got %s at end\n", __FUNCTION__, str );

                bufs[(*nFoundP)++] = str;
                if ( *nFoundP == nSought ) {
                    break;
                }
            }

            if ( !FindNextFile( fileH, &data ) ) {
                XP_ASSERT( GetLastError() == ERROR_NO_MORE_FILES );
                break;
            }
        }

        (void)FindClose( fileH );
    }
} /* locateOneDir */

XP_U16
ceLocateNDicts( MPFORMAL XP_UCHAR** bufs, XP_U16 nSought )
{
    XP_U16 nFound = 0;
    wchar_t pathBuf[257];

    pathBuf[0] = 0;

    locateOneDir( MPPARM(mpool) pathBuf, bufs, nSought, &nFound );
    return nFound;
} /* ceLocateNthDict */

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

XP_UCHAR*
bname( XP_UCHAR* in )
{
    XP_U16 len = (XP_U16)XP_STRLEN(in);
    XP_UCHAR* out = in + len - 1;

    while ( *out != '\\' && out >= in ) {
        --out;
    }
    return out + 1;
} /* bname */

#endif /* ifndef STUBBED_DICT */
