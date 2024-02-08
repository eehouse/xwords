/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE -j3"; -*- */
/*
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All rights
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

#include "dllist.h"
#include "dutil.h"              /* for NULL ??? */

void
dll_map( DLHead* list, DLMapProc proc, void* closure )
{
    for ( ; !!list; list = list->next ) {
        (*proc)( list, closure );
    }
}

DLHead*
dll_insert( DLHead* head, DLHead* node, DLCompProc proc )
{
    DLHead* next;
    DLHead* prev = NULL;
    for ( next = head; !!next; next = next->next ) {
        if ( 0 <= (*proc)( next, node ) ) {
            break;
        }
        prev = next;
    }

    node->prev = prev;
    node->next = next;

    DLHead* newHead;
    if ( !!prev ) {
        newHead = head;
        prev->next = node;
    } else {
        newHead = node;
    }
    if ( !!next ) {
        next->prev = node;
    }
    XP_ASSERT( !!newHead );
    return newHead;
}

DLHead*
dll_remove( DLHead* list, DLHead* node )
{
    DLHead* newHead = list;
    if ( list == node ) {
        newHead = list->next;
        if ( !!newHead ) {
            newHead->prev = NULL;
        }
    } else {
        if ( !!node->prev ) {
            node->prev->next = node->next;
        }
        if ( !!node->next ) {
            node->next->prev = node->prev;
        }
    }
    return newHead;
}

DLHead*
dll_sort( DLHead* list, DLCompProc proc )
{
    DLHead* result = NULL;

    while ( !!list ) {
        DLHead* node = list;
        list = dll_remove( list, node );
        result = dll_insert( result, node, proc );
    }
    return result;
}
