/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 2000-2008 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _CEDRAW_H_
#define _CEDRAW_H_

#include "cemain.h"

typedef struct CEDrawCtx CEDrawCtx;

/* Should match number of icons */
typedef enum {
    CENSTATE_NONE
    ,CENSTATE_TRYING_RELAY
    ,CENSTATE_HAVE_RELAY
    ,CENSTATE_ALL_HERE

    ,CENSTATE_NSTATES
} CeNetState;

CEDrawCtx* ce_drawctxt_make( MPFORMAL HWND mainWin, CEAppGlobals* globals );
void ce_draw_update( CEDrawCtx* dctx );
void ce_draw_erase( CEDrawCtx* dctx, const RECT* invalR );
void ce_draw_focus( CEDrawCtx* dctx, const RECT* invalR );
#ifdef XWFEATURE_RELAY
void ce_draw_status( CEDrawCtx* dctx, const RECT* invalR, CeNetState state );
#endif

#ifndef _WIN32_WCE
HBRUSH ce_draw_getFocusBrush( const CEDrawCtx* dctx );
#endif

void ce_draw_toStream( const CEDrawCtx* dctx, XWStreamCtxt* stream );
void ce_draw_fromStream( CEDrawCtx* dctx, XWStreamCtxt* stream, XP_U8 flags );
#endif

