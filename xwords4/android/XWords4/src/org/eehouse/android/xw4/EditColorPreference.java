/* -*- compile-command: "cd ../../../../../; ant install"; -*- */

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
    // private int m_color = 0;
    // private View m_sample = null;
    private boolean m_cancel;

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
        m_cancel = false;
        int color = getPersistedColor();
        setOneByte( view, R.id.edit_red, color >> 16 );
        setOneByte( view, R.id.edit_green, color >> 8 );
        setOneByte( view, R.id.edit_blue, color );
    }
    
    @Override
    protected void onPrepareDialogBuilder( AlertDialog.Builder builder )
    {
        builder.setPositiveButton( null, null );
        builder.setNegativeButton( R.string.button_cancel, 
                                   new DialogInterface.OnClickListener() {
                                       @Override
                                       public void onClick( DialogInterface dg,
                                                           int which )
                                       {
                                           m_cancel = true;
                                       }
                                   } );
        super.onPrepareDialogBuilder( builder );
    }

    @Override
    public void onDismiss( DialogInterface dialog )
    {
        if ( !m_cancel ) {
            int color = (getOneByte( dialog, R.id.edit_red ) << 16)
                | (getOneByte( dialog, R.id.edit_green ) << 8)
                | getOneByte( dialog, R.id.edit_blue );

            persistInt( color );
            View sample = 
                ((AlertDialog)dialog).findViewById( R.id.color_display_sample );
            if ( null != sample ) {
                sample.setBackgroundColor( getPersistedColor() );
            }
        }
    }

    private void setOneByte( View parent, int id, int byt ) {
        byt &= 0xFF;
        SeekBar seekbar = (SeekBar)parent.findViewById( id );
        if ( null != seekbar ) {
            seekbar.setProgress( byt );
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
