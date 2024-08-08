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

#ifndef _STATS_H_
#define _STATS_H_

#include "dutil.h"

void sts_init( XW_DUtilCtxt* duc );
void sts_cleanup( XW_DUtilCtxt* dutil, XWEnv xwe );

typedef enum {
    STAT_NONE = 0,
    STAT_MQTT_RCVD,
    STAT_MQTT_SENT,

    STAT_REG_NOROOM,

    STAT_NEW_SOLO,
    STAT_NEW_TWO,
    STAT_NEW_THREE,
    STAT_NEW_FOUR,
    STAT_NEW_REMATCH,

    STAT_NSTATS,
} STAT;

void sts_increment( XW_DUtilCtxt* dutil, XWEnv xwe, STAT stat );

cJSON* sts_export( XW_DUtilCtxt* duc, XWEnv xwe );
void sts_clearAll( XW_DUtilCtxt* duc, XWEnv xwe );

#endif
