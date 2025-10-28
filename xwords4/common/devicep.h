/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/*
 * Copyright 2020 - 2025 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

#ifndef _DEVICEP_H_
#define _DEVICEP_H_

#include "device.h"

XWStreamCtxt* dvc_makeStream( XW_DUtilCtxt* dutil );
void dvc_storeStream( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* keys[],
                      XWStreamCtxt* stream );
void dvc_removeStream( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* keys[] );
void dvc_parseKey( XP_UCHAR* buf, XP_UCHAR* parts[], XP_U16* nParts );
XWStreamCtxt* dvc_loadStream( XW_DUtilCtxt* dutil, XWEnv xwe,
                              const XP_UCHAR* keys[] );
void dvc_getKeysLike( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* keys[],
                      OnGotKey proc, void* closure );

XWStreamCtxt* dvc_beginUrl( XW_DUtilCtxt* dutil, const XP_UCHAR* host,
                            const XP_UCHAR* prefix );

#endif
