/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights reserved.
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


#ifndef _DICTMGRP_H_
#define _DICTMGRP_H_

#ifdef CPLUS
extern "C" {
#endif

void dmgr_make( XW_DUtilCtxt* dutil );
void dmgr_destroy( XW_DUtilCtxt* dutil, XWEnv xwe );

void dmgr_put( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* key,
               const DictionaryCtxt* dict );
const DictionaryCtxt* dmgr_get( XW_DUtilCtxt* dutil, XWEnv xwe,
                                const XP_UCHAR* key );
void dmgr_remove( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* key );

#ifdef CPLUS
}
#endif

#endif
