/* -*- compile-command: "cd ../../../../../../../../ && ./gradlew insXw4Deb"; -*- */
/*
 * Copyright 2009-2017 by Eric House (xwords@eehouse.org).  All rights
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

import android.text.TextUtils;
import android.content.Context;
import android.os.Bundle;
import android.util.AttributeSet;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.eehouse.android.xw4.loc.LocUtils;
import org.eehouse.android.xw4.TilePickAlert.TilePickState;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;


public class TilePickView extends LinearLayout {
    private static final String TAG = TilePickView.class.getSimpleName();
    private static final String NEW_TILES = "NEW_TILES";
    private static final boolean SHOW_UNAVAIL = false;

    public interface TilePickListener {
        void onTilesChanged( int nToPick, int[] newTiles );
    }

    private ArrayList<Integer> m_pendingTiles;
    private TilePickListener m_listner;
    private TilePickState m_state;
    private Map<Integer, Button> m_buttons = new HashMap<Integer, Button>();

    public TilePickView( Context context, AttributeSet as ) {
        super( context, as );
    }

    protected void init( TilePickListener lstn, TilePickState state,
                         Bundle bundle )
    {
        m_state = state;
        m_listner = lstn;
        m_pendingTiles = (ArrayList<Integer>)bundle.getSerializable( NEW_TILES );
        if ( null == m_pendingTiles ) {
            Log.d( TAG, "creating new m_pendingTiles" );
            m_pendingTiles = new ArrayList<Integer>();
        }

        showPending();
        addTileButtons();
        updateDelButton();

        findViewById( R.id.del ).setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View view ) {
                    removePending();
                    updateDelButton();
                    m_listner.onTilesChanged( m_state.nToPick, getPending() );
                }
            } );

        m_listner.onTilesChanged( m_state.nToPick, getPending() );
    }

    // NOT @Override!!!
    protected void saveInstanceState( Bundle bundle )
    {
        bundle.putSerializable( NEW_TILES, m_pendingTiles );
    }

    private int[] getPending()
    {
        int[] result = new int[m_pendingTiles.size()];
        for ( int ii = 0; ii < result.length; ++ii ) {
            result[ii] = m_pendingTiles.get(ii);
        }
        return result;
    }

    private void addTileButtons()
    {
        Context context = getContext();
        LinearLayout container = (LinearLayout)
            findViewById( R.id.button_bar_container );

        LinearLayout bar = null;
        int barLen = 0;
        int nShown = 0;
        for ( int ii = 0; ii < m_state.faces.length; ++ii ) {
            if ( null != m_state.counts && m_state.counts[ii] == 0 && !SHOW_UNAVAIL ) {
                continue;
            }

            final int dataIndex = ii;
            final int visIndex = nShown++;
            if ( null == bar || 0 == (visIndex % barLen) ) {
                bar = (LinearLayout)
                    LocUtils.inflate( context, R.layout.tile_picker_bar );
                container.addView( bar );
                barLen = bar.getChildCount();
            }

            Button button = (Button)bar.getChildAt( visIndex % barLen );
            m_buttons.put( dataIndex, button );
            button.setVisibility( View.VISIBLE );
            updateButton( dataIndex, 0 );
            button.setOnClickListener( new OnClickListener() {
                    @Override
                    public void onClick( View view ) {
                        onTileClicked( view, dataIndex );
                    }
                } );
        }
    }

    private void onTileClicked( View view, int index )
    {
        // replace the last pick if we don't have room to add a new one
        if ( m_pendingTiles.size() == m_state.nToPick ) {
            removePending();
        }
        m_pendingTiles.add( index );

        updateDelButton();
        updateButton( index, -1 );
        showPending();

        m_listner.onTilesChanged( m_state.nToPick, getPending() );
    }

    private void showPending()
    {
        TextView desc = (TextView)findViewById( R.id.pending_desc );
        if ( m_state.forBlank() ) {
            desc.setVisibility( View.GONE );
        } else {
            List<String> faces = new ArrayList<String>();
            for ( int indx : m_pendingTiles ) {
                faces.add( m_state.faces[indx] );
            }

            desc.setText( LocUtils.getString( getContext(),
                                              R.string.tile_pick_summary_fmt,
                                              TextUtils.join( ",", faces ) ) );
        }
    }

    private int pendingCount( int dataIndex )
    {
        int count = 0;
        for ( int index : m_pendingTiles ) {
            if ( index == dataIndex ) {
                ++count;
            }
        }
        return count;
    }

    private void updateButton( int index, int adjust )
    {
        Button button = m_buttons.get( index );
        Context context = getContext();
        String face = m_state.faces[index];
        if ( !m_state.forBlank() ) {
            int count = m_state.counts[index] - pendingCount( index );
            face = LocUtils.getString( context, R.string.tile_button_txt_fmt,
                                       face, count );

            int vis = count == 0 ? View.INVISIBLE : View.VISIBLE;
            button.setVisibility( vis );
        }
        button.setText( face );
    }

    private void removePending()
    {
        int tile = m_pendingTiles.remove( m_pendingTiles.size() - 1 );
        updateButton( tile, 1 );
        showPending();
    }

    private void updateDelButton()
    {
        int vis = m_state.forBlank() || m_pendingTiles.size() == 0
            ? View.INVISIBLE : View.VISIBLE;
        findViewById( R.id.del ).setVisibility( vis );
    }
}
