/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2003 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _CONNSDLG_H_
#define _CONNSDLG_H_

#include <Event.h>


Boolean ConnsFormHandleEvent( EventPtr event );

#define CONNS_PARAM_ROLE_INDEX 0
#define CONNS_PARAM_ADDR_INDEX 1

#define PopupConnsForm( g, h, addrP ) {                         \
    (g)->dlgParams[CONNS_PARAM_ROLE_INDEX] = (XP_U32)(h);       \
    (g)->dlgParams[CONNS_PARAM_ADDR_INDEX] = (XP_U32)(addrP);   \
    FrmPopupForm( XW_CONNS_FORM );                              \
}

#endif
