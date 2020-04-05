/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2017 by Eric House (xwords@eehouse.org).  All rights reserved.
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

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;

import java.io.Serializable;


import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify;
import org.eehouse.android.xw4.loc.LocUtils;

public class TilePickAlert extends XWDialogFragment
    implements TilePickView.TilePickListener {
    private static final String TAG = TilePickAlert.class.getSimpleName();
    private static final String TPS = "TPS";
    private static final String ACTION = "ACTION";
    private TilePickView m_view;
    private TilePickState m_state;
    private Action m_action;
    private AlertDialog m_dialog;
    private int[] m_selTiles = new int[0];

    public static class TilePickState implements Serializable {
        public int col;
        public int row;
        public int playerNum;
        public int[] counts;
        public String[] faces;
        public boolean isInitial;
        public int nToPick;
        
        public TilePickState( int player, String[] faces, int col, int row ) {
            this.col = col; this.row = row; this.playerNum = player;
            this.faces = faces;
            this.nToPick = 1;
        }
        public TilePickState( boolean isInitial, int playerNum, int nToPick,
                              String[] faces, int[] counts ) {
            this.playerNum = playerNum;
            this.isInitial = isInitial;
            this.nToPick = nToPick;
            this.faces = faces;
            this.counts = counts;
        }

        public boolean forBlank() { return null == counts; }
    }

    public static TilePickAlert newInstance( Action action, TilePickState state )
    {
        TilePickAlert result = new TilePickAlert();
        Bundle args = new Bundle();
        args.putSerializable( ACTION, action );
        args.putSerializable( TPS, state );
        result.setArguments( args );
        return result;
    }

    public TilePickAlert() {}

    @Override
    public void onSaveInstanceState( Bundle bundle )
    {
        bundle.putSerializable( TPS, m_state );
        bundle.putSerializable( ACTION, m_action );
        m_view.saveInstanceState( bundle );
        super.onSaveInstanceState( bundle );
    }

    @Override
    public Dialog onCreateDialog( Bundle sis )
    {
        if ( null == sis ) {
            sis = getArguments();
        }
        m_state = (TilePickState)sis.getSerializable( TPS );
        m_action = (Action)sis.getSerializable( ACTION );
        
        Activity activity = getActivity();
        Assert.assertNotNull( activity );
        m_view = (TilePickView)LocUtils.inflate( activity, R.layout.tile_picker );
        m_view.init( this, m_state, sis );

        int resId = m_state.forBlank()
            ? R.string.title_blank_picker : R.string.tile_tray_picker;
        AlertDialog.Builder ab = LocUtils.makeAlertBuilder( activity )
            .setTitle( resId )
            .setView( m_view );
        if ( !m_state.forBlank() ) {
            DialogInterface.OnClickListener lstnr =
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick( DialogInterface dialog, int which ) {
                        onDone();
                    }
                };
            ab.setPositiveButton( buttonTxt(), lstnr );
        }
        m_dialog = ab.create();
        return m_dialog;
    }

    protected String getFragTag() { return TAG; }

    // TilePickView.TilePickListener interface
    @Override
    public void onTilesChanged( int nToPick, int[] newTiles )
    {
        m_selTiles = newTiles;
        boolean haveAll = nToPick == newTiles.length;
        if ( haveAll && m_state.forBlank() ) {
            onDone();
        } else if ( null != m_dialog ) {
            m_dialog.getButton( AlertDialog.BUTTON_POSITIVE )
                .setText( buttonTxt() );
        }
    }

    @Override
    public void onCancel( DialogInterface dialog )
    {
        super.onCancel( dialog );

        Activity activity = getActivity();
        if ( activity instanceof DlgClickNotify ) {
            DlgClickNotify notify = (DlgClickNotify)activity;
            notify.onDismissed( m_action );
        }
    }

    private void onDone()
    {
        Activity activity = getActivity();
        if ( activity instanceof DlgClickNotify ) {
            DlgClickNotify notify = (DlgClickNotify)activity;
            notify.onPosButton( m_action, m_state, m_selTiles );
        } else {
            Assert.failDbg();
        }
        dismiss();
    }

    private String buttonTxt()
    {
        Context context = getContext();
        int left = m_state.nToPick - m_selTiles.length;
        String txt = 0 == left
            ? LocUtils.getString( context, android.R.string.ok )
            : LocUtils.getString( context, R.string.tilepick_all_fmt, left );
        return txt;
    }
}
