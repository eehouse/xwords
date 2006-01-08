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

#ifndef __XWDOC_H__
#define __XWDOC_H__

#include <eikdoc.h>

// Forward references
class CXWordsAppUi;
class CEikApplication;

/*! 
  @class CXWordsDocument
  
  @discussion An instance of class CXWordsDocument is the Document part of the
  Eikon application framework for the XWords example application
  */
class CXWordsDocument : public CEikDocument
{
 public:

    /*!
      @function NewL
  
      @discussion Construct a CXWordsDocument for the Eikon
      application aApp using two phase construction, and return a
      pointer to the created object @param aApp application creating
      this document @result a pointer to the created instance of
      CXWordsDocument
    */
    static CXWordsDocument* NewL(CEikApplication& aApp);

    /*!
      @function NewLC
  
      @discussion Construct a CXWordsDocument for the Eikon
      application aApp using two phase construction, and return a
      pointer to the created object @param aApp application creating
      this document @result a pointer to the created instance of
      CXWordsDocument
    */
    static CXWordsDocument* NewLC(CEikApplication& aApp);

    /*!
      @function ~CXWordsDocument
  
      @discussion Destroy the object and release all memory objects
    */
    ~CXWordsDocument();

 public: // from CEikDocument
    /*!
      @function CreateAppUiL 
  
      @discussion Create a CXWordsAppUi object and return a
      pointer to it @result a pointer to the created instance of the
      AppUi created
    */
    CEikAppUi* CreateAppUiL();

 private:

    /*!
      @function ConstructL
  
      @discussion Perform the second phase construction of a
      CXWordsDocument object
    */
    void ConstructL();

    /*!
      @function CXWordsDocument
  
      @discussion Perform the first phase of two phase construction 
      @param aApp application creating this document
    */
    CXWordsDocument(CEikApplication& aApp);

};

#endif //__XWDOC_H__
