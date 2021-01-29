/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
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

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.TypedArray;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.View;
import android.widget.EditText;
import android.widget.SeekBar;
import androidx.preference.DialogPreference;
import androidx.preference.PreferenceViewHolder;

import org.eehouse.android.xw4.loc.LocUtils;

public class EditColorPreference extends DialogPreference
    implements PrefsActivity.DialogProc {
    private static final String TAG = EditColorPreference.class.getSimpleName();

    private Context mContext;
    private int mCurColor;
    private View mWidget;
    // m_updateText: prevent loop that resets edittext cursor
    private boolean m_updateText = true;
    private static final int m_seekbarIds[] = { R.id.seek_red, R.id.seek_green,
                                                R.id.seek_blue };
    private static final int m_editIds[] = { R.id.edit_red, R.id.edit_green,
                                             R.id.edit_blue };

    private class SBCL implements SeekBar.OnSeekBarChangeListener {
        int m_index;
        View m_sample;
        EditText m_editTxt;
        public SBCL( View parent, EditText editTxt, int indx )
        {
            m_index = indx;
            m_sample = parent.findViewById( R.id.color_edit_sample );
            m_editTxt = editTxt;
        }

        @Override
        public void onProgressChanged( SeekBar seekBar, int progress,
                                       boolean fromUser )
        {
            if ( m_updateText ) {
                m_editTxt.setText( String.format( "%d", progress ) );
            }

            int shift = 16 - (m_index * 8);
            // mask out the byte we're changing
            int color = mCurColor & ~(0xFF << shift);
            // add in the new version of the byte
            color |= progress << shift;
            mCurColor = color;
            m_sample.setBackgroundColor( mCurColor );
        }

        @Override
        public void onStartTrackingTouch( SeekBar seekBar ) {}
        @Override
        public void onStopTrackingTouch( SeekBar seekBar ) {}
    }

    private class TCL implements TextWatcher {
        private SeekBar m_seekBar;
        public TCL( SeekBar seekBar ) { m_seekBar = seekBar; }
        @Override
        public void afterTextChanged( Editable s ) {}
        @Override
        public void beforeTextChanged( CharSequence s, int st, int cnt, int a ) {}
        @Override
        public void onTextChanged( CharSequence s, int start,
                                   int before, int count )
        {
            int val;
            try {
                val = Integer.parseInt( s.toString() );
            } catch ( java.lang.NumberFormatException nfe ) {
                val = 0;
            }
            m_updateText = false; // don't call me recursively inside seekbar
            m_seekBar.setProgress( val );
            m_updateText = true;
        }
    }

    public EditColorPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
        mContext = context;

        setWidgetLayoutResource( R.layout.color_display );
    }

    @Override
    public void onBindViewHolder( PreferenceViewHolder holder )
    {
        mWidget = holder.itemView;
        setWidgetColor();

        super.onBindViewHolder( holder );
    }

    private void setWidgetColor()
    {
        mWidget.findViewById( R.id.color_display_sample )
            .setBackgroundColor( getPersistedColor() );
    }

    @Override
    protected Object onGetDefaultValue( TypedArray arr, int index )
    {
        return arr.getInteger( index, 0 );
    }

    @Override
    protected void onSetInitialValue( boolean restoreValue, Object defaultValue )
    {
        if ( !restoreValue ) {
            persistInt( (Integer)defaultValue );
        }
    }

    // PrefsActivity.DialogProc interface
    @Override
    public XWDialogFragment makeDialogFrag()
    {
        return new ColorEditDialogFrag( this );
    }

    public static class ColorEditDialogFrag extends XWDialogFragment
        implements DialogInterface.OnShowListener {

        private EditColorPreference mPref;
        private View mView;

        ColorEditDialogFrag( EditColorPreference pref ) { mPref = pref; }

        @Override
        public Dialog onCreateDialog( Bundle sis )
        {
            mView = LocUtils.inflate( mPref.getContext(), R.layout.color_edit );

            DialogInterface.OnClickListener onOk =
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick( DialogInterface di,
                                         int which )
                    {
                        Log.d( TAG, "onClick()" );

                        int color = (getOneByte( di, R.id.seek_red ) << 16)
                            | (getOneByte( di, R.id.seek_green ) << 8)
                            | getOneByte( di, R.id.seek_blue );

                        mPref.persistInt( color );
                        mPref.setWidgetColor();
                        // notifyChanged();
                    }
                };

            Dialog dialog = LocUtils.makeAlertBuilder( mPref.getContext() )
                .setView( mView )
                .setTitle( mPref.getTitle() )
                .setPositiveButton( android.R.string.ok, onOk )
                .setNegativeButton( android.R.string.cancel, null )
                .create();

            dialog.setOnShowListener( this );
            return dialog;
        }

        @Override
        public void onShow( DialogInterface dlg )
        {
            mPref.onBindDialogView( mView );
        }

        @Override
        protected String getFragTag() { return getClass().getSimpleName(); }
    }

    private void onBindDialogView( View view )
    {
        LocUtils.xlateView( mContext, view );

        mCurColor = getPersistedColor();
        setOneByte( view, 0 );
        setOneByte( view, 1 );
        setOneByte( view, 2 );

        view.findViewById( R.id.color_edit_sample )
            .setBackgroundColor( mCurColor );
    }

    private void setOneByte( View parent, int indx )
    {
        int shift = 16 - (indx*8);
        int byt = (mCurColor >> shift) & 0xFF;
        SeekBar seekbar = (SeekBar)parent.findViewById( m_seekbarIds[indx] );
        EditText edittext = (EditText)parent.findViewById( m_editIds[indx] );

        if ( null != seekbar ) {
            seekbar.setProgress( byt );

            seekbar.setOnSeekBarChangeListener( new SBCL( parent, edittext,
                                                          indx ) );
        }

        if ( null != edittext ) {
            edittext.setText( String.format( "%d", byt ) );
            edittext.addTextChangedListener( new TCL( seekbar ) );
        }
    }

    private static int getOneByte( DialogInterface parent, int id )
    {
        int val = 0;
        Dialog dialog = (Dialog)parent;
        SeekBar seekbar = (SeekBar)dialog.findViewById( id );
        if ( null != seekbar ) {
            val = seekbar.getProgress();
        }
        return val;
    }

    private int getPersistedColor()
    {
        return 0xFF000000 | getPersistedInt(0);
    }
}
