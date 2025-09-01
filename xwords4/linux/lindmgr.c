/* 
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <limits.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/stat.h>

#include "lindmgr.h"
#include "gsrcwrap.h"
#include "strutils.h"

struct LinDictMgr {
    void* closure;
    DictAddedProc ap;
    DictRemovedProc rp;

    int inotifyFD;

    GHashTable* dirs;
};

typedef struct _FindData {
    int soughtFD;
    const char* foundDir;
} FindData;

static gboolean
ghrFunc ( gpointer key, gpointer value, gpointer user_data )
{
    FindData* fd = (FindData*)user_data;
    gboolean found = fd->soughtFD == (int)(long)value;
    if ( found ) {
        fd->foundDir = key;
    }
    return found;
}

const char*
dirForFD( LinDictMgr* ldm, int fd )
{
    FindData fda = { .soughtFD = fd, };
    gpointer foundFD = g_hash_table_find( ldm->dirs, ghrFunc, &fda );
    XP_ASSERT( ((int)(long)foundFD) == fd );
    // XP_LOGFF( "(%d) => %s", fd, fda.foundDir );
    return fda.foundDir;
}

static struct inotify_event*
nextEvent(struct inotify_event* event, ssize_t* bytesLeft )
{
    /* Skip the current event */
    ssize_t siz = sizeof(*event) + event->len;
    // XP_LOGFF( "figured siz: %ld as %ld + %d", siz, sizeof(*event), event->len );
    *bytesLeft -= siz;
    // XP_LOGFF( "this size: %ld; left: %ld", siz, *bytesLeft );

    /* If there's space for another, return ptr to it */
    struct inotify_event* result = NULL;
    if ( sizeof(*event) <= *bytesLeft ) {
        unsigned char* ptr = (unsigned char*)event;
        ptr += sizeof(*event) + event->len;
        result = (struct inotify_event*)ptr;
    }
    // LOG_RETURNF( "%p", result );
    return result;
}

static gboolean
handle_inotify( GIOChannel* source, GIOCondition condition, gpointer data )
{
    LinDictMgr* ldm = (LinDictMgr*)data;
    int fd = g_io_channel_unix_get_fd( source );
    unsigned char buf[sizeof(struct inotify_event) + NAME_MAX + 1];

    ssize_t nRead = read( fd, buf, sizeof(buf) );
    // XP_LOGFF( "read %ld from %d", nRead, fd );
    // LOG_HEX( buf, nRead, __func__ );

    struct inotify_event* event;
    for ( event = (struct inotify_event*)buf; !!event;
          event = nextEvent(event, &nRead) ) {
        int wd = event->wd;
        // XP_LOGFF( "wd: %d; len: %d", wd, event->len );
        const char* dir = dirForFD( ldm, wd );

        XP_Bool isCreate = XP_FALSE;
        XP_Bool isDelete = XP_FALSE;
        switch ( event->mask ) {
        case IN_DELETE:
            isDelete = XP_TRUE;
            // XP_LOGFF( "%s deleted", event->name );
            break;
        case IN_CREATE:
            isCreate = XP_TRUE;
            // XP_LOGFF( "%s created", event->name );
            break;

        case IN_MOVED_FROM:
            // XP_LOGFF( "%s moved from", event->name );
            isDelete = XP_TRUE;
            break;
        case IN_MOVED_TO:
            // XP_LOGFF( "%s moved to", event->name );
            isCreate = XP_TRUE;
            break;
        default:
            // XP_LOGFF( "event 0x%x unhandled (%s)", event->mask, event->name );
            break;
        }

        if ( isDelete || isCreate ) {
            const char* dictName = event->name;
            if ( g_str_has_suffix( dictName, ".xwd" ) ) {
                char buf[NAME_MAX + 1];
                snprintf( buf, VSIZE(buf), "%s/%s", dir, dictName );
                if ( isCreate ) {
                    (*ldm->ap)(ldm->closure, dictName);
                } else if ( isDelete ) {
                    (*ldm->rp)(ldm->closure, dictName);
                } else {
                    XP_ASSERT(0);
                }
            }
        }
    }

    XP_USE( condition );
    XP_USE( data );
    return TRUE;
}

static gboolean
equalFunc( gconstpointer a, gconstpointer b )
{
    return 0 == strcmp( (char*)a, (char*)b );
}

static int
addListener( LinDictMgr* ldm, const char* fullPath )
{
    int fd = inotify_add_watch(ldm->inotifyFD, fullPath, IN_ALL_EVENTS
                               // IN_DELETE|IN_CREATE|IN_MOVED_FROM|IN_MOVED_TO
                               );

    XP_LOGFF( "stored %s with fd %d", fullPath, fd );
    XP_ASSERT( 0 < fd );
    return fd;
}

LinDictMgr*
ldm_init(DictAddedProc ap, DictRemovedProc rp, void* closure)
{
    LinDictMgr* ldm = g_malloc0( sizeof(*ldm) );
    ldm->ap = ap;
    ldm->rp = rp;
    ldm->closure = closure;

    ldm->inotifyFD = inotify_init();
    ADD_SOCKET( ldm, ldm->inotifyFD, handle_inotify );

    ldm->dirs = g_hash_table_new( g_str_hash, equalFunc );

    return ldm;
}

void
ldm_destroy( LinDictMgr* ldm )
{
    XP_LOGFF( "not cleaning up!!!" );
    g_free( ldm );
}

void
ldm_addDir( LinDictMgr* ldm, const char* path )
{
    char* fullPath = realpath( path, NULL );
    XP_LOGFF( "(path=%s)", fullPath );

    if ( !g_hash_table_contains( ldm->dirs, fullPath ) ) {
        int fd = addListener( ldm, fullPath );
        gboolean isNew = g_hash_table_insert( ldm->dirs, fullPath, (void*)(long)fd );
        XP_ASSERT( isNew );
    } else {
        XP_LOGFF( "%s already there", fullPath );
        free( fullPath );
    }
}


typedef struct _FindDictDirData {
    const char* dictNameIn;
    char pathOut[NAME_MAX + 1];
} FindDictDirData;

static gboolean
findFileFunc( gpointer key, gpointer XP_UNUSED(value), gpointer user_data )
{
    FindDictDirData* fddd = (FindDictDirData*)user_data;
    snprintf( fddd->pathOut, sizeof(fddd->pathOut), "%s/%s", (char*)key,
              fddd->dictNameIn );
    if ( !g_str_has_suffix( fddd->pathOut, ".xwd" ) ) {
        strcat( fddd->pathOut, ".xwd" );
    }

    struct stat statBuf;
    int statResult = stat( fddd->pathOut, &statBuf );
    XP_LOGFF( "%s exists? %d", fddd->pathOut, statResult );
    gboolean found = statResult == 0;
    return found;
}

/* Look for the file in each directory. If found, build complete path */
XP_Bool
ldm_pathFor( LinDictMgr* ldm, const char* dictName, char buf[], XP_U16 bufLen )
{
    FindDictDirData data = { .dictNameIn = dictName, };
    gpointer foundFD = g_hash_table_find( ldm->dirs, findFileFunc, &data );
    if ( !!foundFD ) {
        XP_SNPRINTF( buf, bufLen, "%s", data.pathOut );
    }
    return !!foundFD;
}

static void
listDictsFunc( gpointer key, gpointer XP_UNUSED(value), gpointer user_data )
{
    GSList** listPtr = (GSList**)user_data;
    const char* path = (char*)key;
    GDir* dir = g_dir_open( path, 0, NULL );
    if ( !!dir ) {
        for ( ; ; ) {
            const gchar* name = g_dir_read_name( dir );
            if ( !name ) {
                break;
            }
            if ( g_str_has_suffix( name, ".xwd" ) ) {
                gint len = strlen(name) - 4;
                *listPtr = g_slist_prepend( *listPtr, g_strndup( name, len ) );
            }
        }
        g_dir_close( dir );
    }
}

GSList*
ldm_listDicts( LinDictMgr* ldm )
{
    GSList* result = NULL;
    g_hash_table_foreach ( ldm->dirs, listDictsFunc, &result );
    return result;
}
