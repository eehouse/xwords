/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2023 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;

import java.util.HashMap;
import java.util.Map;

import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.jni.XwJNI.RematchOrder;
import org.eehouse.android.xw4.loc.LocUtils;

public class RematchConfigView extends LinearLayout
    implements RadioGroup.OnCheckedChangeListener
{
    private static final String TAG = RematchConfigView.class.getSimpleName();
    private static final String KEY_LAST_RO = TAG + "/key_last_ro";

    private Context mContext;
    private DlgDelegate.HasDlgDelegate mDlgDlgt;
    private RadioGroup mGroup;
    private GameUtils.GameWrapper mWrapper;
    private Map<Integer, RematchOrder> mRos = new HashMap<>();
    private boolean mInflated;
    private String mNameStr;
    private int[] mNewOrder;
    private String mSep;
    private boolean mUserEditing = false;
    private boolean mNAShown;
    private EditWClear mEWC;
    private RematchOrder mCurRO;

    public RematchConfigView( Context cx, AttributeSet as )
    {
        super( cx, as );
        mContext = cx;
    }

    public void configure( long rowid, DlgDelegate.HasDlgDelegate dlgDlgt )
    {
        mDlgDlgt = dlgDlgt;
        mWrapper = GameUtils.makeGameWrapper( mContext, rowid );
        trySetup();
    }

    public String getName()
    {
        return mEWC.getText().toString();
    }

    @Override
    protected void onFinishInflate()
    {
        mInflated = true;
        trySetup();
    }

    @Override
    protected void onDetachedFromWindow()
    {
        if ( null != mWrapper ) {
            mWrapper.close();
            mWrapper = null;
        }
        super.onDetachedFromWindow();
    }

    // RadioGroup.OnCheckedChangeListener
    @Override
    public void onCheckedChanged( RadioGroup group, int checkedId )
    {
        if ( !mUserEditing && null != mNameStr ) {
            mUserEditing = ! mNameStr.equals( getName() );
        }

        mCurRO = mRos.get( checkedId );
        mNewOrder = XwJNI.server_figureOrder( mWrapper.gamePtr(), mCurRO );

        if ( mUserEditing ) {
            if ( !mNAShown ) {
                mNAShown = true;
                mDlgDlgt.makeNotAgainBuilder( R.string.key_na_rematch_edit,
                                              R.string.na_rematch_edit )
                    .show();
            }
        } else {
            mNameStr = TextUtils.join( mSep, mWrapper.gi().playerNames(mNewOrder) );
            mEWC.setText( mNameStr );
        }
    }

    public int[] getNewOrder()
    {
        DBUtils.setIntFor( mContext, KEY_LAST_RO, mCurRO.ordinal() );
        return mNewOrder;
    }

    private void trySetup()
    {
        if ( mInflated && null != mWrapper ) {
            mSep = LocUtils.getString( mContext, R.string.vs_join );
            mGroup = (RadioGroup)findViewById( R.id.group );
            mGroup.setOnCheckedChangeListener( this );
            mEWC = (EditWClear)findViewById( R.id.name );

            boolean[] results = XwJNI.server_canOfferRematch( mWrapper.gamePtr() );
            if ( results[0] && results[1] ) {
                int ordinal = DBUtils.getIntFor( mContext, KEY_LAST_RO, 0 );
                RematchOrder lastSel = RematchOrder.values()[ordinal];

                for ( RematchOrder ro : RematchOrder.values() ) {
                    RadioButton button = new RadioButton( mContext );
                    button.setText( LocUtils.getString( mContext, ro.getStrID() ) );
                    mGroup.addView( button );
                    mRos.put( button.getId(), ro );
                    if ( lastSel == ro ) {
                        button.setChecked( true );
                    }
                }
            }
        }
    }
}
