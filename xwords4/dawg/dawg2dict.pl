#!/usr/bin/perl
#
# Copyright 2004 by Eric House (xwords@eehouse.org)
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


# Given a .pdb or .xwd file, print all the words in the DAWG to
# stdout.

use strict;
use Fcntl;

my $gInFile;
my $gDoRaw = 0;
my $gFileType;
my $gNodeSize;

use Fcntl 'SEEK_CUR';
sub systell { sysseek($_[0], 0, SEEK_CUR) }

sub usage() {
    print STDERR "USAGE: $0 "
        . "[-raw] "
        . "-dict <xwdORpdb>"
        . "\n"
        . "\t(Takes a .pdb or .xwd and prints its words to stdout)\n";
    exit 1;
}

sub parseARGV() {

    while ( my $parm = shift(@ARGV) ) {
        if ( $parm eq "-raw" ) {
            $gDoRaw = 1;
        } elsif ( $parm eq "-dict" ) {
            $gInFile = shift(@ARGV);
        } else {
            usage();
        }
    }

    if ( $gInFile =~ m|.xwd$| ) {
        $gFileType = "xwd";
    } elsif ( $gInFile =~ m|.pdb$| ) {
        $gFileType = "pdb";
    } else {
        usage();
    }
} # parseARGV

sub countSpecials($) {
    my ( $facesRef ) = @_;
    my $count = 0;

    map { ++$count if ( ord($_) < 32 ); } @$facesRef;
    return $count;
} # countSpecials

sub readXWDFaces($$$) {
    my ( $fh, $facRef, $nSpecials ) = @_;

    my $buf;
    my $nRead = sysread( $fh, $buf, 1 );
    my $nChars = unpack( 'c', $buf );

    my @faces;
    for ( my $i = 0; $i < $nChars; ++$i ) {
        my $nRead = sysread( $fh, $buf, 2 );
        push( @faces, chr(unpack( "n", $buf ) ) );
    }

    ${$nSpecials} = countSpecials( \@faces );
    @{$facRef} = @faces;
    return $nChars;
} # readXWDFaces

sub skipBitmap($) {
    my ( $fh ) = @_;
    my $buf;
    sysread( $fh, $buf, 1 );
    my $nCols = unpack( 'C', $buf );
    if ( $nCols > 0 ) {
        sysread( $fh, $buf, 1 );
        my $nRows = unpack( 'C', $buf );
        my $nBytes = (($nRows * $nCols) + 7) / 8;

        sysread( $fh, $buf, $nBytes );
    }
} # skipBitmap

sub getSpecials($$$) {
    my ( $fh, $nSpecials, $specRef ) = @_;

    my @specials;
    for ( my $i = 0; $i < $nSpecials; ++$i ) {
        my $buf;
        sysread( $fh, $buf, 1 );
        my $len = unpack( 'C', $buf );
        sysread( $fh, $buf, $len );
        push( @specials, $buf );
        skipBitmap( $fh );
        skipBitmap( $fh );
    }

    @{$specRef} = @specials;
} # getSpecials

sub readNodesToEnd($) {
    my ( $fh ) = @_;
    my @nodes;
    my $count = 0;
    my $offset = 4 - $gNodeSize;
    my ( $buf, $nRead );

    do {
        $nRead = sysread( $fh, $buf, $gNodeSize, $offset );
        $count += $nRead;
        my $node = unpack( 'N', $buf );
        push( @nodes, $node );
    } while ( $nRead == $gNodeSize );
    die "out of sync? nRead=$nRead, count=$count" if $nRead != 0;

    return @nodes;
} # readNodesToEnd

sub nodeSizeFromFlags($) {
    my ( $flags ) = @_;
    if ( $flags == 2 ) {
        return 3;
    } elsif ( $flags == 3 ) {
        return 4;
    } else {
        die "invalid dict flags $flags";
    }
} # nodeSizeFromFlags

sub mergeSpecials($$) {
    my ( $facesRef, $specialsRef ) = @_;
    for ( my $i = 0; $i < @$facesRef; ++$i ) {
        my $ref = ord($$facesRef[$i]);
        if ( $ref < 32 ) {
            $$facesRef[$i] = $$specialsRef[$ref];
            #print STDERR "set $ref to $$specialsRef[$ref]\n";
        }
    }
}

sub prepXWD($$$$) {
    my ( $fh, $facRef, $nodesRef, $startRef ) = @_;

    my $buf;
    my $nRead = sysread( $fh, $buf, 2 );
    my $flags = unpack( "n", $buf );

    $gNodeSize = nodeSizeFromFlags( $flags );

    my $nSpecials;
    my $faceCount = readXWDFaces( $fh, $facRef, \$nSpecials );

    # skip xloc header
    $nRead = sysread( $fh, $buf, 2 );

    # skip values info.
    sysread( $fh, $buf, $faceCount * 2 );

    my @specials;
    getSpecials( $fh, $nSpecials, \@specials );
    mergeSpecials( $facRef, \@specials );

#    printf STDERR "at 0x%x before offset read\n", systell($fh);
    sysread( $fh, $buf, 4 );
    $$startRef = unpack( 'N', $buf );
#    print STDERR "startRef=$$startRef\n";

    my @nodes = readNodesToEnd( $fh );

    @$nodesRef = @nodes;
} # prepXWD

sub readPDBSpecials($$$$$) {
    my ( $fh, $nChars, $nToRead, $nSpecials, $specRef ) = @_;

    my ( $nRead, $buf );

    # first skip counts and values, and xloc header
    $nRead += sysread( $fh, $buf, ($nChars * 2) + 2 );

    while ( $nSpecials-- ) {
        $nRead += sysread( $fh, $buf, 8 ); # sizeof(Xloc_specialEntry)
        my @chars = unpack( 'C8', $buf );
        my $str;
        foreach my $char (@chars) {
            if ( $char == 0 ) { # null-terminated on palm
                last;
            }
            $str .= chr($char);
        }
        push( @$specRef, $str );
    }

    $nRead += sysread( $fh, $buf, $nToRead - $nRead ); # skip bitmaps

    return $nRead;
} # readPDBSpecials

sub prepPDB($$$$) {
    my ( $fh, $facRef, $nodesRef, $startRef ) = @_;

    $$startRef = 0; # always for palm?

    my $buf;
    # skip header info
    my $nRead = sysread( $fh, $buf, 76 );
    $nRead += sysread( $fh, $buf, 2 );
    my $nRecs = unpack( 'n', $buf );

    my @offsets;
    for ( my $i = 0; $i < $nRecs; ++$i ) {
        $nRead += sysread( $fh, $buf, 4 );
        push( @offsets, unpack( 'N', $buf ) );
        $nRead += sysread( $fh, $buf, 4 ); # skip
    }

    die "too far" if $nRead > $offsets[0];
    while ( $nRead < $offsets[0] ) {
        $nRead += sysread( $fh, $buf, 1 );
    }

    my $facesOffset = $offsets[1];
    my $nChars = ($offsets[2] - $facesOffset) / 2;
    $nRead += sysread( $fh, $buf, $facesOffset - $nRead );
    my @tmp = unpack( 'Nc6n', $buf );
    $gNodeSize = nodeSizeFromFlags( $tmp[7] );

    my @faces;
    for ( my $i = 0; $i < $nChars; ++$i ) {
        $nRead += sysread( $fh, $buf, 2 );
        push( @faces, chr(unpack( "n", $buf ) ) );
    }
    @{$facRef} = @faces;

    die "out of sync: $nRead != $offsets[2]" if $nRead != $offsets[2];

    my @specials;
    $nRead += readPDBSpecials( $fh, $nChars, $offsets[3] - $nRead,
                               countSpecials($facRef), \@specials );
    mergeSpecials( $facRef, \@specials );

    die "out of sync" if $nRead != $offsets[3];
    my @nodes = readNodesToEnd( $fh );

    @$nodesRef = @nodes;
} # prepPDB

sub parseNode($$$$$) {
    my ( $node, $chrIndex, $nextEdge, $accepting, $last ) = @_;

    if ( $gNodeSize == 4 ) {
        $$accepting = ($node & 0x00008000) != 0;
        $$last = ($node & 0x00004000) != 0;
        $$chrIndex = ($node & 0x00003f00) >> 8;
        $$nextEdge = ($node >> 16) + (($node & 0x000000FF) << 16);
    } elsif( $gNodeSize == 3 ) {
        $$accepting = ($node & 0x00000080) != 0;
        $$last = ($node & 0x00000040) != 0;
        $$chrIndex = $node & 0x0000001f;
        $$nextEdge = ($node >> 8) + (($node & 0x00000020) << 11);
    }

   # printf "%x: acpt=$$accepting; last=$$last; "
   #    . "next=$$nextEdge; ci=$$chrIndex\n", $node;
} # parseNode

sub printStr($$) {
    my ( $strRef, $facesRef ) = @_;

    print join( "", map {$$facesRef[$_]} @$strRef), "\n";
} # printStr

# Given an array of 4-byte nodes, a start index. and another array of
# two-byte faces, print out all of the words in the nodes array.
sub printDAWG($$$$) {
    my ( $strRef, $arrRef, $start, $facesRef ) = @_;

    die "infinite recursion???" if @$strRef > 15;

    for ( ; ; ) {
        my $node = $$arrRef[$start++];
        my $nextEdge;
        my $chrIndex;
        my $accepting;
        my $lastEdge;

        parseNode( $node, \$chrIndex, \$nextEdge, \$accepting, \$lastEdge );

        push( @$strRef, $chrIndex );
        if ( $accepting ) {
            printStr( $strRef, $facesRef );
        }

        if ( $nextEdge != 0 ) {
            printDAWG( $strRef, $arrRef, $nextEdge, $facesRef );
        }

        pop( @$strRef );

        if ( $lastEdge ) {
            last;
        }
    }
} # printDAWG

sub printNodes($$) {
    my ( $nr, $fr ) = @_;
    
    my $len = @$nr;
    for ( my $i = 0; $i < $len; ++$i ) {
        my $node = $$nr[$i];

        my ( $chrIndex, $nextEdge, $accepting, $lastEdge );
        parseNode( $node, \$chrIndex, \$nextEdge, \$accepting, \$lastEdge );

        printf "%.8x: (%.8x) %2d(%s) %.8x ", $i, $node, $chrIndex, 
        $$fr[$chrIndex], $nextEdge;
        print ($accepting? "A":"a");
        print " ";
        print ($lastEdge? "L":"l");
        print "\n";
    }
}

#################################################################
# main
#################################################################


parseARGV();

sysopen(INFILE, $gInFile, O_RDONLY) or die "couldn't open $gInFile: $!\n";;
binmode INFILE;

my @faces;
my @nodes;
my $startIndex;

if ( $gFileType eq "xwd" ){ 
    prepXWD( *INFILE, \@faces, \@nodes, \$startIndex );
} elsif ( $gFileType eq "pdb" ) {
    prepPDB( *INFILE, \@faces, \@nodes, \$startIndex );
}
close INFILE;

die "no nodes!!!" if 0 == @nodes;
if ( $gDoRaw ) {
    printNodes( \@nodes, \@faces );
} else {
    printDAWG( [], \@nodes, $startIndex, \@faces );
}

exit 0;
