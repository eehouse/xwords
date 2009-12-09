//{{NO_DEPENDENCIES}}
// Microsoft Developer Studio generated include file.
// Used by xwords4.rc
//

#include "contypct.h"

// Stuff needed by C and .rc files...
// 'Doze expects a carraige return followed by a linefeed
#define XP_CR "\015\012"


#define IDS_APP_TITLE                   1
#define IDC_XWORDS4                     3
#define IDI_XWORDS4                     101
#define IDM_MENU                        102
#define IDD_SAVEDGAMESDLG               103
#define IDD_SAVENAMEDLG                 107
#define IDD_GAMEINFO                    104
#define IDD_STRBOX                      106
#define IDB_RIGHTARROW                  111
#define IDB_DOWNARROW                   112
#define IDD_ASKBLANK                    113
#define IDD_ASKPASS                     116
#define IDD_OPTIONSDLG                  117
#define IDD_COLORSDLG                   118
#define IDD_COLOREDITDLG                119
#define IDM_MAIN_MENUBAR                120
#define IDM_OKCANCEL_MENUBAR            121
#define IDM_DONE_MENUBAR                122
#define IDM_OK_MENUBAR                  123
#define IDB_ORIGIN                      124
#ifdef XWFEATURE_SEARCHLIMIT
# define IDD_ASKHINTLIMTS               125
#endif
#ifndef XWFEATURE_STANDALONE_ONLY
# define IDD_CONNSSDLG                  126
#endif
#ifdef ALLOW_CHOOSE_FONTS
# define IDD_FONTSSDLG                  127
#endif
#define IDD_LOCALESDLG                  128

#ifdef XWFEATURE_RELAY
# define IDB_NETARROW                    129
#endif

#define REMOTE_CHECK1                   1005
#define NAME_EDIT1                      1006
#define ROBOT_CHECK1                    1007
#define PASS_EDIT1                      1008

#define REMOTE_CHECK2                   1009
#define NAME_EDIT2                      1010
#define ROBOT_CHECK2                    1011
#define PASS_EDIT2                      1012

#define REMOTE_CHECK3                   1013
#define NAME_EDIT3                      1014
#define ROBOT_CHECK3                    1015
#define PASS_EDIT3                      1016

#define REMOTE_CHECK4                   1017
#define NAME_EDIT4                      1018
#define ROBOT_CHECK4                    1019
#define PASS_EDIT4                      1020

#define IDC_COMBO1                      1021
#define PLAYERNUM_COMBO                 1022
#define TIMER_CHECK                     1024
#define NAME_EDIT5                      1025
#define TIMER_EDIT                      1026
#define BLANKFACE_COMBO                 1029
#define IDC_PWDLABEL                    1031
#define PASS_EDIT                       1032
#define IDC_NPLAYERSLIST                1034
#define OPTIONS_BUTTON                  1035
#define IDC_RADIOGLOBAL                 1036
#define IDC_RADIOLOCAL                  1037
#define IDC_LEFTYCHECK                  1038
#define IDC_CHECKCOLORPLAYED            1039
#define IDC_CHECKSMARTROBOT             1040
#define IDC_CHECKHINTSOK                1041
#define IDC_CHECKSHOWCURSOR             1042
#define IDC_CHECKROBOTSCORES            1043
#define IDC_SKIPCONFIRM                 1044
#define IDC_HIDETILEVALUES              1045
#define IDC_PREFCOLORS                  1046
#define IDC_PREFLOCALE                  1047
#define IDC_PREFFONTS                   1048
#define PHONIES_LABEL                   1049
#define GIROLECONF_BUTTON               1050
#define GIJUGGLE_BUTTON                 1051
#define IDC_TOTAL_LABEL                 1052
#define IDC_REMOTE_LABEL                1053
#define IDC_PICKTILES                   1054
#define IDC_BPICK                       1055
#define IDC_PICKMSG                     1056
#ifdef FEATURE_TRAY_EDIT
# define IDC_CPICK                      1057
# define IDC_BACKUP                     1058
#endif
#ifdef XWFEATURE_SEARCHLIMIT
# define IDC_CHECKHINTSLIMITS           1059
#endif

/* buttons and lables must be parallel arrays so CLRSEL_LABEL_OFFSET
   works. */
#define DLBLTR_BUTTON                  1060
#define DBLWRD_BUTTON                  1061
#define TPLLTR_BUTTON                  1062
#define TPLWRD_BUTTON                  1063
#define EMPCELL_BUTTON                 1064
#define TBACK_BUTTON                   1065
#define FOCUSCLR_BUTTON                1066
#define PLAYER1_BUTTON                 1067
#define PLAYER2_BUTTON                 1068
#define PLAYER3_BUTTON                 1069
#define PLAYER4_BUTTON                 1070

#define DLBLTR_LABEL                   1071
#define DBLWRD_LABEL                   1072
#define TPLLTR_LABEL                   1073
#define TPLWRD_LABEL                   1074
#define EMPCELL_LABEL                  1075
#define TBACK_LABEL                    1076
#define FOCUSCLR_LABEL                 1077
#define PLAYER1_LABEL                  1078
#define PLAYER2_LABEL                  1079
#define PLAYER3_LABEL                  1080
#define PLAYER4_LABEL                  1081

#define DLBLTR_SAMPLE                  1082
#define DBLWRD_SAMPLE                  1083
#define TPLLTR_SAMPLE                  1084
#define TPLWRD_SAMPLE                  1085
#define EMPCELL_SAMPLE                 1086
#define TBACK_SAMPLE                   1087
#define FOCUSCLR_SAMPLE                1088
#define PLAYER1_SAMPLE                 1089
#define PLAYER2_SAMPLE                 1090
#define PLAYER3_SAMPLE                 1091
#define PLAYER4_SAMPLE                 1092

#define CLRSEL_LABEL_OFFSET (DLBLTR_LABEL-DLBLTR_BUTTON)

/* editor dlg: assumption is that the edit field's ID is one more
   than the corresponding slider's */
#ifdef MY_COLOR_SEL
# define CLREDT_SLIDER1                 1093
# define RED_EDIT                       1094
# define CLREDT_SLIDER2                 1095
# define GREEN_EDIT                     1096
# define CLREDT_SLIDER3                 1097
# define BLUE_EDIT                      1098

# define RED_LABEL                      1099
# define GREEN_LABEL                    1100
# define BLUE_LABEL                     1101
# define CLSAMPLE_BUTTON_ID             1123
#endif // MY_COLOR_SEL

#ifdef ALLOW_CHOOSE_FONTS
# define FONTS_LABEL                    1123
# define FONTS_COMBO                    1124
# define IDC_FONTSUPDOWN                1125
# define FONTS_COMBO_PPC                1126
# define FONTSIZE_LABEL                 1127
# define FONTSIZE_COMBO                 1128
# define IDC_FONTSIZEUPDOWN             1129
# define FONTSIZE_COMBO_PPC             1130
#endif

/* Dll/language picker */
#define LOCALES_COMBO                   1131
#define IDC_LOCALESUPDOWN               1132
#define LOCALES_COMBO_PPC               1133

#define IDC_ROLELABEL                   1134
#define IDC_NAMELABEL                   1135
#define IDC_ROBOTLABEL                  1136
#define IDC_PASSWDLABEL                 1137
#define HC_MIN_LABEL                    1138
#define HC_MAX_LABEL                    1139
#define LOCALES_LABEL                   1140

#ifdef NEEDS_CHOOSE_CONNTYPE
# define IDC_CCONVIA_LAB                 1106
#endif

#define IDC_INVITE_LAB                   1107
#ifdef XWFEATURE_RELAY
# define IDC_CRELAYNAME_LAB              1108
# define IDC_CRELAYPORT_LAB              1109

# define INVITE_EDIT                     1110
# define IDC_INVITE_HELP_HOST            1111
# define IDC_INVITE_HELP_GUEST           1112
# ifndef RELAY_NOEDIT_ADDR
#  define RELAYPORT_EDIT                 1113
#  define RELAYNAME_EDIT                 1114
# endif

#endif

#define IDC_BLUET_ADDR_LAB               1300
#ifdef XWFEATURE_BLUETOOTH
# define IDC_BLUET_ADDR_EDIT             1301
# define IDC_BLUET_ADDR_BROWSE           1302
#endif
/* #define IDS_UPDOWN                       1118 */

/* Direct IP connection */
# define IDC_IPNAME_LAB                  1303
# define IPNAME_EDIT                     1304
/* SMS connection */
# define IDC_SMS_PHONE_LAB               1305
# define IDC_SMS_PHONE_EDIT              1306
# define IDC_SMS_PORT_LAB                1307
# define IDC_SMS_PORT_EDIT               1308

#define IDC_SVGM_SELLAB                  1127
/* Let's remove these until they're implemented */
#define IDC_SVGM_EDITLAB                 1131
#define IDC_SVGM_CHANGE                  1130
#define IDC_SVGM_DUP                     1129
#define IDC_SVGM_DEL                     1128
#define IDC_SVGM_OPEN                    1120

#define IDC_SVGN_SELLAB                  1125
#define IDC_SVGN_EDIT                    1122


#define ID_FILE_EXIT                    40002
#define IDM_HELP_ABOUT                  40003
#define ID_FILE_ABOUT                   40004
#define ID_GAME_GAMEINFO                40005
#define ID_GAME_HISTORY                 40006
#define ID_GAME_FINALSCORES             40007
#define ID_GAME_TILECOUNTSANDVALUES     40008
#define ID_GAME_TILESLEFT               40009
#define ID_MOVE_HINT                    40010
#ifdef XWFEATURE_SEARCHLIMIT
# define ID_MOVE_LIMITEDHINT            40011
#endif
#define ID_MOVE_NEXTHINT                40012
#define ID_MOVE_UNDOCURRENT             40013
#define ID_MOVE_UNDOLAST                40014
#define ID_MOVE_TRADE                   40015
#define ID_MOVE_JUGGLE                  40016
#define ID_MOVE_HIDETRAY                40017
#define ID_MOVE_TURNDONE                40018
#define ID_MOVE_FLIP                    40019
#define ID_MOVE_VALUES                  40027
#define ID_FILE_NEWGAME                 40020
#define ID_FILE_SAVEDGAMES              40021
#define ID_EDITTEXT                     40022
#define ID_FILE_PREFERENCES             40023
#define ID_GAME_RESENDMSGS              40025
#define ID_FILE_FULLSCREEN              40026

#define ID_INITIAL_SOFTID               ID_MOVE_TURNDONE

#ifndef _WIN32_WCE
# define W32_DUMMY_ID                   40028
#endif

#define ID_COLORS_RES                   9999
#define ID_BONUS_RES                    9998
#define ID_DLLVERS_RES                  9997

#define IDM_MAIN_COMMAND1				40000
#define IDS_DUMMY                       40001

#include "strids.h"

// These are in sets of three, and must be consecutive and in the right order within each set
#define PHONIES_COMBO                   1200
#define IDC_PHONIESUPDOWN               1201 
#define PHONIES_COMBO_PPC               1202

#define HC_MIN_COMBO                    1203
#define HC_MIN_UPDOWN                   1204
#define HC_MIN_COMBO_PPC                1205

#define HC_MAX_COMBO                    1206
#define HC_MAX_UPDOWN                   1207
#define HC_MAX_COMBO_PPC                1208

#define IDC_SVGM_GAMELIST               1209
#define IDC_SVGM_UPDOWN                 1210
#define IDC_SVGM_GAMELIST_PPC           1211

#define BLANKFACE_LIST                  1212
#define IDC_ASKBLANK_UPDOWN             1213
#define BLANKFACE_LIST_PPC              1214

#define IDC_DICTLIST                    1215
#define IDC_DICTUPDOWN                  1216
#define IDC_DICTLIST_PPC                1217

#define IDC_NPLAYERSCOMBO               1218
#define IDC_NPLAYERSUPDOWN              1219
#define IDC_NPLAYERSCOMBO_PPC           1220

#ifndef XWFEATURE_STANDALONE_ONLY
# define IDC_ROLECOMBO                   1224
# define IDC_ROLEUPDOWN                  1225
# define IDC_ROLECOMBO_PPC               1226

# ifdef NEEDS_CHOOSE_CONNTYPE
#  define IDC_CONNECT_COMBO               1221
#  define IDC_CONNECTUPDOWN               1222
#  define IDC_CONNECT_COMBO_PPC           1223
# endif

#endif  /* XWFEATURE_STANDALONE_ONLY */

#define IDC_DICTLABEL                    1227
