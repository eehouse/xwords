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
#include "ceresstr.h"

static void colorButton( DRAWITEMSTRUCT* dis, HBRUSH brush );

#ifdef MY_COLOR_SEL

typedef struct ClrEditDlgState {
    CeDlgHdr dlgHdr; 
    HWND parent;
    HWND sampleButton;
    XP_U16 labelID;

    XP_U8 red;
    XP_U8 green;
    XP_U8 blue;

    XP_Bool inited;
    XP_Bool cancelled;
} ClrEditDlgState;

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
    initEditAndSlider( hDlg, CLREDT_SLIDER1, eState->red );
    initEditAndSlider( hDlg, CLREDT_SLIDER2, eState->green );
    initEditAndSlider( hDlg, CLREDT_SLIDER3, eState->blue );
} /* initChooseColor */

static XP_U8*
colorForSlider( ClrEditDlgState* eState, XP_U16 sliderID )
{
    switch( sliderID ) {
    case CLREDT_SLIDER1:
        return &eState->red;
    case CLREDT_SLIDER2:
        return &eState->green;
    case CLREDT_SLIDER3:
        return &eState->blue;
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

        InvalidateRect( eState->sampleButton, NULL, TRUE /* erase */ );
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
        InvalidateRect( eState->sampleButton, NULL, FALSE );
    }
} /* updateForField */

static void
colorButtonFromState( ClrEditDlgState* eState, DRAWITEMSTRUCT* dis )
{
    COLORREF ref = RGB( eState->red, eState->green, eState->blue );
    HBRUSH brush = CreateSolidBrush( ref );
    colorButton( dis, brush );
    DeleteObject( brush );
}

LRESULT CALLBACK
EditColorsDlg( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    ClrEditDlgState* eState;
    XP_U16 wid;
    XP_U16 notifyCode;
    NMTOOLBAR* nmToolP; 

    if ( message == WM_INITDIALOG ) {
        SetWindowLongPtr( hDlg, GWL_USERDATA, lParam );

        eState = (ClrEditDlgState*)lParam;
        eState->cancelled = XP_TRUE;
        eState->inited = XP_FALSE;

        ceDlgSetup( &eState->dlgHdr, hDlg, DLG_STATE_TRAPBACK ); 

        wchar_t label[32];
        XP_U16 len = SendDlgItemMessage( eState->parent, eState->labelID, 
                                         WM_GETTEXT, VSIZE(label), 
                                         (long)label );
        if ( len > 0 ) {
            label[len-1] = 0;       /* hack: overwrite ':' */
        }
        wchar_t buf[64];
        swprintf( buf, ceGetResStringL( eState->dlgHdr.globals, 
                                        IDS_EDITCOLOR_FORMAT ), label );
        
        SendMessage( hDlg, WM_SETTEXT, 0, (LPARAM)buf );

        eState->sampleButton = GetDlgItem( hDlg, CLSAMPLE_BUTTON_ID );
        EnableWindow( eState->sampleButton, FALSE );

        return TRUE;
    } else {
        eState = (ClrEditDlgState*)GetWindowLongPtr( hDlg, GWL_USERDATA );
        if ( !eState ) {
            return FALSE;
        }

        if ( !eState->inited ) {
            /* set to true first! Messages will be generated by
               initChooseColor call below */
            eState->inited = XP_TRUE;
            initChooseColor( eState, hDlg );
        }

        if ( ceDoDlgHandle( &eState->dlgHdr, message, wParam, lParam) ) {
            return TRUE;
        }

        switch (message) {

        case WM_DRAWITEM:
            colorButtonFromState( eState, (DRAWITEMSTRUCT*)lParam );
            return TRUE;
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
myChooseColor( CeDlgHdr* dlgHdr, XP_U16 labelID, COLORREF* cref )
{
    ClrEditDlgState state;
    int result;

    XP_MEMSET( &state, 0, sizeof(state) );
    state.dlgHdr.globals = dlgHdr->globals;
    state.red = GetRValue(*cref);
    state.green = GetGValue(*cref);
    state.blue = GetBValue(*cref);
    state.labelID = labelID;
    state.parent = dlgHdr->hDlg;

    XP_LOGF( "setting up IDD_COLOREDITDLG" );

    result = DialogBoxParam( dlgHdr->globals->locInst, 
                             (LPCTSTR)IDD_COLOREDITDLG, 
                             dlgHdr->hDlg, (DLGPROC)EditColorsDlg,
                             (long)&state );

    XP_LOGF( "DialogBoxParam=>%d", result );

    if ( !state.cancelled ) {
        *cref = RGB( state.red, state.green, state.blue );
    }
        
    return !state.cancelled;
} /* myChooseColor */

#endif /* MY_COLOR_SEL */

static void
colorButton( DRAWITEMSTRUCT* dis, HBRUSH brush )
{
    RECT rect = dis->rcItem;

    Rectangle( dis->hDC, rect.left, rect.top, rect.right, rect.bottom );
    InsetRect( &rect, 1, 1 );
    FillRect( dis->hDC, &rect, brush );
}

typedef struct ColorsDlgState {
    CeDlgHdr dlgHdr;
    COLORREF* inColors;

    COLORREF colors[CE_NUM_EDITABLE_COLORS];
    HBRUSH brushes[CE_NUM_EDITABLE_COLORS];
    HWND buttons[CE_NUM_EDITABLE_COLORS];

    XP_Bool cancelled;
    XP_Bool inited;
} ColorsDlgState;

#define FIRST_BUTTON DLBLTR_SAMPLE
#define LAST_BUTTON PLAYER4_SAMPLE

static void
initColorData( ColorsDlgState* cState )
{
    XP_U16 i;

    XP_ASSERT( (LAST_BUTTON - FIRST_BUTTON + 1) == CE_NUM_EDITABLE_COLORS );

    for ( i = 0; i < CE_NUM_EDITABLE_COLORS; ++i ) {
        COLORREF ref = cState->inColors[i];
        HWND button = GetDlgItem( cState->dlgHdr.hDlg, FIRST_BUTTON + i );
        cState->colors[i] = ref;
        cState->brushes[i] = CreateSolidBrush( ref );
        cState->buttons[i] = button;
        EnableWindow( button, FALSE );
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

static XP_Bool
wrapChooseColor( ColorsDlgState* cState, XP_U16 button )
{
    XP_Bool handled = XP_FALSE;
    if ( button >= DLBLTR_BUTTON && button <= PLAYER4_BUTTON ) {
        XP_U16 index = button - DLBLTR_BUTTON;

#ifdef MY_COLOR_SEL
        XP_U16 labelID = button + CLRSEL_LABEL_OFFSET;
        COLORREF clrref = cState->colors[index];

        if ( myChooseColor( &cState->dlgHdr, labelID, &clrref ) ) {
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
        ccs.hwndOwner = cState->dlgHdr.hDlg;
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
        handled = XP_TRUE;
    }
    return handled;
} /* wrapChooseColor */

static void
ceDrawColorButton( ColorsDlgState* cState, DRAWITEMSTRUCT* dis )
{
    HBRUSH brush = brushForButton( cState, dis->hwndItem );
    XP_ASSERT( !!brush );
    
    colorButton( dis, brush );
} /* ceDrawColorButton */

LRESULT CALLBACK
ColorsDlg( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    ColorsDlgState* state;
    BOOL result = FALSE;

    if ( message == WM_INITDIALOG ) {
        SetWindowLongPtr( hDlg, GWL_USERDATA, lParam );

        state = (ColorsDlgState*)lParam;
        state->cancelled = XP_TRUE;
        state->inited = XP_FALSE;

        ceDlgSetup( &state->dlgHdr, hDlg, DLG_STATE_NONE );

        result = TRUE;
    } else {
        state = (ColorsDlgState*)GetWindowLongPtr( hDlg, GWL_USERDATA );
        if ( !!state ) {
            XP_U16 wid;

            if ( !state->inited ) {
                initColorData( state );
                state->inited = XP_TRUE;
            }

/*             XP_LOGF( "%s: event=%s (%d); wParam=0x%x; lParam=0x%lx", */
/*                      __func__, messageToStr(message), message, */
/*                      wParam, lParam ); */

            if ( ceDoDlgHandle( &state->dlgHdr, message, wParam, lParam) ) {
                result = TRUE;
            } else {
                switch (message) {

                case WM_DRAWITEM:
                    ceDrawColorButton( state, (DRAWITEMSTRUCT*)lParam );
                    result = TRUE;
                    break;

                case WM_COMMAND:
                    if ( BN_CLICKED == HIWORD(wParam) ) {
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
                            result = wrapChooseColor( state, wid );
                            break;
                        }
                    }
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
    state.dlgHdr.globals = globals;
    state.inColors = colors;

    (void)DialogBoxParam( globals->locInst, (LPCTSTR)IDD_COLORSDLG, hwnd,
                          (DLGPROC)ColorsDlg, (long)&state );

    if ( !state.cancelled ) {
        XP_U16 i;
        for ( i = 0; i < CE_NUM_EDITABLE_COLORS; ++i ) {
            colors[i] = state.colors[i];
        }
    }
        
    return !state.cancelled;
} /* ceDoColorsEdit */
