// -*-mode: C; fill-column: 78; c-basic-offset: 4; -*-
/* 
 * Copyright 2001 by Eric House (fixin@peak.org).  All rights reserved.
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
#include "dictnry.h"
}

class CAskLetterWindow : public CWindow {
 private:
    Tile blank;
    DictionaryCtxt* fDict;
    XP_UCHAR* resultP;
    CList* list;		/* my own subclass, of course */
 public:
    CAskLetterWindow( DictionaryCtxt* dict, XP_UCHAR* resultP );
    S32 MsgHandler( MSG_TYPE type, CViewable *object, S32 data );
};
