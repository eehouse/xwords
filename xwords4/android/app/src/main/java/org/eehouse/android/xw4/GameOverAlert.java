/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
    implements DialogInterface.OnClickListener, CompoundButton.OnCheckedChangeListener
{
    private static final String TAG = GameOverAlert.class.getSimpleName();
    private static final String SUMMARY = "SUMMARY";
    private static final String TITLE = "TITLE";
    private static final String MSG = "MSG";
    private static final String IN_ARCH = "IN_ARCH";
    private static final String HAS_PENDING = "HAS_PENDING";

    private AlertDialog mDialog;
    private GameSummary mSummary;
    private int mTitleID;
    private String mMsg;
    private ViewGroup mView;
    private boolean mInArchive;
    private CheckBox mArchiveBox;
    private boolean mHasPending;

    public static GameOverAlert newInstance( GameSummary summary,
                                             int titleID, String msg,
                                             boolean hasPending, boolean inArchiveGroup )
    {
        Log.d( TAG, "newInstance(msg=%s)", msg );
        GameOverAlert result = new GameOverAlert();
        Bundle args = new Bundle();
        args.putSerializable( SUMMARY, summary );
        args.putInt( TITLE, titleID );
        args.putString( MSG, msg );
        args.putBoolean( IN_ARCH, inArchiveGroup );
        args.putBoolean( HAS_PENDING, hasPending );
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
        bundle.putBoolean( HAS_PENDING, mHasPending );
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
        boolean hasPending = sis.getBoolean( HAS_PENDING );

        Activity activity = getActivity();
        mView = (ViewGroup)LocUtils.inflate( activity, R.layout.game_over );
        initView();

        AlertDialog.Builder ab = LocUtils.makeAlertBuilder( activity )
            .setTitle( mTitleID )
            .setView( mView )
            .setPositiveButton( android.R.string.ok, this )
            .setNeutralButton( R.string.button_rematch, this )
            ;
        if ( hasPending ) {
            mArchiveBox.setVisibility( View.GONE );
        } else {
            ab.setNegativeButton( R.string.button_delete, this );
        }

        mDialog = ab.create();
        mDialog.setOnShowListener( new DialogInterface.OnShowListener() {
                @Override
                public void onShow( DialogInterface dialog ) {
                    boolean nowChecked = mArchiveBox.isChecked();
                    onCheckedChanged( null, nowChecked );
                }
            });

        Log.d( TAG, "onCreateDialog() => %s", mDialog );
        return mDialog;
    }

    @Override
    protected String getFragTag() { return TAG; }

    @Override
    public void onClick( DialogInterface dialog, int which )
    {
        Action action = null;
        boolean archiveAfter =
            ((CheckBox)mView.findViewById(R.id.archive_check))
            .isChecked();
        switch ( which ) {
        case AlertDialog.BUTTON_NEUTRAL:
            action = Action.REMATCH_ACTION;
            break;
        case AlertDialog.BUTTON_POSITIVE:
            if ( archiveAfter ) {
                action = Action.ARCHIVE_SEL_ACTION;
            }
            break;
        case AlertDialog.BUTTON_NEGATIVE:
            action = Action.DELETE_ACTION;
            break;
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

    @Override
    public void onCheckedChanged( CompoundButton bv, boolean isChecked )
    {
        Utils.enableAlertButton( mDialog, AlertDialog.BUTTON_NEGATIVE, !isChecked );
    }

    private void initView()
    {
        ((TextView)mView.findViewById( R.id.msg )).setText( mMsg );

        mArchiveBox = (CheckBox)mView.findViewById( R.id.archive_check );
        mArchiveBox.setOnCheckedChangeListener( this );
        if ( mInArchive ) {
            mArchiveBox.setVisibility( View.GONE );
        }
    }
}
