/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2020 by Eric House (xwords@eehouse.org).  All rights reserved.
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
import android.graphics.Bitmap;
import android.graphics.Color;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.google.zxing.BarcodeFormat;
import com.google.zxing.MultiFormatWriter;
import com.google.zxing.WriterException;
import com.google.zxing.common.BitMatrix;

import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.loc.LocUtils;

public class InviteView extends ScrollView
    implements RadioGroup.OnCheckedChangeListener {

    private static final String TAG = InviteView.class.getSimpleName();
    private static final String KEY_EXPANDED = TAG + ":expanded";
    private static final int QRCODE_SIZE_SMALL = 320;
    private static final int QRCODE_SIZE_LARGE = QRCODE_SIZE_SMALL * 2;
    
    public interface ItemClicked {
        public void meansClicked( InviteMeans means );
        public void checkButton();
    }

    private ItemClicked mProcs;
    private boolean mIsWho;
    private RadioGroup mGroupTab;
    private LimSelGroup mGroupWho;
    private RadioGroup mGroupHow;
    private Map<RadioButton, InviteMeans> mHowMeans = new HashMap<>();
    private boolean mExpanded = false;
    private NetLaunchInfo mNli;

    public InviteView( Context context, AttributeSet as ) {
        super( context, as );
    }

    public InviteView setChoices( List<InviteMeans> meansList, int sel,
                                  String[] players, int maxPlayers )
    {
        final Context context = getContext();

        boolean haveWho = null != players && 0 < players.length;

        // top/horizontal group or title first
        if ( haveWho ) {
            mGroupTab = (RadioGroup)findViewById( R.id.group_tab );
            mGroupTab.check( R.id.radio_how );
            mGroupTab.setOnCheckedChangeListener( this );
            mGroupTab.setVisibility( View.VISIBLE );
        } else {
            findViewById( R.id.title_tab ).setVisibility( View.VISIBLE );
        }

        mGroupHow = (RadioGroup)findViewById( R.id.group_how );
        mGroupHow.setOnCheckedChangeListener( this );
        final View divider = mGroupHow.findViewById( R.id.local_divider );
        for ( InviteMeans means : meansList ) {
            Assert.assertNotNull( means );
            RadioButton button = (RadioButton)LocUtils
                .inflate( context, R.layout.invite_radio );
            button.setText( LocUtils.getString( context, means.getUserDescID() ) );
            int where = means.isForLocal()
                // -1: place before QRcode-wrapper
                ? mGroupHow.getChildCount() - 1
                : mGroupHow.indexOfChild( divider );
            mGroupHow.addView( button, where );
            mHowMeans.put( button, means );
        }

        if ( haveWho ) {
            mGroupWho = ((LimSelGroup)findViewById( R.id.group_who ))
                .setLimit( maxPlayers )
                .addPlayers( players )
                ;
        }
        mIsWho = false;   // start with how
        showWhoOrHow();

        mExpanded = DBUtils.getBoolFor( context, KEY_EXPANDED, false );
        ((ExpandImageButton)findViewById( R.id.expander ))
            .setOnExpandChangedListener( new ExpandImageButton.ExpandChangeListener() {
                    @Override
                    public void expandedChanged( boolean nowExpanded )
                    {
                        mExpanded = nowExpanded;
                        DBUtils.setBoolFor( context, KEY_EXPANDED, nowExpanded );
                        startQRCodeThread( null );
                    }
                } )
            .setExpanded( mExpanded );

        return this;
    }

    public InviteView setNli( NetLaunchInfo nli )
    {
        startQRCodeThread( nli );
        return this;
    }

    public InviteView setCallbacks( ItemClicked procs )
    {
        mProcs = procs;
        if ( null != mGroupWho ) {
            mGroupWho.setCallbacks( procs );
        }
        return this;
    }

    public Object getChoice()
    {
        Object result = null;
        if ( mIsWho ) {
            result = mGroupWho.getSelected();
        } else {
            int curSel = mGroupHow.getCheckedRadioButtonId();
            if ( 0 <= curSel ) {
                RadioButton button = (RadioButton)findViewById(curSel);
                result = mHowMeans.get( button );
            }
        }
        return result;
    }

    @Override
    public void onCheckedChanged( RadioGroup group, int checkedId )
    {
        if ( -1 != checkedId ) {
            switch( group.getId() ) {
            case R.id.group_tab:
                mIsWho = checkedId == R.id.radio_who;
                showWhoOrHow();
                break;
            case R.id.group_how:
                RadioButton button = (RadioButton)group.findViewById(checkedId);
                InviteMeans means = mHowMeans.get( button );
                mProcs.meansClicked( means );
                setShowQR( means.equals( InviteMeans.QRCODE ) );
                break;
            case R.id.group_who:
                break;
            }

            mProcs.checkButton();
        }
    }

    private void setShowQR( boolean show )
    {
        findViewById( R.id.qrcode_stuff )
            .setVisibility( show ? View.VISIBLE: View.GONE );
    }

    private void showWhoOrHow()
    {
        if ( null != mGroupWho ) {
            mGroupWho.setVisibility( mIsWho ? View.VISIBLE : View.INVISIBLE );
        }
        mGroupHow.setVisibility( mIsWho ? View.INVISIBLE : View.VISIBLE );

        boolean showEmpty = mIsWho && 0 == mGroupWho.getChildCount();
        findViewById( R.id.who_empty )
            .setVisibility( showEmpty ? View.VISIBLE : View.INVISIBLE );
    }

    private void startQRCodeThread( NetLaunchInfo nli )
    {
        if ( null != nli ) {
            mNli = nli;
        }
        if ( null != mNli ) {
            final String url = mNli.makeLaunchUri( getContext() ).toString();
            new Thread( new Runnable() {
                    @Override
                    public void run() {
                        try {
                            int qrSize = mExpanded ? QRCODE_SIZE_LARGE : QRCODE_SIZE_SMALL;
                            MultiFormatWriter multiFormatWriter = new MultiFormatWriter();
                            BitMatrix bitMatrix = multiFormatWriter.encode( url, BarcodeFormat.QR_CODE,
                                                                            qrSize, qrSize );
                            final Bitmap bitmap = Bitmap.createBitmap( qrSize, qrSize,
                                                                       Bitmap.Config.ARGB_8888 );
                            for ( int ii = 0; ii < qrSize; ++ii ) {
                                for ( int jj = 0; jj < qrSize; ++jj ) {
                                    bitmap.setPixel( ii, jj, bitMatrix.get(ii, jj)
                                                     ? Color.BLACK : Color.WHITE );
                                }
                            }

                            post( new Runnable() {
                                    @Override
                                    public void run() {
                                        ImageView iv = (ImageView)findViewById( R.id.qr_view );
                                        iv.setImageBitmap( bitmap );
                                        if ( BuildConfig.NON_RELEASE ) {
                                            TextView tv = (TextView)findViewById( R.id.qr_url );
                                            tv.setVisibility( View.VISIBLE );
                                            tv.setText( url );
                                        }
                                        post ( new Runnable() {
                                                @Override
                                                public void run() {
                                                    scrollTo( 0, getBottom() );
                                                }
                                            } );
                                    }
                                } );
                        } catch ( WriterException we ) {
                            Log.ex( TAG, we );
                        }
                    }
                } ).start();
        }
    }
}
