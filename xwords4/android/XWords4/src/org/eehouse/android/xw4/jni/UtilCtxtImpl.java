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

import org.eehouse.android.xw4.Utils;

public class UtilCtxtImpl implements UtilCtxt {

    public void requestTime() {
        subclassOverride( "requestTime" );
    }

    public int userPickTile( int playerNum, String[] texts )
    {
        subclassOverride( "userPickTile" );
        return 0;
    }

    public String askPassword( String name )
    {
        subclassOverride( "askPassword" );
        return null;
    }

    public void turnChanged()
    {
        subclassOverride( "turnChanged" );
    }

    public boolean engineProgressCallback()
    {
        subclassOverride( "engineProgressCallback" );
        return true;
    }

    public void engineStarting( int nBlanks )
    {
        subclassOverride( "engineStarting" );
    }

    public void engineStopping()
    {
        subclassOverride( "engineStopping" );
    }

    public void setTimer( int why, int when, int handle )
    {
        subclassOverride( "setTimer" );
    }

    public void clearTimer( int why )
    {
        subclassOverride( "clearTimer" );
    }

    public void remSelected()
    {
        subclassOverride( "remSelected" );
    }

    public void setIsServer( boolean isServer )
    {
        subclassOverride( "setIsServer" );
    }

    public String getUserString( int stringCode )
    {
        subclassOverride( "getUserString" );
        return "";
    }

    public boolean userQuery( int id, String query )
    {
        subclassOverride( "userQuery" );
        return false;
    }

    public void userError( int id )
    {
        subclassOverride( "userError" );
    }

    // Probably want to cache the fact that the game over notification
    // showed up and then display it next time game's opened.
    public void notifyGameOver()
    {
        subclassOverride( "notifyGameOver" );
    }

    public boolean warnIllegalWord( String[] words, int turn, boolean turnLost )
    {
        subclassOverride( "warnIllegalWord" );
        return false;
    }

    // These need to go into some sort of chat DB, not dropped.
    public void showChat( String msg )
    {
        subclassOverride( "showChat" );
    }

    private void subclassOverride( String name ) {
        Utils.logf( "%s::%s() called", getClass().getName(), name );
    }

}
