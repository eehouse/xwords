/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).
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

#include "symgmmgr.h"

extern "C" {
#include "xwstream.h"
}
#include "symutil.h"

#if defined SERIES_60
# include <eikenv.h>
#endif
#include <apparc.h>

_LIT( kGameTypeExt, ".xwg" );


// private
CXWGamesMgr::CXWGamesMgr( MPFORMAL CCoeEnv* aCoeEnv, TFileName* aBasePath )
    : iCoeEnv(aCoeEnv), iGameCount(0)
{
    MPASSIGN( this->mpool, mpool );
    
    iDir.Copy( *aBasePath );
    iDir.Append( _L("xwgames" ) );
    iDir.Append( _L("\\" ) );
} // CXWGamesMgr

/* static */ CXWGamesMgr* 
CXWGamesMgr::NewL( MPFORMAL CCoeEnv* aCoeEnv, TFileName* aBasePath )
{
    CXWGamesMgr* self = new CXWGamesMgr( MPPARM(mpool) aCoeEnv, aBasePath );
    User::LeaveIfNull( self );

    // Create the games directory if it doesn't already exist
#if defined SERIES_80
    RFs fs = aCoeEnv->FsSession();
#elif defined SERIES_60
    RFs fs = CEikonEnv::Static()->FsSession();
#endif
    TInt err = fs.MkDirAll( self->iDir );

    if ( err != KErrNone && err != KErrAlreadyExists ) {
        XP_LOGF( "MkDir failed: %d", err );
        User::Leave( err );
    }

	err = fs.SetAtt( self->iDir, KEntryAttHidden, KEntryAttNormal );

    self->BuildListL();

    return self;
} // NewL

void
CXWGamesMgr::BuildListL()
{
    delete iNamesList;

    RFs fs = iCoeEnv->FsSession();

    CDir* file_list;
    TFindFile file_finder( fs );
    TBuf16<6> gameNamePat( _L("*") );
    gameNamePat.Append( kGameTypeExt );
    TInt err = file_finder.FindWildByDir( gameNamePat, iDir, file_list );
    // Leave it empty (null) if nothing in the list.
    if ( err == KErrNone ) {
        CleanupStack::PushL( file_list );

        TInt gameCount = file_list->Count();
        iNamesList = new (ELeave)CDesC16ArrayFlat( gameCount );

        TInt i;
        for ( i = 0; i < file_list->Count(); i++ ) {
            TParse fullentry;
            fullentry.Set( (*file_list)[i].iName, &file_finder.File(), NULL );
            iNamesList->AppendL( fullentry.Name() );
        }
        CleanupStack::PopAndDestroy(file_list); // file_list
    }
} // BuildListL

TInt
CXWGamesMgr::GetNGames() const
{
    if ( !iNamesList ) {
        return 0;
    } else {
        return iNamesList->MdcaCount();
    }
} // GetNGames

CDesC16Array*
CXWGamesMgr::GetNames() const
{
    return iNamesList;
} // GetNames

TBool
CXWGamesMgr::Exists( TGameName* aName )
{
    RFs fs = iCoeEnv->FsSession();

    TFileName nameD;
    GameNameToPath( &nameD, aName );

    TUint attValue;
    TInt err = fs.Att( nameD, attValue );
    XP_LOGF( "fs.Att(%S) => %d", aName, err );
    return err == KErrNone;
}

TBool
CXWGamesMgr::IsLegalName( const TGameName* aName )
{
    RFs fs = iCoeEnv->FsSession();
    return fs.IsValidName( *aName );
} /* IsLegalName */

void
CXWGamesMgr::GameNameToPath( TFileName* aPath, const TDesC16* aName )
{
    aPath->Copy( iDir );
    aPath->Append( *aName );
    aPath->Append( kGameTypeExt );
}

void
CXWGamesMgr::MakeDefaultName( TGameName* aName )
{
    RFs fs = iCoeEnv->FsSession();
    TFileName nameD;
    TGameName tmpName( _L("game") );
    GameNameToPath( &nameD, &tmpName );

    TInt err = CApaApplication::GenerateFileName( fs, nameD );
    User::LeaveIfError( err );

    TParse nameParser;
    nameParser.Set( nameD, NULL, NULL );

    aName->Copy( nameParser.Name() );
} // MakeDefaultName

void 
CXWGamesMgr::StoreGameL( const TGameName* aName, /* does not have extension */
                         XWStreamCtxt* aStream )
{
    RFs fs = iCoeEnv->FsSession();

    // write the stream to a file
    TFileName nameD;
    GameNameToPath( &nameD, aName );
    RFile file;
    TInt err = file.Replace( fs, nameD, EFileWrite );
    XP_ASSERT( err == KErrNone );
    User::LeaveIfError( err );
    CleanupClosePushL(file);

    TInt siz = stream_getSize( aStream );
    TUint8* buf = new (ELeave) TUint8[siz];
    stream_getBytes( aStream, buf, SC(XP_U16, siz) );
    TPtrC8 tbuf( buf, siz );
    err = file.Write( tbuf, siz );
    XP_ASSERT( err == KErrNone );
    delete [] buf;

    CleanupStack::PopAndDestroy( &file ); // file

    BuildListL();
} // StoreGameL

void
CXWGamesMgr::LoadGameL( const TGameName* aName, XWStreamCtxt* aStream )
{
    RFs fs = iCoeEnv->FsSession();

    TFileName nameD;
    GameNameToPath( &nameD, aName );

    RFile file;
    TInt err = file.Open( fs, nameD, EFileRead );
    if ( err != KErrNone ) {
        XP_LOGF( "file.Open() => %d", err );
    }
    User::LeaveIfError( err );
    CleanupClosePushL(file);

    for ( ; ; ) {
        TBuf8<256> buf;
        file.Read( buf, buf.MaxLength() );
        TInt nRead = buf.Size();
        if ( nRead <= 0 ) {
            break;
        }
        stream_putBytes( aStream, (void*)buf.Ptr(), SC(XP_U16, nRead) );
    }

    CleanupStack::PopAndDestroy( &file );
} // LoadGame

TBool
CXWGamesMgr::DeleteSelected( TInt aIndex )
{
    TPtrC16 name = (*iNamesList)[aIndex];
    return DeleteFileFor( &name );
} /* DeleteSelected */

TBool
CXWGamesMgr::DeleteFileFor( TPtrC16* aName )
{
    TFileName nameD;
    GameNameToPath( &nameD, aName );

    RFs fs = iCoeEnv->FsSession();
    TInt err = fs.Delete( nameD );
    TBool success = err == KErrNone;
    if ( success ) {
        BuildListL();
    }
    return success;
}

void
CXWGamesMgr::Rename( const TDesC16* aCurName, const TDesC16* aNewName )
{
    TFileName newName;
    GameNameToPath( &newName, aNewName );

    TFileName anOldName;
    GameNameToPath( &anOldName, aCurName );

    RFs fs = iCoeEnv->FsSession();
    TInt err = fs.Rename( anOldName, newName );
    XP_ASSERT( err == KErrNone );
    User::LeaveIfError( err );

    BuildListL();
} /* Rename */
