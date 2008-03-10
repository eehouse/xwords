//{{NO_DEPENDENCIES}}
// Microsoft Developer Studio generated include file.
// Used by xwords4.rc
//
#define IDS_APP_TITLE                   1
#define IDC_XWORDS4                     3
#define IDI_XWORDS4                     101
#define IDM_MENU                        102
#define IDD_ABOUTBOX                    103
#define IDD_DIALOG1                     104
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
#define IDB_ORIGIN                      124
#ifdef XWFEATURE_SEARCHLIMIT
# define IDD_ASKHINTLIMTS               125
#endif
#ifndef XWFEATURE_STANDALONE_ONLY
# define IDD_CONNSSDLG                  126
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
#define IDC_NPLAYERSCOMBO               1023
#define TIMER_CHECK                     1024
#define NAME_EDIT5                      1025
#define TIMER_EDIT                      1026
#define IDC_DICTCOMBO                   1027
#define IDC_COMBO3                      1028
#define BLANKFACE_COMBO                 1029
#define PHONIES_COMBO                   1030
#define IDC_PWDLABEL                    1031
#define PASS_EDIT                       1032
#define BLANKFACE_LIST                  1033
#define IDC_NPLAYERSLIST                1034
#define OPTIONS_BUTTON                  1035
#define IDC_RADIOGLOBAL                 1036
#define IDC_RADIOLOCAL                  1037
#define IDC_LEFTYCHECK                  1038
#define IDC_CHECKCOLORPLAYED            1039
#define IDC_CHECKSMARTROBOT             1040
#define IDC_CHECKNOHINTS                1041
#define IDC_CHECKSHOWCURSOR             1042
#define IDC_CHECKROBOTSCORES            1043
#define IDC_PREFCOLORS                  1044
#define PHONIES_LABEL                   1045
#define IDC_ROLECOMBO                   1046
#define GIJUGGLE_BUTTON                 1048
#define IDC_TOTAL_LABEL                 1049
#define IDC_REMOTE_LABEL                1050
#define IDC_PICKTILES                   1051
#define IDC_BPICK                       1052
#define IDC_PICKMSG                     1053
#ifdef FEATURE_TRAY_EDIT
# define IDC_CPICK                      1054
# define IDC_PICKALL                    1055
# define IDC_BACKUP                     1056
#endif
#ifdef XWFEATURE_SEARCHLIMIT
# define IDC_CHECKHINTSLIMITS           1057
#endif

/* buttons and lables must be parallel arrays so CLRSEL_LABEL_OFFSET
   works. */
#define DLBLTR_BUTTON                  1058
#define DBLWRD_BUTTON                  1059
#define TPLLTR_BUTTON                  1060
#define TPLWRD_BUTTON                  1061
#define EMPCELL_BUTTON                 1062
#define TBACK_BUTTON                   1063
#define FOCUSCLR_BUTTON                1064
#define PLAYER1_BUTTON                 1065
#define PLAYER2_BUTTON                 1066
#define PLAYER3_BUTTON                 1067
#define PLAYER4_BUTTON                 1068

#define DLBLTR_LABEL                   1069
#define DBLWRD_LABEL                   1070
#define TPLLTR_LABEL                   1071
#define TPLWRD_LABEL                   1072
#define EMPCELL_LABEL                  1073
#define TBACK_LABEL                    1074
#define FOCUSCLR_LABEL                 1075
#define PLAYER1_LABEL                  1076
#define PLAYER2_LABEL                  1077
#define PLAYER3_LABEL                  1078
#define PLAYER4_LABEL                  1079

#define CLRSEL_LABEL_OFFSET (DLBLTR_LABEL-DLBLTR_BUTTON)

/* editor dlg: assumption is that the edit field's ID is one more
   than the corresponding slider's */
#ifdef MY_COLOR_SEL
# define CLREDT_SLIDER1                 1080
# define RED_EDIT                       1081
# define CLREDT_SLIDER2                 1082
# define GREEN_EDIT                     1083
# define CLREDT_SLIDER3                 1084
# define BLUE_EDIT                      1085

# define RED_LABEL                      1086
# define GREEN_LABEL                    1087
# define BLUE_LABEL                     1088
#endif // MY_COLOR_SEL

#define HC_MIN_COMBO                    1089
#define HC_MAX_COMBO                    1090

#define IDC_CCONVIA_LAB                 1091

#define IDC_COOKIE_LAB                   1092
#ifdef XWFEATURE_RELAY
# define IDC_CRELAYNAME_LAB              1093
# define IDC_CRELAYPORT_LAB              1094
# define IDC_CRELAYHINT_LAB              1095

# define IDC_CONNECTCOMBO                1096
# define RELAYNAME_EDIT                  1097
# define RELAYPORT_EDIT                  1098
# define COOKIE_EDIT                     1099

#endif

#define IDC_BLUET_ADDR_LAB               1100
#ifdef XWFEATURE_BLUETOOTH
# define IDC_BLUET_ADDR_EDIT             1101
# define IDC_BLUET_ADDR_BROWSE           1102
#endif



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

#define IDM_MAIN_COMMAND1				40001
#define IDS_MENU                        40002
#define IDS_DUMMY                       40003
#define IDS_CANCEL                      40004
#define IDS_OK                          40005

// Don't use the numbers after 4009: one string needs not to be there
// to stop the progression in cedict.c 
#define IDS_DICTDIRS                    40009

// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        125
#define _APS_NEXT_COMMAND_VALUE         40029
#define _APS_NEXT_CONTROL_VALUE         1087
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif

