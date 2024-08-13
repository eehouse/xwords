/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _TIMERS_H_
#define _TIMERS_H_

# ifdef DUTIL_TIMERS

# include "dutil.h"

void tmr_init( XW_DUtilCtxt* dutil );
void tmr_cleanup( XW_DUtilCtxt* dutil, XWEnv xwe );

/* Pass false for fired if we're clearing unfired timers, e.g. on shutdown */
typedef void (*TimerProc)(void* closure, XWEnv xwe, XP_Bool fired);
TimerKey tmr_set( XW_DUtilCtxt* dutil, XWEnv xwe, XP_U32 inWhenMS,
                  TimerProc proc, void* closure );
void tmr_fired( XW_DUtilCtxt* dutil, XWEnv xwe, TimerKey key );

# else
# define tmr_init( dutil )
# define tmr_cleanup( dutil, xwe )
# endif
#endif
