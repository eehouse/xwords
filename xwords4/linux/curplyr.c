/* 
 * Copyright 2026 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "curplyr.h"
#include "cursesdlgutil.h"

enum {
    SEL_PLAYER,
    SEL_ROLE,
    SEL_CANCEL,
    SEL_OK,

    SEL_NSELS,
};

typedef enum {
    ROLE_HUMAN,
    ROLE_ROBOT,
    ROLE_GUEST,
    ROLE_NROLES,
} PLAYER_ROLE;

typedef struct _CurPlayerState {
    WINDOW* win;
    LocalPlayer player;
    EditState es;
    int sel;
    PLAYER_ROLE role;
    int buttonsLine;
    bool confirmed, cancelled;
} CurPlayerState;

static PLAYER_ROLE
roleFromLP( const LocalPlayer* lp )
{
    PLAYER_ROLE role;
    if ( !lp->isLocal ) {
        role = ROLE_GUEST;
    } else if ( lp->robotIQ == 0 ) {
        role = ROLE_HUMAN;
    } else {
        role = ROLE_ROBOT;
    }
    return role;
}

static void
roleToLP( CurPlayerState* cps )
{
    cps->player.isLocal = cps->role != ROLE_GUEST;
    if ( cps->player.isLocal ) {
        cps->player.robotIQ = cps->role == ROLE_HUMAN? 0 : 1;
    }
}

static const char*
roleStr( PLAYER_ROLE role )
{
    const char* str = NULL;
    switch ( role ) {
    case ROLE_HUMAN: str = "Human"; break;
    case ROLE_ROBOT: str = "Robot"; break;
    case ROLE_GUEST: str = "Guest"; break;
    default: XP_ASSERT(0);
    }
    return str;
}

void
roleName( const LocalPlayer* lp, char buf[32], size_t buflen )
{
    PLAYER_ROLE role = roleFromLP( lp );
    const char* str = roleStr( role );
    XP_SNPRINTF( buf, buflen, "%s", str );
}

static void
updateWindow( CurPlayerState* cps )
{
    LOG_FUNC();
    drawEdit( &cps->es, cps->sel == SEL_PLAYER );

    const char* role = roleStr( cps->role );
    mvwaddstr( cps->win, 3, 1, role );

    const char* buttons[] = { "ROLE", "CANCEL", "OK" };
    short curSelButton = -1;
    switch ( cps->sel ) {
    case SEL_ROLE: curSelButton = 0; break;
    case SEL_CANCEL: curSelButton = 1; break;
    case SEL_OK: curSelButton = 2; break;
    }
    drawButtons( cps->win, 5, cps->buttonsLine, VSIZE(buttons), curSelButton, buttons );
    
    wrefresh( cps->win );
}

static bool
playerEditKeyProc( int key, void* closure )
{
    CurPlayerState* cps = (CurPlayerState*)closure;
    switch ( key )  {
    case 0x161:                 /* shift-tab */
        cps->sel = (cps->sel + SEL_NSELS - 1) % SEL_NSELS;
        break;
    case '\t': cps->sel = (cps->sel + 1) % SEL_NSELS;
        break;
    case '\r':
    case '\n':
        switch (cps->sel) {
        case SEL_ROLE: cps->role = (1 + cps->role) % ROLE_NROLES; break;
        case SEL_OK: cps->confirmed = true; break;
        case SEL_CANCEL: cps->cancelled = true; break;
        }
    default:
        if ( SEL_PLAYER == cps->sel ) {
            handleEdit( &cps->es, key );
        }

    }
    updateWindow( cps );
    return cps->confirmed || cps->cancelled;
}

bool
editPlayerDlg( LaunchParams* params, WINDOW* parent, LocalPlayer* player )
{
    LOG_FUNC();

    CurPlayerState cps = {
        .player = *player,
        .win = makeCenteredBox( parent, 30, 9 ),
        .buttonsLine = 8,
    };

    cps.role = roleFromLP( &cps.player );

    initEdit( &cps.es, cps.win, 2, cps.player.name );
    updateWindow( &cps );

    CursesAppGlobals* aGlobals = (CursesAppGlobals*)params->cag;
    startModalAlert( aGlobals, cps.win, XP_TRUE, playerEditKeyProc, &cps );

    delwin( cps.win );
    wrefresh( parent );

    bool confirmed = cps.confirmed;
    if ( confirmed ) {
        size_t size = VSIZE(cps.player.name);
        getEditText( &cps.es, cps.player.name, &size );

        roleToLP( &cps );
        
        *player = cps.player;
    }

    return confirmed;
}
