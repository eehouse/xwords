/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005-2012 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option.
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

#ifndef _DEVID_H_
#define _DEVID_H_

#include <string>
#include <stdlib.h>

#include "xwrelay.h"

/* DevID protocol.
*
* There are two types.  The first, with a DevIDType greater than
* ID_TYPE_RELAY, is platform-specific and meaningless to the relay (though as
* with GCM-based IDs on Android there may be server code that uses it.)  The
* second, with type of ID_TYPE_RELAY, is specific to the relay.  When the
* relay sees one of the first type, it creates an entry in the devices table
* with a new random 32-bit index that is then used in the msgs and games
* tables.  This index is the second type.
*
* A device always includes a DevID when creating a new game.  It may be of
* either type, and generally should use the latter when possible.  Since the
* latter comes from the relay, the first time a device connects (after
* whatever local event, e.g. registration with GCM, causes it to have an ID of
* the first type that it wants to share) it will have to send the first type.
* When replying to a registration message that included a DevID of the first
* type, the relay always sends the corresponding DevID of the second type,
* which it expects the device to remember. But when replying after receiving a
* DevID of the second type the relay does not echo that value (sends an empty
* string).
*
* Devices or platforms not providing a DevID will return ID_TYPE_NONE as the
* type via util_getDevID().  That single byte will be transmitted to the relay
* which will then skip the registration process and will return an empty
* string as the relay-type DevID in the connection response.
*
*/

#include <assert.h>

using namespace std;

class DevID {
 public:
    DevID() { m_devIDType = ID_TYPE_NONE; }
    DevID(DevIDType typ) { m_devIDType = typ; }
    DevIDRelay asRelayID() const { 
        assert( ID_TYPE_RELAY == m_devIDType );
        return strtoul( m_devIDString.c_str(), NULL, 16 ); 
    }
    string m_devIDString;
    DevIDType m_devIDType;
};

#endif
