/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009 - 2012 by Eric House (xwords@eehouse.org).  All
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

public class MultiService {

    private BTEventListener m_li;

    public enum MultiEvent { BAD_PROTO
                          , BT_ENABLED
                          , BT_DISABLED
                          , SCAN_DONE
                          , HOST_PONGED
                          , NEWGAME_SUCCESS
                          , NEWGAME_FAILURE
                          , MESSAGE_ACCEPTED
                          , MESSAGE_REFUSED
                          , MESSAGE_NOGAME
                          , MESSAGE_RESEND
                          , MESSAGE_FAILOUT
                          , MESSAGE_DROPPED
            };

    public interface BTEventListener {
        public void eventOccurred( MultiEvent event, Object ... args );
    }
    // public interface MultiEventSrc {
    //     public void setBTEventListener( BTEventListener li );
    // }

    public void setListener( BTEventListener li )
    {
        synchronized( this ) {
            m_li = li;
        }
    }

    public void sendResult( MultiEvent event, Object ... args )
    {
        synchronized( this ) {
            if ( null != m_li ) {
                m_li.eventOccurred( event, args );
            }
        }
    }

}