
package org.eehouse.android.xw4.jni;

import android.graphics.Rect;

public interface SyncedDraw {
    void doJNIDraw();
    void doIconDraw( int resID, final Rect rect );
    void prefsChanged();
}
