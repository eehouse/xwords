/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999 - 2002 by Eric House (fixin@peak.org).  All rights reserved.
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


/* Looks like I still need this??? */
void*
memcpy( void* dest, const void* src, unsigned long n )
{
    void* oldDest = dest;
    unsigned char* d = (unsigned char*)dest;
    unsigned char* s = (unsigned char*)src;

    FUNC_HEADER(memcpy);

    if ( dest < src ) {
        while ( n-- > 0 ) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while ( n-- > 0 ) {
            *--d = *--s;
        }
    }

    FUNC_TAIL(memcpy);
    return oldDest;
} /* memcpy */

unsigned long
Byte_Swap32( unsigned long l )
{
    unsigned long result;
    result  = ((l >> 24) & 0xFF);
    result |= ((l >> 8) & 0xFF00);
    result |= ((l << 8) & 0xFF0000);
    result |= ((l << 24) & 0xFF000000);
    return result;
}

unsigned short
Byte_Swap16( unsigned short l )
{
    unsigned short result;
    result  = ((l >> 8) & 0xFF);
    result |= ((l << 8) & 0xFF00);
    return result;
}

void
write_unaligned32( unsigned char* dest, unsigned long val )
{
    int i;
    for ( i = 0; i < 4; ++i ) {
        *dest++ = val & 0x000000FF;
        val >>= 8;
    }
} /* write_unaligned32 */

void
write_unaligned16( unsigned char* dest, unsigned short val )
{
    int i;
    for ( i = 0; i < 2; ++i ) {
        *dest++ = val & 0x00FF;
        val >>= 8;
    }
} /* write_unaligned32 */

/* Need to parse the format string */
Int16
StrPrintF( Char* s, const Char* formatStr, ... )
{
/*     char* str = (char*)formatStr; */
/*     int isLong, useArg, innerDone, done; */
    unsigned long* inArgs = ((unsigned long*)&formatStr) + 1;
/*     int offset = 0; */
    // unsigned char args_68K[48];     /* the va_args stack; 12 params max */

/*     make68KArgs( formatStr, inArgs, args_68K ); */

    return StrVPrintF( s, formatStr, inArgs );

    return 0;
} /* StrPrintF */

/* from file StringMgr.h */
Int16
StrVPrintF( Char* s, const Char* formatStr, _Palm_va_list arg )
{
    Int16 result;
    unsigned long* argv_arm = (unsigned long*)arg;
    unsigned char argv_68k[48];
    unsigned short done, isLong, innerDone, useArg;
    unsigned char* str = (unsigned char*)formatStr;
    unsigned short offset = 0;

    for ( done = 0; !done; ) {
        switch( *str++ ) {
        case '\0':
            done = 1; 
            break;
        case '%':
            isLong = useArg = 0;
            for( innerDone = 0; !innerDone; ) {
                switch( *str++ ) {                
                case '%':
                    innerDone = 1;
                    break;
                case 'l':
                    isLong = 1;
                    break;
                case 's':
                    isLong = 1;
                case 'd':
                case 'x':
                    innerDone = 1;
                    useArg = 1;
                    break;
                default:
                    crash();
                }
            }

            if ( useArg ) {
                unsigned long param;
                param = *argv_arm++;
                if ( isLong ) {
                    write_unaligned32( &argv_68k[offset],
                                       Byte_Swap32(param) );
                    offset += 4;
                } else {
                    write_unaligned16( &argv_68k[offset],
                                       Byte_Swap16((unsigned short)param) );
                    offset += 2;
                }
            }
            break;
        }
    }

    /* now call the OS.... */
    {
        PNOState* sp = GET_CALLBACK_STATE();
        unsigned char stack[] = {
            ADD_TO_STACK4(s, 0),
            ADD_TO_STACK4(formatStr, 4),
            ADD_TO_STACK4(argv_68k, 8),
            0 };
        result = (Int16)
            (*sp->call68KFuncP)( sp->emulStateP, 
                                 // NOT sysTrapStrPrintF !!!!
                                 PceNativeTrapNo(sysTrapStrVPrintF),
                                 stack, 12 );
    }

    return result;
} /* StrVPrintF */

/* from file DateTime.h */
void
TimSecondsToDateTime( UInt32 seconds, DateTimeType* dateTimeP )
{
    FUNC_HEADER(TimSecondsToDateTime);
   {
       DateTimeType dateTime;
       PNOState* sp = GET_CALLBACK_STATE();
       unsigned char stack[] = {
           ADD_TO_STACK4(seconds, 0),
           ADD_TO_STACK4(&dateTime, 4),
           0 };
       (*sp->call68KFuncP)( sp->emulStateP, 
                            PceNativeTrapNo(sysTrapTimSecondsToDateTime),
                            stack, 8 );

       dateTimeP->second = Byte_Swap16( dateTime.second );
       dateTimeP->minute = Byte_Swap16( dateTime.minute );
       dateTimeP->hour = Byte_Swap16( dateTime.hour );
       dateTimeP->day = Byte_Swap16( dateTime.day );
       dateTimeP->month = Byte_Swap16( dateTime.month );
       dateTimeP->year = Byte_Swap16( dateTime.year );
       dateTimeP->weekDay = Byte_Swap16( dateTime.weekDay );
   }
   FUNC_TAIL(TimSecondsToDateTime);
} /* TimSecondsToDateTime */


