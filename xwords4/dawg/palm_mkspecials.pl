#!/usr/bin/perl

# Copyright 2002 by Eric House
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

# Given arguments consisting of triples, first a string and then pbitm
# files representing bitmaps. The format looks like this:

# array [0-n] of { char len; 
#                  char[3] alt txt;
#                  int16 offsetOfLarge;
#                  int16 offsetOfSmall;
#                }
# array [0-n] of { 
#                  bitmapLargeIfPresent;
#                  bitmapSmallIfPresent;
#                }
#
# In addition, there's padding between bitmaps if needed to get the next
# one to a 2-byte boundary.  And the input files are not in PalmOS bitmap
# format, so thay have to get converted into a tmp file before the sizes
# can be known and included in the eventual output.

use strict;

my $tmpfile = "/tmp/tmpout$$";

my $nSpecials = @ARGV / 3;
die "wrong number of args" if (@ARGV % 3) != 0;
my $gOffset = $nSpecials * 8;     # sizeof(Xloc_specialEntry)

open TMPFILE, "> $tmpfile";

for ( my $i = 0; $i < $nSpecials; ++$i ) {
    
    my $size;

    my $str = shift( @ARGV );
    my $len = length($str);
    die "string $str too long" if $len > 3;
    print $str;
    while ( $len < 4 ) {
        ++$len;
        print pack("c", 0 );
    }

    doOneFile( shift( @ARGV ), \*TMPFILE, \$gOffset );
    doOneFile( shift( @ARGV ), \*TMPFILE, \$gOffset );
}

close TMPFILE;

# now append the tempfile
open TMPFILE, "< $tmpfile";
while ( read( TMPFILE, my $buffer, 128 ) ) {
    print $buffer;
}
close TMPFILE;

unlink $tmpfile;

exit 0;


sub doOneFile($$$) {
    my ( $fil, $fh, $offsetR ) = @_;

    my $size = convertBmp($fil, $fh );
    if ( ($size % 2) != 0 ) {
        ++$size;
        print $fh pack( "c", 0 );
    }

    print pack( "n", $size > 0? ${$offsetR} : 0 );

    ${$offsetR} += $size;
} # doOneFile

sub convertBmp($$) {
    my ( $pbitmfile, $fhandle ) = @_;

    if ( $pbitmfile eq "/dev/null" ) {
        return 0;
    } else {

        # for some reason I can't get quote marks to print into tmp.rcp using just `echo`
        open TMP, "> tmp.rcp";
        print TMP "BITMAP ID 1000 \"$pbitmfile\" AUTOCOMPRESS";
        close TMP;

        `pilrc tmp.rcp`;
        print $fhandle `cat Tbmp03e8.bin`;
        my $siz = -s "Tbmp03e8.bin";
        `rm -f tmp.rcp Tbmp03e8.bin`;

        return $siz;
    }
}
