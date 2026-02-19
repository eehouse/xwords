/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2000-2003 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <ctype.h>

#include "comtypes.h"

#include "cursesdlgutil.h"
#include "dbgutil.h"

void
measureAskText( const XP_UCHAR* question, int width, FormatInfo* fip )
{
    XP_U16 maxLen = 0;
    XP_Bool done = XP_FALSE;

    int len = strlen(question);
    const char* end = question + len;
    const char* cur = question;
    for ( int ii = 0; !done; ++ii ) {
        XP_ASSERT( ii < MAX_LINES );
        len = strlen(cur);

        if ( len == 0 ) {
            assert( ii > 0 );
            fip->nLines = ii;
            fip->maxLen = maxLen;
            break;
        }

        fip->line[ii].substr = cur;

        /* Now we need to break the line if 1) there's a <cr>; or 2) it's too
           long. */
        const char* cr = strstr( cur, "\n" );
        if ( NULL != cr && (cr - cur) < width ) {
            len = cr - cur;
        } else if ( len > width ) {
            const char* s = cur + width;
            while ( *s != ' ' && s > cur ) {
                --s;
            }
            assert( s > cur );  /* deal with this!! */
            len = s - cur;
        }
        fip->line[ii].len = len;
        if ( maxLen < len ) {
            maxLen = len;
        }

        cur += len + 1;         /* skip the <cr>/space */
        if ( cur > end ) {
            cur = end;
        }
    }
} /* measureAskText */

void
drawButtons( WINDOW* win, XP_U16 line, short spacePerButton, 
             short numButtons, short curSelButton, const char** button1 )
{
    for ( int ii = 0; ii < numButtons; ++ii ) {
        short len = strlen( *button1 );

        if ( ii == curSelButton ) {
            wstandout( win );
        }
        mvwprintw( win, line, ((ii+1) * spacePerButton) - (len/2),
                   "[%s]", *button1 );
        if ( ii == curSelButton ) {
            wstandend( win );
        }
        ++button1;
    }
    wrefresh( win );
} /* drawButtons */

WINDOW*
makeCenteredBox( WINDOW* parent, int width, int height )
{
    int parentX, parentY;
    getmaxyx( parent, parentY, parentX );

    WINDOW* win = newwin( height, width, (parentY-height)/2,
                          (parentX-width)/2 );
    wclear( win );
    box( win, '|', '-');
    return win;
}

void
initEdit( EditState* es, WINDOW* win, int msgLine, const char* initial )
{
    es->msgLine = msgLine;
    mvwaddnstr( win, es->msgLine, es->msgCol, " ",  VSIZE(es->msgBuf) );
    es->win = win;
    es->offset = 0;
    if ( !!initial ) {
        es->offset = snprintf( es->msgBuf, VSIZE(es->msgBuf), "%s", initial );
        XP_ASSERT( es->offset < VSIZE(es->msgBuf) );
    }
    es->msgBuf[es->offset] = '\0';
}

void
drawEdit( EditState* es, bool isFocussed  )
{
    LOG_FUNC();
    WINDOW* win = es->win;
    if ( isFocussed ) {
        wstandout( win );
    }
    const gchar* prompt = "->";
    es->msgCol = 1 + strlen(prompt);
    mvwaddstr( win, es->msgLine, 1, prompt );
    mvwaddstr( win, es->msgLine, es->msgCol, es->msgBuf );
    if ( isFocussed ) {
        wstandend( win );
    }
}

void
getEditText( EditState* es, gchar buf[], size_t* lenp )
{
    es->msgBuf[es->offset] = '\0';
    size_t nwritten = snprintf( buf, *lenp, "%s", es->msgBuf );
    if ( nwritten >= *lenp ) {
        nwritten = *lenp - 1;
    }
    *lenp = nwritten;
    buf[*lenp] = '\0';
}

void
handleEdit( EditState* es, int ch )
{
    if ( isprint(ch) && es->offset < VSIZE(es->msgBuf) - 1) {
        es->msgBuf[es->offset++] = ch;
        es->msgBuf[es->offset] = '\0';
    } else if ( ch == 127 && es->offset > 0 ) {
        XP_LOGFF( "ch: %d", ch );
        es->msgBuf[--es->offset] = ' ';
    }
}

/* If we call wgetch in a loop, we're blocking. But we can also install a
 * stdin handler that calls (*proc)(), and return.
 */
typedef struct _StdioState {
    WINDOW* win;
    OnKeyProc proc;
    void* closure;
    GMainLoop* loop;            /* used if we're blocking */
    guint src;
} StdioState;

static bool
dlgKeyProc( int key, gpointer data )
{
    LOG_FUNC();
    StdioState* ss = (StdioState*)data;
    XP_LOGFF( "got ch: %d", key );
    bool finished = (*ss->proc)( key, ss->closure );
    if ( finished ) {
        if ( !!ss->loop ) {
            g_main_loop_quit( ss->loop );
        }
        g_free( ss );
    }
    gboolean result = !finished;
    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

void
startModalAlert( CursesAppGlobals* aGlobals, WINDOW* win, XP_Bool block,
                 OnKeyProc proc, void* closure )
{
    LOG_FUNC();
    StdioState* ss = g_malloc0( sizeof(*ss) );
    ss->win = win;
    ss->proc = proc;
    ss->closure = closure;

    cursesPushKeyHandler( aGlobals, dlgKeyProc, ss );

    /* If we're to return a result, we need to block. Otherwise we can just return. */
    if ( block ) {
        GMainLoop* loop = ss->loop = g_main_loop_new( NULL, FALSE );
        g_main_loop_run( ss->loop );
        g_main_loop_unref( loop ); /* ss has been freed */
    }
    LOG_RETURN_VOID();
}

#endif
