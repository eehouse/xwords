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

#ifndef _XWASKDLG_H_
#define _XWASKDLG_H_

extern "C" {
#include "comtypes.h"
#include "xwstream.h"
#include "mempool.h"
}

#include <e32base.h>
#include <eikdialg.h>

class CXWAskDlg : public CEikDialog
{
 public:
    static TBool DoAskDlg( MPFORMAL XWStreamCtxt* aStream, TBool aKillStream );
    static TBool DoAskDlg( MPFORMAL TBuf16<128>* aMessage );

    static void DoInfoDlg( MPFORMAL XWStreamCtxt* aStream, TBool aKillStream );

    ~CXWAskDlg();

 private:
    CXWAskDlg( MPFORMAL XWStreamCtxt* aStream, TBool aKillStream );
    CXWAskDlg( MPFORMAL TBuf16<128>* aMessage);

 private:
    TBool OkToExitL( TInt aKeyCode );
    void PreLayoutDynInitL();

    /* One or the other of these will be set/used */
    XWStreamCtxt* iStream;
    TBuf16<128>* iMessage;

    TBool iKillStream;
    MPSLOT
};

#endif
