/* -*- compile-command: "make MEMDEBUG=TRUE -j5"; -*- */
/* 
 * Copyright 2000-2020 by Eric House (xwords@eehouse.org).  All rights
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

#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>              /* BAD: use glib to support utf8 */
#ifdef DEBUG
# include <execinfo.h>          /* for backtrace */
#endif

#include "movestak.h"
#include "linuxutl.h"
#include "main.h"
#include "linuxdict.h"
#include "linuxmain.h"
#include "gamesdb.h"
#include "LocalizedStrIncludes.h"

#ifdef DEBUG

static void
debugf( const char* format, va_list ap )
{
    struct timespec tp = {0};
    int res = clock_gettime( CLOCK_REALTIME, &tp );
    XP_ASSERT( 0 == res );
    struct tm* timp = localtime( &tp.tv_sec );

    fprintf( stderr, "<%d:%lx> %.2d:%.2d:%.2d:%03ld ", getpid(),
             pthread_self(), timp->tm_hour, timp->tm_min, timp->tm_sec,
             tp.tv_nsec / 1000000 );

    vfprintf( stderr, format, ap );
    fprintf( stderr, "%c", '\n' );
}

void 
linux_debugf( const char* format, ... )
{
    va_list ap;
    va_start(ap, format);
    debugf( format, ap );
    va_end( ap );
}

void
linux_debugff( const char* func, const char* file, const char* fmt, ...)
{
    gchar* header = g_strdup_printf( "%s:%s(): %s", file, func, fmt );

    va_list ap;
    va_start( ap, fmt );
    debugf( header, ap );
    va_end( ap );
    g_free( header );
}

void
linux_backtrace( void )
{
    void* buffer[128];
    int nFound = backtrace( buffer, VSIZE(buffer) );
    XP_ASSERT( nFound < VSIZE(buffer) );
    char** traces = backtrace_symbols( buffer, nFound );
    
    XP_U16 ii;
    for ( ii = 0; ii < nFound; ++ii ) {
        XP_LOGF( "trace[%.2d]: %s", ii, traces[ii] );
    }
    free( traces );
}
#endif

#ifndef MEM_DEBUG
void
linux_freep( void** ptrp )
{
    if ( !!*ptrp ) {
        free( *ptrp );
        *ptrp = NULL;
    }
}
#endif

static DictionaryCtxt*
linux_util_makeEmptyDict( XW_UtilCtxt* XP_UNUSED_DBG(uctx), XWEnv xwe )
{
    XP_DEBUGF( "linux_util_makeEmptyDict called" );
    return linux_dictionary_make( MPPARM(uctx->mpool) xwe, NULL, NULL, XP_FALSE );
} /* linux_util_makeEmptyDict */

#define EM BONUS_NONE
#define DL BONUS_DOUBLE_LETTER
#define DW BONUS_DOUBLE_WORD
#define TL BONUS_TRIPLE_LETTER
#define TW BONUS_TRIPLE_WORD

static XWBonusType*
bonusesFor( XP_U16 boardSize, XP_U16* len )
{
    static XWBonusType scrabbleBoard[] = { 
        TW,//EM,EM,DL,EM,EM,EM,TW,
        EM,DW,//EM,EM,EM,TL,EM,EM,

        EM,EM,DW,//EM,EM,EM,DL,EM,
        DL,EM,EM,DW,//EM,EM,EM,DL,
                            
        EM,EM,EM,EM,DW,//EM,EM,EM,
        EM,TL,EM,EM,EM,TL,//EM,EM,
                            
        EM,EM,DL,EM,EM,EM,DL,//EM,
        TW,EM,EM,DL,EM,EM,EM,DW,
    }; /* scrabbleBoard */

    static XWBonusType seventeen[] = { 
        TW,//EM,EM,DL,EM,EM,EM,TW,
        EM,DW,//EM,EM,EM,TL,EM,EM,

        EM,EM,DW,//EM,EM,EM,DL,EM,
        DL,EM,EM,DW,//EM,EM,EM,DL,
                            
        EM,EM,EM,EM,DW,//EM,EM,EM,
        EM,TL,EM,EM,EM,TL,//EM,EM,
                            
        EM,EM,DL,EM,EM,EM,DL,//EM,
        TW,EM,EM,DL,EM,EM,EM,DW,
        TW,EM,EM,DL,EM,EM,EM,DW,DW,
    }; /* scrabbleBoard */

    XWBonusType* result = NULL;
    if ( boardSize == 15 ) {
        result = scrabbleBoard;
        *len = VSIZE(scrabbleBoard);
    } else if ( boardSize == 17 ) {
        result = seventeen;
        *len = VSIZE(seventeen);
    }

    return result;
}

#ifdef STREAM_VERS_BIGBOARD
void
setSquareBonuses( const CommonGlobals* cGlobals )
{
    XP_U16 nBonuses;
    XWBonusType* bonuses = 
        bonusesFor( cGlobals->gi->boardSize, &nBonuses );
    if ( !!bonuses ) {
        model_setSquareBonuses( cGlobals->game.model, bonuses, nBonuses );
    }
}
#endif

static XWBonusType*
parseBonusFile( XP_U16 nCols, const char* bonusFile )
{
    XWBonusType* result = NULL;
    FILE* file = fopen( bonusFile, "r" );
    if ( !!file ) {
        XP_U16 row = 0;
        XP_U16 col;
        result = malloc( nCols * nCols * sizeof(*result) );
        char line[1024];
        while ( line == fgets( line, sizeof(line), file ) && row < nCols ) {
            bool inComment = false;
            char* ch;
            XWBonusType bonus;
            col = 0;
            for ( ch = line; '\0' != *ch; ++ch ) {
                switch( *ch ) {
                case '#':       /* comment; line is done */
                    inComment = true;
                    break;
                case '+':
                    bonus = BONUS_DOUBLE_LETTER;
                    break;
                case '*': 
                    bonus = BONUS_DOUBLE_WORD;
                    break;
                case '^':
                    bonus = BONUS_TRIPLE_LETTER;
                    break;
                case '!': 
                    bonus = BONUS_TRIPLE_WORD;
                    break;
                case '.': 
                case ' ': 
                    bonus = BONUS_NONE;
                    break;
                case '\n':
                case '\a':
                    break;
                default:
                    if ( !inComment ) {
                        fprintf( stderr, "unexpected char '%c' in %s\n", *ch, bonusFile );
                        exit( 1 );
                    }
                    break;
                }
                if ( !inComment && col < nCols ) {
                    result[(row * nCols) + col] = bonus;
                    ++col;
                    /* Let's just allow anything to follow the 15 letters we
                       care about, e.g. comments */
                    if ( col >= nCols ) {
                        inComment = true;
                    }
                }
            }
            if ( col > 0 && row < nCols - 1) {
                ++row;
            }
        }
        fclose( file );
    }
    return result;
}

static XWBonusType
linux_util_getSquareBonus( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_U16 nCols,
                           XP_U16 col, XP_U16 row )
{
    static XWBonusType* parsedFile = NULL;
    XWBonusType result = EM;

    CommonGlobals* cGlobals = (CommonGlobals*)uc->closure;
    if ( NULL == parsedFile && NULL != cGlobals->params->bonusFile ) {
        if ( !parsedFile ) {
            parsedFile = parseBonusFile( nCols, cGlobals->params->bonusFile );
        }
    }
    if ( NULL != parsedFile ) {
        result = parsedFile[(row*nCols) + col];
    } else {
        XP_U16 nEntries;
        XWBonusType* scrabbleBoard = bonusesFor( 15, &nEntries );

        XP_U16 index, ii;
        if ( col > (nCols/2) ) col = nCols - 1 - col;
        if ( row > (nCols/2) ) row = nCols - 1 - row;
        if ( col > row ) {
            XP_U16 tmp = col;
            col = row;
            row = tmp;
        }
        index = col;
        for ( ii = 1; ii <= row; ++ii ) {
            index += ii;
        }

        if ( index < nEntries) {
            result = scrabbleBoard[index];
        }
    }
    return result;
} /* linux_util_getSquareBonus */

static const DictionaryCtxt*
linux_util_getDict( XW_UtilCtxt* uc, XWEnv xwe,
                    XP_LangCode XP_UNUSED(lang), const XP_UCHAR* dictName )
{
    CommonGlobals* cGlobals = (CommonGlobals*)uc->closure;
    LaunchParams* params = cGlobals->params;
    DictionaryCtxt* result = linux_dictionary_make( MPPARM(uc->mpool) xwe,
                                                    params, dictName, XP_TRUE );
    return result;
}

static void
linux_util_getInviteeName( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe),
                           XP_U16 index, XP_UCHAR* buf, XP_U16* bufLen )
{
    int len = XP_SNPRINTF( buf, *bufLen-1, "(%d invited)", index );
    if ( len < *bufLen ) {
        *bufLen = len + 1;
    } else {
        *bufLen = 0;
    }
}

static XW_DUtilCtxt*
linux_util_getDevUtilCtxt( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe) )
{
    CommonGlobals* cGlobals = (CommonGlobals*)uc->closure;
    return cGlobals->params->dutil;
}

void
linux_util_vt_init( MPFORMAL XW_UtilCtxt* util )
{
#ifdef MEM_DEBUG
    util->mpool = mpool;
#endif
    util->vtable = XP_MALLOC( mpool, sizeof(UtilVtable) );

    util->vtable->m_util_makeEmptyDict = linux_util_makeEmptyDict;
    util->vtable->m_util_getSquareBonus = linux_util_getSquareBonus;
    util->vtable->m_util_getDict = linux_util_getDict;
    util->vtable->m_util_getInviteeName = linux_util_getInviteeName;

    util->vtable->m_util_getDevUtilCtxt = linux_util_getDevUtilCtxt;
}

void
linux_util_vt_destroy( XW_UtilCtxt* util )
{
    XP_FREE( util->mpool, util->vtable );
}

const XP_UCHAR*
linux_getErrString( UtilErrID id, XP_Bool* silent )
{
    *silent = XP_FALSE;
    const char* message = NULL;

    switch( (int)id ) {
    case ERR_TILES_NOT_IN_LINE:
        message = "All tiles played must be in a line.";
        break;
    case ERR_NO_EMPTIES_IN_TURN:
        message = "Empty squares cannot separate tiles played.";
        break;

    case ERR_TOO_FEW_TILES_LEFT_TO_TRADE:
        message = "Too few tiles left to trade.";
        break;

    case ERR_TWO_TILES_FIRST_MOVE:
        message = "Must play two or more pieces on the first move.";
        break;
    case ERR_TILES_MUST_CONTACT:
        message = "New pieces must contact others already in place (or "
            "the middle square on the first move).";
        break;
    case ERR_NOT_YOUR_TURN:
        message = "You can't do that; it's not your turn!";
        break;
    case ERR_NO_PEEK_ROBOT_TILES:
        message = "No peeking at the robot's tiles!";
        break;

#ifndef XWFEATURE_STANDALONE_ONLY
    case ERR_NO_PEEK_REMOTE_TILES:
        message = "No peeking at remote players' tiles!";
        break;
    case ERR_REG_UNEXPECTED_USER:
        message = "Refused attempt to register unexpected user[s].";
        break;
    case ERR_SERVER_DICT_WINS:
        message = "Conflict between Host and Guest dictionaries; Host wins.";
        XP_WARNF( "GTK may have problems here." );
        break;
    case ERR_REG_SERVER_SANS_REMOTE:
        message = "At least one player must be marked remote for a game "
            "started as Host.";
        break;
#endif

    case ERR_NO_EMPTY_TRADE:
        message = "No tiles selected; trade cancelled.";
        break;

    case ERR_TOO_MANY_TRADE:
        message = "More tiles selected than remain in pool.";
        break;

    case ERR_NO_HINT_FOUND:
        message = "Unable to suggest any moves.";
        break;

    case ERR_CANT_UNDO_TILEASSIGN:
        message = "Tile assignment can't be undone.";
        break;

    case ERR_CANT_HINT_WHILE_DISABLED:
        message = "The hint feature is disabled for this game.  Enable "
            "it for a new game using the Preferences dialog.";
        break;

/*     case INFO_REMOTE_CONNECTED: */
/*         message = "Another device has joined the game"; */
/*         break; */

    case ERR_RELAY_BASE + XWRELAY_ERROR_LOST_OTHER:
        *silent = XP_TRUE;
        message = "XWRELAY_ERROR_LOST_OTHER";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_TIMEOUT:
        message = "The relay timed you out; other players "
            "have left or never showed up.";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_HEART_YOU:
        message = "You were disconnected from relay because it didn't "
            "hear from you in too long.";
        break;
    case ERR_RELAY_BASE + XWRELAY_ERROR_HEART_OTHER:
/*         *silent = XP_TRUE; */
        message = "The relay has lost contact with a device in this game.";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_OLDFLAGS:
        message = "You need to upgrade your copy of Crosswords.";
        break;
        
    case ERR_RELAY_BASE + XWRELAY_ERROR_SHUTDOWN:
        message = "Relay disconnected you to shut down (and probably reboot).";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_BADPROTO:
        message = "XWRELAY_ERROR_BADPROTO";
        break;
    case ERR_RELAY_BASE + XWRELAY_ERROR_RELAYBUSY:
        message = "XWRELAY_ERROR_RELAYBUSY";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_OTHER_DISCON:
        *silent = XP_TRUE;      /* happens all the time, and shouldn't matter */
        message = "XWRELAY_ERROR_OTHER_DISCON";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_NO_ROOM:
        message = "No such room.  Has the host connected yet to reserve it?";
        break;
    case ERR_RELAY_BASE + XWRELAY_ERROR_DUP_ROOM:
        message = "That room is reserved by another host.  Rename your room, "
            "become a guest, or try again in a few minutes.";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_TOO_MANY:
        message = "You tried to supply more players than the host expected.";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_DELETED:
        message = "Game deleted .";
        break;
    case ERR_RELAY_BASE + XWRELAY_ERROR_NORECONN:
        message = "Cannot reconnect.";
        break;
    case ERR_RELAY_BASE + XWRELAY_ERROR_DEADGAME:
        message = "Game is listed as dead on relay.";
        break;

    default:
        XP_LOGF( "no code for error: %d", id );
        message = "<unrecognized error code reported>";
    }

    return (XP_UCHAR*)message;
} /* linux_getErrString */

void
formatLMI( const LastMoveInfo* lmi, XP_UCHAR* buf, XP_U16 len )
{
    const XP_Bool inDuplicateMode = lmi->inDuplicateMode;
    const XP_UCHAR* name = inDuplicateMode ? "all" : lmi->names[0];
    switch( lmi->moveType ) {
    case ASSIGN_TYPE:
        XP_SNPRINTF( buf, len, "Tiles assigned to %s", name );
        break;
    case MOVE_TYPE:
        if ( 0 == lmi->nTiles ) {
            XP_SNPRINTF( buf, len, "%s passed", name );
        } else if ( !inDuplicateMode || 1 == lmi->nWinners  ) {
            XP_SNPRINTF( buf, len, "%s played %s for %d points", lmi->names[0],
                         lmi->word, lmi->score );
        } else {
            XP_SNPRINTF( buf, len, "%d players tied for %d points. %s was played",
                         lmi->nWinners, lmi->score, lmi->word );
        }
        break;
    case TRADE_TYPE:
        if ( inDuplicateMode ) {
            XP_SNPRINTF( buf, len, "%d tiles were exchanged", lmi->nTiles );
        } else {
            XP_SNPRINTF( buf, len, "%s traded %d tiles", name, lmi->nTiles );
        }
        break;
    case PHONY_TYPE:
        XP_ASSERT( !inDuplicateMode );
        XP_SNPRINTF( buf, len, "%s lost a turn", name );
        break;
    default:
        XP_ASSERT(0);
        break;
    }
}

void
formatConfirmTrade( CommonGlobals* cGlobals, const XP_UCHAR** tiles,
                    XP_U16 nTiles )
{
    int offset = 0;
    char tileBuf[128];
    int ii;

    XP_ASSERT( nTiles > 0 );
    for ( ii = 0; ii < nTiles; ++ii ) {
        offset += snprintf( &tileBuf[offset], sizeof(tileBuf) - offset, 
                            "%s, ", tiles[ii] );
        XP_ASSERT( offset < sizeof(tileBuf) );
    }
    tileBuf[offset-2] = '\0';

    snprintf( cGlobals->question, VSIZE(cGlobals->question),
              "Are you sure you want to trade the selected tiles (%s)?",
              tileBuf );
}

typedef struct _MsgRec {
    XP_U8* msg;
    XP_U16 msglen;
} MsgRec;

void
initNoConnStorage( CommonGlobals* cGlobals )
{
    XP_ASSERT( NULL == cGlobals->noConnMsgs );
    cGlobals->noConnMsgs = g_hash_table_new( g_str_hash, g_str_equal );
}

XP_Bool
storeNoConnMsg( CommonGlobals* cGlobals, const XP_U8* msg, XP_U16 msglen, 
                const XP_UCHAR* relayID )
{
    XP_ASSERT( 0 < msglen );
    XP_Bool inUse = NULL != cGlobals->noConnMsgs;
    if ( inUse ) {
        GSList* list = g_hash_table_lookup( cGlobals->noConnMsgs, relayID );
        gboolean missing = NULL == list;

        MsgRec* msgrec = g_malloc( sizeof(*msgrec) );
        msgrec->msg = g_malloc( msglen );
        XP_MEMCPY( msgrec->msg, msg, msglen );
        msgrec->msglen = msglen;
        list = g_slist_append( list, msgrec );
        if ( missing ) {
            gchar* key = g_strdup( relayID ); /* will leak */
            g_hash_table_insert( cGlobals->noConnMsgs, key, list );
        }
    }
    return inUse;
}

void
writeNoConnMsgs( CommonGlobals* cGlobals, int fd )
{
    GHashTable* hash = cGlobals->noConnMsgs;
    GList* keys = g_hash_table_get_keys( hash );
    GList* iter;
    for ( iter = keys; !!iter; iter = iter->next ) {
        XP_UCHAR* relayID = (XP_UCHAR*)iter->data;
        GSList* list = (GSList*)g_hash_table_lookup( hash, relayID );
        guint nMsgs = g_slist_length( list );
        XP_ASSERT( 0 < nMsgs );

        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                                    cGlobals->params->vtMgr );
        stream_putU16( stream, 1 ); /* number of relayIDs */
        stream_catString( stream, relayID );
        stream_putU8( stream, '\n' );
        stream_putU16( stream, nMsgs );

        int ii;
        for ( ii = 0; ii < nMsgs; ++ii ) {
            MsgRec* rec = g_slist_nth_data( list, ii );
            stream_putU16( stream, rec->msglen );
            stream_putBytes( stream, rec->msg, rec->msglen );

            g_free( rec->msg );
            g_free( rec );
        }
        g_slist_free( list );

        XP_U16 siz = stream_getSize( stream );
        /* XP_U8 buf[siz]; */
        /* stream_getBytes( stream, buf, siz ); */
        XP_U16 tmp = XP_HTONS( siz );
#ifdef DEBUG
        ssize_t nwritten = 
#endif
            write( fd, &tmp, sizeof(tmp) );
        XP_ASSERT( nwritten == sizeof(tmp) );
#ifdef DEBUG
        nwritten = 
#endif
            write( fd, stream_getPtr( stream ), siz );
        XP_ASSERT( nwritten == siz );
        stream_destroy( stream, NULL_XWE );
    }
    g_list_free( keys );
    g_hash_table_unref( hash );
    cGlobals->noConnMsgs = NULL;
} /* writeNoConnMsgs */

void
formatTimerText( gchar* buf, int bufLen, int secondsLeft )
{
    if ( secondsLeft < 0 ) {
        *buf++ = '-';
        --bufLen;
        secondsLeft *= -1;
    }

    int minutes = secondsLeft / 60;
    int seconds = secondsLeft % 60;
    XP_SNPRINTF( buf, bufLen, "% 1d:%02d", minutes, seconds );
}

void
formatSeconds( int unixSeconds, gchar* buf, int bufLen )
{
    GDateTime* timeval = g_date_time_new_from_unix_local( unixSeconds );
    gchar* str = g_date_time_format( timeval, "%c" );
    int len = strlen( str );
    if ( len < bufLen ) {
        XP_MEMCPY( buf, str, len + 1 );
    }
    g_date_time_unref( timeval );
    g_free( str );
}

#ifdef TEXT_MODEL
/* This is broken for UTF-8, even Spanish */
void
linux_lowerstr( XP_UCHAR* str )
{
    while ( *str ) {
        *str = tolower( *str );
        ++str;
    }
}
#endif
