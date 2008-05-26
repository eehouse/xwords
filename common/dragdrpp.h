/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2008 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _DRAGDRPP_H_
#define _DRAGDRPP_H_


#include "boardp.h"


#ifdef CPLUS
extern "C" {
#endif

XP_Bool dragDropInProgress( const BoardCtxt* board );
XP_Bool dragDropHasMoved( const BoardCtxt* board );

XP_Bool dragDropStart( BoardCtxt* board, BoardObjectType obj,
                       XP_U16 xx, XP_U16 yy );
XP_Bool dragDropContinue( BoardCtxt* board, XP_U16 xx, XP_U16 yy );
XP_Bool dragDropEnd( BoardCtxt* board, XP_U16 xx, XP_U16 yy, XP_Bool* dragged );

XP_Bool dragDropGetBoardTile( const BoardCtxt* board, XP_U16* col, XP_U16* row );
XP_Bool dragDropIsBeingDragged( const BoardCtxt* board, XP_U16 col, XP_U16 row, 
                                XP_Bool* isOrigin );

/* return locations (0-based indices from left) in tray where a drag has added
 * and removed a tile.  Index larger than MAX_TRAY_TILES means invalid: don't
 * use.
 */
void dragDropGetTrayChanges( const BoardCtxt* board, XP_U16* rmvdIndx, 
                             XP_U16* addedIndx );
XP_Bool dragDropIsDividerDrag( const BoardCtxt* board );
#ifdef XWFEATURE_SEARCHLIMIT
XP_Bool dragDropGetHintLimits( const BoardCtxt* board, BdHintLimits* limits );
#endif


void dragDropTileInfo( const BoardCtxt* board, Tile* tile, XP_Bool* isBlank );

#ifdef CPLUS
}
#endif

#endif  /* __DRAGDRPP_H_ */
