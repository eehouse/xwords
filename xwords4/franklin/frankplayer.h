// -*-mode: C; fill-column: 78; c-basic-offset: 4; -*-
/* 
 * Copyright 2001-2002 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _FRANKPLAYER_H_
#define _FRANKPLAYER_H_

extern "C" {
#include "comtypes.h"
#include "game.h"
}

#include "frankdict.h"
#include "frankdlist.h"

#define NUM_SIZES 6

class CPlayersWindow : public CWindow {
 private:
    CurGameInfo fLocalGI;	/* local copy; discard if cancel */
    BOOL* resultP;		/* where to write ok-or-cancel */
    CurGameInfo* fGIRef;	/* copy local to here if not cancel */
    CMenu* countMenu;		/* need to preserve in order to delete */
    CMenu* dictMenu;		/* need to preserve in order to delete */
    CMenu* sizeMenu;
    CMenu* fPhoniesMenu;
    CCheckbox* fTimerEnabled;
    CTextEdit* fTimerField;
    XP_UCHAR sizeNames[10][NUM_SIZES];
    FrankDictList* fDList;
    BOOL fIsNew;
    XP_UCHAR fNumsBuf[10];      /* saves allocs and frees for menu strings */

 public:
    MPSLOT

 public:
    CPlayersWindow( MPFORMAL CurGameInfo* pi, FrankDictList* dlist, BOOL isNew,
                    BOOL allowCancel, BOOL* cancelledP );
    ~CPlayersWindow();
    S32 MsgHandler( MSG_TYPE type, CViewable *object, S32 data );
 private:
    void DisOrEnable( U16 id, BOOL enable );
    void makeDictMenu();
    void makeSizeMenu();
    void makePhoniesMenu();
    void copyIDString( U16 id, XP_UCHAR** where );
	void adjustVisibility();
};

#endif
