/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import android.view.View;
import android.view.ViewGroup;
import android.widget.HorizontalScrollView;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;


import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.HasDlgDelegate;
import org.eehouse.android.xw4.loc.LocUtils;

public class Toolbar implements BoardContainer.SizeChangeListener {
    private static final String TAG = Toolbar.class.getSimpleName();
    public enum Buttons {
        BUTTON_HINT_PREV(R.id.prevhint_button),
        BUTTON_HINT_NEXT(R.id.nexthint_button),
        BUTTON_CHAT(R.id.chat_button, R.string.key_disable_chat_button),
        BUTTON_JUGGLE(R.id.shuffle_button, R.string.key_disable_shuffle_button),
        BUTTON_UNDO(R.id.undo_button, R.string.key_disable_undo_button),
        BUTTON_BROWSE_DICT(R.id.dictlist_button, R.string.key_disable_dicts_button),
        BUTTON_VALUES(R.id.values_button, R.string.key_disable_values_button),
        BUTTON_FLIP(R.id.flip_button, R.string.key_disable_flip_button),
        ;

        private int mId;
        private int mKey;
        private Buttons(int id) { this( id, 0); }
        private Buttons(int id, int key) {
            mId = id;
            mKey = key;
        }
        public int getResId() { return mId; }
        public int getDisableId() { return mKey; }
    };

    private Activity m_activity;
    private DlgDelegate.HasDlgDelegate m_dlgDlgt;
    private LinearLayout m_layout;
    private boolean m_visible;
    private Map<Buttons, Object> m_onClickListeners = new HashMap<>();
    private Map<Buttons, Object> m_onLongClickListeners = new HashMap<>();
    private Set<Buttons> m_enabled = new HashSet<>();

    public Toolbar( Activity activity, HasDlgDelegate dlgDlgt )
    {
        m_activity = activity;
        m_dlgDlgt = dlgDlgt;

        BoardContainer.registerSizeChangeListener( this );
    }

    public void setVisible( boolean visible )
    {
        if ( m_visible != visible ) {
            m_visible = visible;
            doShowHide();
        }
    }

    public ImageButton getButtonFor( Buttons index )
    {
        return (ImageButton)m_activity.findViewById( index.getResId() );
    }

    public Toolbar setListener( Buttons index, final int msgID,
                                final int prefsKey, final Action action )
    {
        m_onClickListeners.put( index, new View.OnClickListener() {
                @Override
                public void onClick( View view ) {
                    // Log.i( TAG, "setListener(): click on %s with action %s",
                    //        view.toString(), action.toString() );
                    m_dlgDlgt.makeNotAgainBuilder( prefsKey, action, msgID )
                        .show();
                }
            } );
        return this;
    }

    public Toolbar setLongClickListener( Buttons index, final int msgID,
                                         final int prefsKey, final Action action )
    {
        m_onLongClickListeners.put( index, new View.OnLongClickListener() {
                public boolean onLongClick( View view ) {
                    m_dlgDlgt.makeNotAgainBuilder( prefsKey, action, msgID )
                        .show();
                    return true;
                }
            } );
        return this;
    }

    public Toolbar update( Buttons index, boolean enable )
    {
        if ( enable ) {
            int disableKeyID = index.getDisableId();
            if ( 0 != disableKeyID ) {
                enable = !XWPrefs.getPrefsBoolean( m_activity, disableKeyID,
                                                   false );
            }
        }

        int id = index.getResId();
        ImageButton button = (ImageButton)m_activity.findViewById( id );
        if ( null != button ) {
            button.setVisibility( enable ? View.VISIBLE : View.GONE );
        }

        if ( enable ) {
            m_enabled.add( index );
        } else {
            m_enabled.remove( index );
        }
        return this;
    }

    protected int enabledCount() { return m_enabled.size(); }

    // SizeChangeListener
    @Override
    public void sizeChanged( int width, int height, boolean isPortrait )
    {
        installListeners();
        doShowHide();
    }

    public void installListeners()
    {
        tryAddListeners( m_onClickListeners );
        tryAddListeners( m_onLongClickListeners );
    }

    private void tryAddListeners( Map<Buttons, Object> map )
    {
        Iterator<Buttons> iter = map.keySet().iterator();
        while ( iter.hasNext() ) {
            Buttons key = iter.next();
            Object listener = map.get( key );
            if ( setListener( key, listener ) ) {
                iter.remove();
            }
        }
    }

    private boolean setListener( Buttons index, Object listener )
    {
        ImageButton button = getButtonFor( index );
        boolean success = null != button;
        if ( success ) {
            if ( listener instanceof View.OnClickListener ) {
                button.setOnClickListener( (View.OnClickListener)listener );
            } else if ( listener instanceof View.OnLongClickListener ) {
                button.setOnLongClickListener( (View.OnLongClickListener)listener );
            } else {
                Assert.failDbg();
            }
        }
        return success;
    }

    private void doShowHide()
    {
        boolean isPortrait = BoardContainer.getIsPortrait();

        if ( null == m_layout ) {
            m_layout = (LinearLayout)LocUtils.inflate( m_activity, R.layout.toolbar );
        } else {
            ((ViewGroup)m_layout.getParent()).removeView( m_layout );
        }
        m_layout.setOrientation( isPortrait ?
                                 LinearLayout.HORIZONTAL : LinearLayout.VERTICAL );

        int scrollerId = isPortrait ? R.id.tbar_parent_hor : R.id.tbar_parent_vert;
        ViewGroup scroller = (ViewGroup)m_activity.findViewById( scrollerId );
        if ( null != scroller ) {
            // Google's had reports of a crash adding second view
            scroller.removeAllViews();
            scroller.addView( m_layout );

            scroller.setVisibility( m_visible? View.VISIBLE : View.GONE );
        }
    }
}
