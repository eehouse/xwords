/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.view.View;

import org.eehouse.android.xw4.DBUtils.GameGroupInfo;

public class GameListGroup extends ExpiringTextView 
    implements SelectableItem.LongClickHandler 
{
    private int m_groupPosition;
    private long m_groupID;
    private boolean m_expanded;
    private SelectableItem m_cb;

    public static GameListGroup makeForPosition( Context context,
                                                 int groupPosition, 
                                                 long groupID,
                                                 SelectableItem cb )
    {
        GameListGroup result = 
            (GameListGroup)Utils.inflate( context, R.layout.game_list_group );
        result.m_cb = cb;
        result.m_groupPosition = groupPosition;
        result.m_groupID = groupID;
        return result;
    }

    public GameListGroup( Context cx, AttributeSet as ) 
    {
        super( cx, as );
    }

    public void setGroupPosition( int groupPosition )
    {
        m_groupPosition = groupPosition;
    }

    public int getGroupPosition()
    {
        return m_groupPosition;
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

    // GameListAdapter.ClickHandler interface
    public void longClicked()
    {
        toggleSelected();
    }

    protected void toggleSelected()
    {
        super.toggleSelected();
        m_cb.itemToggled( this, m_selected );
    }

}
