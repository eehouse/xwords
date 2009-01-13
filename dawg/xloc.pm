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

# The idea here is that all that matters about a language is stored in
# one file (possibly excepting rules for prepping a dictionary).
# There's a list of tile faces, counts and values, and also some
# name-value pairs as needed.  The pairs come first, and then a list
# of tiles.

package xloc;

use strict;
use warnings;

BEGIN {
    use Exporter   ();
    our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);

    $VERSION     = 1.00;

    @ISA         = qw(Exporter);
    @EXPORT      = qw(&ParseTileInfo &GetNTiles &TileFace &TileValue
		      &TileCount &GetValue &WriteMapFile &WriteValuesFile);
    %EXPORT_TAGS = ( );
}

# Returns what's meant to be an opaque object that can be passed back
# for queries.  It's a hash with name-value pairs and an _INFO entry
# containing a list of tile info lists.

sub ParseTileInfo($$) {
    my ( $filePath, $enc ) = @_;
    my %result;

    if ( $enc ) {
        open( INPUT, "<:encoding($enc)", "$filePath" ) 
            or die "couldn't open $filePath";
    } else {
        open( INPUT, "<$filePath" ) 
            or die "couldn't open $filePath";
    }

    my $inTiles = 0;
    my @tiles;
    while ( <INPUT> ) {

        chomp;
        s/\#.*$//;
        s/^\s*$//;                  # nuke all-white-space lines
        next if !length;

        if ( $inTiles ) {
            if ( /<END_TILES>/ ) {
                last;
            } else {
                my ( $count, $val, $face ) = m/^\s*(\w+)\s+(\w+)\s+(.*)\s*$/;
                push @tiles, [ $count, $val, $face ];
            }
        } elsif ( /\w:/ ) {
            my ( $nam, $val ) = split ':', $_, 2;
            $result{$nam} .= $val;
        } elsif ( /<BEGIN_TILES>/ ) {
            $inTiles = 1;
        }

    }

    close INPUT;

    $result{"_TILES"} = [ @tiles ];

    return \%result;
}

sub GetNTiles($) {
    my ( $hashR ) = @_;

    my $listR = ${$hashR}{"_TILES"};

    return 0 + @{$listR};
}

sub GetValue($$) {
    my ( $hashR, $name ) = @_;
    return ${$hashR}{$name};
}

sub WriteMapFile($$$) {
    my ( $hashR, $unicode, $fhr ) = @_;

    my $packStr;
    if ( $unicode ) {
        $packStr = "n";
    } else {
        $packStr = "C";
    }

    my $count = GetNTiles($hashR);
    my $specialCount = 0;
    for ( my $i = 0; $i < $count; ++$i ) {
        my $tileR = GetNthTile( $hashR, $i );
        my $str = ${$tileR}[2];

        if ( $str =~ /\'(.)\'/ ) {
            print $fhr pack($packStr, ord($1) );
        } elsif ( $str =~ /\"(.+)\"/ ) {
            print $fhr pack($packStr, $specialCount++ );
        } elsif ( $str =~ /(\d+)/ ) {
            print $fhr pack( $packStr, $1 );
        } else {
            die "WriteMapFile: unrecognized face format $str, elem $i";
        }
    }
} # WriteMapFile

sub WriteValuesFile($$) {
    my ( $hashR, $fhr ) = @_;

    my $header = GetValue( $hashR,"XLOC_HEADER" );
    die "no XLOC_HEADER found" if ! $header;

    print STDERR "header is $header\n";

    print $fhr pack( "n", hex($header) );

    my $count = GetNTiles($hashR);
    for ( my $i = 0; $i < $count; ++$i ) {
        my $tileR = GetNthTile( $hashR, $i );

        print $fhr pack( "c", TileValue($tileR) );
        print $fhr pack( "c", TileCount($tileR) );
    }

} # WriteValuesFile

sub GetNthTile($$) {
    my ( $hashR, $n ) = @_;
    my $listR = ${$hashR}{"_TILES"};

    return ${$listR}[$n];
}

sub TileFace($) {
    my ( $tileR ) = @_;

    my $str = ${$tileR}[2];

    if ( $str =~ /\'(.)\'/ ) {
        return $1;
    } elsif ( $str =~ /\"(.+)\"/ ) {
        return $1;
    } elsif ( $str =~ /(\d+)/ ) {
        return chr($1);
    } else {
        die "TileFace: unrecognized face format: $str";
    }
}

sub TileValue($) {
    my ( $tileR ) = @_;

    return ${$tileR}[0];
}

sub TileCount($) {
    my ( $tileR ) = @_;

    return ${$tileR}[1];
}

1;
