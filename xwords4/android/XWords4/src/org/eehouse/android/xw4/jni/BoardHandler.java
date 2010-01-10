
package org.eehouse.android.xw4.jni;

import android.content.Context;

public interface BoardHandler {

    void startHandling( JNIThread thread, int gamePtr, CurGameInfo gi );

}
