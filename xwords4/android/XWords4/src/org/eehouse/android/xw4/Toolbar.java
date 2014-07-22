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
import android.widget.LinearLayout;
import android.widget.ImageButton;

import junit.framework.Assert;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.HasDlgDelegate;
import org.eehouse.android.xw4.jni.*;

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

    private static int[][] s_buttonInfo = {
        // BUTTON_BROWSE_DICT
        { R.id.dictlist_button_horizontal, R.id.dictlist_button_vertical },
        // BUTTON_HINT_PREV
        { R.id.prevhint_button_horizontal, R.id.prevhint_button_vertical },
        // BUTTON_HINT_NEXT
        { R.id.nexthint_button_horizontal, R.id.nexthint_button_vertical },
        // BUTTON_FLIP
        { R.id.flip_button_horizontal, R.id.flip_button_vertical },
        // BUTTON_JUGGLE
        {  R.id.shuffle_button_horizontal, R.id.shuffle_button_vertical },
        // BUTTON_ZOOM
        {  R.id.zoom_button_horizontal, R.id.zoom_button_vertical },
        // BUTTON_UNDO
        {  R.id.undo_button_horizontal, R.id.undo_button_vertical },
        // BUTTON_CHAT
        {  R.id.chat_button_horizontal, R.id.chat_button_vertical },
        // BUTTON_VALUES
        {  R.id.values_button_horizontal, R.id.values_button_vertical },
    };

    private Activity m_activity;
    private DlgDelegate.HasDlgDelegate m_dlgDlgt;
    private View m_horLayout;
    private View m_vertLayout;
    private boolean m_visible;

    private enum ORIENTATION { ORIENT_UNKNOWN,
            ORIENT_PORTRAIT,
            ORIENT_LANDSCAPE,
            };
    private ORIENTATION m_curOrient = ORIENTATION.ORIENT_UNKNOWN;

    public Toolbar( Activity activity, HasDlgDelegate dlgDlgt )
    {
        m_activity = activity;
        m_dlgDlgt = dlgDlgt;
        m_horLayout = activity.findViewById( R.id.toolbar_horizontal );
        m_vertLayout = activity.findViewById( R.id.toolbar_vertical );
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
        int id = idForIndex( index );
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

    public void setIsLandscape( boolean landscape )
    {
        if ( landscape && m_curOrient == ORIENTATION.ORIENT_LANDSCAPE ) {
            // do nothing
        } else if ( !landscape && m_curOrient == ORIENTATION.ORIENT_PORTRAIT ) {
            // do nothing
        } else {
            if ( landscape ) {
                m_curOrient = ORIENTATION.ORIENT_LANDSCAPE;
            } else {
                m_curOrient = ORIENTATION.ORIENT_PORTRAIT;
            }
            doShowHide();
        }
    }

    public void update( int index, boolean enable )
    {
        int vis = enable ? View.VISIBLE : View.GONE;
        int id = idForIndex( index );
        ImageButton button = (ImageButton)m_activity.findViewById( id );
        if ( null != button ) {
            button.setVisibility( vis );
        }
    }

    private void doShowHide()
    {
        boolean show;
        if ( null != m_horLayout ) {
            show = m_visible && ORIENTATION.ORIENT_PORTRAIT == m_curOrient;
            m_horLayout.setVisibility( show? View.VISIBLE : View.GONE );
        }
        if ( null != m_vertLayout ) {
            show = m_visible && ORIENTATION.ORIENT_LANDSCAPE == m_curOrient;
            m_vertLayout.setVisibility( show? View.VISIBLE : View.GONE );
        }
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

    private int idForIndex( int index )
    {
        Assert.assertTrue( ORIENTATION.ORIENT_UNKNOWN != m_curOrient );
        int indx = ORIENTATION.ORIENT_PORTRAIT == m_curOrient ? 0 : 1;
        return s_buttonInfo[index][indx];
    }
}
