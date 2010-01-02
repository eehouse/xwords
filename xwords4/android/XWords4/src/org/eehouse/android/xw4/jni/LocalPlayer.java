
package org.eehouse.android.xw4.jni;

public class LocalPlayer {
    public String name;
    public String password;
    public int secondsUsed;
    public boolean isRobot;
    public boolean isLocal;

    public LocalPlayer( String nm ) {
        isLocal = true;
        isRobot = false;
        name = nm;
        password = "";
    }

    public LocalPlayer( String nm, boolean robot ) {
        this( nm );
        isRobot = robot;
    }
}

