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

#ifndef _CURSESMENU_H_
#define _CURSESMENU_H_

#include <ncurses.h>

#ifdef CURSES_SMALL_SCREEN
# define MENU_WINDOW_HEIGHT 1
# define BOARD_OFFSET 0
#else
# define MENU_WINDOW_HEIGHT 5	/* three lines plus borders */
# define BOARD_OFFSET 1
#endif

typedef bool (*CursesMenuHandler)( void* closure, int key );
typedef struct MenuList {
    CursesMenuHandler handler;
    char* desc;
    char* keyDesc;
    char key;
} MenuList;

typedef struct CursesMenuState CursesMenuState;

CursesMenuState* cmenu_init( WINDOW* mainWindow );
void cmenu_dispose( CursesMenuState* state );

void cmenu_clearMenus( CursesMenuState* state );
void cmenu_draw( const CursesMenuState* state );
void cmenu_addMenus( CursesMenuState* state, void* closure, ... );
void cmenu_push( CursesMenuState* state, void* closure, ... );
void cmenu_pop( CursesMenuState* state );
void cmenu_removeMenus( CursesMenuState* state, ... );
bool cmenu_handleKeyEvent( CursesMenuState* state, char ch );

#endif
