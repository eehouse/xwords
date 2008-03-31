/* -*- fill-column: 77; c-basic-offset: 4; compile-command: "make TARGET_OS=wince DEBUG=TRUE" -*- */
/* 
 * Copyright 2004-2008 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <windowsx.h>
#include "stdafx.h" 
#include <commdlg.h>

#include "ceclrsel.h" 
#include "ceutil.h" 
#include "cedebug.h"
#include "debhacks.h"

#ifdef MY_COLOR_SEL

typedef struct ClrEditDlgState {
    CEAppGlobals* globals;

    RECT clrRect;
    HWND parent;
    XP_U16 labelID;

    XP_U8 r;
    XP_U8 b;
    XP_U8 g;

    XP_Bool inited;
    XP_Bool cancelled;
} ClrEditDlgState;

static void
drawColorRect( ClrEditDlgState* eState, HDC hdc )
{
    COLORREF ref = RGB( eState->r, eState->g, eState->b );
    HBRUSH brush = CreateSolidBrush( ref );
    FillRect( hdc, &eState->clrRect, brush );
    DeleteObject( brush );
} /* drawColorRect */

static void
initEditAndSlider( HWND hDlg, XP_U16 sliderID, XP_U8 val )
{
    SendDlgItemMessage( hDlg, sliderID, TBM_SETRANGE, TRUE, 
                        MAKELONG(0,255) );
    SendDlgItemMessage( hDlg, sliderID, TBM_SETPOS, TRUE, 
                        (long)val );
    ceSetDlgItemNum( hDlg, sliderID+1, val );
} /* initEditAndSlider */

static void
initChooseColor( ClrEditDlgState* eState, HWND hDlg )
{
    eState->clrRect.left = 162;
    eState->clrRect.top = 5;
    eState->clrRect.right = 193;
    eState->clrRect.bottom = 90;

    InvalidateRect( hDlg, &eState->clrRect, FALSE );

    initEditAndSlider( hDlg, CLREDT_SLIDER1, eState->r );
    initEditAndSlider( hDlg, CLREDT_SLIDER2, eState->g );
    initEditAndSlider( hDlg, CLREDT_SLIDER3, eState->b );
} /* initChooseColor */

static XP_U8*
colorForSlider( ClrEditDlgState* eState, XP_U16 sliderID )
{
    switch( sliderID ) {
    case CLREDT_SLIDER1:
        return &eState->r;
    case CLREDT_SLIDER2:
        return &eState->g;
    case CLREDT_SLIDER3:
        return &eState->b;
    default:
        XP_LOGF( "huh???" );
        return NULL;
    }
} /* colorForSlider */

static void 
updateForSlider( HWND hDlg, ClrEditDlgState* eState, XP_U16 sliderID )
{
    XP_U8 newColor = (XP_U8)SendDlgItemMessage( hDlg, sliderID, TBM_GETPOS, 
                                                0, 0L );
    XP_U8* colorPtr = colorForSlider( eState, sliderID );
    if ( newColor != *colorPtr ) {
        *colorPtr = newColor;

        ceSetDlgItemNum( hDlg, sliderID+1, (XP_S32)newColor );

        InvalidateRect( hDlg, &eState->clrRect, FALSE );
    }
} /* updateForSlider */

static void
updateForField( HWND hDlg, ClrEditDlgState* eState, XP_U16 fieldID )
{
    XP_S32 newColor = ceGetDlgItemNum( hDlg, fieldID );
    XP_U8* colorPtr = colorForSlider( eState, fieldID - 1 );
    XP_Bool modified = XP_FALSE;;

    if ( newColor > 255 ) {
        newColor = 255;
        modified = XP_TRUE;
    } else if ( newColor < 0 ) {
        newColor = 0;
        modified = XP_TRUE;
    } 
    if ( modified ) {
        ceSetDlgItemNum( hDlg, fieldID, newColor );
    }
    
    if ( newColor != *colorPtr ) {
        *colorPtr = (XP_U8)newColor;

        SendDlgItemMessage( hDlg, fieldID-1, TBM_SETPOS, TRUE, 
                            (long)newColor );
        InvalidateRect( hDlg, &eState->clrRect, FALSE );
    }
} /* updateForField */

LRESULT CALLBACK
EditColorsDlg( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    ClrEditDlgState* eState;
    XP_U16 wid;
    XP_U16 notifyCode;
    NMTOOLBAR* nmToolP; 

    if ( message == WM_INITDIALOG ) {
        SetWindowLong( hDlg, GWL_USERDATA, lParam );

        eState = (ClrEditDlgState*)lParam;
        eState->cancelled = XP_TRUE;
        eState->inited = XP_FALSE;

        ceDlgSetup( eState->globals, hDlg );

        wchar_t label[32];
        XP_U16 len = SendDlgItemMessage( eState->parent, eState->labelID, 
                                         WM_GETTEXT, VSIZE(label), 
                                         (long)label );
        if ( len > 0 ) {
            label[len-1] = 0;       /* hack: overwrite ':' */
        }
        wchar_t buf[64];
        swprintf( buf, L"Edit color for %s", label );
        SendMessage( hDlg, WM_SETTEXT, 0, (LPARAM)buf );

        return TRUE;
    } else {
        eState = (ClrEditDlgState*)GetWindowLong( hDlg, GWL_USERDATA );
        if ( !eState ) {
            return FALSE;
        }

        if ( !eState->inited ) {
            /* set to true first! Messages will be generated by
               initChooseColor call below */
            eState->inited = XP_TRUE;
            initChooseColor( eState, hDlg );
            XP_LOGF( "initChooseColor done" );
        }

        switch (message) {

        case WM_VSCROLL:
            if ( !IS_SMARTPHONE(eState->globals) ) {
                ceDoDlgScroll( hDlg, wParam );
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint( hDlg, &ps );
            drawColorRect( eState, hdc );
            EndPaint( hDlg, &ps );
        }
            break;

        case WM_NOTIFY:
            nmToolP = (NMTOOLBAR*)lParam;
            wid = nmToolP->hdr.idFrom;
            switch ( wid ) {
            case CLREDT_SLIDER1:
            case CLREDT_SLIDER2:
            case CLREDT_SLIDER3:
                updateForSlider( hDlg, eState, wid );
                break;
            }
            break;

        case WM_COMMAND:
            wid = LOWORD(wParam);
            switch( wid ) {
            case RED_EDIT:
            case GREEN_EDIT:
            case BLUE_EDIT:
                notifyCode = HIWORD(wParam);
                if ( notifyCode == EN_CHANGE ) {
                    updateForField( hDlg, eState, wid );
                    return TRUE;
                }
                break;

            case IDOK:
                eState->cancelled = XP_FALSE;
                /* fallthrough */
                
            case IDCANCEL:
                EndDialog(hDlg, wid);
                return TRUE;
            }
        }
    }

    return FALSE;
} /* EditColorsDlg */

static XP_Bool
myChooseColor( CEAppGlobals* globals, HWND parent, XP_U16 labelID, 
               COLORREF* cref )
{
    ClrEditDlgState state;
    int result;

    XP_MEMSET( &state, 0, sizeof(state) );
    state.globals = globals;
    state.r = GetRValue(*cref);
    state.g = GetGValue(*cref);
    state.b = GetBValue(*cref);
    state.labelID = labelID;
    state.parent = parent;

    XP_LOGF( "setting up IDD_COLOREDITDLG" );

    result = DialogBoxParam( globals->hInst, (LPCTSTR)IDD_COLOREDITDLG, 
                             parent, (DLGPROC)EditColorsDlg, (long)&state );

    XP_LOGF( "DialogBoxParam=>%d", result );

    if ( !state.cancelled ) {
        *cref = RGB( state.r, state.g, state.b );
    }
        
    return !state.cancelled;
} /* myChooseColor */

#endif /* MY_COLOR_SEL */

typedef struct ColorsDlgState {
    HWND hDlg;
    CEAppGlobals* globals;
    COLORREF* inColors;

    COLORREF colors[CE_NUM_EDITABLE_COLORS];
    HBRUSH brushes[CE_NUM_EDITABLE_COLORS];
    HWND buttons[CE_NUM_EDITABLE_COLORS];

    XP_Bool cancelled;
    XP_Bool inited;
} ColorsDlgState;

#define FIRST_BUTTON DLBLTR_BUTTON
#define LAST_BUTTON PLAYER4_BUTTON

static void
initColorData( ColorsDlgState* cState )
{
    XP_U16 i;

    XP_ASSERT( (LAST_BUTTON - FIRST_BUTTON + 1) == CE_NUM_EDITABLE_COLORS );

    for ( i = 0; i < CE_NUM_EDITABLE_COLORS; ++i ) {
        COLORREF ref = cState->inColors[i];
        cState->colors[i] = ref;
        cState->brushes[i] = CreateSolidBrush( ref );
        cState->buttons[i] = GetDlgItem( cState->hDlg, FIRST_BUTTON + i );
    }
} /* initColorData */

static HBRUSH
brushForButton( ColorsDlgState* cState, HWND hwndButton )
{
    XP_U16 i;
    for ( i = 0; i < CE_NUM_EDITABLE_COLORS; ++i ) {
        if ( cState->buttons[i] == hwndButton ) {
            return cState->brushes[i];
        }
    }
    return NULL;
} /* brushForButton */

static void
deleteButtonBrushes( ColorsDlgState* cState )
{
    XP_U16 i;
    for ( i = 0; i < CE_NUM_EDITABLE_COLORS; ++i ) {
        DeleteObject( cState->brushes[i] );
    }
} /* deleteButtonBrushes */

static void
wrapChooseColor( ColorsDlgState* cState, XP_U16 button )
{
    if ( button >= DLBLTR_BUTTON && button <= PLAYER4_BUTTON ) {
        XP_U16 index = button-DLBLTR_BUTTON;

#ifdef MY_COLOR_SEL
        XP_U16 labelID = button + CLRSEL_LABEL_OFFSET;
        COLORREF clrref = cState->colors[index];

        if ( myChooseColor( cState->globals, cState->hDlg, labelID, &clrref ) ) {
            cState->colors[index] = clrref;
            DeleteObject( cState->brushes[index] );
            cState->brushes[index] = CreateSolidBrush( clrref );
            XP_LOGF( "%s: may need to invalidate the button since "
                     "color's changed", __func__ );
        }
#else
        CHOOSECOLOR ccs;
        BOOL hitOk;
        COLORREF arr[16];
        XP_U16 i;

        XP_MEMSET( &ccs, 0, sizeof(ccs) );
        XP_MEMSET( &arr, 0, sizeof(arr) );

        for ( i = 0; i < CE_NUM_EDITABLE_COLORS; ++i ) {
            arr[i] = cState->colors[i];
        }

        ccs.lStructSize = sizeof(ccs);
        ccs.hwndOwner = cState->hDlg;
        ccs.rgbResult = cState->colors[index];
        ccs.lpCustColors = arr;

        ccs.Flags = CC_ANYCOLOR | CC_RGBINIT | CC_FULLOPEN;

        hitOk = ChooseColor( &ccs );

        if ( hitOk ) {
            cState->colors[index] = ccs.rgbResult;
            DeleteObject( cState->brushes[index] );
            cState->brushes[index] = CreateSolidBrush( ccs.rgbResult );
        }
#endif
    }
} /* wrapChooseColor */

/* I'd prefer to use normal buttons, letting the OS draw them except for
 * their background color, but MS docs don't seem to allow any way to do
 * that.  I'm either totally on my own drawing the button or they're all in
 * the same color and so useless.  So they're just rects with a black outer
 * rect to show focus.
 */
static void
ceDrawColorButton( ColorsDlgState* cState, DRAWITEMSTRUCT* dis )
{
    HBRUSH brush = brushForButton( cState, dis->hwndItem );
    XP_ASSERT( !!brush );
    
    RECT rect = dis->rcItem;
    XP_Bool hasFocus = ((dis->itemAction & ODA_FOCUS) != 0)
        && ((dis->itemState & ODS_FOCUS) != 0);

    Rectangle( dis->hDC, rect.left, rect.top, rect.right, rect.bottom );
    InsetRect( &rect, 1, 1 );
    if ( hasFocus ) {
        Rectangle( dis->hDC, rect.left, rect.top, rect.right, rect.bottom );
        (void)SendMessage( cState->hDlg, DM_SETDEFID, dis->CtlID, 0 );
    }
    InsetRect( &rect, 1, 1 );
    FillRect( dis->hDC, &rect, brush );
} /* ceDrawColorButton */

LRESULT CALLBACK
ColorsDlg( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    ColorsDlgState* state;
    BOOL result = FALSE;

    if ( message == WM_INITDIALOG ) {
        SetWindowLong( hDlg, GWL_USERDATA, lParam );

        state = (ColorsDlgState*)lParam;
        state->cancelled = XP_TRUE;
        state->inited = XP_FALSE;
        state->hDlg = hDlg;

        ceDlgSetup( state->globals, hDlg );

        result = TRUE;
    } else {
        state = (ColorsDlgState*)GetWindowLong( hDlg, GWL_USERDATA );
        if ( !!state ) {
            XP_U16 wid;

            if ( !state->inited ) {
                initColorData( state );
                state->inited = XP_TRUE;
            }

/*             XP_LOGF( "%s: event=%s (%d); wParam=0x%x; lParam=0x%lx",  */
/*                      __func__, messageToStr(message), message,  */
/*                      wParam, lParam ); */

            switch (message) {

            case WM_VSCROLL:
                if ( !IS_SMARTPHONE(state->globals) ) {
                    ceDoDlgScroll( hDlg, wParam );
                }
                break;

            case WM_DRAWITEM:   /* passed when button has BS_OWNERDRAW style */
                ceDoDlgFocusScroll( hDlg, 
                      /* Fake out ceDoDlgFocusScroll, passing ctrl itself */
                      (WPARAM)((DRAWITEMSTRUCT*)lParam)->hwndItem, 
                      (LPARAM)TRUE );
                ceDrawColorButton( state, (DRAWITEMSTRUCT*)lParam );
                result = TRUE;
                break;

            case WM_COMMAND:
                wid = LOWORD(wParam);
                switch( wid ) {

                case IDOK:
                    state->cancelled = XP_FALSE;
                    /* fallthrough */

                case IDCANCEL:
                    deleteButtonBrushes( state );
                    EndDialog(hDlg, wid);
                    result = TRUE;
                    break;

                default:
                    /* it's one of the color buttons.  Set up with the
                       appropriate color and launch ChooseColor */
                    wrapChooseColor( state, wid );
                    result = TRUE;
                    break;
                }
            }
        }
    }

    return result;
} /* ColorsDlg */

XP_Bool
ceDoColorsEdit( HWND hwnd, CEAppGlobals* globals, COLORREF* colors )
{
    ColorsDlgState state;

    XP_MEMSET( &state, 0, sizeof(state) );
    state.globals = globals;
    state.inColors = colors;

    (void)DialogBoxParam( globals->hInst, (LPCTSTR)IDD_COLORSDLG, hwnd,
                          (DLGPROC)ColorsDlg, (long)&state );

    if ( !state.cancelled ) {
        XP_U16 i;
        for ( i = 0; i < CE_NUM_EDITABLE_COLORS; ++i ) {
            colors[i] = state.colors[i];
        }
    }
        
    return !state.cancelled;
} /* ceDoColorsEdit */
