/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */ 
/* 
 * Copyright 1997-2020 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#include <glib.h>
#include <ncurses.h>

#include "cursesmenu.h"
#include "xptypes.h"
#include "comtypes.h"
#include "cursesmain.h"
#include "linuxmain.h"
#include "gsrcwrap.h"

struct CursesMenuState {
    WINDOW* menuWin;
    GSList* menuLists;
};

static gboolean
handle_stdin( GIOChannel* XP_UNUSED_DBG(source), GIOCondition condition,
              gpointer data )
{
    if ( 0 != (G_IO_IN & condition) ) {
#ifdef DEBUG
        gint fd = g_io_channel_unix_get_fd( source );
        XP_ASSERT( 0 == fd );
#endif
        CursesMenuState* state = (CursesMenuState*)data;
        int ch = wgetch( state->menuWin );
        cmenu_handleKeyEvent( state, ch );
    }
    return TRUE;
}


CursesMenuState*
cmenu_init( WINDOW* mainWindow )
{
    CursesMenuState* result = g_malloc0( sizeof(*result) );

    int width, height;
    getmaxyx( mainWindow, height, width );
    result->menuWin = newwin( MENU_WINDOW_HEIGHT, width, 
                              height-MENU_WINDOW_HEIGHT, 0 );
    nodelay( result->menuWin, 1 );		/* don't block on getch */

    ADD_SOCKET( result, 0, handle_stdin );

    return result;
}

void
cmenu_dispose( CursesMenuState* state )
{
    XP_ASSERT( !!state );
    g_free( state );
}

typedef struct _MenuListElem {
    MenuList* list;
    void* closure;
} MenuListElem;
#define PUSH_TOKEN ((MenuListElem*)-1)

void
cmenu_pop( CursesMenuState* state )
{
    /* pop off the front of the list until we've popped a PUSH_TOKEN */
    for ( ; ; ) {
        MenuListElem* elem = state->menuLists->data;
        state->menuLists = g_slist_remove_link( state->menuLists,
                                                state->menuLists );
        if ( PUSH_TOKEN == elem ) {
            break;
        } else {
            g_free( elem );
        }
    }
    cmenu_draw( state );
}

static void
addMenus( CursesMenuState* state, void* closure, va_list ap )
{
    for ( ; ; ) {
        MenuList* param = va_arg(ap, MenuList*);
        if ( !param ) {
            break;
        }
        MenuListElem* elem = g_malloc0( sizeof( *elem ) );
        elem->closure = closure;
        elem->list = param;
        state->menuLists = g_slist_prepend( state->menuLists, elem );
    }
    cmenu_draw( state );
}

void
cmenu_addMenus( CursesMenuState* state, void* closure, ... )
{
    va_list ap;
    va_start( ap, closure );
    addMenus( state, closure, ap );
    va_end(ap);
}

void cmenu_push( CursesMenuState* state, void* closure, ... )
{
    if ( !!state ) {
        state->menuLists = g_slist_prepend( state->menuLists, PUSH_TOKEN );

        va_list ap;
        va_start( ap, closure );
        addMenus( state, closure, ap );
        va_end(ap);
    }
}

void
cmenu_removeMenus( CursesMenuState* state, ... )
{
    va_list ap;
    va_start( ap, state );
    for ( ; ; ) {
        MenuList* param = va_arg(ap, MenuList*);
        if ( !param ) {
            break;
        }
        for ( GSList* iter = state->menuLists; !!iter; iter = iter->next ) {
            MenuListElem* elem = iter->data;
            if ( elem->list == param ) {
                state->menuLists = g_slist_remove( state->menuLists, elem );
                break;
            }
        }
    }    
    va_end( ap );

    cmenu_draw( state );
}

bool
cmenu_handleKeyEvent( CursesMenuState* state, char ch )
{
    bool result = false;
    for ( GSList* iter = state->menuLists; !!iter; iter = iter->next ) {
        const MenuListElem* elem = iter->data;
        if ( PUSH_TOKEN == elem ) {
            break;
        }
        for ( const MenuList* list = elem->list; !!list->handler; ++list ) {
            if ( list->key == ch ) {
                result = (*list->handler)(elem->closure, ch);
                goto done;
            }
        }
    }
 done:
    return result;
}

static void
fmtMenuItem( const MenuList* item, char* buf, int maxLen )
{
    snprintf( buf, maxLen, "%s %s", item->keyDesc, item->desc );
}

void
cmenu_draw( const CursesMenuState* state )
{
    WINDOW* win = state->menuWin;
    wclear( win );
    int line = 0;
    int col = 0;
    int maxLen = 0;
    for ( GSList* iter = state->menuLists; !!iter; iter = iter->next ) {
        const MenuListElem* elem = iter->data;
        if ( PUSH_TOKEN == elem ) {
            break;
        }
        for ( MenuList* list = elem->list; !!list->handler; ++list ) {
            char buf[32];
            fmtMenuItem( list, buf, sizeof(buf) );
            int len = strlen(buf);
            if ( maxLen < len ) {
                maxLen = len;
            }

            mvwaddstr( win, line, col, buf );
            if ( ++line >= MENU_WINDOW_HEIGHT ) {
                line = 0;
                col += maxLen + 1;
                maxLen = 0;
            }
        }
    }
    wrefresh( win );
}
