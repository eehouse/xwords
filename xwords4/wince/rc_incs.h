// -*- mode: c; compile-command: "make TARGET_OS=wince DEBUG=TRUE"; -*-

#ifdef _WIN32_WCE
# define UDS_EXPANDABLE 0x0200
# define UDS_NOSCROLL 0x0400
# define LISTBOX_CONTROL_FLAGS \
         NOT LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | NOT WS_BORDER | WS_TABSTOP
# define SPINNER_CONTROL_FLAGS \
    UDS_AUTOBUDDY | UDS_HORZ | UDS_ALIGNRIGHT | UDS_ARROWKEYS |\
        UDS_SETBUDDYINT | UDS_EXPANDABLE
#endif

#ifdef _WIN32_WCE
# define XWCOMBO(id,xx,yy,ww,ht1,exf1,ht2,exf2)                            \
    LISTBOX         id, xx,yy,ww,ht1, LISTBOX_CONTROL_FLAGS | exf1         \
    CONTROL "",     id+1, UPDOWN_CLASS, SPINNER_CONTROL_FLAGS,0,0,0,0      \
    COMBOBOX        id+2,xx,yy,ww,ht2,                                     \
                    CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP
#else
# define XWCOMBO(id,xx,yy,ww,ht1,exf1,ht2,exf2)                            \
    COMBOBOX        id+2,xx,yy,ww,ht1,                                     \
                    CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP | exf2
#endif

#define ABOUT_VERSION "4.4 b9"

#define DLL_VERS_RESOURCE              \
ID_DLLVERS_RES DLLV MOVEABLE PURE      \
BEGIN                                  \
    CUR_DLL_VERSION                    \
END
