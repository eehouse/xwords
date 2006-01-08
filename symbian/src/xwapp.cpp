/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).  (based on sample
 * app helloworldbasic "Copyright (c) 2002, Nokia. All rights
 * reserved.")
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

#include "xwdoc.h"
#include "xwapp.h"
#include "xwords.hrh"

// UID for the application, this should correspond to the uid defined in the
// mmp file.  (This is an official number from Symbian.  I've been allocated a
// block of 10 of which this is the first.)
static const TUid KUidXWordsApp = {XW_UID3};
 
CApaDocument*
CXWordsApplication::CreateDocumentL()
{  
    // Create an HelloWorldBasic document, and return a pointer to it
    CApaDocument* document = CXWordsDocument::NewL(*this);
    return document;
}

TUid
CXWordsApplication::AppDllUid() const
{

    return KUidXWordsApp;
}
