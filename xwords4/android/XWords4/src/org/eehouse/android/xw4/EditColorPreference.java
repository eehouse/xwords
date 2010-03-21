/* -*- compile-command: "cd ../../../../../; ant install"; -*- */

package org.eehouse.android.xw4;

import android.preference.DialogPreference;
import android.content.Context;
import android.content.DialogInterface;
import android.util.AttributeSet;
import android.view.View;
import android.widget.EditText;
import android.app.Dialog;
// import android.app.AlertDialog;

import junit.framework.Assert;

public class EditColorPreference extends DialogPreference {

    private Context m_context;
    // private int m_color = 0;
    private View m_sample = null;

    public EditColorPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
        m_context = context;
        
        setWidgetLayoutResource( R.layout.color_display );
        setDialogLayoutResource( R.layout.color_edit );
    }
    
    @Override
    protected void onBindView( View parent ) 
    {
        super.onBindView( parent );
        m_sample = parent.findViewById( R.id.color_display_sample );
        if ( null != m_sample ) {
            m_sample.setBackgroundColor( getPersistedColor() );
        }
    }

    @Override
    protected void onBindDialogView( View view )
    {
        int color = getPersistedColor();
        setOneByte( view, R.id.edit_red, color >> 16 );
        setOneByte( view, R.id.edit_green, color >> 8 );
        setOneByte( view, R.id.edit_blue, color );
    }

    @Override
    public void onDismiss( DialogInterface dialog )
    {
        int color = (getOneByte( dialog, R.id.edit_red ) << 16)
            | (getOneByte( dialog, R.id.edit_green ) << 8)
            | getOneByte( dialog, R.id.edit_blue );
        color |= 0xFF000000;

        // Need to restore the preference, not set the background color
        persistString( String.format( "%d", color) );
    }

    private void setOneByte( View parent, int id, int byt ) {
        byt &= 0xFF;
        EditText et = (EditText)parent.findViewById( id );
        if ( null != et ) {
            et.setText( String.format("%d", byt ) );
        }
    }

    private int getOneByte( DialogInterface parent, int id ) {
        int val = 0;
        Dialog dialog = (Dialog)parent;
        EditText et = (EditText)dialog.findViewById( id );
        if ( null != et ) {
            String str = et.getText().toString();
            val = Integer.decode( str );
        }
        return val;
    }

    private int getPersistedColor()
    {
        String val = getPersistedString("");
        int color;
        try {
            color = 0xFF000000 | Integer.decode( val );
        } catch ( java.lang.NumberFormatException nfe ) {
            color = 0xFF7F7F7F;
        }
        return color;
    }
}
