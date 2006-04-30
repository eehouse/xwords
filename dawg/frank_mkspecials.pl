#!/usr/bin/perl

# Copyright 2001 by Eric House (xwords@eehouse.org)
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
# files representing bitmaps.  For each triple, print out the string and
# then the converted bitmaps.

use strict;

while ( @ARGV ) {
  my $str = shift();
  my $largebmp = shift();
  my $smallbmp = shift();

  doOne( $str, $largebmp, $smallbmp );
}

sub doOne {
  my ( $str, $largebmp, $smallbmp ) = @_;

  print pack( "C", length($str) );
  print $str;

  print STDERR "looking at $largebmp", "\n";

  die "file $largebmp does not exist\n" unless -e $largebmp;
  print `cat $largebmp | ../pbitm2bin.pl`;
  die "file $smallbmp does not exist\n" unless -e $smallbmp;
  print `cat $smallbmp | ../pbitm2bin.pl`;
}


