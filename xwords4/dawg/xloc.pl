#!/usr/bin/perl

# Copyright 2002 by Eric House (fixin@peak.org).  All rights reserved.
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

my $arg = shift(@ARGV);
my $outfile = shift(@ARGV);
my $lang = shift(@ARGV);
my $path = "./$lang";
my $infoFile = "$path/info.txt";

die "info file $infoFile not found\n" if ! -s $infoFile;


my $xlocToken = xloc::ParseTileInfo($infoFile);

open OUTFILE, "> $outfile";
# For f*cking windoze linefeeds
binmode( OUTFILE );

if ( $arg eq "-t" ) {
    xloc::WriteMapFile( $xlocToken, 0, \*OUTFILE );
} elsif ( $arg eq "-tn" ) {
    xloc::WriteMapFile( $xlocToken, 1, \*OUTFILE );
} elsif ( $arg eq "-v" ) {
    xloc::WriteValuesFile( $xlocToken, \*OUTFILE );
}

close OUTFILE;
