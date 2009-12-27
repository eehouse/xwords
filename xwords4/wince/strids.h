/* -*- mode: c; -*- */

/* Don't change these casually.  .exe and .dll must agree on them.  Also, they
 * should be kept sequential since an array of length
 * CE_LAST_RES_ID-CE_FIRST_RES_ID is allocated in ceresstr.c.
 */

#ifndef _STRIDS_H_
#define _STRIDS_H_

/* CUR_DLL_VERSION is included in all .dll resources and can be used to check
 * whether a given resource can be trusted.  Up it when changing the order of
 * strings.  Which do only when unavoidable.
 */
#define CUR_DLL_VERSION 0x0003


#define CE_FIRST_RES_ID                                              40002

#define IDS_MENU                                           CE_FIRST_RES_ID
#define IDS_CANCEL                                                   40003
#define IDS_OK                                                       40004
#define IDS_ABOUT                                                    40005
#define IDS_DONE                                                     40006
#define IDS_LANGUAGE_NAME                                            40007
#define IDS_NEW_GAME                                                 40008
#define IDS_DICTLOC                                                  40009
#define IDS_SAVENAME                                                 40010
#define IDS_DUPENAME                                                 40011
#define IDS_RENAME                                                   40012
#define IDS_REMAINING_TILES_ADD                                      40013
#define IDS_UNUSED_TILES_SUB                                         40014
#define IDS_BONUS_ALL                                                40015
#define IDS_TURN_SCORE                                               40016
#define IDS_COMMIT_CONFIRM                                           40017
#define IDS_LOCAL_NAME                                               40018
#define IDS_IGNORE_L                                                 40019
#define IDS_WARN_L                                                   40020
#define IDS_DISALLOW_L                                               40021
#define IDS_NONLOCAL_NAME                                            40022
#define IDS_TIME_PENALTY_SUB                                         40023
#define IDS_CUMULATIVE_SCORE                                         40024
#define IDS_MOVE_ACROSS                                              40025
#define IDS_MOVE_DOWN                                                40026
#define IDS_TRAY_AT_START                                            40027
#define IDS_NEW_TILES                                                40028
#define IDS_TRADED_FOR                                               40029
#define IDS_PASS                                                     40030
#define IDS_PHONY_REJECTED                                           40031
#define IDS_ROBOT_TRADED                                             40032
#define IDS_ROBOT_MOVED                                              40033
#define IDS_REMOTE_MOVED                                             40034
#define IDS_PASSED                                                   40035
#define IDS_REMTILES_L                                               40036
#define IDS_SUMMARYSCORED                                            40037
#define IDS_TRADED                                                   40038
#define IDS_LOSTTURN                                                 40039
#define IDS_TOTALPLAYERS                                             40040
#define IDS_VALUES_HEADER                                            40041
#define IDS_TILES_NOT_IN_LINE                                        40042
#define IDS_NO_EMPTIES_IN_TURN                                       40043
#define IDS_TWO_TILES_FIRST_MOVE                                     40044
#define IDS_TILES_MUST_CONTACT                                       40045
#define IDS_NOT_YOUR_TURN                                            40046
#define IDS_NO_PEEK_ROBOT_TILES                                      40047
#define IDS_CANT_TRADE_MID_MOVE                                      40048
#define IDS_TOO_FEW_TILES_LEFT_TO_TRADE                              40049
#define IDS_CANT_UNDO_TILEASSIGN                                     40050
#define IDS_CANT_HINT_WHILE_DISABLED                                 40051
#define IDS_QUERY_TRADE                                              40052
#define IDS_DOUBLE_LETTER                                            40053
#define IDS_DOUBLE_WORD                                              40054
#define IDS_TRIPLE_LETTER                                            40055
#define IDS_TRIPLE_WORD                                              40056
#define IDS_INTRADE_MW                                               40057
#define IDS_COUNTSVALS_L                                             40058
#define IDS_GAMEHIST_L                                               40059
#define IDS_FINALSCORE_L                                             40060
#define IDS_QUESTION_L                                               40061
#define IDS_FYI_L                                                    40062
#define IDS_ILLEGALWRD_L                                             40063
#define IDS_WRDNOTFOUND                                              40064
#define IDS_USEANYWAY                                                40065
#define IDS_CANNOTOPEN_GAME                                          40066
#define IDS_NODICT_L                                                 40067
#define IDS_ABOUT_L                                                  40068
#define IDS_OVERWRITE                                                40069
#define IDS_ENDNOW                                                   40070
#define IDS_CANNOTOPEN_DICT                                          40071
#define IDS_CONFIM_DELETE                                            40072
#define IDS_ROLE_STANDALONE                                          40073
#define IDS_ROLE_HOST                                                40074
#define IDS_ROLE_GUEST                                               40075
#define IDS_PLAYER_FORMAT                                            40076
#define IDS_UNTITLED_FORMAT                                          40077
#define IDS_PASSWDFMT_L                                              40078
#define IDS_FILEEXISTSFMT_L                                          40079
#define IDS_NEED_TOUCH                                               40080
#define IDS_EDITCOLOR_FORMAT                                         40081
#define IDS_LANG_CHANGE_RESTART                                      40082

#ifndef XWFEATURE_STANDALONE_ONLY
# define IDS_CONN_RELAY_L                                            40083
# define IDS_CONN_DIRECT                                             40084
# define IDS_LOCALPLAYERS                                            40085
# define IDS_NO_PEEK_REMOTE_TILES                                    40086
# define IDS_REG_UNEXPECTED_USER                                     40087
# define IDS_SERVER_DICT_WINS                                        40088
# define IDS_REG_SERVER_SANS_REMOTE                                  40089
# define IDS_RESEND_STANDALONE                                       40090
# define IDS_PHONE_OFF                                               40091
# define IDS_NETWORK_FAILED                                          40092


# ifdef XWFEATURE_SMS
#  define IDS_SMS_CONN_L                                             40093
# endif

# ifdef XWFEATURE_IP_DIRECT
#  define IDS_DIRECT_CONN_L                                          40094
# endif

# ifdef XWFEATURE_RELAY
#  define IDS_XWRELAY_ERROR_TIMEOUT                                  40095
#  define IDS_ERROR_HEART_YOU                                        40096
#  define IDS_XWRELAY_ERROR_HEART_OTHER                              40097
#  define IDS_XWRELAY_ERROR_LOST_OTHER                               40098
#  define IDS_XWRELAY_RELAY_INCOMPAT                                 40099
#  define IDS_RELAY_ALLHERE                                          40100
#  define IDS_RELAY_HOST_WAITINGD                                    40101
#  define IDS_RELAY_GUEST_WAITINGD                                   40102
#  define IDS_ERROR_NO_ROOM                                          40103
#  define IDS_ERROR_DUP_ROOM                                         40104
#  define IDS_ERROR_TOO_MANY                                         40105
# endif
#endif

#if ! defined XWFEATURE_STANDALONE_ONLY
# define CE_LAST_RES_ID                                              40106
#else
# define CE_LAST_RES_ID                                              40082
#endif

#endif
