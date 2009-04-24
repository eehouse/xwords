/*
 * Generates #defines that are best kept sequential
 */
#include <stdio.h>

static char* ids[] = {
    "IDS_MENU"
    ,"IDS_CANCEL"
    ,"IDS_OK"
    ,"IDS_ABOUT"
    ,"IDS_DONE"
    ,"IDS_LANGUAGE_NAME"
    ,"IDS_NEW_GAME"
    ,"IDS_DICTLOC"
    ,"IDS_SAVENAME"
    ,"IDS_DUPENAME"
    ,"IDS_RENAME"
    ,"IDS_REMAINING_TILES_ADD"
    ,"IDS_UNUSED_TILES_SUB"
    ,"IDS_BONUS_ALL"
    ,"IDS_TURN_SCORE"
    ,"IDS_COMMIT_CONFIRM"
    ,"IDS_LOCAL_NAME"
    ,"IDS_REM"
    ,"IDS_IGNORE_L"
    ,"IDS_WARN_L"
    ,"IDS_DISALLOW_L"
    ,"IDS_NONLOCAL_NAME"
    ,"IDS_TIME_PENALTY_SUB"
    ,"IDS_CUMULATIVE_SCORE"
    ,"IDS_MOVE_ACROSS"
    ,"IDS_MOVE_DOWN"
    ,"IDS_TRAY_AT_START"
    ,"IDS_NEW_TILES"
    ,"IDS_TRADED_FOR"
    ,"IDS_PASS"
    ,"IDS_PHONY_REJECTED"
    ,"IDS_ROBOT_TRADED"
    ,"IDS_ROBOT_MOVED"
    ,"IDS_REMOTE_MOVED"
    ,"IDS_PASSED"
    ,"IDS_REMTILES_L"
    ,"IDS_SUMMARYSCORED"
    ,"IDS_TRADED"
    ,"IDS_LOSTTURN"
    ,"IDS_TOTALPLAYERS"
    ,"IDS_VALUES_HEADER"
    ,"IDS_TILES_NOT_IN_LINE"
    ,"IDS_NO_EMPTIES_IN_TURN"
    ,"IDS_TWO_TILES_FIRST_MOVE"
    ,"IDS_TILES_MUST_CONTACT"
    ,"IDS_NOT_YOUR_TURN"
    ,"IDS_NO_PEEK_ROBOT_TILES"
    ,"IDS_CANT_TRADE_MID_MOVE"
    ,"IDS_TOO_FEW_TILES_LEFT_TO_TRADE"
    ,"IDS_CANT_UNDO_TILEASSIGN"
    ,"IDS_CANT_HINT_WHILE_DISABLED"
    ,"IDS_QUERY_TRADE"
    ,"IDS_DOUBLE_LETTER"
    ,"IDS_DOUBLE_WORD"
    ,"IDS_TRIPLE_LETTER"
    ,"IDS_TRIPLE_WORD"
    ,"IDS_INTRADE_MW"
    ,"IDS_COUNTSVALS_L"
    ,"IDS_GAMEHIST_L"
    ,"IDS_FINALSCORE_L"
    ,"IDS_QUESTION_L"
    ,"IDS_FYI_L"
    ,"IDS_ILLEGALWRD_L"
    ,"IDS_WRDNOTFOUND"
    ,"IDS_USEANYWAY"
    ,"IDS_CANNOTOPEN_GAME"
    ,"IDS_NODICT_L"
    ,"IDS_ABOUT_L"
    ,"IDS_OVERWRITE"
    ,"IDS_ENDNOW"
    ,"IDS_CANNOTOPEN_DICT"
    ,"IDS_CONFIM_DELETE"
    ,"IDS_ROLE_STANDALONE"
    ,"IDS_ROLE_HOST"
    ,"IDS_ROLE_GUEST"
    ,"IDS_PLAYER_FORMAT"
    ,"IDS_UNTITLED_FORMAT"
    ,"IDS_CONN_RELAY"
    ,"IDS_CONN_DIRECT"
    ,"IDS_CONN_SMS"
#ifndef XWFEATURE_STANDALONE_ONLY
    ,"IDS_LOCALPLAYERS"
    ,"IDS_NO_PEEK_REMOTE_TILES"
    ,"IDS_REG_UNEXPECTED_USER"
    ,"IDS_SERVER_DICT_WINS"
    ,"IDS_REG_SERVER_SANS_REMOTE"
# ifdef XWFEATURE_RELAY
    ,"IDS_XWRELAY_ERROR_TIMEOUT"
    ,"IDS_ERROR_HEART_YOU"
    ,"IDS_XWRELAY_ERROR_HEART_OTHER"
    ,"IDS_XWRELAY_ERROR_LOST_OTHER"
# endif
#endif
};

#define FIRST_ID 40002

int 
main( int argc, char** argv )
{
    int firstID = FIRST_ID;
    int ii;

    printf( "/* -*- mode: c; -*- */\n" );
    printf( "/***********************************************************\n" );
    printf( " * GENERATED CODE; DO NOT EDIT\n" );
    printf( " * (edit scripts/strids.c instead)\n" );
    printf( " ***********************************************************/\n" );
    printf( "\n" );

    for ( ii = 0; ii < sizeof(ids)/sizeof(ids[0]); ++ii ) {
        printf( "#define %-60s %d\n", ids[ii], firstID++ );
    }

    printf( "\n" );
    printf( "#define %-60s %d\n", "CE_FIRST_RES_ID", FIRST_ID );
    printf( "#define %-60s %d\n", "CE_LAST_RES_ID", FIRST_ID + ii - 1 );

    return 0;

    argc = argc;
    argv = argv;
}
