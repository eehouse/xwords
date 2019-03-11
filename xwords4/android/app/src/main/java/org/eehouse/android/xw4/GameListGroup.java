/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2012 by Eric House (xwords@eehouse.org).  All rights
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
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

import org.eehouse.android.xw4.loc.LocUtils;

public class GameListGroup extends ExpiringLinearLayout
    implements SelectableItem.LongClickHandler,
               View.OnClickListener,
               View.OnLongClickListener
{

    private long m_groupID;
    private boolean m_expanded;
    private SelectableItem m_cb;
    private GroupStateListener m_gcb;
    private TextView m_etv;
    private boolean m_selected;
    private int m_nGames;
    private DrawSelDelegate m_dsdel;
    private ImageButton m_expandButton;

    public static GameListGroup makeForPosition( Context context,
                                                 View convertView,
                                                 long groupID,
                                                 int nGames,
                                                 boolean expanded,
                                                 SelectableItem cb,
                                                 GroupStateListener gcb )
    {
        GameListGroup result = null;
        if ( null != convertView && convertView instanceof GameListGroup ) {
            result = (GameListGroup)convertView;

            // Hack: once an ExpiringLinearLayout has a background it's not
            // set up to be reused without one.  Until that's fixed, don't
            // reuse in that case.
            if ( result.hasDelegate() ) {
                result = null;
            }
        }
        if ( null == result ) {
            result = (GameListGroup)
                LocUtils.inflate( context, R.layout.game_list_group );
        }
        result.m_cb = cb;
        result.m_gcb = gcb;
        result.m_groupID = groupID;
        result.m_nGames = nGames;
        result.m_expanded = expanded;

        result.setButton();     // in case onFinishInflate already called

        return result;
    }

    public GameListGroup( Context cx, AttributeSet as )
    {
        super( cx, as );
    }

    @Override
    protected void onFinishInflate()
    {
        super.onFinishInflate();
        m_etv = (TextView)findViewById( R.id.game_name );
        m_expandButton = (ImageButton)findViewById( R.id.expander );

        // click on me OR the button expands/contracts...
        setOnClickListener( this );
        m_expandButton.setOnClickListener( this );

        m_dsdel = new DrawSelDelegate( this );
        setOnLongClickListener( this );

        setButton();
    }

    public long getGroupID()
    {
        return m_groupID;
    }

    public void setSelected( boolean selected )
    {
        // If new value and state not in sync, force change in state
        if ( selected != m_selected ) {
            toggleSelected();
        }
    }

    protected void setText( String text )
    {
        m_etv.setText( text );
    }

    // GameListAdapter.ClickHandler interface
    public void longClicked()
    {
        toggleSelected();
    }

    protected void toggleSelected()
    {
        m_selected = !m_selected;
        m_dsdel.showSelected( m_selected );
        m_cb.itemToggled( this, m_selected );
    }

    //////////////////////////////////////////////////
    // View.OnLongClickListener interface
    //////////////////////////////////////////////////
    public boolean onLongClick( View view )
    {
        boolean handled = ! XWApp.CONTEXT_MENUS_ENABLED;
        if ( handled ) {
            longClicked();
        }
        return handled;
    }

    //////////////////////////////////////////////////
    // View.OnClickListener interface
    //////////////////////////////////////////////////
    public void onClick( View view )
    {
        if ( 0 < m_nGames ) {
            m_expanded = !m_expanded;
            m_gcb.onGroupExpandedChanged( this, m_expanded );
            setButton();
        }
    }

    private void setButton()
    {
        if ( null != m_expandButton ) {
            m_expandButton.setVisibility( 0 == m_nGames ?
                                          View.GONE : View.VISIBLE );
            m_expandButton.setImageResource( m_expanded ?
                                             R.drawable.expander_ic_maximized :
                                             R.drawable.expander_ic_minimized);
        }
    }

}
