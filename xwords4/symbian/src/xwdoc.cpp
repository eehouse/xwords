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

#include "xwappui.h"
#include "xwdoc.h"

// Standard Symbian OS construction sequence
CXWordsDocument* CXWordsDocument::NewL(CEikApplication& aApp)
    {
    CXWordsDocument* self = NewLC(aApp);
    CleanupStack::Pop(self);
    return self;
    }

CXWordsDocument* CXWordsDocument::NewLC(CEikApplication& aApp)
    {
    CXWordsDocument* self = new (ELeave) CXWordsDocument(aApp);
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
    }

void CXWordsDocument::ConstructL()
    {
	// no implementation required
    }    

CXWordsDocument::CXWordsDocument(CEikApplication& aApp) : CEikDocument(aApp) 
    {
	// no implementation required
    }

CXWordsDocument::~CXWordsDocument()
    {
	// no implementation required
    }

CEikAppUi* CXWordsDocument::CreateAppUiL()
    {
    // Create the application user interface, and return a pointer to it,
    // the framework takes ownership of this object
    CEikAppUi* appUi = new (ELeave) CXWordsAppUi;
    return appUi;
    }

