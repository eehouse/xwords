/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2023 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "cmdspipe.h"

XP_Bool
cmds_readCmd( CmdBuf* cb, const char* buf )
{
    XP_Bool success = XP_TRUE;
    XP_MEMSET( cb, 0, sizeof(*cb) );

    XP_LOGFF( "got buf: %s", buf );

    gchar** strs = g_strsplit ( buf, " ", 100 );
    if ( 0 == strcmp( strs[0], "null" ) ) {
        cb->cmd = CMD_NONE;
    } else if ( 0 == strcmp( strs[0], "quit" ) ) {
        cb->cmd = CMD_QUIT;
    } else {
        XP_ASSERT(0);
        success = XP_FALSE;
    }
    g_strfreev( strs );

    return success;
}
