/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option.
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

#include "querybld.h"
#include "xwrelay_priv.h"

QueryBuilder&
QueryBuilder::appendQueryf( const char* fmt, ... )
{
    bool done;
    do {
        va_list ap;
        va_start( ap, fmt );
        done = m_query.catf( fmt, ap );
        va_end( ap );
    } while ( !done );
    return *this;
}

QueryBuilder&
QueryBuilder::appendParam( const char* value )
{
    m_paramIndices.push_back( m_paramBuf.size() );
    m_paramBuf.catf( "%s%c", value, '\0' );
    return *this;
}

QueryBuilder&
QueryBuilder::appendParam( int value )
{
    m_paramIndices.push_back( m_paramBuf.size() );
    m_paramBuf.catf( "%d%c", value, '\0' );
    return *this;
}

/* When done adding params, some of which contain $$, turn these into an order
 * progression of $1, $2 .. $9. Note assumption that we don't go above 9 since
 */
void
QueryBuilder::finish() 
{
    assert( 0 == m_paramValues.size() );

    size_t ii;
    const char* base = m_paramBuf.c_str();
    for ( ii = 0; ii < m_paramIndices.size(); ++ii ) {
        const char* ptr = m_paramIndices[ii] + base;
        m_paramValues.push_back( ptr );
    }

    for ( size_t count = 0; ; ++count ) {
        const char* str = m_query.c_str();
        const char* ptr = strstr( str, "$$" );
        if ( !ptr ) {
            assert( count == m_paramIndices.size() );
            break;
        }
        assert( count < 9 );
        m_query[1 + ptr - str] = '1' + count;
    }
}
