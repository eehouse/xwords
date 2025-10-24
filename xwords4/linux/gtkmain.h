/* Copyright 1997 - 2013 by Eric House (xwords@eehouse.org) All rights
 * reserved.
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

#ifndef _GTKMAIN_H_
#define _GTKMAIN_H_

#include "main.h"
#include "gtkboard.h"

int gtkmain( LaunchParams* params );
void addGTKGame( LaunchParams* params, GameRef gr );
void windowDestroyed( GtkGameGlobals* globals );
void make_rematch( GtkAppGlobals* apg, GameRef parent,
                   XP_Bool archiveAfter, XP_Bool deleteAfter );
void inviteReceivedGTK( void* closure, const NetLaunchInfo* invite );
void msgReceivedGTK( void* closure, const CommsAddrRec* from, XP_U32 gameID,
                     const XP_U8* buf, XP_U16 len );
void gameGoneGTK( void* closure, const CommsAddrRec* from, XP_U32 gameID );
void resizeFromSaved( GtkWidget* window, sqlite3* pDb, const gchar* key );
void saveSize( const GdkEventConfigure* lastSize, sqlite3* pDb, const gchar* key );
void onGameChangedGTK( LaunchParams* params, GameRef gr, GameChangeEvents gces);
void onPositionsChangedGTK( LaunchParams* params, XWArray* positions);
void onGroupChangedGTK( LaunchParams* params, GroupRef grp,
                        GroupChangeEvents gces );
void onGTKMissingDictAdded( LaunchParams* params, GameRef gr,
                            const XP_UCHAR* dictName );
void onGTKDictGone( LaunchParams* params, GameRef gr, const XP_UCHAR* dictName );
void informMoveGTK( LaunchParams* params, GameRef gr, XWStreamCtxt* expl,
                    XWStreamCtxt* words );
void informGameOverGTK( LaunchParams* params, GameRef gr, XP_S16 quitter );

#endif
