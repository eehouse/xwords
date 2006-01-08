/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _PREFSDLG_H_
#define _PREFSDLG_H_

#include <Event.h>

#include "palmmain.h"

/* How prefs work.
 *
 * Prefs can be called either directly from the main form or from the new
 * game form.  If it's called directly, it creates and initializes a
 * PrefsDlgState instance.  If it's called indirectly, the caller does that
 * for it (or in the newGame case may do it anyway so it has defaults to use
 * if it's never called).  If the user cancels the direct call, any changes
 * are ignored.  If the user cancels when called from the newGame form we'll
 * re-init the structure.
 */

/* both newgame and prefs need to know about this */
Boolean PrefsFormHandleEvent( EventPtr event );
void GlobalPrefsToLocal( PalmAppGlobals* globals );
XP_Bool LocalPrefsToGlobal( PalmAppGlobals* globals );

#endif
