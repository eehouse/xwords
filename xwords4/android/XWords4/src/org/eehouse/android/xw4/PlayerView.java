/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4;

import android.widget.TextView;
import android.content.Context;
import android.util.AttributeSet;

public class PlayerView extends TextView {
    private int m_position;

    public PlayerView( Context cx, AttributeSet as ) {
        super( cx, as );
    }

    public int getPosition() { return m_position; }
    public void setPosition( int indx ) { m_position = indx; }
}
