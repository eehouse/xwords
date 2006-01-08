/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */
/* 
 * Copyright 1997-2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#include <ebm_object.h>

#include "dictnryp.h"
#include "frankdict.h"
#include "frankids.h"

#ifdef CPLUS
extern "C" {
#endif

typedef struct FrankDictionaryCtxt {
    DictionaryCtxt super;

    void* base;
    size_t dictSize;
    BOOL isMMC;			/* means the base must be free()d */
} FrankDictionaryCtxt;

static void frank_dictionary_destroy( DictionaryCtxt* dict );
static void loadSpecialData( FrankDictionaryCtxt* ctxt, U8** ptr );
static U16 countSpecials( FrankDictionaryCtxt* ctxt );
static int tryLoadMMCFile( MPFORMAL XP_UCHAR* dictName, U8** ptrP, 
			   size_t* size );
static U32 ntohl_noalign(U8* n);
static U16 ntohs_noalign(U8** n);

U16
GetDictFlags( ebo_enumerator_t* eboe, FileLoc loc )
{
    U16 flags = FRANK_DICT_FLAGS_ERROR;
    U16 tmp;

    if ( loc == IN_RAM ) {
        ebo_name_t* name = &eboe->name;
        size_t size = EBO_BLK_SIZE;
        U8* ptr = (U8*)OS_availaddr + FLAGS_CHECK_OFFSET;
#ifdef DEBUG
        int result = 
#endif
            ebo_mapin( name, 0, (void*)ptr, &size, 0 );
        XP_ASSERT( result >= 0 );

        tmp = *(U16*)ptr;
        (void)ebo_unmap( ptr, size );

    } else if ( loc == ON_MMC ) {
#ifdef DEBUG
	    long read = 
#endif
            ebo_iread( eboe->index, &tmp, 0, sizeof(tmp) );
        XP_ASSERT( read == sizeof(tmp) );
    }

    XP_DEBUGF( "raw value: 0x%x", tmp );
#if BYTE_ORDER==LITTLE_ENDIAN
    ((char*)&flags)[0] = ((char*)&tmp)[1];
    ((char*)&flags)[1] = ((char*)&tmp)[0];
#else
    flags = tmp;
#endif

    XP_DEBUGF( "GetDictFlags returning 0x%x", flags );
    return flags;
} /* GetDictFlags */

DictionaryCtxt*
frank_dictionary_make( MPFORMAL XP_UCHAR* dictName )
{
    FrankDictionaryCtxt* ctxt = (FrankDictionaryCtxt*)NULL;

    ctxt = (FrankDictionaryCtxt*)XP_MALLOC(mpool, sizeof(*ctxt));
    XP_MEMSET( ctxt, 0, sizeof(*ctxt) );

    dict_super_init( (DictionaryCtxt*)ctxt );

    MPASSIGN( ctxt->super.mpool, mpool );

    if ( !!dictName ) {
        ebo_enumerator_t eboe;

        XP_MEMSET( &eboe.name, 0, sizeof(eboe.name) );

        U8* ptr = (U8*)OS_availaddr + DICT_OFFSET;
        size_t size = EBO_BLK_SIZE * 75; /* PENDING(ehouse) how to find size
                                            of file */
        strcpy( (char*)eboe.name.name, (char*)dictName );
        strcpy( eboe.name.publisher, PUB_ERICHOUSE );
        strcpy( eboe.name.extension, EXT_XWORDSDICT );
        XP_DEBUGF( "makedict: looking for %s.%s\n", dictName,
                   &eboe.name.extension );
        int result = ebo_mapin( &eboe.name, 0, (void*)ptr, &size, 0 );
        XP_DEBUGF( "ebo_mapin returned %d; size=%d\n", result, size );

        int flags;
        if ( result >= 0 ) {
            flags = GetDictFlags( &eboe, IN_RAM );
            if ( flags != 0x0001 && flags != 0x0002 && flags != 0x0003 ) {
                result = -1;
            }
        }

        if ( result < 0 ) {
            result = tryLoadMMCFile( MPPARM(mpool) dictName, &ptr, &size );
            if ( result >= 0 ) {
                ctxt->isMMC = true;
            }
        }

        if ( result >= 0 ) {
            XP_U16 numFaces;
            XP_U16 facesSize;
            XP_U16 charSize;

            /* save for later */
            ctxt->base = (void*)ptr;
            ctxt->dictSize = size;

            ctxt->super.destructor = frank_dictionary_destroy;

            U16 flags = ntohs_noalign( &ptr );
            if ( flags == 0x0001 ) {
                charSize = 1;
                ctxt->super.nodeSize = 3;
            } else if ( flags == 0x0002 ) {
                charSize = 2;
                ctxt->super.nodeSize = 3;
            } else if ( flags == 0x0003 ) {
                charSize = 2;
                ctxt->super.nodeSize = 4;
            } else {
                XP_ASSERT( XP_FALSE );
                charSize = 0; /* shut up compiler */
            }

            ctxt->super.nFaces = numFaces = *ptr++;
            XP_DEBUGF( "read %d faces from dict\n", numFaces );

            facesSize = numFaces * sizeof(ctxt->super.faces16[0]);
            ctxt->super.faces16 = (XP_U16*)XP_MALLOC( mpool, facesSize );
            XP_MEMSET( ctxt->super.faces16, 0, facesSize );
            
            for ( XP_U16 i = 0; i < numFaces; ++i ) {
                if ( charSize == 2 ) {
                    ++ptr;      /* skip the extra byte; screw unicode for
                                   now. :-) */
                }
                ctxt->super.faces16[i] = *ptr++;
            }

            ctxt->super.countsAndValues = 
                (XP_U8*)XP_MALLOC(mpool, numFaces*2);

            ptr += 2;		/* skip xloc header */
            for ( U16 i = 0; i < numFaces*2; i += 2 ) {
                ctxt->super.countsAndValues[i] = *ptr++;
                ctxt->super.countsAndValues[i+1] = *ptr++;
            }

            loadSpecialData( ctxt, &ptr );

            U32 topOffset = ntohl_noalign( ptr );
            ptr += sizeof(topOffset);
            XP_DEBUGF( "incremented ptr by startOffset: %ld", topOffset );

            ctxt->super.topEdge = ptr + topOffset;
            ctxt->super.base = ptr;

#ifdef DEBUG
            XP_U32 dictLength = size - (ptr - ((U8*)ctxt->base));
            /* Can't do this because size is a multiple of 0x1000 created
               during the memory-mapping process.  Need to figure out how to
               get the actual size if it ever matter. */
/*             XP_ASSERT( (dictLength % 3) == 0 ); */
            ctxt->super.numEdges = dictLength / 3;
#endif
        }

        setBlankTile( (DictionaryCtxt*)ctxt );

        ctxt->super.name = frankCopyStr(MPPARM(mpool) dictName);
    }
    return (DictionaryCtxt*)ctxt;
} // frank_dictionary_make

static int
tryLoadMMCFile( MPFORMAL XP_UCHAR* dictName, U8** ptrP, size_t* size )
{
    int result;
    ebo_enumerator_t eboe;

    for ( result = ebo_first_xobject( &eboe ); 
          result == EBO_OK;
          result = ebo_next_xobject( &eboe ) ) {
        U16 flags;

        if ( strcmp( eboe.name.publisher, PUB_ERICHOUSE ) == 0 
             && strcmp( eboe.name.extension, EXT_XWORDSDICT ) == 0
             && strcmp( eboe.name.name, (char*)dictName ) == 0 
             && ( ((flags = GetDictFlags(&eboe, ON_MMC)) == 0x0001)
                  || (flags == 0x0002)
                  || (flags == 0x0003) ) ) {

            XP_DEBUGF( "looking to allocate %ld bytes", eboe.size );

            void* buf = (void*)XP_MALLOC( mpool, eboe.size );
            long read = ebo_iread (eboe.index, buf, 0, eboe.size );
            if ( read != (long)eboe.size ) {
                XP_FREE( mpool, buf );
                result = -1;
            } else {
                *ptrP = (U8*)buf;
                *size = eboe.size;
            }
            break;
        }
    }
    return result;
} /* tryLoadMMCFile */

static U32 
ntohl_noalign( U8* np )
{
    union {
        U32 num;
        unsigned char aschars[4];
    } u;
    S16 i;

#if BYTE_ORDER==LITTLE_ENDIAN
    for ( i = 3; i >= 0; --i ) {
        u.aschars[i] = *np++;
    }
#else
    for ( i = 0; i < 4; ++i ) {
        u.aschars[i] = *np++;
    }
#endif    
    XP_DEBUGF( "ntohl_noalign returning %ld", u.num );
    return u.num;
} /* ntohl_noalign */

static U16
ntohs_noalign( U8** p )
{
    U8* np = *p;
    union {
        U16 num;
        unsigned char aschars[2];
    } u;
    S16 i;

#if BYTE_ORDER==LITTLE_ENDIAN
    for ( i = 1; i >= 0; --i ) {
        u.aschars[i] = *np++;
    }
#else
    for ( i = 0; i < 2; ++i ) {
        u.aschars[i] = *np++;
    }
#endif    
    XP_DEBUGF( "ntohl_noalign returning %ld", u.num );
    *p = np;
    return u.num;
} /*  */

static U16
countSpecials( FrankDictionaryCtxt* ctxt )
{
    U16 result = 0;

    for ( U16 i = 0; i < ctxt->super.nFaces; ++i ) {
        if ( IS_SPECIAL(ctxt->super.faces16[i] ) ) {
            ++result;
        }
    }

    return result;
} /* countSpecials */

static XP_Bitmap*
makeBitmap( FrankDictionaryCtxt* ctxt, U8** ptrp )
{
    U8* ptr = *ptrp;
    IMAGE* bitmap = (IMAGE*)NULL;
    U8 nCols = *ptr++;

    if ( nCols > 0 ) {
        U8 nRows = *ptr++;
        U16 rowBytes = (nCols+7) / 8;

        bitmap = (IMAGE*)XP_MALLOC( ctxt->super.mpool, 
                                    sizeof(IMAGE) + (nRows * rowBytes) );
        bitmap->img_width = nCols;
        bitmap->img_height = nRows;
        bitmap->img_rowbytes = rowBytes;
        bitmap->img_colmode = COLOR_MODE_MONO;
        bitmap->img_palette = (COLOR*)NULL;
        U8* dest = ((U8*)&bitmap->img_buffer) + sizeof(bitmap->img_buffer);
        bitmap->img_buffer = dest;

        U8 srcByte = 0, destByte = 0;
        U8 nBits = nRows*nCols;
        for ( U16 i = 0; i < nBits; ++i ) {
            U8 srcBitIndex = i % 8;
            U8 destBitIndex = (i % nCols) % 8;

            if ( srcBitIndex == 0 ) {
                srcByte = *ptr++;
            }

            U8 srcMask = 1 << (7 - srcBitIndex);
            U8 bit = (srcByte & srcMask) != 0;
            destByte |= bit << (7 - destBitIndex);

            /* we need to put the byte if we've filled it or if we're done
               with the row */
            if ( (destBitIndex==7) || ((i%nCols) == (nCols-1)) ) {
                *dest++ = destByte;
                destByte = 0;
            }
        }
    }

    *ptrp = ptr;
    return (XP_Bitmap*)bitmap;
} /* makeBitmap */

static void
loadSpecialData( FrankDictionaryCtxt* ctxt, U8** ptrp )
{
    U16 nSpecials = countSpecials( ctxt );
    U8* ptr = *ptrp;

    XP_DEBUGF( "loadSpecialData: there are %d specials\n", nSpecials );

    XP_UCHAR** texts = (XP_UCHAR**)XP_MALLOC( ctxt->super.mpool, 
                                              nSpecials * sizeof(*texts) );
    SpecialBitmaps* bitmaps = (SpecialBitmaps*)
        XP_MALLOC( ctxt->super.mpool, nSpecials * sizeof(*bitmaps) );

    for ( Tile i = 0; i < ctxt->super.nFaces; ++i ) {
	
        XP_UCHAR face = ctxt->super.faces16[(short)i];
        if ( IS_SPECIAL(face) ) {

            /* get the string */
            U8 txtlen = *ptr++;
            XP_UCHAR* text = (XP_UCHAR*)XP_MALLOC(ctxt->super.mpool, txtlen+1);
            XP_MEMCPY( text, ptr, txtlen );
            ptr += txtlen;
            text[txtlen] = '\0';
            XP_ASSERT( face < nSpecials );
            texts[face] = text;

            XP_DEBUGF( "making bitmaps for %s", texts[face] );
            bitmaps[face].largeBM = makeBitmap( ctxt, &ptr );
            bitmaps[face].smallBM = makeBitmap( ctxt, &ptr );
        }
    }

    ctxt->super.chars = texts;
    ctxt->super.bitmaps = bitmaps;

    XP_DEBUGF( "loadSpecialData consumed %ld bytes",
               ptr - *ptrp );

    *ptrp = ptr;
} /* loadSpecialData */

#if 0
char* 
frank_dictionary_getName(DictionaryCtxt* dict )
{
    FrankDictionaryCtxt* ctxt = (FrankDictionaryCtxt*)dict;
    return ctxt->name;
} /* frank_dictionary_getName */
#endif

static void
frank_dictionary_destroy( DictionaryCtxt* dict )
{
    FrankDictionaryCtxt* ctxt = (FrankDictionaryCtxt*)dict;
    U16 nSpecials = countSpecials( ctxt );
    U16 i;

    if ( !!dict->chars ) {
        for ( i = 0; i < nSpecials; ++i ) {
            XP_UCHAR* text = dict->chars[i];
            if ( !!text ) {
                XP_FREE( dict->mpool, text );
            }
        }
        XP_FREE( dict->mpool, dict->chars );
    }

    if ( !!dict->countsAndValues ) {
        XP_FREE( dict->mpool, dict->countsAndValues );
    }
    if ( !!ctxt->super.faces16 ) {
        XP_FREE( dict->mpool, ctxt->super.faces16 );
    }

    if ( !!ctxt->super.bitmaps ) {
        for ( i = 0; i < nSpecials; ++i ) {
            XP_Bitmap bmp = ctxt->super.bitmaps[i].largeBM;
            if ( !!bmp ) {
                XP_FREE( ctxt->super.mpool, bmp );
            }
            bmp = ctxt->super.bitmaps[i].smallBM;
            if ( !!bmp ) {
                XP_FREE( ctxt->super.mpool, bmp );
            }
        }
        XP_FREE( ctxt->super.mpool, ctxt->super.bitmaps );
    }

    if ( ctxt->isMMC ) {
        XP_FREE( ctxt->super.mpool, ctxt->base );
    } else {
        (void)ebo_unmap( ctxt->base, ctxt->dictSize );
    }
    XP_FREE( ctxt->super.mpool, ctxt );
} // frank_dictionary_destroy

#ifdef CPLUS
}
#endif
