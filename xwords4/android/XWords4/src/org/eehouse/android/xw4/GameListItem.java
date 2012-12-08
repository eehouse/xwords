/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009-2012 by Eric House (xwords@eehouse.org).  All rights
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

package org.eehouse.android.xw4;

import android.content.Context;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;

public class GameListItem extends LinearLayout 
    implements View.OnClickListener {

    private Context m_context;
    private boolean m_loaded;
    private long m_rowid;
    private View m_hideable;
    private ExpiringTextView m_name;
    private boolean m_expanded, m_haveTurn, m_haveTurnLocal;
    private long m_lastMoveTime;
    private ImageButton m_expandButton;
    private Handler m_handler;

    public GameListItem( Context cx, AttributeSet as ) 
    {
        super( cx, as );
        m_context = cx;
        m_loaded = false;
        m_rowid = DBUtils.ROWID_NOTFOUND;
        m_lastMoveTime = 0;
    }

    public void update( Handler handler, boolean expanded, 
                        long lastMoveTime, boolean haveTurn,
                        boolean haveTurnLocal )
    {
        m_handler = handler;
        m_expanded = expanded;
        m_lastMoveTime = lastMoveTime;
        m_haveTurn = haveTurn;
        m_haveTurnLocal = haveTurnLocal;
        m_hideable = (LinearLayout)findViewById( R.id.hideable );
        m_name = (ExpiringTextView)findViewById( R.id.game_name );
        m_expandButton = (ImageButton)findViewById( R.id.expander );
        m_expandButton.setOnClickListener( this );
        showHide();
    }

    public void setLoaded( boolean loaded )
    {
        if ( m_loaded != loaded ) {
            m_loaded = loaded;
            // This should be enough to invalidate
            findViewById( R.id.view_unloaded )
                .setVisibility( loaded ? View.GONE : View.VISIBLE );
            findViewById( R.id.view_loaded )
                .setVisibility( loaded ? View.VISIBLE : View.GONE );
        }
    }

    public boolean isLoaded()
    {
        return m_loaded;
    }

    public void setRowID( long rowid )
    {
        m_rowid = rowid;
    }

    public long getRowID()
    {
        return m_rowid;
    }

    // View.OnClickListener interface
    public void onClick( View view ) {
        m_expanded = !m_expanded;
        DBUtils.setExpanded( m_rowid, m_expanded );
        showHide();
    }

    private void showHide()
    {
        m_expandButton.setImageResource( m_expanded ?
                                         R.drawable.expander_ic_maximized :
                                         R.drawable.expander_ic_minimized);
        m_hideable.setVisibility( m_expanded? View.VISIBLE : View.GONE );

        m_name.setBackgroundColor( android.R.color.transparent );
        m_name.setPct( m_handler, m_haveTurn && !m_expanded, 
                       m_haveTurnLocal, m_lastMoveTime );
    }


}
