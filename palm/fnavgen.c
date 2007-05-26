/* -*- mode: c; -*- */

#include <stdio.h>
#include <assert.h>

#include "xwords4defines.h"

typedef struct fnavElem {
  unsigned short objectID;
  unsigned short objectFlags;
  unsigned short objectAbove;
  unsigned short objectBelow;
} fnavElem;

typedef struct fnavHeader {
  unsigned short formID;        /* not part of resource!!!! */

  unsigned short version;           /* always 1 */
  unsigned short objcount;
  unsigned short headerSizeInBytes; /* always 20 */
  unsigned short listSizeInBytes;   /* always 8 */
  unsigned short navflags;
  unsigned short initialHint;
  unsigned short jumpToHint;
  unsigned short bottomLeftHint;
} fnavHeader;


fnavHeader gamesHeader = {
  XW_NEWGAMES_FORM,
  0,                            /* fill this in */
  0,                            /* fill this in */
  0,                            /* fill this in */
  0,                            /* fill this in */
  0x0001,                       /* force obj focus mode */
  0,
  0,
  0,
};

fnavElem gamesElems[] = {
  { XW_ROBOT_1_CHECKBOX_ID, 0, 0,                      XW_ROBOT_2_CHECKBOX_ID },
  { XW_ROBOT_2_CHECKBOX_ID, 0, XW_ROBOT_1_CHECKBOX_ID, XW_ROBOT_3_CHECKBOX_ID },
  { XW_ROBOT_3_CHECKBOX_ID, 0, XW_ROBOT_2_CHECKBOX_ID, XW_ROBOT_4_CHECKBOX_ID },
  { XW_ROBOT_4_CHECKBOX_ID, 0, XW_ROBOT_3_CHECKBOX_ID, XW_SOLO_GADGET_ID },

  { XW_SOLO_GADGET_ID,      0, XW_ROBOT_4_CHECKBOX_ID, XW_SERVER_GADGET_ID },
  { XW_SERVER_GADGET_ID,    0, XW_SOLO_GADGET_ID,      XW_CLIENT_GADGET_ID },
  { XW_CLIENT_GADGET_ID,    0, XW_SERVER_GADGET_ID,    0 }
};

unsigned short gamesElemsShort[] = {

#ifndef XWFEATURE_STANDALONE_ONLY
  XW_SOLO_GADGET_ID,     
  XW_SERVER_GADGET_ID,   
  XW_CLIENT_GADGET_ID,   
#endif

  XW_NPLAYERS_SELECTOR_ID,
  XW_PREFS_BUTTON_ID,

  XW_REMOTE_1_CHECKBOX_ID,
  XW_PLAYERNAME_1_FIELD_ID,
  XW_ROBOT_1_CHECKBOX_ID,
  XW_PLAYERPASSWD_1_TRIGGER_ID,

  XW_REMOTE_2_CHECKBOX_ID,
  XW_PLAYERNAME_2_FIELD_ID,
  XW_ROBOT_2_CHECKBOX_ID,
  XW_PLAYERPASSWD_2_TRIGGER_ID,

  XW_REMOTE_3_CHECKBOX_ID,
  XW_PLAYERNAME_3_FIELD_ID,
  XW_ROBOT_3_CHECKBOX_ID,
  XW_PLAYERPASSWD_3_TRIGGER_ID,

  XW_REMOTE_4_CHECKBOX_ID,
  XW_PLAYERNAME_4_FIELD_ID,
  XW_ROBOT_4_CHECKBOX_ID,
  XW_PLAYERPASSWD_4_TRIGGER_ID,

  XW_DICT_SELECTOR_ID,
  XW_CANCEL_BUTTON_ID,
  XW_OK_BUTTON_ID

};

static void
usage( char* name )
{
  fprintf( stderr, "usage: %s outfile\n", name );
  exit( 1 );
} /*  */

static void
write_network_short( FILE* fil, unsigned short s )
{
  unsigned short tmp = htons( s );
  fwrite( &tmp, sizeof(tmp), 1, fil );
} /* write_network_short */

write_fnav( fnavHeader* header, unsigned short* idArray, 
            fnavElem* elems, int count )
{
  char nameBuf[32];
  FILE* fil;
  int i;

  assert( !idArray || !elems );

  sprintf( nameBuf, "fnav%.4x.bin", header->formID );
  fil = fopen( nameBuf, "w" );
  fprintf( stderr, "created file %s\n", nameBuf );

  write_network_short( fil, 1 );
  write_network_short( fil, count );
  write_network_short( fil, 20 );
  write_network_short( fil, 8 );
  write_network_short( fil, header->navflags );
  write_network_short( fil, header->initialHint );
  write_network_short( fil, header->jumpToHint );
  write_network_short( fil, header->bottomLeftHint );

  /* Two words of padding.  Docs disagree, but Blazer's resources have
     'em */
  write_network_short( fil, 0 );
  write_network_short( fil, 0 );

  if ( !!elems ) {

    for ( i = 0; i < count; ++i ) {
      write_network_short( fil, elems->objectID );
      write_network_short( fil, elems->objectFlags );
      write_network_short( fil, elems->objectAbove );
      write_network_short( fil, elems->objectBelow );

      ++elems;
    }

  } else {
    unsigned short prevID = 0;
    unsigned short id = *idArray++;

    while ( count-- ) {
      unsigned short nextID;

      if ( count == 0 ) {
        nextID = 0;
      } else {
        nextID = *idArray++;
      }

      write_network_short( fil, id );
      write_network_short( fil, 0 );
      write_network_short( fil, prevID );
      write_network_short( fil, nextID );

      if ( !nextID ) {
        break;
      }
    
      prevID = id;
      id = nextID;
    }
  }

  fclose(fil);
} /* write_fnav */

int 
main( int argc, char** argv )
{
  int i;
  char* outFName;

  if ( argc != 1 ) {
    usage( argv[0] );
  }

  write_fnav( &gamesHeader, gamesElemsShort, NULL, 
              VSIZE(gamesElemsShort) );
/*               sizeof(gamesElems)/sizeof(gamesElems[0]) ); */


} /* main */
