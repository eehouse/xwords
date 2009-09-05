/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2009 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _CONTYPCT_H_
#define _CONTYPCT_H_

#define CONN_TYPE_COUNT 0

#ifndef XWFEATURE_STANDALONE_ONLY

#ifdef XWFEATURE_RELAY
# define PREV_COUNT CONN_TYPE_COUNT
# undef CONN_TYPE_COUNT
# define CONN_TYPE_COUNT (PREV_COUNT+1)
# undef PREV_COUNT
#endif

#ifdef XWFEATURE_BLUETOOTH
# define PREV_COUNT CONN_TYPE_COUNT
# undef CONN_TYPE_COUNT
# define CONN_TYPE_COUNT (PREV_COUNT+1)
# undef PREV_COUNT
#endif

#ifdef XWFEATURE_SMS
# define PREV_COUNT CONN_TYPE_COUNT
# undef CONN_TYPE_COUNT
# define CONN_TYPE_COUNT (PREV_COUNT+1)
# undef PREV_COUNT
#endif

#ifdef XWFEATURE_IR
# define PREV_COUNT CONN_TYPE_COUNT
# undef CONN_TYPE_COUNT
# define CONN_TYPE_COUNT (PREV_COUNT+1)
# undef PREV_COUNT
#endif

#if CONN_TYPE_COUNT > 1
# define NEEDS_CHOOSE_CONNTYPE
#endif

#endif  /* XWFEATURE_STANDALONE_ONLY */

#endif
