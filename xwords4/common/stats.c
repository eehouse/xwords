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

#include "stats.h"

typedef struct StatsState {
    // XP_U32 stats[STS_KEY_COUNT];
} StatsState;

void
sts_init( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe) )
{
    LOG_FUNC();
    StatsState* ss = XP_CALLOC( duc->mpool, sizeof(*ss) );
    duc->statsState = ss;
}

void
sts_cleanup( XW_DUtilCtxt* dutil )
{
    XP_USE( dutil );
    XP_FREEP( dutil->mpool, &dutil->statsState );
}

cJSON*
sts_export( XW_DUtilCtxt* XP_UNUSED(dutil), XWEnv XP_UNUSED(xwe) )
{
    cJSON* result = cJSON_CreateObject();
    return result;
}

/* void */
/* sts_increment( XW_DUtilCtxt* dutil, XWEnv XP_UNUSED(xwe), STS_KEY key ) */
/* { */
/*     StatsState* ss = dutil->statsState; */
/*     ++ss->stats[key]; */
/* } */

void
sts_clearAll( XW_DUtilCtxt* XP_UNUSED(dutil), XWEnv XP_UNUSED(xwe) )
{
}
