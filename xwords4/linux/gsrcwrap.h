/* 
 * Copyright 2020 by Eric House (xwords@eehouse.org).  All rights reserved.
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


#ifndef _GSRCWRAP_H_
#define _GSRCWRAP_H_

guint _wrapIdle( GSourceFunc function, gpointer data, const char* procName,
                 const char* caller );
guint _wrapWatch( gpointer data, int socket, GIOFunc func,
                  const char* procName, const char* caller );

void gsw_logIdles();

#define ADD_ONETIME_IDLE( PROC, CLOSURE )       \
    _wrapIdle( PROC, CLOSURE, #PROC, __func__ )

#define ADD_SOCKET( CLOSURE, SOCKET, PROC )                 \
    _wrapWatch( CLOSURE, SOCKET, PROC, #PROC, __func__ )
    
#endif
