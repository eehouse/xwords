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

sub skipBitmaps($) {
    my ( $fh ) = @_;
    my $buf;
    sysread( $fh, $buf, 1 );
    my $nCols = unpack( 'C', $buf );
    die "not doing real bitmaps yet" if $nCols;
}

sub getSpecials($$$) {
    my ( $fh, $nSpecials, $specRef ) = @_;

    my @specials;
    for ( my $i = 0; $i < $nSpecials; ++$i ) {
        my $buf;
        sysread( $fh, $buf, 1 );
        my $len = unpack( 'C', $buf );
        sysread( $fh, $buf, $len );
        push( @specials, $buf );
        skipBitmaps( $fh );
        skipBitmaps( $fh );
    }

    @{$specRef} = @specials;
} # getSpecials

sub prepXWD($$$$) {
    my ( $fh, $facRef, $nodesRef, $startRef ) = @_;

    my $buf;
    my $nRead = sysread( $fh, $buf, 2 );
    my $flags = unpack( "n", $buf );

    if ( $flags == 2 ) {
        $gNodeSize = 3;
    } elsif ( $flags == 3 ) {
        $gNodeSize = 4;
    } else {
        die "invalid dict flags";
    }

    my $nSpecials;
    my $faceCount = readFaces( $fh, $facRef, \$nSpecials );

    # skip xloc header
    $nRead = sysread( $fh, $buf, 2 );

    # skip values info.
    sysread( $fh, $buf, $faceCount * 2 );

    my @specials;
    getSpecials( $fh, $nSpecials, \@specials );

    sysread( $fh, $buf, 4 );
    $$startRef = unpack( 'N', $buf );

    my @nodes;
    my $count = 0;
    my $offset = 4 - $gNodeSize;
    do {
        $nRead = sysread( $fh, $buf, $gNodeSize, $offset );
        $count += $nRead;
        my $node = unpack( 'N', $buf );
        push( @nodes, $node );
    } while ( $nRead == $gNodeSize );
    die "out of sync? nRead=$nRead, count=$count" if $nRead != 0;

    @$nodesRef = @nodes;
} # prepXWD

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
    my ( $strRef, $accepted ) = @_;

    if ( $accepted ) {
        print join( "", @$strRef ), "\n";
#     } else {
#         print  "partial: ", join( "", @$strRef ), "\n";
    }
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

        die "index $chrIndex out of range" if $chrIndex > 26 || $chrIndex < 0;
        push( @$str, $$facesRef[$chrIndex] );
        printStr( $str, $accepting );

        if ( $nextEdge != 0 ) {
            printDAWGInternal( $str, $arrRef, $nextEdge, $facesRef );
        }

        pop( @$str );

        # print  "2. lastEdge=$lastEdge\n";
        if ( $lastEdge ) {
            last;
        }
    }
} # printDAWGInternal

sub printDAWG($$$) {
    my ( $arrRef, $start, $facesRef ) = @_;

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

sysopen(INFILE, "$gInFile", O_RDONLY) or die "couldn't open: $!\n";;
binmode INFILE;

my @faces;
my @nodes;
my $startIndex;

if ( $gFileType eq "xwd" ){ 
    prepXWD( *INFILE, \@faces, \@nodes, \$startIndex );
} elsif ( $gFileType eq "pdb" ) {
    die "not doing .pdbs yet";
}

close INFILE;

printDAWG( \@nodes, $startIndex, \@faces );
