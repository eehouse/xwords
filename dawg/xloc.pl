#!/usr/bin/perl

# Copyright 2002 by Eric House (xwords@eehouse.org).  All rights reserved.
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

# test and wrapper file for xloc.pm

use strict;
use xloc;

my $unicode = -1;
my $doval = 0;
my $enc;
my $outfile;

my $arg;
while ( $arg = $ARGV[0] ) {
    if ( $arg eq '-enc' ) {
        $enc = $ARGV[1];
        shift @ARGV;
    } elsif ( $arg eq "-tn" ) {
        $unicode = 1;
    } elsif ( $arg eq "-t" ) {
        $unicode = 0;
    } elsif ( $arg eq "-v" ) {
        $doval = 1;
    } elsif ( $arg eq '-out' ) {
        $outfile = $ARGV[1];
        shift @ARGV;
    } else {
        die "unknown arg $arg\n";
    }
    shift @ARGV;
}

my $infoFile = "info.txt";

die "info file $infoFile not found\n" if ! -s $infoFile;

my $xlocToken = xloc::ParseTileInfo($infoFile, $enc);

open OUTFILE, "> $outfile";
# For f*cking windoze linefeeds
binmode( OUTFILE );

if ( $unicode ne -1 ) {
    xloc::WriteMapFile( $xlocToken, $unicode, \*OUTFILE );
} elsif ( $doval ) {
    xloc::WriteValuesFile( $xlocToken, \*OUTFILE );
}

close OUTFILE;
