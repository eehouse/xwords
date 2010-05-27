/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
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

import android.preference.DialogPreference;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.View;
import android.widget.SeekBar;
import android.app.Dialog;
import android.content.SharedPreferences;
import android.app.AlertDialog;

import junit.framework.Assert;

public class EditColorPreference extends DialogPreference {

    private Context m_context;
    private int m_curColor;

    private class SBCL implements SeekBar.OnSeekBarChangeListener {
        int m_shift;
        View m_sample;
        public SBCL( View parent, int shift )
        {
            m_shift = shift;
            m_sample = parent.findViewById( R.id.color_edit_sample );
        }

        public void onProgressChanged( SeekBar seekBar, int progress, 
                                       boolean fromUser )
        {
            // mask out the byte we're changing
            int color = m_curColor & ~(0xFF << m_shift);
            // add in the new version of the byte
            color |= progress << m_shift;
            m_curColor = color;
            m_sample.setBackgroundColor( m_curColor );
        }

        public void onStartTrackingTouch( SeekBar seekBar ) {}

        public void onStopTrackingTouch( SeekBar seekBar ) {}
    }

    public EditColorPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
        m_context = context;
        
        setWidgetLayoutResource( R.layout.color_display );
        setDialogLayoutResource( R.layout.color_edit );
    }

    @Override
    protected Object onGetDefaultValue(TypedArray a, int index) {
        return a.getInteger(index, 0);
    }

    @Override
    protected void onSetInitialValue(boolean restoreValue, Object defaultValue) {
        if ( !restoreValue ) {
            persistInt( (Integer)defaultValue );
        }
    }
    
    @Override
    protected void onBindView( View parent ) 
    {
        super.onBindView( parent );
        View sample = parent.findViewById( R.id.color_display_sample );
        sample.setBackgroundColor( getPersistedColor() );
    }

    @Override
    protected void onBindDialogView( View view )
    {
        m_curColor = getPersistedColor();
        setOneByte( view, R.id.seek_red,  16 );
        setOneByte( view, R.id.seek_green,  8 );
        setOneByte( view, R.id.seek_blue, 0 );

        View sample = (View)view.findViewById( R.id.color_edit_sample );
        sample.setBackgroundColor( m_curColor );
    }
    
    @Override
    protected void onPrepareDialogBuilder( AlertDialog.Builder builder )
    {
        DialogInterface.OnClickListener lstnr = 
            new DialogInterface.OnClickListener() {
                @Override
                public void onClick( DialogInterface dialog, int which )
                {
                    int color = (getOneByte( dialog, R.id.seek_red ) << 16)
                        | (getOneByte( dialog, R.id.seek_green ) << 8)
                        | getOneByte( dialog, R.id.seek_blue );

                    persistInt( color );
                    notifyChanged();
                }
            };
        builder.setPositiveButton( R.string.button_ok, lstnr );
        super.onPrepareDialogBuilder( builder );
    }

    private void setOneByte( View parent, int id, int shift ) 
    {
        int byt = (m_curColor >> shift) & 0xFF;
        SeekBar seekbar = (SeekBar)parent.findViewById( id );
        if ( null != seekbar ) {
            seekbar.setProgress( byt );

            seekbar.setOnSeekBarChangeListener( new SBCL( parent, shift ) );
        }
    }

    private int getOneByte( DialogInterface parent, int id ) {
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
