/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#include "xptypes.h"
}

#include <f32file.h>
#include "symdict.h"
#include "symutil.h"


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

#ifdef DEBUG
static void
printBitmap( CFbsBitmap* bmp )
{
    TSize bmpSize = bmp->SizeInPixels();
    TBitmapUtil butil( bmp );
    butil.Begin( TPoint(0,0) );
    TInt row, col;

    for ( row = 0; row < bmpSize.iHeight; ++row ) {
        char buf[64];
        for ( col = 0; col < bmpSize.iWidth; ++col ) {
            butil.SetPos( TPoint(col, row) );
            if ( butil.GetPixel() ) {
                buf[col] = '*';
            } else {
                buf[col] = '_';
            }
        }
        buf[col] = '\0';
        XP_LOGF( "row %d: %s", row, buf );
    }

    butil.End();
}
#else
#define printBitmap(b)
#endif

static XP_Bitmap*
symMakeBitmap( SymDictCtxt* /*ctxt*/, RFile* file )
{
    XP_U8 nCols = readXP_U8( file );
    CFbsBitmap* bitmap = (CFbsBitmap*)NULL;
    const TDisplayMode dispMode = EGray2;

    if ( nCols > 0 ) {
        
        XP_U8 nRows = readXP_U8( file );
        XP_U8 srcByte = 0;
        XP_U16 nBits;
        bitmap = new (ELeave) CFbsBitmap();
        bitmap->Create( TSize(nCols, nRows), dispMode );
        
        TBitmapUtil butil( bitmap );
        butil.Begin( TPoint(0,0) );

        TInt col, row;
        nBits = nRows * nCols;
        TInt curBit = 0;
        for ( row = 0; curBit < nBits; ++row ) {
            for ( col = 0; col < nCols; ++col ) {
                TUint32 value;
                TInt index = curBit % 8;

                if ( index == 0 ) {
                    srcByte = readXP_U8( file );
                }

                value = ((srcByte & 0x80) == 0) ? 1L : 0L;

                butil.SetPos( TPoint(col, row) );
                butil.SetPixel( value );

                srcByte <<= 1;
                ++curBit;
            }
        }
        butil.End();

        printBitmap( bitmap );
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
        XP_ASSERT( err == KErrNone );
        TInt nRead = buf.Size();
        if ( nRead <= 0 ) {
            break;
        }
        XP_MEMCPY( (void*)(dictBuf + offset), (void*)buf.Ptr(), nRead );
        offset += nRead;
    }
} // readFileToBuf

DictionaryCtxt*
sym_dictionary_makeL( MPFORMAL TFileName* base, const XP_UCHAR* aDictName )
{
    if ( !aDictName ) {
        SymDictCtxt* ctxt = (SymDictCtxt*)XP_MALLOC( mpool, sizeof( *ctxt ) );
        XP_MEMSET( ctxt, 0, sizeof(*ctxt) );
        MPASSIGN( ctxt->super.mpool, mpool );
        return &ctxt->super;
    } else {

        TBuf16<32> dname16;
        dname16.Copy( TPtrC8(aDictName) );
        base->Append( dname16 );
        base->Append( _L(".xwd") );
        SymDictCtxt* ctxt = NULL;

        RFs fileSession;
        User::LeaveIfError(fileSession.Connect());
        CleanupClosePushL(fileSession);

        RFile file;
        TInt err = file.Open( fileSession, *base, EFileRead );
        if ( err != KErrNone ) {
            XP_LOGDESC16( base );
            XP_LOGF( "file.Open => %d", err );
        }
        User::LeaveIfError( err );
        CleanupClosePushL(file);

        ctxt = (SymDictCtxt*)XP_MALLOC( mpool, sizeof(*ctxt) );
        User::LeaveIfNull( ctxt );
        XP_MEMSET( ctxt, 0, sizeof( *ctxt ) );
        MPASSIGN( ctxt->super.mpool, mpool );

        dict_super_init( (DictionaryCtxt*)ctxt );

        ctxt->super.destructor = sym_dictionary_destroy;
        XP_ASSERT( ctxt->super.name == NULL );
        symReplaceStrIfDiff( MPPARM(mpool) &ctxt->super.name, aDictName );

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
        TInt i;
        for ( i = 0; i < numFaces; ++i ) {
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

        if ( dawgSize > SC(TInt, sizeof(XP_U32)) ) {
            XP_U32 offset = readXP_U32( &file );
            dawgSize -= sizeof(XP_U32);

            XP_ASSERT( dawgSize % ctxt->super.nodeSize == 0 );
# ifdef DEBUG
            ctxt->super.numEdges = dawgSize / ctxt->super.nodeSize;
# endif

            if ( dawgSize > 0 ) {
                XP_DEBUGF( "setting topEdge; offset = %d", offset );

                XP_U8* dictBuf = (XP_U8*)XP_MALLOC( mpool, dawgSize );
                User::LeaveIfNull( dictBuf ); // will leak ctxt (PENDING...)

                readFileToBuf( dictBuf, &file );

                ctxt->super.base = (array_edge*)dictBuf;

                ctxt->super.topEdge = ctxt->super.base 
                    + (offset * ctxt->super.nodeSize);
                ctxt->super.topEdge = ctxt->super.base 
                    + (offset * ctxt->super.nodeSize);
            } else {
                ctxt->super.topEdge = (array_edge*)NULL;
                ctxt->super.base = (array_edge*)NULL;
            }
        }

        CleanupStack::PopAndDestroy( &file ); // file
        CleanupStack::PopAndDestroy( &fileSession ); // fileSession

        return &ctxt->super;
    }
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
            CFbsBitmap* bitmap = (CFbsBitmap*)dict->bitmaps[i].smallBM;
            delete bitmap;
            bitmap = (CFbsBitmap*)dict->bitmaps[i].largeBM;
            delete bitmap;
        }
        XP_FREE( dict->mpool, dict->bitmaps );
    }

    if ( dict->base != NULL ) {
        XP_FREE( dict->mpool, dict->base );
    }

    if ( dict->name ) {
        XP_FREE( dict->mpool, dict->name );
    }

    XP_FREE( dict->mpool, dict );
}
