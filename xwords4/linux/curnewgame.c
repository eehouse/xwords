/* 
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights reserved.
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


#ifdef PLATFORM_NCURSES

#include "curnewgame.h"
#include "cursesask.h"
#include "dbgutil.h"
#include "comtypes.h"
#include "comms.h"
#include "linuxmain.h"
#include <ncurses.h>
#include <ctype.h>


gboolean
curNewGameDialog( LaunchParams* params, CurGameInfo* gi,
                  CommsAddrRec* addr, XP_Bool isNewGame,
                  XP_Bool fireConnDlg )
{
    XP_USE(fireConnDlg);  /* Not implemented for ncurses */

    gboolean result = false;

    /* Set some reasonable defaults if this is a new game */
    if ( isNewGame ) {
        if ( 0 == gi->nPlayers ) {
            gi->nPlayers = 2;
        }
        if ( 0 == gi->boardSize ) {
            gi->boardSize = 15;  /* Standard crossword board */
        }
        if ( 0 == gi->traySize ) {
            gi->traySize = 7;    /* Standard Scrabble tile count */
        }
        if ( 0 == gi->bingoMin ) {
            gi->bingoMin = 7;    /* All tiles for bingo bonus */
        }

        /* Set up default players */
        for ( int ii = 0; ii < gi->nPlayers && ii < MAX_NUM_PLAYERS; ++ii ) {
            LocalPlayer* lp = &gi->players[ii];
            if ( 0 == strlen(lp->name) ) {
                if ( ii == 0 ) {
                    XP_STRNCPY( lp->name, "Human", MAX_PLAYERNAME_LEN );
                    lp->isLocal = XP_TRUE;
                    lp->robotIQ = 0;  /* Not a robot */
                } else {
                    XP_SNPRINTF( lp->name, MAX_PLAYERNAME_LEN, "Robot_%d", ii );
                    lp->isLocal = XP_TRUE;
                    lp->robotIQ = 1;  /* Smart robot */
                }
            }
        }

        /* Set default dictionary if not specified */
        if ( 0 == strlen(gi->dictName) ) {
            XP_STRNCPY( gi->dictName, "CollegeEng_2to8.xwd", MAX_DICTNAME_LEN );
        }
    }

    /* Interactive tabbed dialog for configuring game */
    int height = 20;
    int width = 70;
    int startx = (COLS - width) / 2;
    int starty = (LINES - height) / 2;

    WINDOW* dlg = newwin(height, width, starty, startx);
    if (!dlg) {
        return false;
    }

    keypad(dlg, TRUE);

    /* Define navigation fields */
    enum {
        FIELD_NUM_PLAYERS,
        FIELD_PLAYER1_NAME,
        FIELD_PLAYER1_TYPE,
        FIELD_PLAYER2_NAME, 
        FIELD_PLAYER2_TYPE,
        FIELD_PLAYER3_NAME,
        FIELD_PLAYER3_TYPE,
        FIELD_PLAYER4_NAME,
        FIELD_PLAYER4_TYPE,
        FIELD_BOARD_SIZE,
        FIELD_BUTTONS,
        FIELD_COUNT
    };

    int currentField = FIELD_NUM_PLAYERS;
    XP_Bool configuring = XP_TRUE;

    while (configuring) {
        /* Clear content area */
        werase(dlg);
        box(dlg, 0, 0);
        mvwprintw(dlg, 0, 2, " New Game Configuration ");

        /* Display game settings header */
        mvwprintw(dlg, 2, 2, "Board: %dx%d  Dictionary: %s", 
                  gi->boardSize, gi->boardSize, gi->dictName);

        /* Number of players field */
        int row = 4;
        if (currentField == FIELD_NUM_PLAYERS) {
            wattron(dlg, A_REVERSE);
        }
        mvwprintw(dlg, row, 2, "Number of players: %d [+/- to change]", gi->nPlayers);
        if (currentField == FIELD_NUM_PLAYERS) {
            wattroff(dlg, A_REVERSE);
        }
        row += 2;

        /* Player configuration fields */
        mvwprintw(dlg, row++, 2, "Players:");
        for (int ii = 0; ii < gi->nPlayers && ii < MAX_NUM_PLAYERS; ii++) {
            LocalPlayer* lp = &gi->players[ii];

            /* Player name field */
            if (currentField == FIELD_PLAYER1_NAME + (ii * 2)) {
                wattron(dlg, A_REVERSE);
            }
            mvwprintw(dlg, row, 4, "%d. Name: %-15s", ii + 1, lp->name);
            if (currentField == FIELD_PLAYER1_NAME + (ii * 2)) {
                wattroff(dlg, A_REVERSE);
            }

            /* Player type field */
            if (currentField == FIELD_PLAYER1_TYPE + (ii * 2)) {
                wattron(dlg, A_REVERSE);
            }
            const char* typeStr;
            if (!lp->isLocal) {
                typeStr = "Remote";
            } else if (lp->robotIQ > 0) {
                typeStr = "Robot";
            } else {
                typeStr = "Human";
            }
            mvwprintw(dlg, row, 35, "Type: %s", typeStr);
            if (currentField == FIELD_PLAYER1_TYPE + (ii * 2)) {
                wattroff(dlg, A_REVERSE);
            }
            row++;
        }

        /* Board size field */
        row++;
        if (currentField == FIELD_BOARD_SIZE) {
            wattron(dlg, A_REVERSE);
        }
        mvwprintw(dlg, row, 2, "Board size: %dx%d", gi->boardSize, gi->boardSize);
        if (currentField == FIELD_BOARD_SIZE) {
            wattroff(dlg, A_REVERSE);
        }
        row += 2;

        /* Action buttons */
        if (currentField == FIELD_BUTTONS) {
            wattron(dlg, A_REVERSE);
        }
        mvwprintw(dlg, row, 2, "[S]tart Game   [C]ancel");
        if (currentField == FIELD_BUTTONS) {
            wattroff(dlg, A_REVERSE);
        }

        /* Instructions */
        mvwprintw(dlg, height - 2, 2, "TAB/↑↓: Navigate  ENTER: Edit inline  SPACE: Toggle  +/-: Change count");

        wrefresh(dlg);

        int ch = wgetch(dlg);
        switch (ch) {
            case '\t': /* Tab - next field */
            case KEY_DOWN:
                do {
                    currentField = (currentField + 1) % FIELD_COUNT;
                    /* Skip player fields that don't exist */
                    if ((currentField == FIELD_PLAYER2_NAME || currentField == FIELD_PLAYER2_TYPE) && gi->nPlayers < 2) continue;
                    if ((currentField == FIELD_PLAYER3_NAME || currentField == FIELD_PLAYER3_TYPE) && gi->nPlayers < 3) continue;
                    if ((currentField == FIELD_PLAYER4_NAME || currentField == FIELD_PLAYER4_TYPE) && gi->nPlayers < 4) continue;
                    break;
                } while (1);
                break;

            case KEY_BTAB: /* Shift+Tab - previous field */
            case KEY_UP:
                do {
                    currentField = (currentField - 1 + FIELD_COUNT) % FIELD_COUNT;
                    /* Skip player fields that don't exist */
                    if ((currentField == FIELD_PLAYER2_NAME || currentField == FIELD_PLAYER2_TYPE) && gi->nPlayers < 2) continue;
                    if ((currentField == FIELD_PLAYER3_NAME || currentField == FIELD_PLAYER3_TYPE) && gi->nPlayers < 3) continue;
                    if ((currentField == FIELD_PLAYER4_NAME || currentField == FIELD_PLAYER4_TYPE) && gi->nPlayers < 4) continue;
                    break;
                } while (1);
                break;

            case '\r':
            case '\n':
            case KEY_ENTER:
                if (currentField == FIELD_NUM_PLAYERS) {
                    /* Edit number of players inline */
                    mvwprintw(dlg, 4, 2, "Number of players: [  ] (1-4, then ENTER)");
                    wmove(dlg, 4, 20);
                    wrefresh(dlg);
                    echo();
                    curs_set(1);

                    char numBuffer[4] = "";
                    int pos = 0;
                    int editCh;
                    while ((editCh = wgetch(dlg)) != '\r' && editCh != '\n' && editCh != 27) { /* ESC to cancel */
                        if (editCh >= '1' && editCh <= '4' && pos == 0) {
                            numBuffer[pos++] = editCh;
                            numBuffer[pos] = '\0';
                            mvwaddch(dlg, 4, 20, editCh);
                            wrefresh(dlg);
                        } else if ((editCh == KEY_BACKSPACE || editCh == 127) && pos > 0) {
                            pos--;
                            numBuffer[pos] = '\0';
                            mvwaddch(dlg, 4, 20, ' ');
                            wmove(dlg, 4, 20);
                            wrefresh(dlg);
                        }
                    }

                    noecho();
                    curs_set(0);

                    if (editCh != 27 && pos > 0) { /* Not cancelled and has input */
                        int newNum = atoi(numBuffer);
                        if (newNum >= 1 && newNum <= MAX_NUM_PLAYERS) {
                            /* Initialize any new players */
                            for (int ii = gi->nPlayers; ii < newNum; ii++) {
                                LocalPlayer* lp = &gi->players[ii];
                                if (ii == 0) {
                                    XP_STRNCPY(lp->name, "Human", MAX_PLAYERNAME_LEN);
                                    lp->isLocal = XP_TRUE;
                                    lp->robotIQ = 0; /* Default first player to human */
                                } else {
                                    XP_SNPRINTF(lp->name, MAX_PLAYERNAME_LEN, "Robot_%d", ii + 1);
                                    lp->isLocal = XP_TRUE;
                                    lp->robotIQ = 1; /* Default other players to robot */
                                }
                            }
                            gi->nPlayers = newNum;
                        }
                    }
                } else if (currentField >= FIELD_PLAYER1_NAME && currentField <= FIELD_PLAYER4_NAME) {
                    /* Edit player name inline */
                    int playerIndex = (currentField - FIELD_PLAYER1_NAME) / 2;
                    if (playerIndex < gi->nPlayers) {
                        int nameRow = 7 + playerIndex;
                        mvwprintw(dlg, nameRow, 4, "%d. Name: [               ] (ESC to cancel)", playerIndex + 1);
                        wmove(dlg, nameRow, 13);
                        wrefresh(dlg);
                        echo();
                        curs_set(1);

                        char nameBuffer[MAX_PLAYERNAME_LEN + 1];
                        XP_STRNCPY(nameBuffer, gi->players[playerIndex].name, sizeof(nameBuffer));

                        /* Show current name */
                        int nameLen = strlen(nameBuffer);
                        mvwprintw(dlg, nameRow, 13, "%-15s", nameBuffer);
                        wmove(dlg, nameRow, 13 + nameLen);
                        wrefresh(dlg);

                        int pos = nameLen;
                        int editCh;
                        while ((editCh = wgetch(dlg)) != '\r' && editCh != '\n' && editCh != 27) { /* ESC to cancel */
                            if (editCh >= 32 && editCh <= 126 && pos < MAX_PLAYERNAME_LEN - 1) { /* Printable chars */
                                nameBuffer[pos++] = editCh;
                                nameBuffer[pos] = '\0';
                                mvwprintw(dlg, nameRow, 13, "%-15s", nameBuffer);
                                wmove(dlg, nameRow, 13 + pos);
                                wrefresh(dlg);
                            } else if ((editCh == KEY_BACKSPACE || editCh == 127) && pos > 0) {
                                pos--;
                                nameBuffer[pos] = '\0';
                                mvwprintw(dlg, nameRow, 13, "%-15s", nameBuffer);
                                wmove(dlg, nameRow, 13 + pos);
                                wrefresh(dlg);
                            }
                        }

                        noecho();
                        curs_set(0);

                        if (editCh != 27) { /* Not cancelled */
                            XP_STRNCPY(gi->players[playerIndex].name, nameBuffer, MAX_PLAYERNAME_LEN);
                        }
                    }
                } else if (currentField == FIELD_BUTTONS) {
                    /* Start game */
                    result = XP_TRUE;
                    configuring = XP_FALSE;
                }
                break;

            case ' ': /* Space - toggle values */
                if (currentField >= FIELD_PLAYER1_TYPE && currentField <= FIELD_PLAYER4_TYPE) {
                    /* Cycle through Human → Robot → Remote */
                    int playerIndex = (currentField - FIELD_PLAYER1_TYPE) / 2;
                    if (playerIndex < gi->nPlayers) {
                        LocalPlayer* lp = &gi->players[playerIndex];
                        if (lp->isLocal && lp->robotIQ == 0) {
                            /* Human → Robot */
                            lp->robotIQ = 1;
                            XP_SNPRINTF(lp->name, MAX_PLAYERNAME_LEN, "Robot_%d", playerIndex + 1);
                        } else if (lp->isLocal && lp->robotIQ > 0) {
                            /* Robot → Remote */
                            lp->isLocal = XP_FALSE;
                            lp->robotIQ = 0;
                            XP_SNPRINTF(lp->name, MAX_PLAYERNAME_LEN, "Remote_%d", playerIndex + 1);
                        } else {
                            /* Remote → Human */
                            lp->isLocal = XP_TRUE;
                            lp->robotIQ = 0;
                            if (playerIndex == 0) {
                                XP_STRNCPY(lp->name, "Human", MAX_PLAYERNAME_LEN);
                            } else {
                                XP_SNPRINTF(lp->name, MAX_PLAYERNAME_LEN, "Player_%d", playerIndex + 1);
                            }
                        }
                    }
                }
                break;

            case '+':
                if (currentField == FIELD_NUM_PLAYERS && gi->nPlayers < MAX_NUM_PLAYERS) {
                    gi->nPlayers++;
                    /* Initialize new player */
                    LocalPlayer* lp = &gi->players[gi->nPlayers - 1];
                    XP_SNPRINTF(lp->name, MAX_PLAYERNAME_LEN, "Robot_%d", gi->nPlayers);
                    lp->isLocal = XP_TRUE;
                    lp->robotIQ = 1; /* Default to robot */
                } else if (currentField == FIELD_BOARD_SIZE && gi->boardSize < 21) {
                    gi->boardSize++;
                }
                break;

            case '-':
                if (currentField == FIELD_NUM_PLAYERS && gi->nPlayers > 1) {
                    gi->nPlayers--;
                    /* If current field is beyond available players, move to valid field */
                    while ((currentField >= FIELD_PLAYER2_NAME && gi->nPlayers < 2) ||
                           (currentField >= FIELD_PLAYER3_NAME && gi->nPlayers < 3) ||
                           (currentField >= FIELD_PLAYER4_NAME && gi->nPlayers < 4)) {
                        currentField = (currentField - 1 + FIELD_COUNT) % FIELD_COUNT;
                    }
                } else if (currentField == FIELD_BOARD_SIZE && gi->boardSize > 9) {
                    gi->boardSize--;
                }
                break;

            case 's':
            case 'S':
                result = XP_TRUE;
                configuring = XP_FALSE;
                break;

            case 'c':
            case 'C':
            case 27: /* ESC */
                result = XP_FALSE;
                configuring = XP_FALSE;
                break;
        }
    }

    delwin(dlg);

    /* If accepted, set appropriate device role based on player configuration */
    if ( result ) {
        XP_Bool hasRemotePlayers = XP_FALSE;

        /* Check if any players are remote */
        for ( int ii = 0; ii < gi->nPlayers && ii < MAX_NUM_PLAYERS; ++ii ) {
            if ( !gi->players[ii].isLocal ) {
                hasRemotePlayers = XP_TRUE;
                break;
            }
        }

        /* Set device role: standalone for all-local games, host for games with remote players */
        gi->deviceRole = hasRemotePlayers ? ROLE_ISHOST : ROLE_STANDALONE;

        /* Set up communication address if needed for network games */
        if ( hasRemotePlayers && NULL != addr ) {
            makeSelfAddress( addr, params );

            /* Copy connection type from address to game info */
            CommsConnType typ;
            for ( XP_U32 state = 0; addr_iter( addr, &typ, &state ); ) {
                types_addType( &gi->conTypes, typ );
            }
        }

        /* Ensure game name is set */
        if ( 0 == strlen(gi->gameName) ) {
            XP_SNPRINTF( gi->gameName, MAX_GAMENAME_LEN, "Game_%lu", 
                        (unsigned long)time(NULL) );
        }
    }

    LOG_GI( gi, __func__);
    logAddr( params->dutil, addr, __func__ );
    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

#endif /* PLATFORM_NCURSES */
