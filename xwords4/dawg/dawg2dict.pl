#!/usr/bin/perl
#

# Given a .pdb or .xwd file, print all the words in the DAWG.
# Optionally write the values and faces to files whose names are
# provided.

use strict;
use Fcntl;

my $gValueFileName;
my $gASCIIFacesFileName;
my $gUTFFacesFileName;
my $gInFile;
my $gFileType;
my $gNodeSize;

sub usage() {
    print STDERR "USAGE: $0 "
        . "[-vf valuesFileName] "
        . "[-fa asciiFacesFileName] "
        . "[-fu unicodeFacesFileName] "
        . "xwdORpdb"
        . "\n";

}

sub parseARGV {

    my $arg;
    while ( my $arg = shift(@ARGV) ) {

      SWITCH: {
          if ($arg =~ /-vf/) {$gValueFileName = shift(@ARGV), last SWITCH;}
          if ($arg =~ /-fa/) {$gASCIIFacesFileName = shift(@ARGV);
                              last SWITCH;}
          if ($arg =~ /-fu/) {$gUTFFacesFileName = shift(@ARGV);
                              last SWITCH;}

          # Get here it must be the final arg, the input file name.
          $gInFile = $arg;
          if ( 0 != @ARGV ) {
              usage();
              exit 1;
          }
      }
    }

    if ( $gInFile =~ m|.xwd$| ) {
        $gFileType = "xwd";
    } elsif ( $gInFile =~ m|.pdb$| ) {
        $gFileType = "pdb";
    } else {
        usage();
        exit 1;
    }

    return 1;
} # parseARGV

sub countSpecials($) {
    my ( $lref ) = @_;
    my $count = 0;
    foreach my $val (@$lref) {
        if ( ord($val) < 32 ) {
            ++$count;
        }
    }
    return $count;
} # countSpecials

sub readFaces($$$) {
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
} # readFaces

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
        die "invalid dict flags";
    }
} # nodeSizeFromFlags

sub mergeSpecials($$) {
    my ( $facesRef, $specialsRef ) = @_;
    for ( my $i = 0; $i < @$facesRef; ++$i ) {
        my $ref = ord($$facesRef[$i]);
        if ( $ref < 32 ) {
            $$facesRef[$i] = $$specialsRef[$ref];
            print STDERR "set $ref to $$specialsRef[$ref]\n";
        }
    }
}

sub prepXWD($$$$) {
    my ( $path, $facRef, $nodesRef, $startRef ) = @_;

    sysopen(INFILE, $path, O_RDONLY) or die "couldn't open $path: $!\n";;
    binmode INFILE;

    my $buf;
    my $nRead = sysread( INFILE, $buf, 2 );
    my $flags = unpack( "n", $buf );

    $gNodeSize = nodeSizeFromFlags( $flags );

    my $nSpecials;
    my $faceCount = readFaces( *INFILE, $facRef, \$nSpecials );

    # skip xloc header
    $nRead = sysread( INFILE, $buf, 2 );

    # skip values info.
    sysread( INFILE, $buf, $faceCount * 2 );

    my @specials;
    getSpecials( *INFILE, $nSpecials, \@specials );
    mergeSpecials( $facRef, \@specials );

    sysread( INFILE, $buf, 4 );
    $$startRef = unpack( 'N', $buf );

    my @nodes = readNodesToEnd( *INFILE );

    close INFILE;

    @$nodesRef = @nodes;
} # prepXWD

sub prepPDB($$$$) {
    my ( $path, $facRef, $nodesRef, $startRef ) = @_;

    $$startRef = 0; # always for palm?

    sysopen(INFILE, $path, O_RDONLY) or die "couldn't open $path: $!\n";;
    binmode INFILE;

    my $buf;
    # skip header info
    my $nRead = sysread( INFILE, $buf, 76 );
    $nRead += sysread( INFILE, $buf, 2 );
    my $nRecs = unpack( 'n', $buf );

    my @offsets;
    for ( my $i = 0; $i < $nRecs; ++$i ) {
        $nRead += sysread( INFILE, $buf, 4 );
        push( @offsets, unpack( 'N', $buf ) );
        $nRead += sysread( INFILE, $buf, 4 ); # skip
    }

    die "too far" if $nRead > $offsets[0];
    while ( $nRead < $offsets[0] ) {
        $nRead += sysread( INFILE, $buf, 1 );
    }

    my $facesOffset = $offsets[1];
    my $nChars = ($offsets[2] - $facesOffset) / 2;
    $nRead += sysread( INFILE, $buf, $facesOffset - $nRead );
    my @tmp = unpack( 'Nccccccn', $buf );
    $gNodeSize = nodeSizeFromFlags( $tmp[7] );

    my @faces;
    for ( my $i = 0; $i < $nChars; ++$i ) {
        $nRead += sysread( INFILE, $buf, 2 );
        push( @faces, chr(unpack( "n", $buf ) ) );
    }
    @{$facRef} = @faces;

    die "out of sync: $nRead != $offsets[2]" if $nRead != $offsets[2];

    # now skip count and values.  We'll want to get the "specials"
    # shortly.
    $nRead += sysread( INFILE, $buf, $offsets[3] - $nRead );

    die "out of sync" if $nRead != $offsets[3];
    my @nodes = readNodesToEnd( *INFILE );
    close INFILE;

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

sub printDAWGInternal($$$$) {
    my ( $str, $arrRef, $start, $facesRef ) = @_;

    for ( ; ; ) {
        my $node = $$arrRef[$start++];
        my $nextEdge;
        my $chrIndex;
        my $accepting;
        my $lastEdge;

        parseNode( $node, \$chrIndex, \$nextEdge, \$accepting, \$lastEdge );

        push( @$str, $chrIndex );
        if ( $accepting ) {
            printStr( $str, $facesRef );
        }

        if ( $nextEdge != 0 ) {
            printDAWGInternal( $str, $arrRef, $nextEdge, $facesRef );
        }

        pop( @$str );

        if ( $lastEdge ) {
            last;
        }
    }
} # printDAWGInternal

sub printDAWG($$$) {
    my ( $arrRef, $start, $facesRef ) = @_;

    die "no nodes!!!" if 0 == @$arrRef;

    my @str;
    printDAWGInternal( \@str, $arrRef, $start, $facesRef );
}


#################################################################
# main
#################################################################


if ( !parseARGV() ) {
    usage();
    exit 1;
}

my @faces;
my @nodes;
my $startIndex;

if ( $gFileType eq "xwd" ){ 
    prepXWD( $gInFile, \@faces, \@nodes, \$startIndex );
} elsif ( $gFileType eq "pdb" ) {
    prepPDB( $gInFile, \@faces, \@nodes, \$startIndex );
    print STDERR join( ",", @faces), "\n";
}

printDAWG( \@nodes, $startIndex, \@faces );

if ( $gASCIIFacesFileName ) {
    open FACES, "> $gASCIIFacesFileName";
    foreach my $face (@faces) {
        print FACES pack('cc', 0, $face );
    }
    close FACES;
}
