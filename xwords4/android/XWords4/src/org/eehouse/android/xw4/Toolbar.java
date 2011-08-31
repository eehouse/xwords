/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

package org.eehouse.android.xw4;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ImageButton;

import org.eehouse.android.xw4.jni.*;

public class Toolbar {

    private static class TBButtonInfo {
        public TBButtonInfo( int id/*, int idVert*/ ) {
            m_id = id;
        }
        public int m_id;
    }

    public static final int BUTTON_HINT_PREV = 0;
    public static final int BUTTON_HINT_NEXT = 1;
    public static final int BUTTON_FLIP = 2;
    public static final int BUTTON_JUGGLE = 3;
    public static final int BUTTON_ZOOM = 4;
    public static final int BUTTON_UNDO = 5;
    public static final int BUTTON_CHAT = 6;

    private static TBButtonInfo[] s_buttonInfo = {
        // BUTTON_HINT_PREV
        new TBButtonInfo(R.id.prevhint_button_horizontal ),
        // BUTTON_HINT_NEXT
        new TBButtonInfo(R.id.nexthint_button_horizontal ),
        // BUTTON_FLIP
        new TBButtonInfo(R.id.flip_button_horizontal ),
        // BUTTON_JUGGLE
        new TBButtonInfo( R.id.shuffle_button_horizontal ),
        // BUTTON_ZOOM
        new TBButtonInfo( R.id.zoom_button_horizontal ),
        // BUTTON_UNDO
        new TBButtonInfo( R.id.undo_button_horizontal ),
        // BUTTON_CHAT
        new TBButtonInfo( R.id.chat_button_horizontal ),
    };

    private XWActivity m_activity;
    private View m_me;

    private enum ORIENTATION { ORIENT_UNKNOWN,
            ORIENT_PORTRAIT,
            ORIENT_LANDSCAPE,
            };
    private ORIENTATION m_curOrient = ORIENTATION.ORIENT_UNKNOWN;

    public Toolbar( XWActivity activity, int id )
    {
        m_activity = activity;
        m_me = activity.findViewById( id );
    }

    public void setVisibility( int vis )
    {
        m_me.setVisibility( vis );
    }

    public void setListener( int index, View.OnClickListener listener )
    {
        TBButtonInfo info = s_buttonInfo[index];
        ImageButton button = (ImageButton)m_activity.findViewById( info.m_id );
        button.setOnClickListener( listener );
    }

    public void setListener( int index, final int msgID, final int prefsKey, 
                             final int callback )
    {
        View.OnClickListener listener = new View.OnClickListener() {
                public void onClick( View view ) {
                    m_activity.showNotAgainDlgThen( msgID, prefsKey, callback );
                }
            };
        setListener( index, listener );
    }

    public void update( int index, boolean enable )
    {
        TBButtonInfo info = s_buttonInfo[index];
        int vis = enable ? View.VISIBLE : View.GONE;

        ImageButton button = (ImageButton)m_activity.findViewById( info.m_id );
        button.setVisibility( vis );
    }

}
