/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.TextView;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.loc.LocUtils;

public class GameOverAlert extends XWDialogFragment
    implements DialogInterface.OnClickListener
{
    private static final String TAG = GameOverAlert.class.getSimpleName();
    private static final String SUMMARY = "SUMMARY";
    private static final String TITLE = "TITLE";
    private static final String MSG = "MSG";
    private static final String IN_ARCH = "IN_ARCH";

    private AlertDialog m_dialog;
    private GameSummary mSummary;
    private int mTitleID;
    private String mMsg;
    private ViewGroup m_view;
    private boolean mInArchive;
    private CheckBox mArchiveBox;
    // private boolean mArchiveChecked;

    public static GameOverAlert newInstance( GameSummary summary,
                                             int titleID, String msg,
                                             boolean inArchiveGroup )
    {
        Log.d( TAG, "newInstance(msg=%s)", msg );
        GameOverAlert result = new GameOverAlert();
        Bundle args = new Bundle();
        args.putSerializable( SUMMARY, summary );
        args.putInt( TITLE, titleID );
        args.putString( MSG, msg );
        args.putBoolean( IN_ARCH, inArchiveGroup );
        result.setArguments( args );
        Log.d( TAG, "newInstance() => %s", result );
        return result;
    }

    public GameOverAlert() {}

    @Override
    public void onSaveInstanceState( Bundle bundle )
    {
        bundle.putSerializable( SUMMARY, mSummary );
        bundle.putInt( TITLE, mTitleID );
        bundle.putString( MSG, mMsg );
        bundle.putBoolean( IN_ARCH, mInArchive );
        super.onSaveInstanceState( bundle );
    }
    
    @Override
    public Dialog onCreateDialog( Bundle sis )
    {
        Log.d( TAG, "onCreateDialog()" );
        if ( null == sis ) {
            sis = getArguments();
        }
        mSummary = (GameSummary)sis.getSerializable( SUMMARY );
        mTitleID = sis.getInt( TITLE );
        mMsg = sis.getString( MSG );
        mInArchive = sis.getBoolean( IN_ARCH );

        Activity activity = getActivity();
        m_view = (ViewGroup)LocUtils.inflate( activity, R.layout.game_over );
        initView();

        AlertDialog.Builder ab = LocUtils.makeAlertBuilder( activity )
            .setTitle( mTitleID )
            .setView( m_view )
            .setPositiveButton( android.R.string.ok, this )
            .setNeutralButton( R.string.button_rematch, this )
            ;

        m_dialog = ab.create();
        Log.d( TAG, "onCreateDialog() => %s", m_dialog );
        return m_dialog;
    }

    @Override
    protected String getFragTag() { return TAG; }

    // @Override
    // public void onCheckedChanged( CompoundButton buttonView, boolean isChecked )
    // {
    //     Log.d( TAG, "onCheckedChanged(%b)", isChecked );
    //     mArchiveChecked = isChecked;
    // }

    @Override
    public void onClick( DialogInterface dialog, int which )
    {
        Action action = null;
        boolean archiveAfter =
            ((CheckBox)m_view.findViewById(R.id.archive_check))
            .isChecked();
        if ( which == AlertDialog.BUTTON_NEUTRAL ) {
            action = Action.REMATCH_ACTION;
        } else if ( which == AlertDialog.BUTTON_POSITIVE && archiveAfter ) {
            action = Action.ARCHIVE_SEL_ACTION;
        }

        if ( null != action ) {
            Activity activity = getActivity();
            if ( activity instanceof DlgDelegate.DlgClickNotify ) {
                DlgDelegate.DlgClickNotify notify
                    = (DlgDelegate.DlgClickNotify)activity;
                notify.onPosButton( action, archiveAfter );
            }
        }
    }

    // private void trySend( Action action, boolean bool )
    // {
    //     Log.d( TAG, "trySend(%s)", action );
    // }

    private void initView()
    {
        ((TextView)m_view.findViewById( R.id.msg )).setText( mMsg );

        mArchiveBox = (CheckBox)m_view.findViewById( R.id.archive_check );
        if ( mInArchive ) {
            mArchiveBox.setVisibility( View.GONE );
        }
    }
}
