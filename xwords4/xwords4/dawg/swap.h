/* 
 * Copyright 1997 by Eric House (fixin@peak.org).  All rights reserved.
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
 
#ifndef _SWAP_H_
#define _SWAP_H_

#if BYTE_ORDER == LITTLE_ENDIAN
static unsigned short swap_short( unsigned short s ) { 
    return s >> 8 | s << 8;
}

static unsigned long swap_long( unsigned long l ) { 
    return l >> 24 | (l>>8 & 0x0000FF00) | (l<<8 & 0x00FF0000) | l << 24;
}
#else
# define swap_short(s) (s)
# define swap_long(l) (l)
#endif

#endif /* _SWAP_H_ */
