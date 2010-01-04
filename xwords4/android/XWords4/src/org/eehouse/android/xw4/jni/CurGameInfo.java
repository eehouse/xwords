
package org.eehouse.android.xw4.jni;


public class CurGameInfo {

    private static final String BUILTIN_DICT = "OWL2_2to9.xwd";

    public enum XWPhoniesChoice { PHONIES_IGNORE, PHONIES_WARN, PHONIES_DISALLOW };
    public enum DeviceRole { SERVER_STANDALONE, SERVER_ISSERVER, SERVER_ISCLIENT };

    public String dictName;
    public LocalPlayer[] players;
    public int gameID;
    public int gameSeconds; /* for timer */
    public int nPlayers;
    public int boardSize;
    public DeviceRole serverRole;

    public boolean hintsNotAllowed;
    public boolean  timerEnabled;
    public boolean  allowPickTiles;
    public boolean  allowHintRect;
    public int robotSmartness;
    public XWPhoniesChoice phoniesAction;
    public boolean confirmBTConnect;   /* only used for BT */

    public CurGameInfo() {
        nPlayers = 3;
        boardSize = 15;
        players = new LocalPlayer[nPlayers];
        serverRole = DeviceRole.SERVER_STANDALONE;
        dictName = BUILTIN_DICT;
        hintsNotAllowed = false;
        players[0] = new LocalPlayer( "Eric");
        players[1] = new LocalPlayer( "Kati", true );
        players[2] = new LocalPlayer( "Brynn", true );
    }
}

