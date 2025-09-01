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

static int
atEndProc(const DLHead* XP_UNUSED(dl1), const DLHead* XP_UNUSED(dl2))
{
    return -1;
}

DLHead*
dll_append( DLHead* list, DLHead* node )
{
    DLHead* result = dll_insert( list, node, atEndProc );
    XP_ASSERT( !node->_next );
    return result;
}

DLHead*
dll_insert( DLHead* head, DLHead* node, DLCompProc proc )
{
    DLHead* next;
    DLHead* prev = NULL;
    for ( next = head; !!next; next = next->_next ) {
        if ( NULL == proc || 0 <= (*proc)( next, node ) ) {
            break;
        }
        prev = next;
    }

    node->_prev = prev;
    node->_next = next;

    DLHead* newHead;
    if ( !!prev ) {
        newHead = head;
        prev->_next = node;
    } else {
        newHead = node;
    }
    if ( !!next ) {
        next->_prev = node;
    }
    XP_ASSERT( !!newHead );
    return newHead;
}

DLHead*
dll_remove( DLHead* list, DLHead* node )
{
    XP_ASSERT( !list->_prev );
    DLHead* newHead = list;
    if ( list == node ) {
        newHead = list->_next;
        if ( !!newHead ) {
            newHead->_prev = NULL;
        }
    } else {
        if ( !!node->_prev ) {
            node->_prev->_next = node->_next;
        }
        if ( !!node->_next ) {
            node->_next->_prev = node->_prev;
        }
    }
    return newHead;
}

static ForEachAct
lengthProc(const DLHead* XP_UNUSED(dl1), void* closure)
{
    XP_U16* count = (XP_U16*)closure;
    ++*count;
    return FEA_OK;
}

XP_U16
dll_length( const DLHead* list )
{
    XP_U16 result = 0;
    if ( !!list ) {
        dll_map( (DLHead*)list, lengthProc, NULL, &result );
    }
    return result;
}

DLHead*
dll_map( DLHead* list, DLMapProc mapProc, DLDisposeProc dispProc,
         void* closure )
{
    DLHead* newHead = list;
    while ( !!list ) {
        DLHead* next = list->_next;
        ForEachAct fea = (*mapProc)( list, closure );
        if ( 0 != (FEA_REMOVE & fea) ) {
            DLHead* victim = list;
            next = victim->_next;

            if ( victim == newHead ) {
                newHead = next;
            }

            if ( !!victim->_prev ) {
                victim->_prev->_next = victim->_next;
            }
            if ( !!victim->_next ) {
                victim->_next->_prev = victim->_prev;
            }

            if ( !!dispProc ) {
                (*dispProc)( list, closure );
            }
        }
        if ( 0 != (FEA_EXIT & fea) ) {
            goto done;
        }

        list = next;
    }
 done:
    return newHead;
}

static ForEachAct
removeAllProc( const DLHead* XP_UNUSED(elem), void* XP_UNUSED(closure) )
{
    return FEA_OK | FEA_REMOVE;
}

void
dll_removeAll( DLHead* list, DLDisposeProc dispProc, void* closure )
{
    dll_map( list, removeAllProc, dispProc, closure );
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
