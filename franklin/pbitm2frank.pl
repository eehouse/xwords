#!/usr/bin/perl
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

# write a pbitm out in franklin "format"
#
# usage: pbitm2frank.pl infile outName

use strict;

my $infile = $ARGV[0];
my $outname = $ARGV[1];

my @lines;
my $width = -1;
my $height = 0;

open (INFILE, $infile) or die "can't find file $infile\n";

print qq (
/* This file generated from $infile; do not edit!!! */

);

while ( <INFILE> ) {
  ++$height;

  s/\s//;			# get rid of whitespace
  push( @lines, $_ );

  my $len = length($_);
  if ( $width == -1 ) {
    $width = $len;
  } elsif ( $len != $width ) {
    die "line $height width differs";
  }
}

my $rowbytes = ($width + 7) >> 3;
my $structName = "${outname}_struct" ;

print qq( typedef struct $structName {
  IMAGE img;
  U8 data[$height * $rowbytes];
} $structName;

);

printStruct(0);

print "#ifdef USE_INVERTED\n";
printStruct(1);
print "#endif /* USE_INVERTED */\n";


sub printStruct() {
  my ($invert) = @_;

  my $thisName = $outname;

  if ( $invert ) {
    $thisName .= "_inverted";
  }

  print qq(
$structName $thisName = {
   { $width, $height, $rowbytes,
   COLOR_MODE_MONO, 0, (const COLOR *) 0, (U8*)NULL },
    {
    );

    foreach my $line (@lines) {
      printLine( $width, $line, $invert );
    }

    print "    }\n};\n";
} # printStruct

sub printLine() {
  my ($len, $line, $invert) = @_;

  $line .= '-------';		# pad with 7 0s

  if ( $invert ) {
    $line =~ s/#/h/g;
    $line =~ s/\-/#/g;
    $line =~ s/h/\-/g;
  }

  for ( my $i = 0; $len > 0; ++$i ) {
    my $byte = 0;
    my $subline = substr($line, $i*8, 8 );

    for ( my $j = 0; $j < 8; ++$j ) {
      my $ch = substr( $subline, $j, 1 );
      if ( $ch eq '-' ) {
      } elsif ( $ch eq '#' ) {
	$byte |= 1 << (7-$j);
      } else {
	print STDERR ("unexpected char $ch at offset ",
		      ($i*8)+$j, " in line $height\n");
	die;
      }
    }

    printf( "\t0x%x, ", $byte );
    $len -= 8;
  }

  print "\t/* ", substr($line, 0, -7), " */\n";
}				# printLine
