
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
        nPlayers = 2;
        boardSize = 15;
        players = new LocalPlayer[nPlayers];
        serverRole = DeviceRole.SERVER_STANDALONE;
        dictName = BUILTIN_DICT;
        hintsNotAllowed = false;
        players[0] = new LocalPlayer( "Player 1");
        players[1] = new LocalPlayer( "Player 2", true );
    }

    public CurGameInfo( CurGameInfo src ) {
        nPlayers = src.nPlayers;
        boardSize = src.boardSize;
        players = new LocalPlayer[nPlayers];
        serverRole = src.serverRole;
        dictName = src.dictName;
        hintsNotAllowed = src.hintsNotAllowed;
        
        int ii;
        for ( ii = 0; ii < nPlayers; ++ii ) {
            players[ii] = new LocalPlayer( src.players[ii] );
        }
    }

}

