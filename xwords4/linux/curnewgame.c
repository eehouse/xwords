/* 
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights reserved.
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


#ifdef PLATFORM_NCURSES

#include "curnewgame.h"

gboolean
curNewGameDialog( LaunchParams* params, CurGameInfo* gi,
                  CommsAddrRec* addr, XP_Bool isNewGame,
                  XP_Bool fireConnDlg )
{
    XP_USE(params);
    XP_USE(gi);
    XP_USE(addr);
    XP_USE(isNewGame);
    XP_USE(fireConnDlg);
    XP_ASSERT(0);               /* not implemented. :-) */
    return false;
}

#endif /* PLATFORM_NCURSES */
