/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2010 - 2012 by Eric House (xwords@eehouse.org).  All
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

package org.eehouse.android.xw4;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import java.util.HashSet;
import java.util.Set;

import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.UtilCtxt;
import org.eehouse.android.xw4.jni.UtilCtxtImpl;

public class XWService extends Service {

    protected static MultiService s_srcMgr = null;
    private static Set<String> s_seen = new HashSet<String>();

    private UtilCtxt m_utilCtxt;

    @Override
    public IBinder onBind( Intent intent )
    {
        return null;
    }

    public final static void setListener( MultiService.MultiEventListener li )
    {
        if ( null == s_srcMgr ) {
            // DbgUtils.logf( "XWService.setListener: registering %s", li.getClass().getName() );
            s_srcMgr = new MultiService();
        }
        s_srcMgr.setListener( li );
    }

    protected void sendResult( MultiEvent event, Object ... args )
    {
        if ( null != s_srcMgr ) {
            s_srcMgr.sendResult( event, args );
        } else {
            DbgUtils.logdf( "XWService.sendResult: dropping %s event", event.toString() );
        }
    }

    // Check that we aren't already processing an invitation with this
    // inviteID.
    protected boolean checkNotDupe( NetLaunchInfo nli )
    {
        String inviteID = nli.inviteID();
        boolean isDupe;
        synchronized( s_seen ) {
            isDupe = s_seen.contains( inviteID );
            if ( !isDupe ) {
                s_seen.add( inviteID );
            }
        }
        DbgUtils.logdf( "XWService.checkNotDupe(%s) => %b", inviteID, !isDupe );
        return !isDupe;
    }

    protected UtilCtxt getUtilCtxt()
    {
        if ( null == m_utilCtxt ) {
            m_utilCtxt = new UtilCtxtImpl( this );
        }
        return m_utilCtxt;
    }
}
