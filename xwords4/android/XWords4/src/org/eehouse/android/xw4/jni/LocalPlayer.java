/* -*- compile-command: "cd ../../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4.jni;

public class LocalPlayer {
    public String name;
    public String password;
    public int secondsUsed;
    public boolean isRobot;
    public boolean isLocal;

    private LocalPlayer() {}

    public LocalPlayer( int num )
    {
        isLocal = true;
        isRobot = false;
        // This should be a template in strings.xml
        name = "Player " + (num + 1);
        password = "";
    }

    public LocalPlayer( final LocalPlayer src )
    {
        isLocal = src.isLocal;
        isRobot = src.isRobot;
        name = src.name;
        password = src.password;
        secondsUsed = src.secondsUsed;
    }
}

