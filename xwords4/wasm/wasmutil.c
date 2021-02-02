
#include "util.h"
#include "comtypes.h"
#include "main.h"

typedef struct _WasmUtilCtx {
    XW_UtilCtxt super;

    XW_DUtilCtxt* dctxt;
    void* closure;
} WasmUtilCtx;

static XWStreamCtxt*
wasm_util_makeStreamFromAddr(XW_UtilCtxt* uc, XWEnv xwe,
                              XP_PlayerAddr channelNo )
{
    LOG_FUNC();
    return NULL;
}

static XWBonusType
wasm_util_getSquareBonus( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 boardSize,
                          XP_U16 col, XP_U16 row )
{
#define BONUS_DIM 8
    static const int s_buttsBoard[BONUS_DIM][BONUS_DIM] = {
        { BONUS_TRIPLE_WORD,  BONUS_NONE,         BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_TRIPLE_WORD },
        { BONUS_NONE,         BONUS_DOUBLE_WORD,  BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_TRIPLE_LETTER,BONUS_NONE,BONUS_NONE },

        { BONUS_NONE,         BONUS_NONE,         BONUS_DOUBLE_WORD,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE },
        { BONUS_DOUBLE_LETTER,BONUS_NONE,         BONUS_NONE,BONUS_DOUBLE_WORD,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_LETTER },
                            
        { BONUS_NONE,         BONUS_NONE,         BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_WORD,BONUS_NONE,BONUS_NONE,BONUS_NONE },
        { BONUS_NONE,         BONUS_TRIPLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_TRIPLE_LETTER,BONUS_NONE,BONUS_NONE },
                            
        { BONUS_NONE,         BONUS_NONE,         BONUS_DOUBLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE },
        { BONUS_TRIPLE_WORD,  BONUS_NONE,         BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_WORD },
    }; /* buttsBoard */

    int half = boardSize / 2;
    if ( col > half ) { col = (half*2) - col; }
    if ( row > half ) { row = (half*2) - row; }
    XP_ASSERT( col < BONUS_DIM && row < BONUS_DIM );
    return s_buttsBoard[row][col];
}

static void
wasm_util_userError( XW_UtilCtxt* uc, XWEnv xwe, UtilErrID id )
{
    LOG_FUNC();
}

static void
wasm_util_notifyMove( XW_UtilCtxt* uc, XWEnv xwe, XWStreamCtxt* stream )
{
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    Globals* globals = (Globals*)wuctxt->closure;
    if ( board_commitTurn( globals->game.board, NULL, XP_TRUE, XP_TRUE, NULL ) ) {
        board_draw( globals->game.board, NULL );
    }
}

static void
wasm_util_notifyTrade( XW_UtilCtxt* uc, XWEnv xwe, const XP_UCHAR** tiles,
                       XP_U16 nTiles )
{
    LOG_FUNC();
}
static void
wasm_util_notifyPickTileBlank( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 playerNum,
                                        XP_U16 col, XP_U16 row,
                                        const XP_UCHAR** tileFaces, 
                                        XP_U16 nTiles )
{
    LOG_FUNC();
}

static void
wasm_util_informNeedPickTiles( XW_UtilCtxt* uc, XWEnv xwe, XP_Bool isInitial,
                                        XP_U16 player, XP_U16 nToPick,
                                        XP_U16 nFaces, const XP_UCHAR** faces,
                                        const XP_U16* counts )
{
    LOG_FUNC();
}

static void
wasm_util_informNeedPassword( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 playerNum,
                              const XP_UCHAR* name )
{
    LOG_FUNC();
}

static void
wasm_util_trayHiddenChange(XW_UtilCtxt* uc, XWEnv xwe, 
                           XW_TrayVisState newState,
                           XP_U16 nVisibleRows )
{
    LOG_FUNC();
}

static void
wasm_util_yOffsetChange( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 maxOffset,
                         XP_U16 oldOffset, XP_U16 newOffset )
{
    LOG_FUNC();
}

static void
wasm_util_turnChanged(XW_UtilCtxt* uc, XWEnv xwe, XP_S16 newTurn)
{
    LOG_FUNC();
}

static void
wasm_util_notifyDupStatus( XW_UtilCtxt* uc, XWEnv xwe, XP_Bool amHost,
                                    const XP_UCHAR* msg )
{
    LOG_FUNC();
}

static void
wasm_util_informMove( XW_UtilCtxt* uc, XWEnv xwe, XP_S16 turn, 
                               XWStreamCtxt* expl, XWStreamCtxt* words )
{
    LOG_FUNC();
}

static void
wasm_util_informUndo( XW_UtilCtxt* uc, XWEnv xwe )
{
    LOG_FUNC();
}

static void
wasm_util_informNetDict( XW_UtilCtxt* uc, XWEnv xwe, XP_LangCode lang,
                                  const XP_UCHAR* oldName,
                                  const XP_UCHAR* newName,
                                  const XP_UCHAR* newSum,
                                  XWPhoniesChoice phoniesAction )
{
    LOG_FUNC();
}

static void
wasm_util_notifyGameOver( XW_UtilCtxt* uc, XWEnv xwe, XP_S16 quitter )
{
    LOG_FUNC();
}

static XP_Bool wasm_util_engineProgressCallback( XW_UtilCtxt* uc, XWEnv xwe )
{
    LOG_FUNC();
    return XP_FALSE;
}

static void
wasm_util_setTimer( XW_UtilCtxt* uc, XWEnv xwe, XWTimerReason why, XP_U16 when,
                    XWTimerProc proc, void* closure )
{
    XP_LOGFF( "(why: %d)", why );
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    Globals* globals = (Globals*)wuctxt->closure;
    main_set_timer( globals, why, when, proc, closure );
    LOG_RETURN_VOID();
}

static void
wasm_util_clearTimer( XW_UtilCtxt* uc, XWEnv xwe, XWTimerReason why )
{
    LOG_FUNC();
}

static void
wasm_util_requestTime( XW_UtilCtxt* uc, XWEnv xwe )
{
    LOG_FUNC();
}

static XP_Bool
wasm_util_altKeyDown( XW_UtilCtxt* uc, XWEnv xwe )
{
    LOG_FUNC();
    return XP_FALSE;
}

static DictionaryCtxt*
wasm_util_makeEmptyDict( XW_UtilCtxt* uc, XWEnv xwe )
{
    LOG_FUNC();
    return NULL;
}

static void
wasm_util_notifyIllegalWords( XW_UtilCtxt* uc, XWEnv xwe, BadWordInfo* bwi,
                                       XP_U16 turn, XP_Bool turnLost )
{
    LOG_FUNC();
}

static void
wasm_util_remSelected(XW_UtilCtxt* uc, XWEnv xwe)
{
    LOG_FUNC();
}

static void
wasm_util_timerSelected(XW_UtilCtxt* uc, XWEnv xwe, XP_Bool inDuplicateMode,
                                 XP_Bool canPause)
{
    LOG_FUNC();
}

static void
wasm_util_formatPauseHistory( XW_UtilCtxt* uc, XWEnv xwe, XWStreamCtxt* stream,
                              DupPauseType typ, XP_S16 turn,
                              XP_U32 secsPrev, XP_U32 secsCur,
                              const XP_UCHAR* msg )
{
    LOG_FUNC();
}

static void
wasm_util_bonusSquareHeld( XW_UtilCtxt* uc, XWEnv xwe, XWBonusType bonus )
{
    LOG_FUNC();
}

static void
wasm_util_playerScoreHeld( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 player )
{
    LOG_FUNC();
}

#ifdef XWFEATURE_BOARDWORDS
static void
wasm_util_cellSquareHeld( XW_UtilCtxt* uc, XWEnv xwe, XWStreamCtxt* words )
{
    LOG_FUNC();
}
#endif

static void
wasm_util_informMissing(XW_UtilCtxt* uc, XWEnv xwe, XP_Bool isServer, 
                        const CommsAddrRec* addr, XP_U16 nDevs,
                        XP_U16 nMissing )
{
    LOG_FUNC();
}

static void
wasm_util_addrChange( XW_UtilCtxt* uc, XWEnv xwe, const CommsAddrRec* oldAddr,
                      const CommsAddrRec* newAddr )
{
    LOG_FUNC();
}

static void
wasm_util_informWordsBlocked( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 nBadWords,
                               XWStreamCtxt* words, const XP_UCHAR* dictName )
{
    LOG_FUNC();
}

#ifdef XWFEATURE_SEARCHLIMIT
static XP_Bool
wasm_util_getTraySearchLimits( XW_UtilCtxt* uc, XWEnv xwe, 
                               XP_U16* min, XP_U16* max )
{
    LOG_FUNC();
}
#endif

static void
wasm_util_showChat( XW_UtilCtxt* uc, XWEnv xwe, const XP_UCHAR* const msg, 
                    XP_S16 from, XP_U32 timestamp )
{
    LOG_FUNC();
}

static XW_DUtilCtxt*
wasm_util_getDevUtilCtxt( XW_UtilCtxt* uc, XWEnv xwe )
{
    LOG_FUNC();
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    LOG_RETURNF( "%p", wuctxt->dctxt );
    return wuctxt->dctxt;
}

XW_UtilCtxt*
wasm_util_make( MPFORMAL CurGameInfo* gi, XW_DUtilCtxt* dctxt, void* closure )
{
    LOG_FUNC();
    WasmUtilCtx* wuctxt = XP_MALLOC( mpool, sizeof(*wuctxt) );
    wuctxt->super.vtable = XP_MALLOC( mpool, sizeof(*wuctxt->super.vtable) );
    wuctxt->super.mpool = mpool;
    wuctxt->super.gameInfo = gi;

    wuctxt->dctxt = dctxt;
    wuctxt->closure = closure;

    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_userError, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_makeStreamFromAddr, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_getSquareBonus, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_userError, wasm );

    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_notifyMove, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_notifyTrade, wasm );
                          SET_VTABLE_ENTRY( wuctxt->super.vtable, util_notifyPickTileBlank, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_informNeedPickTiles, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_informNeedPassword, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_trayHiddenChange, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_yOffsetChange, wasm );
#ifdef XWFEATURE_TURNCHANGENOTIFY
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_turnChanged, wasm );
#endif
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_notifyDupStatus, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_informMove, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_informUndo, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_informNetDict, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_notifyGameOver, wasm );
#ifdef XWFEATURE_HILITECELL
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_hiliteCell, wasm );
#endif

    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_setTimer, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_clearTimer, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_requestTime, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_altKeyDown, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_makeEmptyDict, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_notifyIllegalWords, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_remSelected, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_timerSelected, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_formatPauseHistory, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_bonusSquareHeld, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_playerScoreHeld, wasm );

#ifdef XWFEATURE_BOARDWORDS
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_cellSquareHeld, wasm );
#endif
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_informMissing, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_addrChange, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_informWordsBlocked, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_showChat, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_getDevUtilCtxt, wasm );

    LOG_RETURNF( "%p", wuctxt );
    return (XW_UtilCtxt*)wuctxt;
}

void
wasm_util_destroy( XW_UtilCtxt* util )
{
    LOG_FUNC();
    XP_ASSERT(0);
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)util;
    XP_FREEP( wuctxt->super.mpool, &wuctxt->super.vtable );
    XP_FREEP( wuctxt->super.mpool, &wuctxt );
}
