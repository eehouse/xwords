/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (fixin@peak.org).  (based on sample
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

#ifndef _XWORDSAPPVIEW_H_
#define _XWORDSAPPVIEW_H_


#include <eikbctrl.h>

#include "game.h"
#include "symdraw.h"

/*! 
  @class CXWordsAppView
  
  @discussion This is the main view for Crosswords.  Owns all the
  common-code-created objects, passes events to them, etc.  Should be
  the only sample code file modified to any significant degree.
  */
//class CXWordsAppView : public CEikBorderedControl // ( which : CCoeControl )
class CXWordsAppView : public CCoeControl
{
 public:

    /*!
      @function NewL
   
      @discussion Create a CXWordsAppView object, which will draw itself to aRect
      @param aRect the rectangle this view will be drawn to
      @result a pointer to the created instance of CXWordsAppView
    */
    static CXWordsAppView* NewL(const TRect& aRect);

    /*!
      @function NewLC
   
      @discussion Create a CXWordsAppView object, which will draw itself to aRect
      @param aRect the rectangle this view will be drawn to
      @result a pointer to the created instance of CXWordsAppView
    */
    static CXWordsAppView* NewLC(const TRect& aRect);


    /*!
      @function ~CXWordsAppView
  
      @discussion Destroy the object and release all memory objects
    */
    ~CXWordsAppView();


 public:  // from CEikBorderedControl
    /*!
      @function Draw
  
      @discussion Draw this CXWordsAppView to the screen
      @param aRect the rectangle of this view that needs updating
    */
    void Draw(const TRect& aRect) const;
  

 private:

    /*!
      @function ConstructL
  
      @discussion  Perform the second phase construction of a CXWordsAppView object
      @param aRect the rectangle this view will be drawn to
    */
    void ConstructL(const TRect& aRect);

    /*!
      @function CXWordsAppView
  
      @discussion Perform the first phase of two phase construction 
    */
    CXWordsAppView();

    /* Added by eeh */
 public:
    int HandleCommand( TInt aCommand );
    TBool HandleKeyEvent( const TKeyEvent& aKeyEvent );

    void RunAlert( TInt resourceID );

 private:
    /* open game from prefs or start a new one. */
    void InitGameL();
    void DeleteGame();
    void SetUpUtil();
    void PositionBoard();

    static void        sym_util_requestTime( XW_UtilCtxt* uc );
    static VTableMgr*  sym_util_getVTManager( XW_UtilCtxt* uc );
    static XP_U32      sym_util_getCurSeconds( XW_UtilCtxt* uc );

    static TInt TimerCallback( TAny* aThis );

    CurGameInfo iGi;
    CommonPrefs iCp;
    XW_UtilCtxt iUtil;
    XWGame      iGame;
    DrawCtx*    iDraw;
    XP_Bool     iBoardPosInval;


    VTableMgr*  iVtMgr;
    TTime       iStartTime;
    TInt        iTimerRunCount;
    CIdle*      iRequestTimer;


    MPSLOT
};


#endif // _XWORDSAPPVIEW_H_
