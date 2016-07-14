/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009-2014 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ImageButton;

import junit.framework.Assert;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.HasDlgDelegate;
import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.loc.LocUtils;

public class Toolbar {
    public static final int BUTTON_BROWSE_DICT = 0;
    public static final int BUTTON_HINT_PREV = 1;
    public static final int BUTTON_HINT_NEXT = 2;
    public static final int BUTTON_FLIP = 3;
    public static final int BUTTON_JUGGLE = 4;
    public static final int BUTTON_ZOOM = 5;
    public static final int BUTTON_UNDO = 6;
    public static final int BUTTON_CHAT = 7;
    public static final int BUTTON_VALUES = 8;

    private static int[] s_buttonInfo = {
        // BUTTON_BROWSE_DICT
        R.id.dictlist_button,
        // BUTTON_HINT_PREV
        R.id.prevhint_button,
        // BUTTON_HINT_NEXT
        R.id.nexthint_button,
        // BUTTON_FLIP
        R.id.flip_button,
        // BUTTON_JUGGLE
        R.id.shuffle_button,
        // BUTTON_ZOOM
        R.id.zoom_button,
        // BUTTON_UNDO
        R.id.undo_button,
        // BUTTON_CHAT
        R.id.chat_button,
        // BUTTON_VALUES
        R.id.values_button,
    };

    private Activity m_activity;
    private DlgDelegate.HasDlgDelegate m_dlgDlgt;
    private LinearLayout m_layout;
    private boolean m_visible;
    private Boolean m_isPortrait;

    public Toolbar( Activity activity, HasDlgDelegate dlgDlgt )
    {
        m_activity = activity;
        m_dlgDlgt = dlgDlgt;
    }

    public void setVisible( boolean visible )
    {
        if ( m_visible != visible ) {
            m_visible = visible;
            doShowHide();
        }
    }

    public ImageButton getViewFor( int index )
    {
        int id = s_buttonInfo[index];
        ImageButton button = (ImageButton)m_activity.findViewById( id );
        return button;
    }

    public void setListener( int index, final int msgID, final int prefsKey,
                             final Action action )
    {
        View.OnClickListener listener = new View.OnClickListener() {
                public void onClick( View view ) {
                    m_dlgDlgt.showNotAgainDlgThen( msgID, prefsKey, action );
                }
            };
        setListener( index, listener );
    }

    public void setLongClickListener( int index, final int msgID,
                                      final int prefsKey, final Action action )
    {
        View.OnLongClickListener listener = new View.OnLongClickListener() {
                public boolean onLongClick( View view ) {
                    m_dlgDlgt.showNotAgainDlgThen( msgID, prefsKey, action );
                    return true;
                }
            };
        setLongClickListener( index, listener );
    }

    protected void setIsPortrait( boolean isPortrait )
    {
        if ( null == m_isPortrait || m_isPortrait != isPortrait ) {
            m_isPortrait = isPortrait;
            doShowHide();
        }
    }

    public void update( int index, boolean enable )
    {
        int vis = enable ? View.VISIBLE : View.GONE;
        int id = s_buttonInfo[index];
        ImageButton button = (ImageButton)m_activity.findViewById( id );
        if ( null != button ) {
            button.setVisibility( vis );
        }
    }

    private void doShowHide()
    {
        Assert.assertTrue( null != m_isPortrait );
        if ( null == m_layout ) {
            m_layout = (LinearLayout)LocUtils.inflate( m_activity, R.layout.toolbar );
        } else {
            ((ViewGroup)m_layout.getParent()).removeView( m_layout );
        }
        m_layout.setOrientation( m_isPortrait ?
                                 LinearLayout.HORIZONTAL : LinearLayout.VERTICAL );

        int id = m_isPortrait ? R.id.tbar_parent_hor : R.id.tbar_parent_vert;
        ViewGroup scroller = (ViewGroup)m_activity.findViewById( id );
        if ( null != scroller ) {
            // Google's had reports of a crash adding second view
            scroller.removeAllViews();
            scroller.addView( m_layout );
        }

        m_layout.setVisibility( m_visible? View.VISIBLE : View.GONE );
    }

    private void setListener( int index, View.OnClickListener listener )
    {
        ImageButton button = getViewFor( index );
        if ( null != button ) {
            button.setOnClickListener( listener );
        }
    }

    private void setLongClickListener( int index,
                                       View.OnLongClickListener listener )
    {
        ImageButton button = getViewFor( index );
        if ( null != button ) {
            button.setOnLongClickListener( listener );
        }
    }
}
