/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 2005 by Eric House (fixin@peak.org).  All rights reserved.
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

extern "C" {
#include "dictnryp.h"
#include "mempool.h"
}

#include <f32file.h>
#include "symdict.h"


typedef struct SymDictCtxt {
    DictionaryCtxt super;

} SymDictCtxt;

static void sym_dictionary_destroy( DictionaryCtxt* dict );



static XP_U8
readXP_U8( RFile* file )
{
    TBuf8<1> buf;
    TInt err = file->Read( buf, 1 );
    XP_ASSERT( err == KErrNone );
    return *buf.Ptr();
} // readXP_U8

static XP_U16
readXP_U16( RFile* file )
{
    XP_U16 result;
    TBuf8<2> buf;
    TInt err = file->Read( buf, 2 );
    XP_ASSERT( err == KErrNone );
    return XP_NTOHS( *(XP_U16*)buf.Ptr() );
} // readXP_U16

static XP_U32
readXP_U32( RFile* file )
{
    TBuf8<4> buf;
    TInt err = file->Read( buf, 4 );
    XP_ASSERT( err == KErrNone );
    return XP_NTOHL( *(XP_U32*)buf.Ptr() );
} // readXP_U32

static XP_U16
symCountSpecials( SymDictCtxt* ctxt )
{
    XP_U16 result = 0;
    XP_U16 i;

    for ( i = 0; i < ctxt->super.nFaces; ++i ) {
        if ( IS_SPECIAL(ctxt->super.faces16[i] ) ) {
            ++result;
        }
    }

    return result;
} /* symCountSpecials */

static XP_Bitmap*
symMakeBitmap( SymDictCtxt* ctxt, RFile* file )
{
    XP_U8 nCols = readXP_U8( file );
//     CEBitmapInfo* bitmap = (CEBitmapInfo*)NULL;
    XP_Bitmap* bitmap = NULL;

    if ( nCols > 0 ) {
        XP_ASSERT( 0 );         // don't do this yet!!!!
#if 0
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
#endif
    }

    return (XP_Bitmap*)bitmap;
} /* symMakeBitmap */

static void
symLoadSpecialData( SymDictCtxt* ctxt, RFile* file )
{
    TInt i;
    TInt nSpecials = symCountSpecials( ctxt );
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
            XP_U8 txtlen = readXP_U8( file );
            XP_UCHAR* text = (XP_UCHAR*)XP_MALLOC(ctxt->super.mpool, txtlen+1);
            TPtr8 desc( text, txtlen );
            file->Read( desc, txtlen );
            text[txtlen] = '\0';
            XP_ASSERT( face < nSpecials );
            texts[face] = text;

            XP_DEBUGF( "making bitmaps for %s", texts[face] );
            bitmaps[face].largeBM = symMakeBitmap( ctxt, file );
            bitmaps[face].smallBM = symMakeBitmap( ctxt, file );
        }
    }

    ctxt->super.chars = texts;
    ctxt->super.bitmaps = bitmaps;
    XP_LOGF( "returning from symLoadSpecialData" );
} // symLoadSpecialData

static void
readFileToBuf( XP_UCHAR* dictBuf, const RFile* file )
{
    XP_U32 offset = 0;
    for ( ; ; ) {
        TBuf8<1024> buf;
        TInt err = file->Read( buf, buf.MaxLength() );
        TInt nRead = buf.Size();
        if ( nRead <= 0 ) {
            break;
        }
        XP_MEMCPY( (void*)(dictBuf + offset), (void*)buf.Ptr(), nRead );
        offset += nRead;
    }
} // readFileToBuf

DictionaryCtxt*
sym_dictionary_makeL( MPFORMAL const XP_UCHAR* aDictName )
{
    _LIT( dir,"z:\\system\\apps\\XWORDS\\" );
    TFileName nameD;            /* need the full path to name in this */
    nameD.Copy( dir );
    TBuf8<32> dname8(aDictName);
    TBuf16<32> dname16;
    dname16.Copy( dname8 );
    nameD.Append( dname16 );
    nameD.Append( _L(".xwd") );
    SymDictCtxt* ctxt = NULL;
    TInt err;

    RFs fileSession;
    User::LeaveIfError(fileSession.Connect());
    CleanupClosePushL(fileSession);

    RFile file;
    User::LeaveIfError( file.Open( fileSession, nameD, EFileRead ) );
    CleanupClosePushL(file);

    ctxt = (SymDictCtxt*)XP_MALLOC( mpool, sizeof(*ctxt) );
    XP_MEMSET( ctxt, 0, sizeof( *ctxt ) );
    dict_super_init( (DictionaryCtxt*)ctxt );
    ctxt->super.destructor = sym_dictionary_destroy;
    MPASSIGN( ctxt->super.mpool, mpool );

    XP_U16 flags = readXP_U16( &file );
    XP_LOGF( "read flags are: 0x%x", (TInt)flags );

    TInt numFaces = readXP_U8( &file );
    ctxt->super.nFaces = (XP_U8)numFaces;
    XP_DEBUGF( "read %d faces from dict", (TInt)numFaces );

    ctxt->super.faces16 = (XP_U16*)
        XP_MALLOC( mpool, numFaces * sizeof(ctxt->super.faces16[0]) );
#ifdef NODE_CAN_4
    if ( flags == 0x0002 ) {
        ctxt->super.nodeSize = 3;
    } else if ( flags == 0x0003 ) {
        ctxt->super.nodeSize = 4;
    } else {
        XP_DEBUGF( "flags=0x%x", flags );
        XP_ASSERT( 0 );
    }

    ctxt->super.is_4_byte = ctxt->super.nodeSize == 4;

    for ( TInt i = 0; i < numFaces; ++i ) {
        ctxt->super.faces16[i] = readXP_U16( &file );
    }
#else
    error will robinson....;
#endif

    ctxt->super.countsAndValues = 
        (XP_U8*)XP_MALLOC( mpool, numFaces*2 );
    (void)readXP_U16( &file );  // skip xloc header

    for ( i = 0; i < numFaces*2; i += 2 ) {
        ctxt->super.countsAndValues[i] = readXP_U8( &file );
        ctxt->super.countsAndValues[i+1] = readXP_U8( &file );
    }

    symLoadSpecialData( ctxt, &file );

    // Now, until we figure out how/whether Symbian does memory
    // mapping of files, we need to allocate a buffer to hold the
    // entire freaking DAWG... :-(
    TInt dawgSize;
    (void)file.Size( dawgSize );
    TInt pos = 0;
    file.Seek( ESeekCurrent, pos );
    dawgSize -= pos;
    XP_U32 offset;
    if ( dawgSize > sizeof(XP_U32) ) {
        offset = readXP_U32( &file );
        dawgSize -= sizeof(XP_U32);

        XP_ASSERT( dawgSize % ctxt->super.nodeSize == 0 );
# ifdef DEBUG
        ctxt->super.numEdges = dawgSize / ctxt->super.nodeSize;
# endif
    }

    if ( dawgSize > 0 ) {
        XP_DEBUGF( "setting topEdge; offset = %ld", offset );

        XP_U8* dictBuf = (XP_U8*)XP_MALLOC( mpool, dawgSize );
        User::LeaveIfNull( dictBuf ); // will leak ctxt (PENDING...)

        readFileToBuf( dictBuf, &file );

        ctxt->super.base = (array_edge*)dictBuf;

        ctxt->super.topEdge = ctxt->super.base 
            + (offset * ctxt->super.nodeSize);
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

    CleanupStack::PopAndDestroy(); // file
    CleanupStack::PopAndDestroy(); // fileSession

    return &ctxt->super;
} // sym_dictionary_make

static void
sym_dictionary_destroy( DictionaryCtxt* dict )
{
    SymDictCtxt* sctx = (SymDictCtxt*)dict;
    XP_U16 nSpecials = symCountSpecials( sctx );
    XP_U16 i;

    if ( dict->countsAndValues != NULL ) {
        XP_FREE( sctx->super.mpool, dict->countsAndValues );
    }
    if ( dict->faces16 != NULL ) {
        XP_FREE( sctx->super.mpool, dict->faces16 );
    }

    if ( !!sctx->super.chars ) {
        for ( i = 0; i < nSpecials; ++i ) {
            XP_UCHAR* text = sctx->super.chars[i];
            if ( !!text ) {
                XP_FREE( sctx->super.mpool, text );
            }
        }
        XP_FREE( dict->mpool, dict->chars );
    }

    if ( !!sctx->super.bitmaps ) {
        for ( i = 0; i < nSpecials; ++i ) {
            // Delete sym-specific bitmap data
        }
        XP_FREE( dict->mpool, dict->bitmaps );
    }

    if ( dict->base != NULL ) {
        XP_FREE( dict->mpool, dict->base );
    }

    XP_FREE( dict->mpool, dict );
}
