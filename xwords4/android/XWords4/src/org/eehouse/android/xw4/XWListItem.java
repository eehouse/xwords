/* -*- compile-command: "cd ../../../../../; ant install"; -*- */

package org.eehouse.android.xw4;

import android.widget.TextView;
import android.content.Context;
import android.util.AttributeSet;

public class XWListItem extends TextView {
    private int m_position;

    public XWListItem( Context cx, AttributeSet as ) {
        super( cx, as );
    }

    public int getPosition() { return m_position; }
    public void setPosition( int indx ) { m_position = indx; }
}
