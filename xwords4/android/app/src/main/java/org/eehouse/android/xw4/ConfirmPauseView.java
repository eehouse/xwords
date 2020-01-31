/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2019 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
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

import android.text.Editable;
import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;

import java.util.HashSet;

// Edit text should start out empty

public class ConfirmPauseView extends LinearLayout
    implements View.OnClickListener, OnItemSelectedListener, EditWClear.TextWatcher {
    private static final String TAG = ConfirmPauseView.class.getSimpleName();
    private static final String PAUSE_MSGS_KEY = TAG + "/pause_msgs";
    private static final String UNPAUSE_MSGS_KEY = TAG + "/unpause_msgs";

    private Boolean mIsPause;
    private boolean mInflateFinished;
    private boolean mInited;
    private HashSet<String> mSavedMsgs;
    private Button mForgetButton;
    private Button mRememberButton;
    private Spinner mSpinner;
    private EditWClear mMsgEdit;

    public ConfirmPauseView( Context context, AttributeSet as ) {
        super( context, as );
    }

    @Override
    protected void onFinishInflate()
    {
        mInflateFinished = true;
        initIfReady();
    }

    private void initIfReady()
    {
        if ( !mInited && mInflateFinished && null != mIsPause ) {
            mInited = true;

            Context context = getContext();

            int id = mIsPause ? R.string.pause_expl : R.string.unpause_expl;
            ((TextView)findViewById(R.id.confirm_pause_expl))
                .setText( id );

            mForgetButton = (Button)findViewById( R.id.pause_forget_msg );
            mForgetButton.setOnClickListener( this );
            mRememberButton = (Button)findViewById( R.id.pause_save_msg );
            mRememberButton.setOnClickListener( this );
            mSpinner = (Spinner)findViewById( R.id.saved_msgs );
            mMsgEdit = (EditWClear)findViewById( R.id.msg_edit );
            mMsgEdit.addTextChangedListener( this );

            String key = mIsPause ? PAUSE_MSGS_KEY : UNPAUSE_MSGS_KEY;
            mSavedMsgs = (HashSet<String>)DBUtils
                .getSerializableFor( context, key );
            if ( null == mSavedMsgs ) {
                mSavedMsgs = new HashSet<>();
            }

            populateSpinner();
            mSpinner.setOnItemSelectedListener( this );
            setMsg( "" );
            // onTextChanged( "" );
        }
    }

    private void populateSpinner()
    {
        final ArrayAdapter<String> adapter =
            new ArrayAdapter<>( getContext(), android.R.layout.simple_spinner_item );
        for ( String msg : mSavedMsgs ) {
            adapter.add( msg );
        }
        mSpinner.setAdapter( adapter );
    }

    @Override
    public void onItemSelected( AdapterView<?> parent, View spinner,
                                int position, long id )
    {
        String msg = (String)parent.getAdapter().getItem( position );
        setMsg( msg );
        onTextChanged( msg );
    }

    @Override
    public void onNothingSelected( AdapterView<?> p ) {}

    @Override
    public void onClick( View view )
    {
        Log.d( TAG, "onClick() called" );
        String msg = getMsg();
        if ( view == mRememberButton && 0 < msg.length() ) {
            mSavedMsgs.add( msg );
        } else if ( view == mForgetButton ) {
            mSavedMsgs.remove( msg );
            setMsg( "" );
        } else {
            Assert.assertFalse( BuildConfig.DEBUG );
        }
        String key = mIsPause ? PAUSE_MSGS_KEY : UNPAUSE_MSGS_KEY;
        DBUtils.setSerializableFor( getContext(), key, mSavedMsgs );
        populateSpinner();
    }

    // from EditWClear.TextWatcher
    @Override
    public void onTextChanged( String msg )
    {
        Log.d( TAG, "onTextChanged(%s)", msg );
        boolean hasText = 0 < msg.length();
        boolean matches = mSavedMsgs.contains( msg );
        mForgetButton.setEnabled( hasText && matches );
        mRememberButton.setEnabled( hasText && !matches );
    }

    ConfirmPauseView setIsPause( boolean isPause )
    {
        mIsPause = isPause;
        initIfReady();
        return this;
    }

    String getMsg()
    {
        return mMsgEdit.getText().toString();
    }

    private void setMsg( String msg )
    {
        mMsgEdit.setText( msg );
    }
}
