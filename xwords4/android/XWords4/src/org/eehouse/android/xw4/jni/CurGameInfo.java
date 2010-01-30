/* -*- compile-command: "cd ../../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4.jni;

import java.util.Random;
import android.content.Context;

import org.eehouse.android.xw4.Utils;

public class CurGameInfo {

    public static final int MAX_NUM_PLAYERS = 4;

    public enum XWPhoniesChoice { PHONIES_IGNORE, PHONIES_WARN, PHONIES_DISALLOW };
    public enum DeviceRole { SERVER_STANDALONE, SERVER_ISSERVER, SERVER_ISCLIENT };

    public String dictName;
    public LocalPlayer[] players;
    public int gameID;
    public int nPlayers;
    public int boardSize;
    public DeviceRole serverRole;

    public boolean hintsNotAllowed;
    public boolean  timerEnabled;
    public boolean  allowPickTiles;
    public boolean  allowHintRect;
    public boolean  showColors;
    public int robotSmartness;
    public XWPhoniesChoice phoniesAction;
    public boolean confirmBTConnect;   /* only used for BT */

    public CurGameInfo( Context context ) {
        nPlayers = 2;
        boardSize = 15;
        players = new LocalPlayer[MAX_NUM_PLAYERS];
        serverRole = DeviceRole.SERVER_STANDALONE;
        dictName = Utils.listDicts( context, 1 )[0];
        hintsNotAllowed = false;
        phoniesAction = XWPhoniesChoice.PHONIES_IGNORE;
        timerEnabled = false;
        allowPickTiles = false;
        allowHintRect = false;
        showColors = true;
        robotSmartness = 1;

        // Always create MAX_NUM_PLAYERS so jni code doesn't ever have
        // to cons up a LocalPlayer instance.
        int ii;
        for ( ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
            players[ii] = new LocalPlayer(ii);
        }
    }

    public CurGameInfo( CurGameInfo src ) {
        nPlayers = src.nPlayers;
        boardSize = src.boardSize;
        players = new LocalPlayer[MAX_NUM_PLAYERS];
        serverRole = src.serverRole;
        dictName = src.dictName;
        hintsNotAllowed = src.hintsNotAllowed;
        phoniesAction = src.phoniesAction;
        timerEnabled = src.timerEnabled;
        allowPickTiles = src.allowPickTiles;
        allowHintRect = src.allowHintRect;
        showColors = src.showColors;
        robotSmartness = src.robotSmartness;
        
        int ii;
        for ( ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
            players[ii] = new LocalPlayer( src.players[ii] );
        }
    }

    public boolean addPlayer() 
    {
        boolean canAdd = nPlayers < MAX_NUM_PLAYERS;
        if ( canAdd ) {
            // LocalPlayer newPlayer = new LocalPlayer( nPlayers );
            // players[nPlayers++] = newPlayer;
            ++nPlayers;
        }
        return canAdd;
    }

    public boolean moveUp( int which )
    {
        boolean canMove = which > 0 && which < nPlayers;
        if ( canMove ) {
            LocalPlayer tmp = players[which-1];
            players[which-1] = players[which];
            players[which] = tmp;
        }
        return canMove;
    }

    public boolean moveDown( int which )
    {
        return moveUp( which + 1 );
    }

    public boolean delete( int which )
    {
        boolean canDelete = nPlayers > 1;
        if ( canDelete ) {
            int ii;
            for ( ii = which; ii < nPlayers - 1; ++ii ) {
                moveDown( ii );
            }
            --nPlayers;
            players[nPlayers] = new LocalPlayer(nPlayers);
        }
        return canDelete;
    }

    public boolean juggle()
    {
        boolean canJuggle = nPlayers > 1;
        if ( canJuggle ) {
            // for each element, exchange with randomly chocsen from
            // range <= to self.
            Random rgen = new Random();

            Utils.logf( "nPlayers: " + nPlayers );
            Utils.logf( "players.length: " + players.length );

            for ( int ii = nPlayers - 1; ii > 0; --ii ) {
                // Contrary to docs, nextInt() comes back negative!
                int rand = Math.abs(rgen.nextInt()); 
                int indx = rand % (ii+1);
                if ( indx != ii ) {
                    LocalPlayer tmp = players[ii];
                    players[ii] = players[indx];
                    players[indx] = tmp;
                }
            }
        }
        return canJuggle;
    }

}

