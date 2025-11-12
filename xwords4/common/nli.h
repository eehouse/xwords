/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001 - 2015 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _INVIT_H_
#define _INVIT_H_

// #include "comms.h"
#include "nlityp.h"
#include "xwstream.h"
#include "game.h"

/* InviteInfo
 *
 * A representation of return addresses sent with an invitation so that the
 * recipient has all it needs to create a game and connect back.
 */

void
nli_init( NetLaunchInfo* nli, const CurGameInfo* gi, const CommsAddrRec* addr,
          XP_U16 nPlayersH, XP_U16 forceChannel );


XP_Bool nli_makeFromStream( NetLaunchInfo* nli, XWStreamCtxt* stream );
void nli_saveToStream( const NetLaunchInfo* nli, XWStreamCtxt* stream );

/* Populate a CurGameInfo from a NetLaunchInfo */
void nliToGI( MPFORMAL XW_DUtilCtxt* dutil, XWEnv xwe,
              const NetLaunchInfo* nli, CurGameInfo* gi );
/* Populate a CommsAddrRec */
void nli_makeAddrRec( const NetLaunchInfo* nli, CommsAddrRec* addr );

void nli_setDevID( NetLaunchInfo* nli, XP_U32 devID );
void nli_setInviteID( NetLaunchInfo* nli, const XP_UCHAR* inviteID );
void nli_setGameName( NetLaunchInfo* nli, const XP_UCHAR* gameName );
void nli_setMQTTDevID( NetLaunchInfo* nli, const MQTTDevID* mqttDevID );
void nli_setPhone( NetLaunchInfo* nli, const XP_UCHAR* phone );

#ifdef XWFEATURE_NLI_FROM_ARGV
XP_Bool nli_fromArgv( MPFORMAL NetLaunchInfo* nlip, int argc, const char** argv );
#endif

# ifdef DEBUG
void logNLI( const NetLaunchInfo* nli, const char* callerFunc, const int callerLine );
# define LOGNLI(nli) \
    logNLI( (nli), __func__, __LINE__ )
# else
#  define LOGNLI(nli)
# endif


#endif
