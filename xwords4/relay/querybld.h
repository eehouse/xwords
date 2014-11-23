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

#ifndef _QUERYBLD_H_
#define _QUERYBLD_H_

#include <vector>

#include "strwpf.h"

using namespace std;

class QueryBuilder {
    
 public:
    QueryBuilder& appendQueryf( const char* fmt, ... );
    QueryBuilder& appendParam( const char* value );
    QueryBuilder& appendParam( int value );
    void finish();
    int paramCount() const { return m_paramValues.size(); }
    const char* const* paramValues() const { return &m_paramValues[0]; }
    const char* const c_str() const { return m_query.c_str(); }

 private:
    StrWPF m_query;
    StrWPF m_paramBuf;
    vector<size_t> m_paramIndices;
    vector<const char*> m_paramValues;
};

#endif
