// -*-mode: C; fill-column: 78; c-basic-offset: 4; -*-
/* 
 * Copyright 2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _FRANKPASSWD_H_
#define _FRANKPASSWD_H_

#include "comtypes.h"

class CAskPasswdWindow : public CWindow {
 private:
    const XP_UCHAR* fName;
    XP_UCHAR* fBuf;
    XP_U16* lenp;
    XP_Bool* okP;
    CTextEdit* entry;
    char fLabelBuf[64];
 public:
    CAskPasswdWindow( const XP_UCHAR* name, XP_UCHAR* buf, XP_U16* len, 
		      XP_Bool* result );
    S32 MsgHandler( MSG_TYPE type, CViewable *object, S32 data );
};


#endif /* _FRANKPASSWD_H_ */
