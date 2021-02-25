/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2017 - 2020 by Eric House (xwords@eehouse.org).  All rights
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
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.os.Bundle;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.ActionPair;
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify;


import org.eehouse.android.xw4.loc.LocUtils;

/** Abstract superclass for Alerts that have moved from and are still created
 * inside DlgDelegate
 */
public class DlgDelegateAlert extends XWDialogFragment {
    private static final String TAG = DlgDelegateAlert.class.getSimpleName();
    private static final String STATE_KEY = "STATE_KEY";
    private DlgState m_state;

    public DlgDelegateAlert() {}

    public static DlgDelegateAlert newInstance( DlgState state )
    {
        DlgDelegateAlert result = new DlgDelegateAlert();
        result.addStateArgument( state );
        return result;
    }

    protected final DlgState getState( Bundle sis )
    {
        if ( m_state == null ) {
            if ( null != sis ) {
                m_state = (DlgState)sis.getParcelable( STATE_KEY );
            } else {
                Bundle args = getArguments();
                Assert.assertNotNull( args );
                m_state = DlgState.fromBundle( args );
            }
        }
        return m_state;
    }

    protected void addStateArgument( DlgState state )
    {
        setArguments( state.toBundle() );
    }

    protected void populateBuilder( Context context, DlgState state,
                                    AlertDialog.Builder builder )
    {
        Log.d( TAG, "populateBuilder()" );
        NotAgainView naView = addNAView( state, builder );

        OnClickListener lstnr = mkCallbackClickListener( naView );
        if ( 0 != state.m_posButton ) {
            builder.setPositiveButton( state.m_posButton, lstnr );
        }
        if ( 0 != state.m_negButton ) {
            builder.setNegativeButton( state.m_negButton, lstnr );
        }

        if ( null != state.m_pair ) {
            ActionPair pair = state.m_pair;
            builder.setNeutralButton( pair.buttonStr,
                                      mkCallbackClickListener( pair, naView ) );
        }
    }

    AlertDialog create( AlertDialog.Builder builder ) { return builder.create(); }

    private NotAgainView addNAView( DlgState state, AlertDialog.Builder builder )
    {
        Context context = getActivity();
        NotAgainView naView =
            ((NotAgainView)LocUtils.inflate( context, R.layout.not_again_view ))
            .setMessage( state.m_msg )
            .setShowNACheckbox( 0 != state.m_prefsNAKey )
            ;

        builder.setView( naView );

        return naView;
    }

    @Override
    public final AlertDialog onCreateDialog( Bundle sis )
    {
        Context context = getActivity();
        DlgState state = getState( sis );

        AlertDialog.Builder builder = LocUtils.makeAlertBuilder( context );

        if ( null != state.m_title ) {
            builder.setTitle( state.m_title );
        }

        populateBuilder( context, state, builder );

        return create( builder );
    }

    @Override
    public void onSaveInstanceState( Bundle bundle )
    {
        bundle.putParcelable( STATE_KEY, m_state );
        super.onSaveInstanceState( bundle );
    }

    @Override
    public void onDismiss( DialogInterface dif )
    {
        super.onDismiss( dif );
        Activity activity = getActivity();
        if ( activity instanceof DlgClickNotify ) {
            ((DlgClickNotify)activity)
                .onDismissed( m_state.m_action, m_state.getParams() );
        }
    }

    @Override
    protected String getFragTag()
    {
        return getState(null).m_id.toString();
    }

    @Override
    public boolean belongsOnBackStack()
    {
        boolean result = getState(null).m_id.belongsOnBackStack();
        return result;
    }

    protected void checkNotAgainCheck( DlgState state, NotAgainView naView )
    {
        if ( null != naView && naView.getChecked() ) {
            if ( 0 != state.m_prefsNAKey ) {
                XWPrefs.setPrefsBoolean( getActivity(), m_state.m_prefsNAKey,
                                         true );
            }
        }
    }

    protected OnClickListener mkCallbackClickListener( final ActionPair pair,
                                                       final NotAgainView naView )
    {
        return new OnClickListener() {
            @Override
            public void onClick( DialogInterface dlg, int button ) {
                checkNotAgainCheck( m_state, naView );
                DlgClickNotify xwact = (DlgClickNotify)getActivity();
                xwact.onPosButton( pair.action, m_state.getParams() );
            }
        };
    }

    protected OnClickListener mkCallbackClickListener( ActionPair pair )
    {
        return mkCallbackClickListener( pair, null );
    }

    protected OnClickListener mkCallbackClickListener( final NotAgainView naView )
    {
        OnClickListener cbkOnClickLstnr;
        cbkOnClickLstnr = new OnClickListener() {
                public void onClick( DialogInterface dlg, int button ) {
                    checkNotAgainCheck( m_state, naView );

                    Activity activity = getActivity();
                    if ( Action.SKIP_CALLBACK != m_state.m_action
                         && activity instanceof DlgClickNotify ) {
                        DlgClickNotify notify = (DlgClickNotify)activity;
                        switch ( button ) {
                        case AlertDialog.BUTTON_POSITIVE:
                            notify.onPosButton( m_state.m_action, m_state.getParams() );
                            break;
                        case AlertDialog.BUTTON_NEGATIVE:
                            notify.onNegButton( m_state.m_action, m_state.getParams() );
                            break;
                        default:
                            Log.e( TAG, "unexpected button %d", button );
                            // ignore on release builds
                            Assert.failDbg();
                        }
                    }
                }
            };
        return cbkOnClickLstnr;
    }
}
