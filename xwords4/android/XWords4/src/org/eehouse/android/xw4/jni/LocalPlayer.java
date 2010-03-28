/* -*- compile-command: "cd ../../../../../../; ant install"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

package org.eehouse.android.xw4.jni;

public class LocalPlayer {
    public String name;
    public String password;
    public int secondsUsed;
    public boolean isRobot;
    public boolean isLocal;

    public LocalPlayer( int num )
    {
        isLocal = true;
        isRobot = false;
        // This should be a template in strings.xml
        name = "Player " + (num + 1);
        password = "";
    }

    public LocalPlayer( final LocalPlayer src )
    {
        isLocal = src.isLocal;
        isRobot = src.isRobot;
        if ( null != src.name ) {
            name = new String(src.name);
        }
        if ( null != src.password ) {
            password = new String(src.password);
        }
        secondsUsed = src.secondsUsed;
    }
}

