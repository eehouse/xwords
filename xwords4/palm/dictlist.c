/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/****************************************************************************
 *									    *
 *	Copyright 1999 - 2003 by Eric House (fixin@peak.org).  All rights reserved.
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
 *									    *
 ****************************************************************************/

#include <PalmTypes.h>
#include <Form.h>
#include <VFSMgr.h>
#include <FeatureMgr.h>

#include "callback.h"
#include "dictlist.h"
#include "palmmain.h"
#include "palmutil.h"
#include "palmdict.h"
#include "strutils.h"
#include "xwords4defines.h"
#include "LocalizedStrIncludes.h"

#define TYPE_DAWG 'DAWG'
#ifdef NODE_CAN_4
# define TYPE_XWRDICT 'XwrD'
#else
# define TYPE_XWRDICT 'Xwr3'
#endif

//////////////////////////////////////////////////////////////////////////////
// typedef and #defines
//////////////////////////////////////////////////////////////////////////////

struct PalmDictList {
    XP_U16 nDicts;
    DictListEntry dictArray[1];
};

//////////////////////////////////////////////////////////////////////////////
// Prototypes
//////////////////////////////////////////////////////////////////////////////
static PalmDictList* dictListMakePriv( MPFORMAL XP_U32 creatorSought );


XP_Bool
getNthDict( const PalmDictList* dl, short n, DictListEntry** dle )
{
    XP_Bool exists = !!dl && (dl->nDicts > n);
    if ( exists ) {
        *dle = (DictListEntry*)&dl->dictArray[n];
    }
    return exists;
} /* getNthDict */

XP_Bool
getDictWithName( const PalmDictList* dl, XP_UCHAR* name, 
                 DictListEntry** dlep )
{
    XP_Bool result = XP_FALSE;

    if ( !!dl ) {
        XP_U16 i;
        XP_UCHAR* extName;
        XP_UCHAR oldChName = '\0'; /* shut compiler up */
        DictListEntry* dle = (DictListEntry*)dl->dictArray;

        extName = (XP_UCHAR*)StrStr((const char*)name, 
                                    (const char*)".pdb" );

        if ( !!extName ) {
            oldChName = *extName;
            *extName = '\0';
        }

        for ( i = 0; !result && i < dl->nDicts; ++i ) {
            XP_UCHAR* extCand;
            XP_UCHAR oldChCand = '\0';

            extCand = (XP_UCHAR*)StrStr((const char*)dle->baseName, ".pdb" );
            if ( !!extCand ) {
                oldChCand = *extCand;
                *extCand = '\0';
            }

            if ( 0 == XP_STRCMP( (const char*)name, 
                                 (const char*)dle->baseName ) ) {

                *dlep = dle;
                result = XP_TRUE;
            }

            if ( !!extCand ) {
                *extCand = oldChCand;
            }

            ++dle;
        }

        if ( !!extName ) {
            *extName = oldChName;
        }

    }
    return result;
} /* getDictWithName */

void
cacheDictForName( PalmDictList* dl, XP_UCHAR* dictName, 
                  DictionaryCtxt* dict )
{
    DictListEntry* dle;
    (void)getDictWithName(dl, dictName, &dle );
    XP_ASSERT( getDictWithName(dl, dictName, &dle ) );
    XP_ASSERT( !dle->dict );

    dle->dict = dict;
} /* cacheDictForName */

void
removeFromDictCache( PalmDictList* dl, XP_UCHAR* dictName, 
                     DictionaryCtxt* dict )
{
    DictListEntry* dle;
    (void)getDictWithName( dl, dictName, &dle );
    XP_ASSERT( getDictWithName( dl, dictName, &dle ) );
    XP_ASSERT( !!dle->dict );
    XP_ASSERT( dle->dict == dict );

    dle->dict = NULL;
} /* removeFromDictCache */

static void
addEntry( MPFORMAL PalmDictList** dlp, DictListEntry* dle )
{
    PalmDictList* dl = *dlp;
    DictListEntry* ignore;

    if ( !dl ) {
        dl = (PalmDictList*)XP_MALLOC( mpool, sizeof(*dl) );
        XP_MEMSET( dl, 0, sizeof(*dl) );
    }

    if ( !getDictWithName( dl, dle->baseName, &ignore ) ) {
        XP_U16 size = sizeof(*dl);
        size += dl->nDicts * sizeof( dl->dictArray[0] );

        dl = (PalmDictList*)XP_REALLOC( mpool, (XP_U8*)dl, size );

        dle->dict = NULL;
        XP_MEMCPY( &dl->dictArray[dl->nDicts++], dle, 
                   sizeof( dl->dictArray[0] ) );
    }

    *dlp = dl;
} /* addEntry */

static void
searchDir( MPFORMAL PalmDictList** dlp, UInt16 volNum, unsigned char separator,
           unsigned char* path, XP_U16 pathSize, XP_U32 creatorSought )
{ 
    Err err;
    FileRef dirRef;
    XP_U16 pathLen = XP_STRLEN( (const char*)path );

    err = VFSFileOpen( volNum, (const char*)path, vfsModeRead, &dirRef );
    if ( err == errNone ) {
        UInt32 dEnum = vfsIteratorStart;
        FileInfoType fit;

        fit.nameP = (char*)path + pathLen;

        while ( dEnum != vfsIteratorStop ) {
            XP_UCHAR* ext;
            fit.nameBufLen = pathSize - pathLen;
            err = VFSDirEntryEnumerate( dirRef, &dEnum, &fit );

            if ( err != errNone ) {
                break;
            }
            
            if ( (fit.attributes & vfsFileAttrDirectory) != 0 ) {
                XP_U16 len = XP_STRLEN((const char*)path);
                path[len] = separator;
                path[len+1] = '\0';
                searchDir( MPPARM(mpool) dlp, volNum, separator, 
                           path, pathSize, creatorSought );
            } else if ( (ext = (XP_UCHAR*)StrStr( (const char*)path, ".pdb" ))
                        != NULL ) {

                /* find out if it's a crosswords dict. */
                FileRef fileRef;
                UInt32 type, creator;

                err = VFSFileOpen( volNum, (const char*)path, vfsModeRead,
                                   &fileRef );
                if ( err == errNone ) {
                    err = VFSFileDBInfo( fileRef, NULL, /* name */
                                         NULL, /* attributes */
                                         NULL, /* versionP   */
                                         NULL, /* crDateP    */
                                         NULL, NULL, /*UInt32 *modDateP, UInt32 *bckUpDateP,*/
                                         NULL, NULL, /*UInt32 *modNumP, MemHandle *appInfoHP,*/
                                         NULL,  /*MemHandle *sortInfoHP, */
                                         &type, &creator, 
                                         NULL ); /* nRecords */
                    VFSFileClose( fileRef );
                    
                    if ( (err == errNone) && (type == TYPE_DAWG) && 
                         (creator == creatorSought ) ) {
                        DictListEntry dl;

                        dl.path = copyString( MPPARM(mpool) path );
                        dl.location = DL_VFS;
                        dl.u.vfsData.volNum = volNum;
                        dl.baseName = dl.path + pathLen;

                        addEntry( MPPARM(mpool) dlp, &dl );
                    }
                }
            }
        }

        path[pathLen] = '\0';
        VFSFileClose( dirRef );
    }

} /* searchDir */

static void
tryVFSSearch( MPFORMAL PalmDictList** dlp, XP_U32 creatorSought )
{
    Err err;
    UInt16 volNum;
    UInt32 vEnum;

    vEnum = vfsIteratorStart;
    while ( vEnum != vfsIteratorStop ) {
        unsigned char pathStr[265];
        UInt16 bufLen;

        err = VFSVolumeEnumerate( &volNum, &vEnum );
        if ( err != errNone ) {
            break;
        }

        bufLen = sizeof(pathStr);
        err = VFSGetDefaultDirectory( volNum, ".pdb", (char*)pathStr,
                                      &bufLen );
        
        if ( err == errNone ) {
            pathStr[1] = '\0';
            searchDir( MPPARM(mpool) dlp, volNum, pathStr[0],
                       pathStr, sizeof(pathStr), creatorSought );
        }
    }

} /* tryVFSSearch */

/* if we've allocated extra space in the array as an optimization now's when
 * we pull back */
static void
cleanList( PalmDictList** dl )
{
} /* cleanList */

PalmDictList* 
DictListMake( MPFORMAL_NOCOMMA )
{
    return dictListMakePriv( MPPARM(mpool) TYPE_XWRDICT );
}

static PalmDictList* 
dictListMakePriv( MPFORMAL XP_U32 creatorSought )
{
    Err err;
    DmSearchStateType stateType;
    UInt32 vers;
    PalmDictList* dl = NULL;
    UInt16 cardNo;
    LocalID dbID;
    Boolean newSearch = true;
    XP_Bool found = false;

    /* first the DM case */
    while ( !found ) {
        err = DmGetNextDatabaseByTypeCreator( newSearch, &stateType, TYPE_DAWG, 
                                              creatorSought,
                                              false,// onlyLatestVers,
                                              &cardNo, &dbID );
        if ( err != 0 ) {
            break;
        } else {
            XP_UCHAR nameBuf[33];
            XP_U16 nameLen;
            DictListEntry dle;

            err = DmDatabaseInfo( cardNo, dbID, (char*)nameBuf, NULL, NULL, NULL,
                                  NULL, NULL, NULL, NULL, NULL, NULL, NULL );
            nameLen = XP_STRLEN( (const char*)nameBuf ) + 1;

            dle.location = DL_STORAGE;
            dle.u.dmData.cardNo = cardNo;
            dle.u.dmData.dbID = dbID;
            dle.path = dle.baseName = XP_MALLOC( mpool, nameLen );
            XP_MEMCPY( dle.path, nameBuf, nameLen );

            addEntry( MPPARM(mpool) &dl, &dle );
        }

        newSearch = false;
    }

    /* then the VFS case */
    err = FtrGet( sysFileCVFSMgr, vfsFtrIDVersion, &vers );
    if ( err == errNone ) {
        tryVFSSearch( MPPARM(mpool) &dl, creatorSought );
    }

    cleanList( &dl );

    return dl;
} /* dictListMakePriv */

void
DictListFree( MPFORMAL PalmDictList* dl )
{
    if ( !!dl ) {
        DictListEntry* dle = dl->dictArray;
        XP_U16 i;

        for ( i = 0; i < dl->nDicts; ++i, ++dle ) {
            XP_FREE( mpool, dle->path );
        }

        XP_FREE( mpool, dl );
    }
} /* dictListFree */

XP_U16
DictListCount( PalmDictList* dl )
{
    if ( !dl ) {
        return 0;
    } else {
        return dl->nDicts;
    }
} /* dictListCount */

#ifdef NODE_CAN_4

static XP_Bool
convertOneDict( UInt16 cardNo, LocalID dbID )
{
    Err err;
    UInt32 creator;
    DmOpenRef ref;
    MemHandle h;
    dawg_header* header;
    dawg_header tmp;
    XP_U16 siz;
    unsigned char charTableRecNum;
    XP_U16 nChars;

    /* now modify the flags */
    ref = DmOpenDatabase( cardNo, dbID, dmModeReadWrite );
    XP_ASSERT( ref != 0 );
    h = DmGetRecord( ref, 0 );
    siz = MemHandleSize( h );
    if ( siz < sizeof(*header) ) {
        MemHandleResize( h, sizeof(*header) );
    }

    tmp.flags = 0x0002;
    header = (dawg_header*)MemHandleLock(h);
    charTableRecNum = header->charTableRecNum;
    DmWrite( header, OFFSET_OF(dawg_header,flags), &tmp.flags, 
             sizeof(tmp.flags) );
    MemHandleUnlock(h);
    DmReleaseRecord( ref, 0, true );

    /* Now convert to 16-bit psuedo-unicode */
    h = DmGetRecord( ref, charTableRecNum );
    XP_ASSERT( !!h );
    nChars = MemHandleSize( h );
    err = MemHandleResize( h, nChars * 2 );
    XP_ASSERT( err == errNone );

    if ( err == errNone ) {
        XP_U16 buf[MAX_UNIQUE_TILES+1];
        XP_S16 i;
        XP_U8* ptr = (XP_U8*)MemHandleLock( h );
        for ( i = 0; i < nChars; ++i ) {
            buf[i] = ptr[i];
        }
        DmWrite( ptr, 0, buf, nChars * sizeof(buf[0]) );
        MemHandleUnlock(h);
    }
    err = DmReleaseRecord( ref, charTableRecNum, true );
    XP_ASSERT( err == errNone );

    err = DmCloseDatabase( ref );
    XP_ASSERT( err == errNone );

    if ( err == errNone ) {
        creator = TYPE_XWRDICT;
        err = DmSetDatabaseInfo( cardNo, dbID, NULL,
                                 NULL, NULL, NULL, 
                                 NULL, NULL, 
                                 NULL, NULL, 
                                 NULL, NULL, 
                                 &creator );
        XP_ASSERT( err == errNone );
    }
    return err == errNone;
} /* convertOneDict */

void
offerConvertOldDicts( PalmAppGlobals* globals )
{
    PalmDictList* dl = dictListMakePriv( MPPARM(globals->mpool) 'Xwr3' );
    XP_U16 count = DictListCount(dl);
    Err err;

    if ( count > 0 && palmaskFromStrId( globals, STR_CONFIRM_CONVERTDICT,
                                        -1, STR_NO ) ) {

        XP_U16 i;
        for ( i = 0; i < count; ++i ) {
            DictListEntry* dle;
            if ( getNthDict( dl, i, &dle ) ) {

                if ( dle->location == DL_STORAGE ) {
                    convertOneDict( dle->u.dmData.cardNo, 
                                    dle->u.dmData.dbID );
                } else { 
                    UInt16 cardNo;
                    LocalID dbID;
                    UInt16 volRefNum = dle->u.vfsData.volNum;

                    XP_ASSERT( dle->location == DL_VFS );

                    XP_LOGF( "trying %s", dle->path );

                    /* copy from SD card to storage, convert, copy back */
                    err = VFSImportDatabaseFromFile( volRefNum,
                                                     (const char*)dle->path,
                                                     &cardNo, &dbID );
                    XP_LOGF( "VFSImportDatabaseFromFile => %d", err );
                    if ( err == errNone && convertOneDict( cardNo, dbID ) ) {

                        err = VFSFileDelete( volRefNum, dle->path );
                        XP_LOGF( "VFSFileDelete=>%d", err );
                        if ( err == errNone ) {

                            err = VFSExportDatabaseToFile( volRefNum,
                                                           (const char*)dle->path,
                                                           cardNo, dbID );
                            
                            XP_LOGF( "VFSExportDatabaseToFile => %d", err );

                            XP_ASSERT( err == errNone );
                            err = DmDeleteDatabase( cardNo, dbID );
                            XP_ASSERT( err == errNone );
                        }
                    }
                }
            }
        }
    }

    DictListFree( MPPARM(globals->mpool) dl );
} /* offerConvertOldDicts */
#endif
