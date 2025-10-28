#ifdef PLATFORM_NCURSES

#include "cursinvit.h"
#include "cursesask.h" 
#include "cursqr.h"
#include "dbgutil.h"
#include "comtypes.h"
#include "comms.h"
#include "linuxmain.h"
#include "strutils.h"
#include "knownplyr.h"

#include <ncurses.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

typedef enum {
    TAB_BY_ADDRESS = 0,
    TAB_KNOWN_PLAYERS = 1,
    NUM_TABS
} TabType;

typedef struct _CursesInviteState {
    CommsAddrRec addr;
    gint* nPlayersP;
    gint selectedPlayers;
    XP_Bool cancelled;

    /* Address fields */
    char mqttField[64];
    char bluetoothField[64];
    char smsField[64];

    char buffer[56];

    /* Tab navigation */
    TabType currentTab;
    int currentField;       /* For address tab */
    int currentPlayer;      /* For known players tab */
} CursesInviteState;

static char*
getClipboardText(CursesInviteState* state)
{
    char* result = NULL;

    /* Try multiple clipboard methods */
    const char* cmds[] = {
        "xclip -selection clipboard -o 2>/dev/null",
        "xsel --clipboard --output 2>/dev/null", 
        "wl-paste 2>/dev/null",  /* Wayland */
        "pbpaste 2>/dev/null",   /* macOS */
        NULL
    };

    for (int cmdIndex = 0; cmds[cmdIndex] && !result; cmdIndex++) {
        FILE* fp = popen(cmds[cmdIndex], "r");
        if (fp) {
            char* buffer = state->buffer;
            if (buffer) {
                size_t len = fread(buffer, 1, 255, fp);
                int status = pclose(fp);

                if (status == 0 && len > 0) {
                    buffer[len] = '\0';

                    /* Remove newlines */
                    for (char* ptr = buffer; *ptr; ptr++) {
                        if (*ptr == '\n' || *ptr == '\r') *ptr = '\0';
                    }

                    if (strlen(buffer) > 0) {
                        result = buffer;
                    }
                }
            } else {
                pclose(fp);
            }
        }
    }

    return result;
}

static XP_Bool
editField(WINDOW* parent, const char* prompt, char* buffer, int bufSize,
          CursesInviteState* state )
{
    XP_Bool result = XP_FALSE;
    int py, px, ph, pw;
    getbegyx(parent, py, px);
    getmaxyx(parent, ph, pw);

    int height = 5;
    int width = 50;
    int yy = py + ph/2 - height/2;
    int xx = px + pw/2 - width/2;

    WINDOW* editWin = newwin(height, width, yy, xx);
    if (editWin) {
        box(editWin, 0, 0);
        mvwprintw(editWin, 1, 2, "%s", prompt);
        mvwprintw(editWin, 3, 2, "Ctrl+V/F2: Paste, Enter: OK, Esc: Cancel");
        wrefresh(editWin);

        int ch;
        int pos = strlen(buffer);
        int startCol = 2;
        int maxWidth = width - 4;
        XP_Bool editing = XP_TRUE;

        while (editing) {
            /* Show current text */
            mvwhline(editWin, 2, startCol, ' ', maxWidth);
            mvwprintw(editWin, 2, startCol, "%-*.*s", maxWidth, maxWidth, buffer);

            /* Position cursor */
            int displayPos = pos;
            if (displayPos >= maxWidth) displayPos = maxWidth - 1;
            wmove(editWin, 2, startCol + displayPos);
            wrefresh(editWin);

            ch = wgetch(editWin);

            if (ch == '\n' || ch == KEY_ENTER || ch == '\r') {
                result = XP_TRUE;
                editing = XP_FALSE;
            } else if (ch == 27) { /* ESC */
                editing = XP_FALSE;
            } else if (ch == 22 || ch == ('V' & 0x1F) || ch == KEY_F(2)) { /* Ctrl+V or F2 - paste */
                char* clipText = getClipboardText(state);
                if (clipText && strlen(clipText) > 0) {
                    /* Insert clipboard text at cursor position */
                    int clipLen = strlen(clipText);
                    int remaining = bufSize - 1 - pos;
                    if (clipLen > remaining) clipLen = remaining;

                    if (clipLen > 0) {
                        /* Make room for new text */
                        memmove(buffer + pos + clipLen, buffer + pos, strlen(buffer + pos) + 1);
                        /* Insert new text */
                        memcpy(buffer + pos, clipText, clipLen);
                        pos += clipLen;
                    }
                } else {
                    /* Show message if clipboard is empty or unavailable */
                    mvwprintw(editWin, 4, 2, "No clipboard content (install xclip/xsel)");
                    wrefresh(editWin);
                    napms(1000); /* Show message briefly */
                }
            } else if (ch == KEY_BACKSPACE || ch == '\b' || ch == 127) {
                if (pos > 0) {
                    memmove(buffer + pos - 1, buffer + pos, strlen(buffer + pos) + 1);
                    pos--;
                }
            } else if (ch == KEY_LEFT) {
                if (pos > 0) pos--;
            } else if (ch == KEY_RIGHT) {
                if (pos < (int)strlen(buffer)) pos++;
            } else if (ch == KEY_HOME || ch == 1) { /* Ctrl+A */
                pos = 0;
            } else if (ch == KEY_END || ch == 5) { /* Ctrl+E */
                pos = strlen(buffer);
            } else if (ch == KEY_DC || ch == 4) { /* Delete or Ctrl+D */
                if (pos < (int)strlen(buffer)) {
                    memmove(buffer + pos, buffer + pos + 1, strlen(buffer + pos));
                }
            } else if (isprint(ch) && pos < bufSize - 1) {
                /* Insert character at cursor position */
                int len = strlen(buffer);
                if (len < bufSize - 1) {
                    memmove(buffer + pos + 1, buffer + pos, len - pos + 1);
                    buffer[pos] = ch;
                    pos++;
                }
            }
        }

        delwin(editWin);
    }

    return result;
}

static void
drawTabs(WINDOW* dlg, TabType currentTab)
{
    const char* tabNames[] = {"by address", "Known players"};
    int tabWidth = 15;
    int startX = 2;

    for (int tabIndex = 0; tabIndex < NUM_TABS; tabIndex++) {
        if (tabIndex == currentTab) {
            wattrset(dlg, A_REVERSE);
            mvwprintw(dlg, 1, startX + tabIndex * tabWidth, " %-13s ", tabNames[tabIndex]);
            wattrset(dlg, A_NORMAL);
        } else {
            mvwprintw(dlg, 1, startX + tabIndex * tabWidth, " %-13s ", tabNames[tabIndex]);
        }
    }
}

static void
drawKnownPlayersTab(WINDOW* dlg, CursesInviteState* state, CommonGlobals* cGlobals)
{
    int row = 3;
    mvwprintw(dlg, row++, 2, "Known Players:");
    row++;

    LaunchParams* params = cGlobals->params;
    XW_DUtilCtxt* dutil = params->dutil;

    /* Get known players list */
    XP_U16 nFound = 0;
    kplr_getNames( dutil, NULL_XWE, XP_FALSE, NULL, &nFound );
    const XP_UCHAR* players[nFound];
    kplr_getNames( dutil, NULL_XWE, XP_FALSE, players, &nFound );

    if (nFound > 0) {
        int maxDisplay = nFound > 5 ? 5 : nFound; /* Don't overflow dialog */

        for (int playerIndex = 0; playerIndex < maxDisplay && row < 8; playerIndex++) {
            if (playerIndex == state->currentPlayer) {
                wattrset(dlg, A_REVERSE);
                mvwprintw(dlg, row++, 4, "%-40s", players[playerIndex]);
                wattrset(dlg, A_NORMAL);
            } else {
                mvwprintw(dlg, row++, 4, "%-40s", players[playerIndex]);
            }
        }
    } else {
        mvwprintw(dlg, row, 4, "(No known players found)");
    }
}

static void
updateAddress(CursesInviteState* state, const CurGameInfo* gi)
{
    CommsAddrRec* addr = &state->addr;

    /* Remove existing connection types */
    /* addr_rmType(addr, COMMS_CONN_MQTT); */
    /* addr_rmType(addr, COMMS_CONN_BT); */
    /* addr_rmType(addr, COMMS_CONN_SMS); */

    /* Add connection types based on filled fields using proper API methods */
    /* Only process fields for connection types that are enabled */
    if (types_hasType(gi->conTypes, COMMS_CONN_MQTT) && strlen(state->mqttField) > 0) {
        MQTTDevID devID;
        if ( strToMQTTCDevID( state->mqttField, &devID ) ) {
            addr_addMQTT(addr, &devID);
        }
    }

    if (types_hasType(gi->conTypes, COMMS_CONN_BT) && strlen(state->bluetoothField) > 0) {
        /* For Bluetooth, we assume the field contains the device name/address */
        /* Pass empty string for btAddr since we only have one field */
        addr_addBT(addr, state->bluetoothField, "");
    }

    if (types_hasType(gi->conTypes, COMMS_CONN_SMS) && strlen(state->smsField) > 0) {
        /* Use default port 0 for SMS */
        addr_addSMS(addr, state->smsField, 0);
    }
}

static void
launchQRInvite( CommonGlobals* cGlobals )
{
    LaunchParams* params = cGlobals->params;
    XWStreamCtxt* invite = gr_inviteUrl( params->dutil, cGlobals->gr,
                                         NULL_XWE, NULL, NULL );

    if ( !!invite ) {
        XP_U16 len = stream_getSize( invite );
        XP_UCHAR buf[len+1];
        stream_getBytes( invite, buf, len );
        buf[len] = '\0';
        cursesShowQRDialog( buf, "Game Invitation QR Code" );
        stream_destroy( invite );
    }
}

XP_Bool
cursesInviteDlg( CommonGlobals* cGlobals, CommsAddrRec* addrp, gint* nPlayers )
{
    LOG_FUNC();

    CursesInviteState state = {
        .nPlayersP = nPlayers,
        .selectedPlayers = *nPlayers,
        .cancelled = XP_FALSE,
        .currentTab = TAB_BY_ADDRESS,
        .currentField = 0,
        .currentPlayer = 0
    };

    /* Create dialog window */
    int height = 10;
    int width = 60;
    int yy, xx;
    getmaxyx(stdscr, yy, xx);

    WINDOW* dlg = newwin(height, width, 
                         (yy - height) / 2,
                         (xx - width) / 2);
    keypad(dlg, TRUE);

    /* Determine which connection types are available */
    typedef struct {
        CommsConnType connType;
        const char* label;
        char* field;
    } FieldInfo;

    FieldInfo availableFields[3];
    int numFields = 0;

    LaunchParams* params = cGlobals->params;
    XW_DUtilCtxt* dutil = params->dutil;
    const CurGameInfo* gi = gr_getGI(dutil, cGlobals->gr, NULL_XWE);
    if (types_hasType(gi->conTypes, COMMS_CONN_MQTT)) {
        availableFields[numFields++] = (FieldInfo){
            .connType = COMMS_CONN_MQTT,
            .label = "MQTT DevID",
            .field = state.mqttField
        };
    }

    if (types_hasType(gi->conTypes, COMMS_CONN_BT)) {
        availableFields[numFields++] = (FieldInfo){
            .connType = COMMS_CONN_BT,
            .label = "Bluetooth ",
            .field = state.bluetoothField
        };
    }

    if (types_hasType(gi->conTypes, COMMS_CONN_SMS)) {
        availableFields[numFields++] = (FieldInfo){
            .connType = COMMS_CONN_SMS,
            .label = "SMS Phone ",
            .field = state.smsField
        };
    }

    XP_Bool done = XP_FALSE;

    XP_Bool success = XP_FALSE;

    /* Check if any connection types are available */
    if (numFields > 0) {

        while (!done) {
            werase(dlg);
            box(dlg, 0, 0);

            mvwprintw(dlg, 0, 2, " Send Invitation ");

            /* Draw tabs */
            drawTabs(dlg, state.currentTab);

            int row = 3;

            if (state.currentTab == TAB_BY_ADDRESS) {
                mvwprintw(dlg, row++, 2, "Connection Settings:");
                row++;

                /* Display only available connection types */
                for (int fieldIndex = 0; fieldIndex < numFields; fieldIndex++) {
                    wattrset(dlg, state.currentField == fieldIndex ? A_REVERSE : A_NORMAL);
                    mvwprintw(dlg, row++, 4, "%s: %s", availableFields[fieldIndex].label, availableFields[fieldIndex].field);
                    wattrset(dlg, A_NORMAL);
                }
            } else {
                drawKnownPlayersTab(dlg, &state, cGlobals);
            }

            row = height - 3;
            mvwprintw(dlg, row++, 2, "Players: %d", state.selectedPlayers);

            if (state.currentTab == TAB_BY_ADDRESS) {
                mvwprintw(dlg, height - 2, 2, "Tab: Switch, Enter: Edit, Q: QR Code, O: OK, C: Cancel");
            } else {
                mvwprintw(dlg, height - 2, 2, "Tab: Switch, Enter: Select, Q: QR Code, O: OK, C: Cancel");
            }

            wrefresh(dlg);

            int ch = wgetch(dlg);

            switch (ch) {
            case '\t':
            case KEY_RIGHT:
            case KEY_LEFT:
                /* Switch tabs */
                state.currentTab = (state.currentTab + 1) % NUM_TABS;
                state.currentField = 0;
                state.currentPlayer = 0;
                break;

            case KEY_UP:
                if (state.currentTab == TAB_BY_ADDRESS) {
                    if (state.currentField > 0) state.currentField--;
                } else {
                    if (state.currentPlayer > 0) state.currentPlayer--;
                }
                break;

            case KEY_DOWN:
                if (state.currentTab == TAB_BY_ADDRESS) {
                    if (state.currentField < numFields - 1) state.currentField++;
                } else {
                    /* Count known players for proper bounds checking */
                    const XP_UCHAR* players[32];
                    XP_U16 nFound = VSIZE(players);
                    kplr_getNames(cGlobals->params->dutil, NULL_XWE, XP_TRUE, players, &nFound);
                    int maxDisplay = nFound > 5 ? 5 : nFound;
                    if (state.currentPlayer < maxDisplay - 1) state.currentPlayer++;
                }
                break;

            case '\n':
            case KEY_ENTER:
            case '\r':
                if (state.currentTab == TAB_BY_ADDRESS) {
                    /* Edit the selected field */
                    if (state.currentField < numFields) {
                        char prompt[64];
                        FieldInfo* field = &availableFields[state.currentField];

                        switch (field->connType) {
                        case COMMS_CONN_MQTT:
                            XP_SNPRINTF(prompt, sizeof(prompt), "Enter MQTT Device ID:");
                            break;
                        case COMMS_CONN_BT:
                            XP_SNPRINTF(prompt, sizeof(prompt), "Enter Bluetooth address:");
                            break;
                        case COMMS_CONN_SMS:
                            XP_SNPRINTF(prompt, sizeof(prompt), "Enter SMS phone number:");
                            break;
                        default:
                            XP_SNPRINTF(prompt, sizeof(prompt), "Enter value:");
                            break;
                        }

                        editField(dlg, prompt, field->field, 64, &state);
                    }
                } else {
                    /* Select known player and populate address fields */
                    const XP_UCHAR* players[32];
                    XP_U16 nFound = VSIZE(players);
                    kplr_getNames(cGlobals->params->dutil, NULL_XWE, XP_TRUE, players, &nFound);

                    if (state.currentPlayer < nFound) {
                        const XP_UCHAR* playerName = players[state.currentPlayer];
                        CommsAddrRec addr;
                        XP_U32 lastMod;

                        if (kplr_getAddr(cGlobals->params->dutil, NULL_XWE, playerName, &addr, &lastMod)) {
                            /* Copy address information to our fields */
                            if (addr_hasType(&addr, COMMS_CONN_MQTT)) {
                                formatMQTTDevID(&addr.u.mqtt.devID, state.mqttField, sizeof(state.mqttField));
                            }

                            if (addr_hasType(&addr, COMMS_CONN_BT)) {
                                XP_SNPRINTF(state.bluetoothField, sizeof(state.bluetoothField), "%s", 
                                           addr.u.bt.hostName);
                            }

                            if (addr_hasType(&addr, COMMS_CONN_SMS)) {
                                XP_SNPRINTF(state.smsField, sizeof(state.smsField), "%s", 
                                           addr.u.sms.phone);
                            }

                            /* Switch to address tab to show populated fields */
                            state.currentTab = TAB_BY_ADDRESS;
                            state.currentField = 0;
                        }
                    }
                }
                break;

            case 'q':
            case 'Q':
                /* Show QR code */
                launchQRInvite(cGlobals);
                break;

            case 'o':
            case 'O':
                /* OK - accept and send invitation */
                *nPlayers = state.selectedPlayers;
                XP_LOGFF( "set nPlayers: *nPlayers" );
                done = XP_TRUE;
                break;

            case 'c':
            case 'C':
            case 27: /* ESC */
                /* Cancel */
                state.cancelled = XP_TRUE;
                done = XP_TRUE;
                break;

            case '+':
                if (state.selectedPlayers < 4) {
                    state.selectedPlayers++;
                }
                break;

            case '-':
                if (state.selectedPlayers > 1) {
                    state.selectedPlayers--;
                }
                break;
            }
        }

        if (!state.cancelled) {
            updateAddress(&state, gi);
            *addrp = state.addr;
            success = XP_TRUE;
        }
    } else {
        LOG_RETURNF("%s", "FALSE - no connection types available");
    }

    delwin(dlg);
    LOG_RETURNF("%s", boolToStr(success));
    return success;
}

#endif /* PLATFORM_NCURSES */
