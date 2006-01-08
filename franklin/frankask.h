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
#if 0
#ifndef _FRANKASK_H_
#define _FRANKASK_H_

class CAskDialog : public CWindow {

 private:
    U16* resultP;
    U16 textY;
    U16 textX;
    char* question;
    BOOL drawInProgress;

 public:
    CAskDialog( U16* resultP, char* question, U16 numButtons, ...);
    void Draw();
    S32 MsgHandler( MSG_TYPE type, CViewable *object, S32 data );
};


#endif
#endif /* 0 */
