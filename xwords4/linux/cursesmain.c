/* 
 * Copyright 2000 - 2020 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifdef PLATFORM_NCURSES

#include <ncurses.h>
#include <signal.h>
#include <assert.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netdb.h>		/* gethostbyname */
#include <errno.h>
//#include <net/netinet.h>

#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "cJSON.h"

#include "linuxmain.h"
#include "linuxutl.h"
#include "linuxdict.h"
#include "cursesmain.h"
#include "cursesask.h"
#include "cursesletterask.h"
#include "linuxbt.h"
#include "model.h"
#include "draw.h"
#include "board.h"
#include "engine.h"
/* #include "compipe.h" */
#include "xwstream.h"
#include "xwstate.h"
#include "strutils.h"
#include "server.h"
#include "memstream.h"
#include "util.h"
#include "dbgutil.h"
#include "linuxsms.h"
#include "linuxudp.h"
#include "gamesdb.h"
#include "relaycon.h"
#include "mqttcon.h"
#include "smsproto.h"
#include "device.h"
#include "cursesmenu.h"
#include "cursesboard.h"
#include "curgamlistwin.h"
#include "gsrcwrap.h"
#include "extcmds.h"
#include "knownplyr.h"

#ifndef CURSES_CELL_HT
# define CURSES_CELL_HT 1
#endif
#ifndef CURSES_CELL_WIDTH
# define CURSES_CELL_WIDTH 2
#endif

#define INFINITE_TIMEOUT -1
#define BOARD_SCORE_PADDING 3

struct CursesAppGlobals {
    CommonAppGlobals cag;
    CursesMenuState* menuState;
    CursGameList* gameList;
    CursesBoardState* cbState;
    WINDOW* mainWin;
    int winWidth, winHeight;

    XP_U16 nLinesMenu;
    gchar* lastErr;

    short statusLine;

    struct sockaddr_in listenerSockAddr;
#ifdef USE_GLIBLOOP
    GMainLoop* loop;
    GList* sources;
    int quitpipe[2];
    int winchpipe[2];
#else
    XP_Bool timeToExit;
    short fdCount;
    struct pollfd fdArray[FD_MAX]; /* one for stdio, one for listening socket */
    int timepipe[2];		/* for reading/writing "user events" */
#endif
};

static bool handleOpenGame( void* closure, int key );
static bool handleNewGame( void* closure, int key );
static bool handleDeleteGame( void* closure, int key );
static bool handleSel( void* closure, int key );

const MenuList g_sharedMenuList[] = {
    { handleQuit, "Quit", "Q", 'Q' },
    { handleNewGame, "New Game", "N", 'N' },
    { handleOpenGame, "Open Sel.", "O", 'O' },
    { handleDeleteGame, "Delete Sel.", "D", 'D' },
    { handleSel, "Select up", "J", 'J' },
    { handleSel, "Select down", "K", 'K' },
/*     { handleResend, "Resend", "R", 'R' }, */
/*     { handleSpace, "Raise focus", "<spc>", ' ' }, */
/*     { handleRet, "Click/tap", "<ret>", '\r' }, */
/*     { handleHint, "Hint", "?", '?' }, */

/* #ifdef KEYBOARD_NAV */
/*     { handleLeft, "Left", "H", 'H' }, */
/*     { handleRight, "Right", "L", 'L' }, */
/*     { handleUp, "Up", "J", 'J' }, */
/*     { handleDown, "Down", "K", 'K' }, */
/* #endif */

/*     { handleCommit, "Commit move", "C", 'C' }, */
/*     { handleFlip, "Flip", "F", 'F' }, */
/*     { handleToggleValues, "Show values", "V", 'V' }, */

/*     { handleBackspace, "Remove from board", "<del>", 8 }, */
/*     { handleUndo, "Undo prev", "U", 'U' }, */
/*     { handleReplace, "uNdo cur", "N", 'N' }, */

    { NULL, NULL, NULL, '\0'}
};


#ifdef CURSES_SMALL_SCREEN
const MenuList g_rootMenuListShow[] = {
    { handleRootKeyShow,  "Press . for menu", "", '.' },
    { NULL, NULL, NULL, '\0'}
};

const MenuList g_rootMenuListHide[] = {
    { handleRootKeyHide,  "Clear menu", ".", '.' },
    { NULL, NULL, NULL, '\0'}
};
#endif


static CursesAppGlobals g_globals;	/* must be global b/c of SIGWINCH_handler */

#ifdef KEYBOARD_NAV
/* static void changeMenuForFocus( CursesAppGlobals* globals,  */
/*                                 BoardObjectType obj ); */
/* static XP_Bool handleLeft( CursesAppGlobals* globals ); */
/* static XP_Bool handleRight( CursesAppGlobals* globals ); */
/* static XP_Bool handleUp( CursesAppGlobals* globals ); */
/* static XP_Bool handleDown( CursesAppGlobals* globals ); */
/* static XP_Bool handleFocusKey( CursesAppGlobals* globals, XP_Key key ); */
#else 
# define handleFocusKey( g, k ) XP_FALSE
#endif
/* static void countMenuLines( const MenuList** menuLists, int maxX, int padding, */
/*                             int* nLinesP, int* nColsP ); */
/* static void drawMenuFromList( WINDOW* win, const MenuList** menuLists, */
/*                               int nLines, int padding ); */
/* static CursesMenuHandler getHandlerForKey( const MenuList* list, char ch ); */


#ifdef MEM_DEBUG
# define MEMPOOL cGlobals->util->mpool,
#else
# define MEMPOOL
#endif

/* extern int errno; */

static void
initCurses( CursesAppGlobals* aGlobals )
{
    /* ncurses man page says most apps want this sequence  */
    if ( !aGlobals->cag.params->closeStdin ) {
        aGlobals->mainWin = initscr();
        cbreak();
        noecho();
        nonl();
        intrflush(stdscr, FALSE);
        keypad(stdscr, TRUE);       /* effects wgetch only? */

        getmaxyx( aGlobals->mainWin, aGlobals->winHeight, aGlobals->winWidth );
        XP_LOGFF( "getmaxyx()->w:%d; h:%d", aGlobals->winWidth,
                  aGlobals->winHeight );
    }

    /* globals->statusLine = height - MENU_WINDOW_HEIGHT - 1; */
    /* globals->menuWin = newwin( MENU_WINDOW_HEIGHT, width,  */
    /*                            height-MENU_WINDOW_HEIGHT, 0 ); */
    /* nodelay(globals->menuWin, 1);		/\* don't block on getch *\/ */

} /* initCurses */

#if 0
static void
showStatus( CursesAppGlobals* globals )
{
    char* str;

    switch ( globals->state ) {
    case XW_SERVER_WAITING_CLIENT_SIGNON:
	str = "Waiting for client[s] to connnect";
	break;
    case XW_SERVER_READY_TO_PLAY:
	str = "It's somebody's move";
	break;
    default:
	str = "unknown state";
    }

    
    standout();
    mvaddstr( globals->statusLine, 0, str );
/*     clrtoeol();     */
    standend();

    refresh();
} /* showStatus */
#endif

bool
handleQuit( void* closure, int XP_UNUSED(key) )
{
    CursesAppGlobals* globals = (CursesAppGlobals*)closure;
    g_main_loop_quit( globals->loop );
    return XP_TRUE;
} /* handleQuit */

static void
invokeQuit( void* data )
{
    LaunchParams* params = (LaunchParams*)data;
    CursesAppGlobals* globals = (CursesAppGlobals*)params->appGlobals;
    handleQuit( globals, 0 );
}

static void
figureDims( CursesAppGlobals* aGlobals, cb_dims* dims )
{
    LaunchParams* params = aGlobals->cag.params;
    dims->width = aGlobals->winWidth;
    dims->top = params->cursesListWinHt;
    dims->height = aGlobals->winHeight - params->cursesListWinHt - MENU_WINDOW_HEIGHT;
}

static bool
handleOpenGame( void* closure, int XP_UNUSED(key) )
{
    LOG_FUNC();
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    const GameInfo* gi = cgl_getSel( aGlobals->gameList );
    XP_ASSERT( !!gi );
    cb_dims dims;
    figureDims( aGlobals, &dims );
    cb_open( aGlobals->cbState, gi->rowid, &dims );
    return XP_TRUE;
}

static bool
canMakeFromGI( const CurGameInfo* gi )
{
    LOG_FUNC();
    bool result = 0 < gi->nPlayers
        && !!gi->isoCodeStr[0]
        ;
    bool haveDict = !!gi->dictName;
    bool allHaveDicts = true;
    for ( int ii = 0; result && ii < gi->nPlayers; ++ii ) {
        const LocalPlayer* lp = &gi->players[ii];
        result = !lp->isLocal || (!!lp->name && '\0' != lp->name[0]);
        if ( allHaveDicts ) {
            allHaveDicts = !!lp->dictName;
        }
    }

    result = result && (haveDict || allHaveDicts);

    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

static bool
handleNewGame( void* closure, int XP_UNUSED(key) )
{
    LOG_FUNC();
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;

    // aGlobals->cag.params->needsNewGame = XP_FALSE;

    cb_dims dims;
    figureDims( aGlobals, &dims );

    const CurGameInfo* gi = &aGlobals->cag.params->pgi;
    if ( !canMakeFromGI(gi) ) {
        ca_inform( aGlobals->mainWin, "Unable to create game (check params?)" );
    } else if ( !cb_newGame( aGlobals->cbState, &dims, NULL, NULL ) ) {
        XP_ASSERT(0);
    }
    return XP_TRUE;
}

static bool
handleDeleteGame( void* closure, int XP_UNUSED(key) )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    const char* question = "Are you sure you want to delete the "
        "selected game? This action cannot be undone";
    const char* buttons[] = { "Cancel", "Ok", };
    if ( 1 == cursesask( aGlobals->mainWin, question, /* ?? */
                         VSIZE(buttons), buttons ) ) {

        const GameInfo* gib = cgl_getSel( aGlobals->gameList );
        if ( !!gib ) {
            gdb_deleteGame( aGlobals->cag.params->pDb, gib->rowid );
            cgl_remove( aGlobals->gameList, gib->rowid );
        }
    }
    return XP_TRUE;
}

static bool
handleSel( void* closure, int key )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    XP_ASSERT( key == 'K' || key == 'J' );
    bool down = key == 'J';
    cgl_moveSel( aGlobals->gameList, down );
    return true;
}

/* static XP_Bool */
/* handleResend( CursesAppGlobals* globals ) */
/* { */
/*     if ( !!globals->cGlobals.game.comms ) { */
/*         comms_resendAll( globals->cGlobals.game.comms, COMMS_CONN_NONE, */
/*                          XP_TRUE ); */
/*     } */
/*     return XP_TRUE; */
/* } */

/* #ifdef KEYBOARD_NAV */
/* static void */
/* checkAssignFocus( BoardCtxt* board ) */
/* { */
/*     if ( OBJ_NONE == board_getFocusOwner(board) ) { */
/*         board_focusChanged( board, OBJ_BOARD, XP_TRUE ); */
/*     } */
/* } */
/* #else */
/* # define checkAssignFocus(b) */
/* #endif */

/* static XP_Bool */
/* handleSpace( CursesAppGlobals* globals ) */
/* { */
/*     XP_Bool handled; */
/*     checkAssignFocus( globals->cGlobals.game.board ); */

/*     globals->doDraw = board_handleKey( globals->cGlobals.game.board,  */
/*                                        XP_RAISEFOCUS_KEY, &handled ); */
/*     return XP_TRUE; */
/* } /\* handleSpace *\/ */

/* static XP_Bool */
/* handleRet( CursesAppGlobals* globals ) */
/* { */
/*     XP_Bool handled; */
/*     globals->doDraw = board_handleKey( globals->cGlobals.game.board,  */
/*                                        XP_RETURN_KEY, &handled ); */
/*     return XP_TRUE; */
/* } /\* handleRet *\/ */

/* static XP_Bool */
/* handleHint( CursesAppGlobals* globals ) */
/* { */
/*     XP_Bool redo; */
/*     globals->doDraw = board_requestHint( globals->cGlobals.game.board,  */
/* #ifdef XWFEATURE_SEARCHLIMIT */
/*                                          XP_FALSE, */
/* #endif */
/*                                          XP_FALSE, &redo ); */
/*     return XP_TRUE; */
/* } /\* handleHint *\/ */

#ifdef CURSES_SMALL_SCREEN
static XP_Bool
handleRootKeyShow( CursesAppGlobals* globals )
{
    WINDOW* win;
    MenuList* lists[] = { g_sharedMenuList, globals->menuList, 
                          g_rootMenuListHide, NULL };
    int winMaxY, winMaxX;
    
    wclear( globals->menuWin );
    wrefresh( globals->menuWin ); 

    getmaxyx( globals->boardWin, winMaxY, winMaxX );

    int border = 2;
    int width = winMaxX - (border * 2);
    int padding = 1;            /* for the box */
    int nLines, nCols;
    countMenuLines( lists, width, padding, &nLines, &nCols );

    if ( width > nCols ) {
        width = nCols;
    }

    win = newwin( nLines+(padding*2), width+(padding*2), 
                  ((winMaxY-nLines-padding-padding)/2), (winMaxX-width)/2 );
    wclear( win );
    box( win, '|', '-');

    drawMenuFromList( win, lists, nLines, padding );
    wrefresh( win );

    CursesMenuHandler handler = NULL;
    while ( !handler ) {
        int ch = fgetc( stdin );

        int i;
        for ( i = 0; !!lists[i]; ++i ) {
            handler = getHandlerForKey( lists[i], ch );
            if ( !!handler ) {
                break;
            }
        }
    }

    delwin( win );

    touchwin( globals->boardWin );
    wrefresh( globals->boardWin );
    MenuList* ml[] = { g_rootMenuListShow, NULL };
    drawMenuFromList( globals->menuWin, ml, 1, 0 );
    wrefresh( globals->menuWin ); 

    return handler != NULL && (*handler)(globals);
} /* handleRootKeyShow */

static XP_Bool
handleRootKeyHide( CursesAppGlobals* globals )
{
    globals->doDraw = XP_TRUE;
    return XP_TRUE;
}
#endif


/* static void */
/* fmtMenuItem( const MenuList* item, char* buf, int maxLen ) */
/* { */
/*     snprintf( buf, maxLen, "%s %s", item->keyDesc, item->desc ); */
/* } */

/* static void */
/* countMenuLines( const MenuList** menuLists, int maxX, int padding, */
/*                 int* nLinesP, int* nColsP ) */
/* { */
/*     int nCols = 0; */
/*     /\* The menu space should be wider rather than taller, but line up by */
/*        column.  So we want to use as many columns as possible to minimize the */
/*        number of lines.  So start with one line and lay out.  If that doesn't */
/*        fit, try two.  Given the number of lines, get the max width of each */
/*        column. */
/*     *\/ */

/*     maxX -= padding * 2;        /\* on left and right side *\/ */

/*     int nLines; */
/*     for ( nLines = 1; ; ++nLines ) { */
/*         short line = 0; */
/*         XP_Bool tooFewLines = XP_FALSE; */
/*         int maxThisCol = 0; */
/*         int i; */
/*         nCols = 0; */

/*         for ( i = 0; !tooFewLines && (NULL != menuLists[i]); ++i ) { */
/*             const MenuList* entry; */
/*             for ( entry = menuLists[i]; !tooFewLines && !!entry->handler;  */
/*                   ++entry ) { */
/*                 int width; */
/*                 char buf[32]; */

/*                 /\* time to switch to new column? *\/ */
/*                 if ( line == nLines ) { */
/*                     nCols += maxThisCol; */
/*                     if ( nCols > maxX ) { */
/*                         tooFewLines = XP_TRUE; */
/*                         break; */
/*                     } */
/*                     maxThisCol = 0; */
/*                     line = 0; */
/*                 } */

/*                 fmtMenuItem( entry, buf, sizeof(buf) ); */
/*                 width = strlen(buf) + 2; /\* padding *\/ */

/*                 if ( maxThisCol < width ) { */
/*                     maxThisCol = width; */
/*                 } */

/*                 ++line; */
/*             } */
/*         } */
/*         /\* If we get here without running out of space, we're done *\/ */
/*         nCols += maxThisCol; */
/*         if ( !tooFewLines && (nCols < maxX) ) { */
/*             break; */
/*         } */
/*     } */
    
/*     *nColsP = nCols; */
/*     *nLinesP = nLines; */
/* } /\* countMenuLines *\/ */

/* static void */
/* drawMenuFromList( WINDOW* win, const MenuList** menuLists, */
/*                   int nLines, int padding ) */
/* { */
/*     short line = 0, col, i; */
/*     int winMaxY, winMaxX; */

/*     getmaxyx( win, winMaxY, winMaxX ); */
/*     XP_USE(winMaxY); */

/*     int maxColWidth = 0; */
/*     if ( 0 == nLines ) { */
/*         int ignore; */
/*         countMenuLines( menuLists, winMaxX, padding, &nLines, &ignore ); */
/*     } */
/*     col = 0; */

/*     for ( i = 0; NULL != menuLists[i]; ++i ) { */
/*         const MenuList* entry; */
/*         for ( entry = menuLists[i]; !!entry->handler; ++entry ) { */
/*             char buf[32]; */

/*             fmtMenuItem( entry, buf, sizeof(buf) ); */

/*             mvwaddstr( win, line+padding, col+padding, buf ); */

/*             int width = strlen(buf) + 2; */
/*             if ( width > maxColWidth ) { */
/*                 maxColWidth = width; */
/*             } */

/*             if ( ++line == nLines ) { */
/*                 line = 0; */
/*                 col += maxColWidth; */
/*                 maxColWidth = 0; */
/*             } */

/*         } */
/*     } */
/* } /\* drawMenuFromList *\/ */

static void
writeToPipe( int pipe )
{
    if ( 1 != write( pipe, "!", 1 ) ) {
        XP_ASSERT(0);
    }
}

static void
readFromPipe( GIOChannel* source )
{
    int pipe = g_io_channel_unix_get_fd( source );
    char ch;
#ifdef DEBUG
    ssize_t nRead =
#endif
        read( pipe, &ch, sizeof(ch) );
    XP_ASSERT( nRead == sizeof(ch) && ch == '!' );
}

static void
SIGWINCH_handler( int signal )
{
    assert( signal == SIGWINCH );

    /* Write to pipe to force update */
    writeToPipe( g_globals.winchpipe[1] );
} /* SIGWINCH_handler */

static void 
SIGINTTERM_handler( int XP_UNUSED(signal) )
{
    writeToPipe( g_globals.quitpipe[1] );
}

#ifdef USE_GLIBLOOP
static gboolean
handle_quitwrite( GIOChannel* source, GIOCondition XP_UNUSED(condition), gpointer data )
{
    LOG_FUNC();
    readFromPipe( source );
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)data;
    handleQuit( aGlobals, 0 );
    return TRUE;
}

static gboolean
handle_winchwrite( GIOChannel* source, GIOCondition XP_UNUSED_DBG(condition), gpointer data )
{
    XP_LOGFF( "(condition=%x)", condition );
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)data;

    /* Read from the pipe so it won't call again */
    readFromPipe( source );

    struct winsize ws;
    ioctl( STDIN_FILENO, TIOCGWINSZ, &ws );
    XP_LOGFF( "lines %d, columns %d", ws.ws_row, ws.ws_col );
    aGlobals->winHeight = ws.ws_row;
    aGlobals->winWidth = ws.ws_col;

    resize_term( ws.ws_row, ws.ws_col );

    cgl_resized( aGlobals->gameList, g_globals.winWidth,
                 g_globals.cag.params->cursesListWinHt );
    if ( !!aGlobals->menuState ) {
        cmenu_resized( aGlobals->menuState );
    }

    cb_dims dims;
    figureDims( aGlobals, &dims );
    cb_resized( aGlobals->cbState, &dims );

    LOG_RETURN_VOID();
    return TRUE;
}

#endif

#ifndef USE_GLIBLOOP
#ifdef XWFEATURE_RELAY
static int
figureTimeout( CursesAppGlobals* globals )
{
    int result = INFINITE_TIMEOUT;
    XWTimerReason ii;
    XP_U32 now = util_getCurSeconds( globals->cGlobals.params->util );

    now *= 1000;

    for ( ii = 0; ii < NUM_TIMERS_PLUS_ONE; ++ii ) {
        TimerInfo* tip = &globals->cGlobals.timerInfo[ii];
        if ( !!tip->proc ) {
            XP_U32 then = tip->when * 1000;
            if ( now >= then ) {
                result = 0;
                break;          /* if one's immediate, we're done */
            } else {
                then -= now;
                if ( result == -1 || then < result ) {
                    result = then;
                }
            }
        }
    }
    return result;
} /* figureTimeout */
#else 
# define figureTimeout(g) INFINITE_TIMEOUT
#endif

#ifdef XWFEATURE_RELAY
static void
fireCursesTimer( CursesAppGlobals* globals )
{
    XWTimerReason ii;
    TimerInfo* smallestTip = NULL;

    for ( ii = 0; ii < NUM_TIMERS_PLUS_ONE; ++ii ) {
        TimerInfo* tip = &globals->cGlobals.timerInfo[ii];
        if ( !!tip->proc ) { 
            if ( !smallestTip ) {
                smallestTip = tip;
            } else if ( tip->when < smallestTip->when ) {
                smallestTip = tip;
            }
        }
    }

    if ( !!smallestTip ) {
        XP_U32 now = util_getCurSeconds( globals->cGlobals.params->util ) ;
        if ( now >= smallestTip->when ) {
            if ( linuxFireTimer( &globals->cGlobals, 
                                 smallestTip - globals->cGlobals.timerInfo ) ){
                board_draw( globals->cGlobals.game.board );
            }
        } else {
            XP_LOGFF( "skipping timer: now (%ld) < when (%ld)",
                      now, smallestTip->when );
        }
    }
} /* fireCursesTimer */
#endif
#endif

/* 
 * Ok, so this doesn't block yet.... 
 */
#ifndef USE_GLIBLOOP
/* static XP_Bool */
/* blocking_gotEvent( CursesAppGlobals* globals, int* ch ) */
/* { */
/*     XP_Bool result = XP_FALSE; */
/*     int numEvents, ii; */
/*     short fdIndex; */
/*     XP_Bool redraw = XP_FALSE; */

/*     int timeout = figureTimeout( globals ); */
/*     numEvents = poll( globals->fdArray, globals->fdCount, timeout ); */

/*     if ( timeout != INFINITE_TIMEOUT && numEvents == 0 ) { */
/* #ifdef XWFEATURE_RELAY */
/*         fireCursesTimer( globals ); */
/* #endif */
/*     } else if ( numEvents > 0 ) { */
	
/*         /\* stdin first *\/ */
/*         if ( (globals->fdArray[FD_STDIN].revents & POLLIN) != 0 ) { */
/*             int evtCh = wgetch(globals->mainWin); */
/*             XP_LOGF( "%s: got key: %x", __func__, evtCh ); */
/*             *ch = evtCh; */
/*             result = XP_TRUE; */
/*             --numEvents; */
/*         } */
/*         if ( (globals->fdArray[FD_STDIN].revents & ~POLLIN ) ) { */
/*             XP_LOGF( "some other events set on stdin" ); */
/*         } */

/*         if ( (globals->fdArray[FD_TIMEEVT].revents & POLLIN) != 0 ) { */
/*             char ch; */
/*             if ( 1 != read(globals->fdArray[FD_TIMEEVT].fd, &ch, 1 ) ) { */
/*                 XP_ASSERT(0); */
/*             } */
/*         } */

/*         fdIndex = FD_FIRSTSOCKET; */

/*         if ( numEvents > 0 ) { */
/*             if ( (globals->fdArray[fdIndex].revents & ~POLLIN ) ) { */
/*                 XP_LOGF( "some other events set on socket %d",  */
/*                          globals->fdArray[fdIndex].fd  ); */
/*             } */

/*             if ( (globals->fdArray[fdIndex].revents & POLLIN) != 0 ) { */

/*                 --numEvents; */

/*                 if ( globals->fdArray[fdIndex].fd  */
/*                      == globals->csInfo.server.serverSocket ) { */
/*                     /\* It's the listening socket: call platform's accept() */
/*                        wrapper *\/ */
/*                     (*globals->cGlobals.acceptor)( globals->fdArray[fdIndex].fd,  */
/*                                                    globals ); */
/*                 } else { */
/* #ifndef XWFEATURE_STANDALONE_ONLY */
/*                     unsigned char buf[1024]; */
/*                     int nBytes; */
/*                     CommsAddrRec addrRec; */
/*                     CommsAddrRec* addrp = NULL; */

/*                     /\* It's a normal data socket *\/ */
/*                     switch ( globals->cGlobals.params->conType ) { */
/* #ifdef XWFEATURE_RELAY */
/*                     case COMMS_CONN_RELAY: */
/*                         nBytes = linux_relay_receive( &globals->cGlobals, buf,  */
/*                                                       sizeof(buf) ); */
/*                         break; */
/* #endif */
/* #ifdef XWFEATURE_SMS */
/*                     case COMMS_CONN_SMS: */
/*                         addrp = &addrRec; */
/*                         nBytes = linux_sms_receive( &globals->cGlobals,  */
/*                                                     globals->fdArray[fdIndex].fd, */
/*                                                     buf, sizeof(buf), addrp ); */
/*                         break; */
/* #endif */
/* #ifdef XWFEATURE_BLUETOOTH */
/*                     case COMMS_CONN_BT: */
/*                         nBytes = linux_bt_receive( globals->fdArray[fdIndex].fd,  */
/*                                                    buf, sizeof(buf) ); */
/*                         break; */
/* #endif */
/*                     default: */
/*                         XP_ASSERT( 0 ); /\* fired *\/ */
/*                     } */

/*                     if ( nBytes != -1 ) { */
/*                         XWStreamCtxt* inboundS; */
/*                         redraw = XP_FALSE; */

/*                         inboundS = stream_from_msgbuf( &globals->cGlobals,  */
/*                                                        buf, nBytes ); */
/*                         if ( !!inboundS ) { */
/*                             if ( comms_checkIncomingStream( */
/*                                                            globals->cGlobals.game.comms, */
/*                                                            inboundS, addrp ) ) { */
/*                                 redraw = server_receiveMessage(  */
/*                                                                globals->cGlobals.game.server, inboundS ); */
/*                             } */
/*                             stream_destroy( inboundS ); */
/*                         } */
                
/*                         /\* if there's something to draw resulting from the */
/*                            message, we need to give the main loop time to reflect */
/*                            that on the screen before giving the server another */
/*                            shot.  So just call the idle proc. *\/ */
/*                         if ( redraw ) { */
/*                             curses_util_requestTime(globals->cGlobals.params->util); */
/*                         } */
/*                     } */
/* #else */
/*                     XP_ASSERT(0);   /\* no socket activity in standalone game! *\/ */
/* #endif                          /\* #ifndef XWFEATURE_STANDALONE_ONLY *\/ */
/*                 } */
/*                 ++fdIndex; */
/*             } */
/*         } */

/*         for ( ii = 0; ii < 5; ++ii ) { */
/*             redraw = server_do( globals->cGlobals.game.server, NULL ) || redraw; */
/*         } */
/*         if ( redraw ) { */
/*             /\* messages change a lot *\/ */
/*             board_invalAll( globals->cGlobals.game.board ); */
/*             board_draw( globals->cGlobals.game.board ); */
/*         } */
/*         saveGame( globals->cGlobals ); */
/*     } */
/*     return result; */
/* } /\* blocking_gotEvent *\/ */
#endif

/* static void */
/* remapKey( int* kp ) */
/* { */
/*     /\* There's what the manual says I should get, and what I actually do from */
/*      * a funky M$ keyboard.... */
/*      *\/ */
/*     int key = *kp; */
/*     switch( key ) { */
/*     case KEY_B2:                /\* "center of keypad" *\/ */
/*         key = '\r'; */
/*         break; */
/*     case KEY_DOWN: */
/*     case 526: */
/*         key = 'K'; */
/*         break; */
/*     case KEY_UP: */
/*     case 523: */
/*         key = 'J'; */
/*         break; */
/*     case KEY_LEFT: */
/*     case 524: */
/*         key = 'H'; */
/*         break; */
/*     case KEY_RIGHT: */
/*     case 525: */
/*         key = 'L'; */
/*         break; */
/*     default: */
/*         if ( key > 0x7F ) { */
/*             XP_LOGF( "%s(%d): no mapping", __func__, key ); */
/*         } */
/*         break; */
/*     } */
/*     *kp = key; */
/* } /\* remapKey *\/ */

typedef struct _MenuEntry {
    MenuList* menuItem;
    void* closure;
} MenuEntry;

/* void */
/* drawMenuLargeOrSmall( CursesAppGlobals* globals, const MenuList* menuList, */
/*                       void* closure ) */
/* { */
/* #ifdef CURSES_SMALL_SCREEN */
/*     const MenuList* lists[] = { g_rootMenuListShow, NULL }; */
/* #else */
/*     const MenuList* lists[] = { g_sharedMenuList, menuList, NULL }; */
/* #endif */
/*     wclear( globals->menuWin ); */
/*     drawMenuFromList( globals->menuWin, lists, 0, 0 ); */
/*     wrefresh( globals->menuWin ); */
/* } */

#if 0
static void
initClientSocket( CursesAppGlobals* globals, char* serverName )
{
    struct hostent* hostinfo;
    hostinfo = gethostbyname( serverName );
    if ( !hostinfo ) {
        userError( globals, "unable to get host info for %s\n", serverName );
    } else {
        char* hostName = inet_ntoa( *(struct in_addr*)hostinfo->h_addr );
        XP_LOGFF( "gethostbyname returned %s", hostName );
        globals->csInfo.client.serverAddr = inet_addr(hostName);
        XP_LOGFF( "inet_addr returned %lu", globals->csInfo.client.serverAddr );
    }
} /* initClientSocket */
#endif

/* <<<<<<< HEAD */
/* static void */
/* curses_util_informNeedPassword( XW_UtilCtxt* XP_UNUSED(uc), */
/*                                 XP_U16 XP_UNUSED_DBG(playerNum), */
/*                                 const XP_UCHAR* XP_UNUSED_DBG(name) ) */
/* { */
/*     XP_WARNF( "curses_util_informNeedPassword(num=%d, name=%s", playerNum, name ); */
/* } /\* curses_util_askPassword *\/ */

/* static void */
/* curses_util_yOffsetChange( XW_UtilCtxt* XP_UNUSED(uc),  */
/*                            XP_U16 XP_UNUSED(maxOffset), */
/*                            XP_U16 XP_UNUSED(oldOffset), XP_U16 XP_UNUSED(newOffset) ) */
/* { */
/*     /\* if ( oldOffset != newOffset ) { *\/ */
/*     /\*     XP_WARNF( "curses_util_yOffsetChange(%d,%d,%d) not implemented", *\/ */
/*     /\*               maxOffset, oldOffset, newOffset ); *\/ */
/*     /\* } *\/ */
/* } /\* curses_util_yOffsetChange *\/ */

/* #ifdef XWFEATURE_TURNCHANGENOTIFY */
/* static void */
/* curses_util_turnChanged( XW_UtilCtxt* XP_UNUSED(uc), XP_S16 XP_UNUSED_DBG(newTurn) ) */
/* { */
/*     XP_LOGF( "%s(turn=%d)", __func__, newTurn ); */
/* } */
/* #endif */

/* static void */
/* curses_util_notifyDupStatus( XW_UtilCtxt* XP_UNUSED(uc), */
/*                              XP_Bool amHost, */
/*                              const XP_UCHAR* msg ) */
/* { */
/*     XP_LOGF( "%s(amHost=%d, msg=%s)", __func__, amHost, msg ); */
/* } */

/* static void */
/* curses_util_notifyIllegalWords( XW_UtilCtxt* XP_UNUSED(uc), */
/*                                 BadWordInfo* XP_UNUSED(bwi), */
/*                                 XP_U16 XP_UNUSED(player), */
/*                                 XP_Bool XP_UNUSED(turnLost) ) */
/* { */
/*     XP_WARNF( "curses_util_notifyIllegalWords not implemented" ); */
/* } /\* curses_util_notifyIllegalWord *\/ */

/* static void */
/* curses_util_remSelected( XW_UtilCtxt* uc ) */
/* { */
/*     CursesAppGlobals* globals = (CursesAppGlobals*)uc->closure; */
/*     XWStreamCtxt* stream; */
/*     XP_UCHAR* text; */

/*     stream = mem_stream_make_raw( MPPARM(globals->cGlobals.util->mpool) */
/*                                   globals->cGlobals.params->vtMgr ); */
/*     board_formatRemainingTiles( globals->cGlobals.game.board, stream ); */

/*     text = strFromStream( stream ); */

/*     const char* buttons[] = { "Ok" }; */
/*     (void)cursesask( globals, text, VSIZE(buttons), buttons ); */

/*     free( text ); */
/* } */

/* #ifndef XWFEATURE_STANDALONE_ONLY */
/* static XWStreamCtxt* */
/* curses_util_makeStreamFromAddr(XW_UtilCtxt* uc, XP_PlayerAddr channelNo ) */
/* { */
/*     CursesAppGlobals* globals = (CursesAppGlobals*)uc->closure; */
/*     LaunchParams* params = globals->cGlobals.params; */

/*     XWStreamCtxt* stream = mem_stream_make( MPPARM(uc->mpool) params->vtMgr, */
/*                                             &globals->cGlobals, channelNo, */
/*                                             sendOnClose ); */
/*     return stream; */
/* } /\* curses_util_makeStreamFromAddr *\/ */
/* #endif */

/* #ifdef XWFEATURE_CHAT */
/* static void */
/* curses_util_showChat( XW_UtilCtxt* uc,  */
/*                       const XP_UCHAR* const XP_UNUSED_DBG(msg), */
/*                       XP_S16 XP_UNUSED_DBG(from), XP_U32 XP_UNUSED(timestamp) ) */
/* { */
/*     CursesAppGlobals* globals = (CursesAppGlobals*)uc->closure; */
/*     globals->nChatsSent = 0; */
/* # ifdef DEBUG */
/*     const XP_UCHAR* name = "<unknown>"; */
/*     if ( 0 <= from ) { */
/*         CommonGlobals* cGlobals = &globals->cGlobals; */
/*         name = cGlobals->gi->players[from].name; */
/*     } */
/*     XP_LOGF( "%s: got \"%s\" from %s", __func__, msg, name ); */
/* # endif */
/* } */
/* #endif */

/* static void */
/* setupCursesUtilCallbacks( CursesAppGlobals* globals, XW_UtilCtxt* util ) */
/* { */
/*     util->vtable->m_util_userError = curses_util_userError; */

/*     util->vtable->m_util_informNeedPassword = curses_util_informNeedPassword; */
/*     util->vtable->m_util_yOffsetChange = curses_util_yOffsetChange; */
/* #ifdef XWFEATURE_TURNCHANGENOTIFY */
/*     util->vtable->m_util_turnChanged = curses_util_turnChanged; */
/* #endif */
/*     util->vtable->m_util_notifyDupStatus = curses_util_notifyDupStatus; */
/*     util->vtable->m_util_notifyIllegalWords = curses_util_notifyIllegalWords; */
/*     util->vtable->m_util_remSelected = curses_util_remSelected; */
/* #ifndef XWFEATURE_STANDALONE_ONLY */
/*     util->vtable->m_util_makeStreamFromAddr = curses_util_makeStreamFromAddr; */
/* #endif */
/* #ifdef XWFEATURE_CHAT */
/*     util->vtable->m_util_showChat = curses_util_showChat; */
/* #endif */

/*     util->vtable->m_util_notifyMove = curses_util_notifyMove; */
/*     util->vtable->m_util_notifyTrade = curses_util_notifyTrade; */
/*     util->vtable->m_util_notifyPickTileBlank = curses_util_notifyPickTileBlank; */
/*     util->vtable->m_util_informNeedPickTiles = curses_util_informNeedPickTiles; */
/*     util->vtable->m_util_trayHiddenChange = curses_util_trayHiddenChange; */
/*     util->vtable->m_util_informMove = curses_util_informMove; */
/*     util->vtable->m_util_informUndo = curses_util_informUndo; */
/*     util->vtable->m_util_notifyGameOver = curses_util_notifyGameOver; */
/*     util->vtable->m_util_informNetDict = curses_util_informNetDict; */
/*     util->vtable->m_util_setIsServer = curses_util_setIsServer; */

/* #ifdef XWFEATURE_HILITECELL */
/*     util->vtable->m_util_hiliteCell = curses_util_hiliteCell; */
/* #endif */
/*     util->vtable->m_util_engineProgressCallback =  */
/*         curses_util_engineProgressCallback; */

/*     util->vtable->m_util_setTimer = curses_util_setTimer; */
/*     util->vtable->m_util_clearTimer = curses_util_clearTimer; */
/*     util->vtable->m_util_requestTime = curses_util_requestTime; */

/*     util->closure = globals; */
/* } /\* setupCursesUtilCallbacks *\/ */

/* static CursesMenuHandler */
/* getHandlerForKey( const MenuList* list, char ch ) */
/* { */
/*     CursesMenuHandler handler = NULL; */
/*     while ( list->handler != NULL ) { */
/*         if ( list->key == ch ) { */
/*             handler = list->handler; */
/*             break; */
/*         } */
/*         ++list; */
/*     } */
/*     return handler; */
/* } */

/* static XP_Bool */
/* handleKeyEvent( CursesAppGlobals* globals, const MenuList* list, char ch ) */
/* { */
/*     CursesMenuHandler handler = getHandlerForKey( list, ch ); */
/*     XP_Bool result = XP_FALSE; */
/*     if ( !!handler ) { */
/*         result = (*handler)(globals); */
/*     } */
/*     return result; */
/* } /\* handleKeyEvent *\/ */

/* static XP_Bool */
/* passKeyToBoard( CursesAppGlobals* globals, char ch ) */
/* { */
/*     XP_Bool handled = ch >= 'a' && ch <= 'z'; */
/*     if ( handled ) { */
/*         ch += 'A' - 'a'; */
/*         globals->doDraw = board_handleKey( globals->cGlobals.game.board,  */
/*                                            ch, NULL ); */
/*     } */
/*     return handled; */
/* } /\* passKeyToBoard *\/ */

/* static void */
/* positionSizeStuff( CursesAppGlobals* globals, int width, int height ) */
/* { */
/*     CommonGlobals* cGlobals = &globals->cGlobals; */
/*     BoardCtxt* board = cGlobals->game.board; */
/* #ifdef COMMON_LAYOUT */

/*     BoardDims dims; */
/*     board_figureLayout( board, cGlobals->gi,  */
/*                         0, 0, width, height, 100, */
/*                         150, 200, /\* percents *\/ */
/*                         width*75/100, 2, 1,  */
/*                         XP_FALSE, &dims ); */
/*     board_applyLayout( board, &dims ); */
/* ======= */
/* >>>>>>> android_branch */

/* static const MenuList* */
/* getHandlerForKey( const MenuList* list, char ch ) */
/* { */
/* MenuList* handler = NULL; */
/*     while ( list->handler != NULL ) { */
/*         if ( list->key == ch ) { */
/*             handler = list->handler; */
/*             break; */
/*         } */
/*         ++list; */
/*     } */
/*     return handler; */
/* } */

/* static XP_Bool */
/* handleKeyEvent( CursesAppGlobals* globals, const MenuList* list, char ch ) */
/* { */
/*     const MenuList* entry = getHandlerForKey( list, ch ); */
/*     XP_Bool result = XP_FALSE; */
/*     if ( !!handler ) { */
/*         result = (*entry->handler)(entry->closure); */
/*     } */
/*     return result; */
/* } /\* handleKeyEvent *\/ */

/* static XP_Bool */
/* passKeyToBoard( CursesAppGlobals* XP_UNUSED(globals), char XP_UNUSED(ch) ) */
/* { */
/*     XP_ASSERT(0); */
/*     /\* XP_Bool handled = ch >= 'a' && ch <= 'z'; *\/ */
/*     /\* if ( handled ) { *\/ */
/*     /\*     ch += 'A' - 'a'; *\/ */
/*     /\*     globals->doDraw = board_handleKey( globals->cGlobals.game.board,  *\/ */
/*     /\*                                        ch, NULL ); *\/ */
/*     /\* } *\/ */
/*     /\* return handled; *\/ */
/* } /\* passKeyToBoard *\/ */

#ifdef RELAY_VIA_HTTP
static void
onJoined( void* closure, const XP_UCHAR* connname, XWHostID hid )
{
    LOG_FUNC();
    CursesAppGlobals* globals = (CursesAppGlobals*)closure;
    CommsCtxt* comms = globals->cGlobals.game.comms;
    comms_gameJoined( comms, connname, hid );
}

/* static void */
/* relay_requestJoin_curses( void* closure, const XP_UCHAR* devID, const XP_UCHAR* room, */
/*                           XP_U16 nPlayersHere, XP_U16 nPlayersTotal, */
/*                           XP_U16 seed, XP_U16 lang ) */
/* { */
/*     CursesAppGlobals* globals = (CursesAppGlobals*)closure; */
/*     relaycon_join( globals->cGlobals.params, devID, room, nPlayersHere, nPlayersTotal, */
/*                    seed, lang, onJoined, globals ); */
/* } */
#endif

void
inviteReceivedCurses( void* closure, const NetLaunchInfo* invite )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    sqlite3_int64 rowids[1];
    int nRowIDs = VSIZE(rowids);
    gdb_getRowsForGameID( aGlobals->cag.params->pDb, invite->gameID, rowids, &nRowIDs );
    bool doIt = 0 == nRowIDs;
    if ( ! doIt && !!aGlobals->mainWin ) {
        XP_LOGFF( "duplicate invite; not creating game" );
        /* const gchar* question = "Duplicate invitation received. Accept anyway?"; */
        /* const char* buttons[] = { "Yes", "No" }; */
        /* doIt = 0 == cursesask( aGlobals->mainWin, question, VSIZE(buttons), buttons ); */
    }
    if ( doIt ) {
        cb_dims dims;
        figureDims( aGlobals, &dims );
        cb_newFor( aGlobals->cbState, invite, &dims );
    } else {
        XP_LOGFF( "Not accepting duplicate invitation (nRowIDs(gameID=%X) was %d",
                  invite->gameID, nRowIDs );
    }
}

#ifdef XWFEATURE_RELAY
static void
relayInviteReceivedCurses( void* closure, const NetLaunchInfo* invite )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    inviteReceivedCurses( aGlobals, invite );
}

static void
cursesGotBuf( void* closure, const CommsAddrRec* addr,
              const XP_U8* buf, XP_U16 len )
{
    LOG_FUNC();
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    XP_U32 clientToken;
    XP_ASSERT( sizeof(clientToken) < len );
    XP_MEMCPY( &clientToken, &buf[0], sizeof(clientToken) );
    buf += sizeof(clientToken);
    len -= sizeof(clientToken);

    sqlite3_int64 rowid;
    XP_U16 gotSeed;
    rowidFromToken( XP_NTOHL( clientToken ), &rowid, &gotSeed );

    /* Figure out if the device is live, or we need to open the game */
    cb_feedRow( aGlobals->cbState, rowid, gotSeed, buf, len, addr );

    /* if ( seed == comms_getChannelSeed( cGlobals->game.comms ) ) { */
    /*     gameGotBuf( cGlobals, XP_TRUE, buf, len, addr ); */
    /* } else { */
    /*     XP_LOGF( "%s(): dropping packet; meant for a different device", */
    /*              __func__ ); */
    /* } */
    /* LOG_RETURN_VOID(); */
}
#endif

static void
smsInviteReceivedCurses( void* closure, const NetLaunchInfo* nli )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    inviteReceivedCurses( aGlobals, nli );
}

static void
smsMsgReceivedCurses( void* closure, const CommsAddrRec* from, XP_U32 gameID, 
                      const XP_U8* buf, XP_U16 len )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    cb_feedGame( aGlobals->cbState, gameID, buf, len, from );
}

void
mqttMsgReceivedCurses( void* closure, const CommsAddrRec* from,
                       XP_U32 gameID, const XP_U8* buf, XP_U16 len )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    cb_feedGame( aGlobals->cbState, gameID, buf, len, from );
}

void
gameGoneCurses( void* XP_UNUSED(closure), const CommsAddrRec* XP_UNUSED(from),
                XP_U32 XP_UNUSED_DBG(gameID) )
{
    XP_LOGFF( "(gameID=%d)", gameID );
}

#ifdef XWFEATURE_RELAY
static void
cursesGotForRow( void* XP_UNUSED(closure), const CommsAddrRec* XP_UNUSED(from),
                 sqlite3_int64 XP_UNUSED(rowid), const XP_U8* XP_UNUSED(buf),
                 XP_U16 XP_UNUSED(len) )
{
    // CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    LOG_FUNC();
    /* gameGotBuf( &globals->cGlobals, XP_TRUE, buf, len, from ); */
    XP_ASSERT( 0 );
    LOG_RETURN_VOID();
}

static gint
curses_requestMsgs( gpointer data )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)data;
    XP_UCHAR devIDBuf[64] = {};
    gdb_fetch_safe( aGlobals->cag.params->pDb, KEY_RDEVID, NULL, devIDBuf,
                    sizeof(devIDBuf) );
    if ( '\0' != devIDBuf[0] ) {
        relaycon_requestMsgs( aGlobals->cag.params, devIDBuf );
    } else {
        XP_LOGFF( "not requesting messages as don't have relay id" );
    }
    return 0;                   /* don't run again */
}

static void
cursesNoticeRcvd( void* closure )
{
    LOG_FUNC();
    CursesAppGlobals* globals = (CursesAppGlobals*)closure;
#ifdef DEBUG
    guint res =
#endif
        ADD_ONETIME_IDLE( curses_requestMsgs, globals );
    XP_ASSERT( res > 0 );
}

static gboolean
keepalive_timer( gpointer data )
{
    LOG_FUNC();
    curses_requestMsgs( data );
    return TRUE;
}

static void
cursesDevIDReceived( void* closure, const XP_UCHAR* devID,
                     XP_U16 maxInterval )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    sqlite3* pDb = aGlobals->cag.params->pDb;
    if ( !!devID ) {
        XP_LOGFF( "(devID='%s')", devID );

        /* If we already have one, make sure it's the same! Else store. */
        gchar buf[64];
        XP_Bool have = gdb_fetch_safe( pDb, KEY_RDEVID, NULL, buf, sizeof(buf) )
            && 0 == strcmp( buf, devID );
        if ( !have ) {
            gdb_store( pDb, KEY_RDEVID, devID );
            XP_LOGFF( "storing new devid: %s", devID );
            cgl_draw( aGlobals->gameList );
        }
        (void)g_timeout_add_seconds( maxInterval, keepalive_timer, aGlobals );
    } else {
        XP_LOGFF( "bad relayid" );
        gdb_remove( pDb, KEY_RDEVID );

        DevIDType typ;
        const XP_UCHAR* devID = linux_getDevID( aGlobals->cag.params, &typ );
        relaycon_reg( aGlobals->cag.params, NULL, typ, devID );
    }
}

static void
cursesErrorMsgRcvd( void* closure, const XP_UCHAR* msg )
{
    CursesAppGlobals* globals = (CursesAppGlobals*)closure;
    if ( !!globals->lastErr && 0 == strcmp( globals->lastErr, msg ) ) {
        XP_LOGFF( "skipping error message from relay" );
    } else {
        g_free( globals->lastErr );
        globals->lastErr = g_strdup( msg );
        const char* buttons[] = { "Ok" };
        (void)cursesask( globals->mainWin, msg, VSIZE(buttons), buttons );
    }
}
#endif

/* static gboolean */
/* chatsTimerFired( gpointer data ) */
/* { */
/*     CursesAppGlobals* globals = (CursesAppGlobals*)data; */
/*     XWGame* game = &globals->cGlobals.game; */
/*     GameStateInfo gsi; */

/*     game_getState( game, &gsi ); */

/*     if ( gsi.canChat && 3 > globals->nChatsSent ) { */
/*         XP_UCHAR msg[128]; */
/*         struct tm* timp; */
/*         struct timeval tv; */
/*         struct timezone tz; */

/*         gettimeofday( &tv, &tz ); */
/*         timp = localtime( &tv.tv_sec ); */

/*         snprintf( msg, sizeof(msg), "%x: Saying hi via chat at %.2d:%.2d:%.2d",  */
/*                   comms_getChannelSeed( game->comms ), */
/*                   timp->tm_hour, timp->tm_min, timp->tm_sec ); */
/*         XP_LOGF( "%s: sending \"%s\"", __func__, msg ); */
/*         board_sendChat( game->board, msg ); */
/*         ++globals->nChatsSent; */
/*     } */

/*     return TRUE; */
/* } */

/* static XP_U16 */
/* feedBufferCurses( CommonGlobals* cGlobals, sqlite3_int64 rowid,  */
/*                   const XP_U8* buf, XP_U16 len, const CommsAddrRec* from ) */
/* { */
/*     gameGotBuf( cGlobals, XP_TRUE, buf, len, from ); */

/*     /\* GtkGameGlobals* globals = findOpenGame( apg, rowid ); *\/ */

/*     /\* if ( !!globals ) { *\/ */
/*     /\*     gameGotBuf( &globals->cGlobals, XP_TRUE, buf, len, from ); *\/ */
/*     /\*     seed = comms_getChannelSeed( globals->cGlobals.game.comms ); *\/ */
/*     /\* } else { *\/ */
/*     /\*     GtkGameGlobals tmpGlobals; *\/ */
/*     /\*     if ( loadGameNoDraw( &tmpGlobals, apg->params, rowid ) ) { *\/ */
/*     /\*         gameGotBuf( &tmpGlobals.cGlobals, XP_FALSE, buf, len, from ); *\/ */
/*     /\*         seed = comms_getChannelSeed( tmpGlobals.cGlobals.game.comms ); *\/ */
/*     /\*         saveGame( &tmpGlobals.cGlobals ); *\/ */
/*     /\*     } *\/ */
/*     /\*     freeGlobals( &tmpGlobals ); *\/ */
/*     /\* } *\/ */
/*     /\* return seed; *\/ */
/* } */

/* static void */
/* smsMsgReceivedCurses( void* closure, const CommsAddrRec* from,  */
/*                       XP_U32 XP_UNUSED(gameID), */
/*                       const XP_U8* buf, XP_U16 len ) */
/* { */
/*     LOG_FUNC(); */
/*     CommonGlobals* cGlobals = (CommonGlobals*)closure; */
/*     gameGotBuf( cGlobals, XP_TRUE, buf, len, from ); */
/*     LOG_RETURN_VOID(); */
/*     /\* LaunchParams* params =  cGlobals->params; *\/ */

/*     /\* sqlite3_int64 rowids[4]; *\/ */
/*     /\* int nRowIDs = VSIZE(rowids); *\/ */
/*     /\* getRowsForGameID( params->pDb, gameID, rowids, &nRowIDs ); *\/ */
/*     /\* for ( int ii = 0; ii < nRowIDs; ++ii ) { *\/ */
/*     /\*     gameGotBuf( cGlobals, XP_TRUE, buf, len, from ); *\/ */
/*     /\*     // feedBufferCurses( cGlobals, rowids[ii], buf, len, from ); *\/ */
/*     /\* } *\/ */
/* } */

static void
onGameSaved( CursesAppGlobals* aGlobals, sqlite3_int64 rowid, bool isNew )
{
    cgl_refreshOne( aGlobals->gameList, rowid, isNew );
}

static XP_Bool
newGameWrapper( void* closure, CurGameInfo* gi, XP_U32* newGameIDP )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;

    cb_dims dims;
    figureDims( aGlobals, &dims );

    return cb_newGame( aGlobals->cbState, &dims, gi, newGameIDP );
}

static XP_Bool
makeMoveIfWrapper( void* closure, XP_U32 gameID, XP_Bool tryTrade )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    return cb_makeMoveIf( aGlobals->cbState, gameID, tryTrade );
}

static void
addInvitesWrapper( void* closure, XP_U32 gameID, XP_U16 nRemotes,
                   const CommsAddrRec destAddrs[] )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    cb_addInvites( aGlobals->cbState, gameID, nRemotes, destAddrs );
}

static void
newGuestWrapper( void* closure, const NetLaunchInfo* nli )
{
    inviteReceivedCurses( closure, nli );
}

static const CommonGlobals*
getForGameIDWrapper( void* closure, XP_U32 gameID )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    return cb_getForGameID( aGlobals->cbState, gameID );
}

static void
quitWrapper( void* closure )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    handleQuit( aGlobals, 0 );
}

static XP_Bool
makeRematchWrapper( void* closure, XP_U32 gameID, RematchOrder ro,
                    XP_U32* newGameIDP )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    return cb_makeRematch( aGlobals->cbState, gameID, ro, newGameIDP );
}

static XP_Bool
sendChatWrapper( void* closure, XP_U32 gameID, const char* msg )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    return cb_sendChat( aGlobals->cbState, gameID, msg );
}

static XP_Bool
undoMoveWrapper( void* closure, XP_U32 gameID )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    return cb_undoMove( aGlobals->cbState, gameID );
}

static XP_Bool
resignWrapper( void* closure, XP_U32 gameID )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    return cb_resign( aGlobals->cbState, gameID );
}

static cJSON*
getKPsWrapper( void* closure )
{
    CursesAppGlobals* aGlobals = (CursesAppGlobals*)closure;
    XW_DUtilCtxt* dutil = aGlobals->cag.params->dutil;

    XP_U16 nFound = 0;
    kplr_getNames( dutil, NULL_XWE, XP_FALSE, NULL, &nFound );
    const XP_UCHAR* players[nFound];
    kplr_getNames( dutil, NULL_XWE, XP_FALSE, players, &nFound );

    cJSON* result = cJSON_CreateArray();
    for ( int ii = 0; ii < nFound; ++ii ) {
        cJSON* entry = cJSON_CreateObject();
        cJSON_AddStringToObject( entry, "name", players[ii]);

        CommsAddrRec addr;
        if ( kplr_getAddr( dutil, NULL_XWE, players[ii],
                           &addr, NULL ) ) {

            if ( addr_hasType( &addr, COMMS_CONN_MQTT ) ) {
                XP_UCHAR buf[17];
                formatMQTTDevID( &addr.u.mqtt.devID, buf, VSIZE(buf) );
                cJSON_AddStringToObject( entry, "devID", buf );
            }
            if ( addr_hasType( &addr, COMMS_CONN_SMS ) ) {
                cJSON_AddStringToObject( entry, "phone", addr.u.sms.phone );
            }
        }

        cJSON_AddItemToArray( result, entry );
    }

    return result;
}

void
cursesmain( XP_Bool XP_UNUSED(isServer), LaunchParams* params )
{
    memset( &g_globals, 0, sizeof(g_globals) );
    g_globals.cag.params = params;
    params->appGlobals = &g_globals;

    params->cmdProcs.quit = invokeQuit;

    initCurses( &g_globals );
    if ( !params->closeStdin ) {
        g_globals.menuState = cmenu_init( g_globals.mainWin );
        cmenu_push( g_globals.menuState, &g_globals, g_sharedMenuList, NULL );
    }

    g_globals.loop = g_main_loop_new( NULL, FALSE );

    g_globals.cbState = cb_init( &g_globals, params, g_globals.menuState,
                                 onGameSaved );

    g_globals.gameList = cgl_init( params, g_globals.winWidth, params->cursesListWinHt );
    cgl_refresh( g_globals.gameList );

    CmdWrapper wr = {
        .params = params,
        .closure = &g_globals,
        .procs = {
            .quit = quitWrapper,
            .newGame = newGameWrapper,
            .addInvites = addInvitesWrapper,
            .newGuest = newGuestWrapper,
            .makeMoveIf = makeMoveIfWrapper,
            .getForGameID = getForGameIDWrapper,
            .makeRematch = makeRematchWrapper,
            .sendChat = sendChatWrapper,
            .undoMove = undoMoveWrapper,
            .resign = resignWrapper,
            .getKPs = getKPsWrapper,
        },
    };
    GSocketService* cmdService = cmds_addCmdListener( &wr );

# ifdef DEBUG
    int piperesult = 
# endif
        pipe( g_globals.quitpipe );
    XP_ASSERT( piperesult == 0 );
    ADD_SOCKET( &g_globals, g_globals.quitpipe[0], handle_quitwrite );

    pipe( g_globals.winchpipe );
    ADD_SOCKET( &g_globals, g_globals.winchpipe[0], handle_winchwrite );

    struct sigaction act = { .sa_handler = SIGINTTERM_handler };
    sigaction( SIGINT, &act, NULL );
    sigaction( SIGTERM, &act, NULL );
    struct sigaction act2 = { .sa_handler = SIGWINCH_handler };
    sigaction( SIGWINCH, &act2, NULL );

#ifdef XWFEATURE_RELAY
    if ( params->useUdp ) {
        RelayConnProcs procs = {
            .inviteReceived = relayInviteReceivedCurses,
            .msgReceived = cursesGotBuf,
            .msgForRow = cursesGotForRow,
            .msgNoticeReceived = cursesNoticeRcvd,
            .devIDReceived = cursesDevIDReceived,
            .msgErrorMsg = cursesErrorMsgRcvd,
        };

        relaycon_init( params, &procs, &g_globals,
                       params->connInfo.relay.relayName,
                       params->connInfo.relay.defaultSendPort );

        XP_Bool idIsNew = linux_setupDevidParams( params );
        linux_doInitialReg( params, idIsNew );
    }
#endif
    mqttc_init( params );

#ifdef XWFEATURE_SMS
    gchar* myPhone = NULL;
    XP_U16 myPort = 0;
    if ( parseSMSParams( params, &myPhone, &myPort ) ) {
        SMSProcs smsProcs = {
            .inviteReceived = smsInviteReceivedCurses,
            .msgReceived = smsMsgReceivedCurses,
        };
        linux_sms_init( params, myPhone, myPort, &smsProcs, &g_globals );
    }
#endif

    if ( 0 == cgl_getNGames( g_globals.gameList ) ) {
        if ( params->forceNewGame ) {
            handleNewGame( &g_globals, 0 );
        }
    } else {
        /* Always open a game (at random). Without that it won't attempt to
           connect and stalls are likely in the test script case at least. If
           that's annoying when running manually add a launch flag */
        cgl_setSel( g_globals.gameList, -1 );
        handleOpenGame( &g_globals, 0 );
    }

    g_main_loop_run( g_globals.loop );

    g_object_unref( cmdService );

    cb_closeAll( g_globals.cbState );
    
#ifdef XWFEATURE_BLUETOOTH
    // linux_bt_close( &g_globals.cGlobals );
#endif
#ifdef XWFEATURE_SMS
    // linux_sms_close( &g_globals.cGlobals );
#endif
#ifdef XWFEATURE_IP_DIRECT
    // linux_udp_close( &g_globals.cGlobals );
#endif

    cgl_destroy( g_globals.gameList );

    endwin();

    dvc_store( params->dutil, NULL_XWE );

#ifdef XWFEATURE_RELAY
    if ( params->useUdp ) {
        relaycon_cleanup( params );
    }
#endif

    linux_sms_cleanup( params );
    mqttc_cleanup( params );
} /* cursesmain */
#endif /* PLATFORM_NCURSES */
