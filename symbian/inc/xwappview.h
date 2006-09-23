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

#ifndef _XWORDSAPPVIEW_H_
#define _XWORDSAPPVIEW_H_


#include <eikbctrl.h>

#include "game.h"
#include "comms.h"
#include "memstream.h"
#include "symdraw.h"
#include "symgmmgr.h"
#include "symssock.h"
#include "symrsock.h"
#include "xwrelay.h"

typedef enum  {
    EGamesLoc
    ,EDictsLoc
    ,EPrefsLoc
} TDriveReason;

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
    static CXWordsAppView* NewL(const TRect& aRect, CEikApplication* aApp );

    /*!
      @function NewLC
   
      @discussion Create a CXWordsAppView object, which will draw itself to aRect
      @param aRect the rectangle this view will be drawn to
      @result a pointer to the created instance of CXWordsAppView
    */
    static CXWordsAppView* NewLC(const TRect& aRect, CEikApplication* aApp );


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
    CXWordsAppView( CEikApplication* aApp );

    /* Added by eeh */
 public:
    int HandleCommand( TInt aCommand );
    void Exiting();
    TBool HandleKeyEvent( const TKeyEvent& aKeyEvent );
    void UserErrorFromID( TInt aResource );
    XP_Bool UserQuery( UtilQueryID aId, XWStreamCtxt* aStream );

 private:
    typedef enum {
        EUtilRequest
        , EProcessPacket
        , ENumReasons
    } XWTimerReason_symb ;

    /* open game from prefs or start a new one. */
    void MakeOrLoadGameL();
    void DeleteGame();
    void SetUpUtil();
    void PositionBoard();
    void DisplayFinalScoresL();
    XWStreamCtxt* MakeSimpleStream( MemStreamCloseCallback cb,
                                    XP_U16 channelNo = CHANNEL_NONE );
    TBool AskFromResId( TInt aResource );
    TBool FindAllDicts();
    TBool LoadPrefs();
    TBool AskSaveGame() { return ETrue; }
    void SaveCurrentGame() {}
    void NotImpl();
    void GetXwordsRWDir( TFileName* aPathRef, TDriveReason aWhy );
    void InitPrefs();
    void WritePrefs();
    void SaveCurGame();

    void LoadOneGameL( TGameName* aGameName );
    void StoreOneGameL( TGameName* aGameName );
    TBool DoSavedGames();
    TBool DoNewGame();
    void DoImmediateDraw();
    void DrawGameName() const;
    void StartIdleTimer( XWTimerReason_symb aWhy );

    static void            sym_util_requestTime( XW_UtilCtxt* uc );
    static VTableMgr*      sym_util_getVTManager( XW_UtilCtxt* uc );
    static XP_U32          sym_util_getCurSeconds( XW_UtilCtxt* uc );
    static void            sym_util_notifyGameOverL( XW_UtilCtxt* uc );
    static void            sym_util_userError( XW_UtilCtxt* uc, UtilErrID id );
    static DictionaryCtxt* sym_util_makeEmptyDict( XW_UtilCtxt* uc );
    static XWStreamCtxt*   sym_util_makeStreamFromAddr( XW_UtilCtxt* uc,
                                                        XP_U16 channelNo );

    static void            sym_util_setTimer( XW_UtilCtxt* uc, 
                                              XWTimerReason why, XP_U16 when,
                                              TimerProc proc, void* closure );
#ifdef BEYOND_IR
    static void            sym_util_listenPortChange( XW_UtilCtxt* uc, 
                                                      XP_U16 listenPort );
    static void            sym_util_addrChange( XW_UtilCtxt* uc, 
                                                const CommsAddrRec* aOld,
                                                const CommsAddrRec* aNew );
#endif

#ifdef XWFEATURE_STANDALONE_ONLY
# define SYM_SEND      (TransportSend)NULL
#else
# define SYM_SEND      sym_send

    static XP_S16      sym_send( XP_U8* buf, XP_U16 len, 
                                 const CommsAddrRec* addr, void* closure );
    static void        sym_send_on_close( XWStreamCtxt* stream, 
                                          void* closure );
    static void PacketReceived( const TDesC8* aBuf, void* aClosure );

#endif

    static TInt TimerCallback( TAny* aThis );
    static TInt HeartbeatTimerCallback( TAny* closure );

    void SetHeartbeatCB( TimerProc aHBP, void* aHBC) {
        iHeartbeatProc =aHBP; iHeartbeatClosure = aHBC;
    }
    void GetHeartbeatCB( TimerProc* aHBP, void** aHBC) {
        *aHBP = iHeartbeatProc; *aHBC = iHeartbeatClosure;
    }

    CEikApplication* iApp;      /* remove if there's some way to get from
                                   env  */
    CurGameInfo iGi;
    CommonPrefs iCp;
#ifndef XWFEATURE_STANDALONE_ONLY
    CommsAddrRec iCommsAddr;    /* for default settings */
#endif
    XW_UtilCtxt iUtil;
    XWGame      iGame;
    DrawCtx*    iDraw;
    TGameName   iCurGameName;
    TRect       iTitleBox;

    VTableMgr*  iVtMgr;
    TTime       iStartTime;
    TInt        iTimerRunCount;
    CIdle*      iRequestTimer;
    TInt        iTimerReasons[ENumReasons];

    CXWGamesMgr* iGamesMgr;

    CDesC16ArrayFlat* iDictList;   /* to pass into the dialog */

    CDeltaTimer* iDeltaTimer;

    TimerProc    iHeartbeatProc;
    void*        iHeartbeatClosure;
    TCallBack    iHeartbeatCB;
    TDeltaTimerEntry iHeartbeatDTE;
    XP_Bool      iHBQueued;

#ifndef XWFEATURE_STANDALONE_ONLY
    CSendSocket* iSendSock;
    CDesC8ArrayFlat* iNewPacketQueue;
#endif

    MPSLOT
};

#endif // _XWORDSAPPVIEW_H_
