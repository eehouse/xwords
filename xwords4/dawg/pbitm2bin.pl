#!/usr/bin/perl
#
# Copyright 2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#
#
# Given a pbitm on stdin, a text bitmap file where '#' indicates a set
# bit and '-' indicates a clear bit, convert into binary form (on
# stdout) where there's one bit per bit plus a byte each for the width
# and height.  Nothing for bitdepth at this point.  And no padding: if
# the number of bits in a row isn't a multiple of 8 then one byte will
# hold the last bits of one row and the first of another.

use strict;

my $nRows = 0;
my $nCols = 0;
my $bits = "";			# save the chars in a single string to start

# first gather information and sanity-check the data

while (<>) {
  chomp;
  my $len = length();

  if ( $nCols == 0 ) {
    $nCols = $len;
  } else { 
    die "line of inconsistent length" if $nCols != $len ;
  }
  if ( $nCols == 0 ) {
    last;
  }

  $bits .= $_;
  ++$nRows;
}

my $len = length($bits);
print pack( "C", $nCols );

# if we've been given an empty file, print out a single null byte and
# be done.  That'll be the convention for "non-existant bitmap".
if ( $len == 0 ) {
  exit 0;
}
print pack( "C", $nRows );
printf STDERR "emitting %dx%d bitmap\n", $nCols, $nRows;
  

my @charlist = split( //,$bits);
my $byte = 0;

for ( my $count = 0; ; ++$count ) {

  my $ch = $charlist[$count];
  my $bitindex = $count % 8;

  $ch == '-' || $ch == '#' || die "unknown char $ch";

  my $bit = ($ch eq '#')? 1:0;

  $byte |= $bit << (7 - $bitindex);

  my $lastPass = $count + 1 == $len;
  if ( $bitindex == 7 || $lastPass ) {
    print pack( "C", $byte );
    if ( $lastPass ) {
      last;
    }
    $byte = 0;
  }

}  # for loop
