/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */
/* 
 * Copyright 1997-2009 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <PalmTypes.h>
#include <DataMgr.h>
#include <VFSMgr.h>
#include <FeatureMgr.h>

#include "dictnryp.h"
#include "dawg.h"
#include "strutils.h"
#include "palmdict.h"
#include "dictlist.h"
#include "dictui.h"
#include "palmmain.h"
#include "pace_man.h"           /* READ_UNALIGNED16 */
#include "LocalizedStrIncludes.h"

typedef struct DictStart {
    unsigned long indexStart;
    array_edge* array;
} DictStart;


#define NO_REFCOUNT -2

struct PalmDictionaryCtxt {
    DictionaryCtxt super;
    dawg_header* headerRecP;
    DmOpenRef dbRef;
    XP_U16 nRecords;
    UInt16 cardNo;              /* added to track VFS imported file for */
    LocalID dbID;               /* deletion later */
    XP_UCHAR location;          /* VFS or storage mem */
    XP_S8 refCount;
    PalmDictList* dl;
    DictStart dictStarts[1];
};

#ifdef NODE_CAN_4
typedef unsigned short FaceType;
#else
typedef unsigned char FaceType;
#endif

static void palm_dictionary_destroy( DictionaryCtxt* dict );
static XP_U16 countSpecials( FaceType* ptr, UInt16 nChars );
static void setupSpecials( MPFORMAL PalmDictionaryCtxt* ctxt, 
                           Xloc_specialEntry* specialStart, XP_U16 nSpecials );
static array_edge* palm_dict_edge_for_index_multi( const DictionaryCtxt* dict, 
                                                   XP_U32 index );

DictionaryCtxt*
palm_dictionary_make( MPFORMAL PalmAppGlobals* globals, 
                      const XP_UCHAR* dictName, PalmDictList* dl )
{
    Boolean found;
    UInt16 cardNo;
    LocalID dbID;
    DmOpenRef dbRef;
    PalmDictionaryCtxt tDictBuf;
    PalmDictionaryCtxt* ctxt;
    MemHandle tmpH;
    dawg_header* headerRecP;
    unsigned char* charPtr;
    UInt16 nChars, nSpecials;
    XP_U32 offset;
    DictListEntry* dle;
    Err err;
    XP_U16 ii;
    FaceType* facePtr;
#ifdef NODE_CAN_4
    XP_U16 flags;
    XP_U16 nodeSize = 3;        /* init to satisfy compiler */
#endif
    XP_U32 totalSize;

    /* check and see if there's already a dict for this name.  If yes,
       increment its refcount and return. */
    if ( !!dictName && getDictWithName( dl, dictName, &dle ) ) {
        PalmDictionaryCtxt* dict = (PalmDictionaryCtxt*)dle->dict;
        if ( !!dict ) {
            ++dict->refCount;
            return (DictionaryCtxt*)dict;
        }
    }

    if ( !!dictName ) {
        ctxt = &tDictBuf;
    } else {
        /* If the name's NULL, we still need to create a dict, as the server
           may be called to fill it in from the stream later. */
        ctxt = XP_MALLOC( mpool, sizeof(*ctxt) );
    }

    XP_MEMSET( ctxt, 0, sizeof(*ctxt) );
    MPASSIGN( ctxt->super.mpool, mpool );

    dict_super_init( (DictionaryCtxt*)ctxt );

    if ( !!dictName ) {
        XP_U8 buf[64];
        XP_ASSERT( XP_STRLEN((const char*)dictName) > 0 );

        ctxt->super.name = copyString( mpool, dictName );
        ctxt->super.destructor = palm_dictionary_destroy;

        found = getDictWithName( dl, dictName, &dle );

        if ( !found ) {
            goto errExit;
        }

        if ( dle->location == DL_VFS ) {
            const XP_UCHAR* str = getResString( globals, STR_DICT_COPY_EXPL );
            WinDrawChars( str, XP_STRLEN(str), 5, 40 );

            err = VFSImportDatabaseFromFile( dle->u.vfsData.volNum, 
                                             (const char*)dle->path,
                                             &cardNo, &dbID );
            if ( err != errNone ) {
                goto errExit;
            }
        } else {
            cardNo = dle->u.dmData.cardNo;
            dbID = dle->u.dmData.dbID;
        }

        ctxt->refCount = 1;
        ctxt->dl = dl;
        ctxt->dbID = dbID;
        ctxt->cardNo = cardNo;
        ctxt->location = dle->location;

        ctxt->dbRef = dbRef = DmOpenDatabase( cardNo, dbID, dmModeReadOnly );
        tmpH = DmQueryRecord( dbRef, 0 ); // <- <eeh> should be a constant
        ctxt->headerRecP = headerRecP = (dawg_header*)MemHandleLock( tmpH );
        XP_ASSERT( MemHandleLockCount(tmpH) == 1 );

#ifdef NODE_CAN_4
        flags = XP_NTOHS( headerRecP->flags );
        if ( flags == 0x0002 ) {
            XP_ASSERT( nodeSize == 3 );
        } else if ( flags == 0x0003 ) {
            nodeSize = 4;
        } else {
            XP_WARNF( "got flags of %d", flags );
            XP_ASSERT(0);
            return NULL;
        }
#endif

        tmpH = DmQueryRecord( dbRef, headerRecP->charTableRecNum );
        XP_ASSERT( !!tmpH );
        facePtr = (FaceType*)MemHandleLock( tmpH );
        XP_ASSERT( MemHandleLockCount( tmpH ) == 1 );
        ctxt->super.nFaces = nChars = MemPtrSize(facePtr) / sizeof(*facePtr);
        for ( ii = 0; ii < nChars; ++ii ) {
            XP_ASSERT( ii < VSIZE(buf) );
#ifdef NODE_CAN_4
            buf[ii] = READ_UNALIGNED16( &facePtr[ii] );
#else
            buf[ii] = facePtr[ii];
#endif
        }
        dict_splitFaces( &ctxt->super, buf, nChars, nChars );
        nSpecials = countSpecials( facePtr, nChars );
        MemPtrUnlock( facePtr );

        tmpH = DmQueryRecord( dbRef, headerRecP->valTableRecNum );
        charPtr = (unsigned char*)MemHandleLock(tmpH);
        XP_ASSERT( MemHandleLockCount( tmpH ) == 1 );
        // use 2; ARM thinks sizeof(Xloc_header) is 4.
        ctxt->super.langCode = *charPtr;
        ctxt->super.countsAndValues = charPtr + 2;

        /* for those dicts with special chars */
        if ( nSpecials > 0 ) {
            Xloc_specialEntry* specialStart = (Xloc_specialEntry*)
                (ctxt->super.countsAndValues + (ctxt->super.nFaces*2));
            setupSpecials( MPPARM(mpool) ctxt, specialStart, nSpecials );
        }

        if ( headerRecP->firstEdgeRecNum == 0 ) { /* emtpy dict */
            ctxt->super.topEdge = NULL;
            ctxt->nRecords = 0;
        } else {
            MemHandle record;
            XP_U16 size;
            short index;
            XP_U16 nRecords;

            nRecords = DmNumRecords(dbRef) - headerRecP->firstEdgeRecNum; 

            /* alloacate now that we know the size. */
            ctxt = XP_MALLOC( mpool, 
                              sizeof(*ctxt) + (nRecords * sizeof(DictStart)));
            XP_MEMCPY( ctxt, &tDictBuf, sizeof(tDictBuf) );
            ctxt->nRecords = nRecords;
#ifdef NODE_CAN_4
            ctxt->super.nodeSize = (XP_U8)nodeSize;
            ctxt->super.is_4_byte = nodeSize == 4;
#endif

            totalSize = 0;
            for ( index = 0; index < nRecords; ++index ) {
                record = DmQueryRecord( dbRef, index
                                        + headerRecP->firstEdgeRecNum);
                totalSize += MemHandleSize( record );
            }

            /* NOTE: need to use more than one feature to support having
               multiple dicts open at once. */
            err = ~errNone;     /* so test below will pass if nRecords == 1 */
#ifdef XWFEATURE_COMBINEDAWG
            if ( nRecords > 1 ) {
                void* dawgBase;
                err = FtrPtrNew( APPID, DAWG_STORE_FEATURE, totalSize,
                                 &dawgBase );
                if ( err == errNone ) {
                    for ( index = 0, offset = 0; index < nRecords; ++index ) {
                        record = DmQueryRecord( dbRef, index
                                                + headerRecP->firstEdgeRecNum );
                        size = MemHandleSize( record );
                        err = DmWrite( dawgBase, offset, 
                                       MemHandleLock( record ), size );
                        XP_ASSERT( err == errNone );
                        MemHandleUnlock( record );
                        offset += size;
#ifdef DEBUG
#ifdef NODE_CAN_4
                        ctxt->super.numEdges += size / nodeSize;
#else
                        ctxt->super.numEdges += size / 3;
#endif
#endif
                    }

                    ctxt->super.base = dawgBase;
                    ctxt->super.topEdge = dawgBase;
                } else {
                    XP_LOGF( "unable to use Ftr for dict; err=%d", err );
                }
            }
#endif

            if ( err != errNone ) {
                offset = 0;
                for ( index = 0; index < nRecords; ++index ) {
                    record = DmQueryRecord( dbRef, index + headerRecP->firstEdgeRecNum );
                    size = MemHandleSize( record );

                    ctxt->dictStarts[index].indexStart = offset;

                    /* cast to short to avoid libc call */
                    XP_ASSERT( size < 0xFFFF );
#ifdef NODE_CAN_4
                    XP_ASSERT( 0 == (size % nodeSize) );
                    offset += size / nodeSize;
#else
                    XP_ASSERT( ((unsigned short)size % 3 )==0);
                    offset += ((unsigned short)size) / 3;
#endif
                    ctxt->dictStarts[index].array = 
                        (array_edge*)MemHandleLock( record );
                    XP_ASSERT( MemHandleLockCount(record) == 1 );
                }

                XP_ASSERT( index == ctxt->nRecords );
                ctxt->dictStarts[index].indexStart = 0xFFFFFFFFL;

                ctxt->super.topEdge = ctxt->dictStarts[0].array;
#ifdef DEBUG
                ctxt->super.numEdges = offset;
#endif
                ctxt->super.func_edge_for_index
                    = palm_dict_edge_for_index_multi;
            }
        }

        setBlankTile( (DictionaryCtxt*)ctxt );

        cacheDictForName( dl, dictName, (DictionaryCtxt*)ctxt );
    } else {
        ctxt->refCount = NO_REFCOUNT;
    }

    return (DictionaryCtxt*)ctxt;

 errExit:
    if ( !!ctxt ) {
        XP_ASSERT( ctxt == &tDictBuf );
    }
    return NULL;
} /* palm_dictionary_make */

void
dict_splitFaces( DictionaryCtxt* dict, const XP_U8* bytes,
                 XP_U16 nBytes, XP_U16 nFaces )
{
    XP_U16 facesLen = nFaces * 2;
    XP_UCHAR* faces = XP_MALLOC( dict->mpool, facesLen );
    XP_UCHAR** ptrs = XP_MALLOC( dict->mpool, nFaces * sizeof(ptrs[0]));
    XP_U16 ii;
    XP_UCHAR* next = faces;

    for ( ii = 0; ii < nFaces; ++ii ) {
        ptrs[ii] = next;
        *next++ = *bytes++;
        *next++ = '\0';
    }
    XP_ASSERT( next == faces + facesLen );
    XP_ASSERT( !dict->faces );
    dict->faces = faces;
    XP_ASSERT( !dict->facePtrs );
    dict->facePtrs = ptrs;
} /* dict_splitFaces */

static XP_U16
countSpecials( FaceType* ptr, UInt16 nChars )
{
    XP_U16 result = 0;

    while ( nChars-- ) {
        FaceType face;
#ifdef NODE_CAN_4
        XP_ASSERT( sizeof(face) == 2 );
        face = READ_UNALIGNED16( ptr );
        ++ptr;
#else
        face = *ptr++;
#endif
        if ( IS_SPECIAL(face) ) {
            ++result;
        }
    }
    return result;
} /* countSpecials */

/* We need an array of ptrs to bitmaps plus an array of ptrs to the text
 * equivalents.
 */
static void
setupSpecials( MPFORMAL PalmDictionaryCtxt* ctxt, 
               Xloc_specialEntry* specialStart, XP_U16 nSpecials )
{
    XP_U16 i;
    char* base;
    XP_UCHAR** chars;
    SpecialBitmaps* bitmaps;

    base = (char*)specialStart;
    chars = XP_MALLOC( mpool, nSpecials * sizeof(*chars) );
    bitmaps = XP_MALLOC( mpool, nSpecials * sizeof(*bitmaps) );

    XP_MEMSET( bitmaps, 0, nSpecials * sizeof(*bitmaps ) );

    for ( i = 0; i < nSpecials; ++i ) {
        XP_U16 hasLarge, hasSmall;

        chars[i] = specialStart->textVersion;

        /* This may not work!  Get rid of NTOHS???? */
        hasLarge = READ_UNALIGNED16( &specialStart->hasLarge );
        if ( hasLarge ) {
            bitmaps[i].largeBM = base + hasLarge;
        }
        hasSmall = READ_UNALIGNED16( &specialStart->hasSmall );
        if ( hasSmall ) {
            bitmaps[i].smallBM = base + hasSmall;
        }
        ++specialStart;
    }

    ctxt->super.bitmaps = bitmaps;
    ctxt->super.chars = chars;
} /* setupSpecials */

static void
palm_dictionary_destroy( DictionaryCtxt* dict )
{
    PalmDictionaryCtxt* ctxt = (PalmDictionaryCtxt*)dict;

    if ( ctxt->refCount != NO_REFCOUNT && --ctxt->refCount == 0 ) {
        short i;
        DmOpenRef dbRef;
        dawg_header* headerRecP;

        if ( !!ctxt->super.name ) {
            /* Remove from dict list */
            removeFromDictCache( ctxt->dl, ctxt->super.name );
        }

        dbRef = ctxt->dbRef;
        headerRecP = ctxt->headerRecP;

        XP_ASSERT( !!dbRef );
    
        MemPtrUnlock( ctxt->super.countsAndValues - 2 );//sizeof(Xloc_header) );

        XP_FREE( dict->mpool, ctxt->super.facePtrs );
        XP_FREE( dict->mpool, ctxt->super.faces );

#ifdef XWFEATURE_COMBINEDAWG
        /* Try first to delete the feature. */
        if ( FtrPtrFree( APPID, DAWG_STORE_FEATURE ) == ftrErrNoSuchFeature ) {
#endif
            for ( i = 0; i < ctxt->nRecords; ++i ) {
                XP_ASSERT( !!ctxt->dictStarts[i].array );
                MemPtrUnlock( ctxt->dictStarts[i].array );
            }
#ifdef XWFEATURE_COMBINEDAWG
        } else {
            XP_ASSERT( ctxt->dictStarts[0].array == NULL );
        }
#endif

        MemPtrUnlock( headerRecP );

        DmCloseDatabase( dbRef );

        if ( !!ctxt->super.name ) {
            XP_FREE( dict->mpool, ctxt->super.name );
        }
        if ( !!ctxt->super.bitmaps ) {
            XP_FREE( dict->mpool, ctxt->super.bitmaps );
        }
        if ( !!ctxt->super.chars ) {
            XP_FREE( dict->mpool, ctxt->super.chars );
        }

        /* if we copied the db to memory we should nuke it, since user would
           have copied it himself if he wanted it here permanently */
        if ( ctxt->location == DL_VFS ) {
            DmDeleteDatabase( ctxt->cardNo, ctxt->dbID );
        }

        XP_FREE( dict->mpool, dict );
    }
} /* palm_dictionary_destroy */

#ifdef OVERRIDE_EDGE_FOR_INDEX
static array_edge* 
palm_dict_edge_for_index_multi( const DictionaryCtxt* dict, XP_U32 index )
{
    PalmDictionaryCtxt* ctxt = (PalmDictionaryCtxt*)dict;
    array_edge* result;
    
    XP_ASSERT( index < dict->numEdges );

    if ( index == 0 ) {
        result = NULL;
    } else {
        DictStart* headerP;
        for ( headerP = &ctxt->dictStarts[1];
              index >= headerP->indexStart;
              ++headerP ) {
            /* do nothing */
        }
        --headerP;
        index -= headerP->indexStart;

        /* To avoid linking in __mulsi3, do the math without a variable  */
        if ( 0 ) {
#ifdef NODE_CAN_4
        } else if ( dict->nodeSize == 4 ) {
            index *= 4;         /* better to shift (<<= 2) here? */
# ifdef DEBUG
        } else if ( dict->nodeSize != 3 ) {
            XP_ASSERT( 0 );
# endif
#endif
        } else {
            index *= 3;
        }
        result = headerP->array + index;
    }

    return result;
} /* dict_edge_for_index */

#endif
