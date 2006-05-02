#!/usr/bin/perl

##############################################################################
# adapted from C++ code Copyright (C) 2000 Falk Hueffner
# This version Copyright (C) 2002 Eric House (xwords@eehouse.org)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA
##############################################################################

# inputs: 0. Name of file mapping letters to 0..31 values.  In English
# case just contains A..Z.  This will be used to translate the tries
# on output.
#         1. Max number of bytes per binary output file.
#
#         2. Basename of binary files for output.

#         3. Name of file to which to write the number of the
# startNode, since I'm not rewriting a bunch of code to expect Falk's
# '*' node at the start.
#

#         In STDIN, the text file to be compressed.  It absolutely
# must be sorted.  The sort doesn't have to follow the order in the
# map file, however.

# This is meant eventually to be runnable as part of a cgi system for
# letting users generate Crosswords dicts online.



use strict;
use POSIX;

my $gFirstDiff;
my @gCurrentWord;
my $gCurrentWord;               # save so can check for sortedness
my $gDone = 0;
my @gInputStrings;
my $gNeedsSort = 1;             # read from cmd line eventually
my @gNodes;                       # final array of nodes
my $gNBytesPerOutfile = 0xFFFFFFFF;
my $gTableFile;
my $gOutFileBase;
my $gStartNodeOut;
my $gInFileName;
my $gKillIfMissing = 1;
my $gTermChar = '/n';
my $gDumpText = 0;              # dump the dict as text after?
my $gCountFile;
my $gBytesPerNodeFile;          # where to write whether node size 3 or 4
my $gWordCount = 0;
my %gTableHash;
my $gBlankIndex;
my @gRevMap;
my $debug = 0;
my %gSubsHash;
my $gForceFour = 0;             # use four bytes regardless of need?
my $gNBytesPerNode;
my $gUseUnicode;

main();

exit;

##############################################################################

sub main() { 

    if ( !parseARGV() ) {
        usage();
        exit();
    }

    makeTableHash();

    my $infile;

    if ( $gInFileName ) {
        open $infile, "<$gInFileName";
    } else {
        $infile = \*STDIN;
    }

    @gInputStrings = parseAndSort( $infile );
    if ( $gInFileName ) {
        close $infile;
    }

    # Do I need this stupid thing?  Better to move the first row to
    # the front of the array and patch everything else.  Or fix the
    # non-palm dictionary format to include the offset of the first
    # node.

    my $dummyNode = 0xFFFFFFFF;
    @gNodes = ( $dummyNode );
    
    readNextWord();

    my $firstRootChildOffset = buildNode(0);

    moveTopToFront( \$firstRootChildOffset );

    if ( $gStartNodeOut ) {
        writeOutStartNode( $gStartNodeOut, $firstRootChildOffset );
    }

    print STDERR "\n... dumping table ...\n" if $debug;
    printNodes( \@gNodes, "done with main" ) if $debug;

    # write out the number of nodes if requested
    if ( $gCountFile ) {
        open OFILE, "> $gCountFile";
        print OFILE pack( "N", $gWordCount );
        close OFILE;
        print STDERR "wrote out: got $gWordCount words\n";
    }

    if ( $gOutFileBase ) {
        emitNodes( $gNBytesPerOutfile, $gOutFileBase );
    }

    if ( $gDumpText && @gNodes > 0 ) {
        printOneLevel( $firstRootChildOffset, "" );
    }

    if ( $gBytesPerNodeFile ) {
        open OFILE, "> $gBytesPerNodeFile";
        print OFILE $gNBytesPerNode;
        close OFILE;
    }
    print STDERR "Used $gNBytesPerNode per node.\n";
} # main

# We now have an array of nodes with the last subarray being the
# logical top of the tree.  Move them to the start, fixing all fco
# refs, so that legacy code like Palm can assume top==0.
#
# Note: It'd probably be a bit faster to integrate this with emitNodes
# -- unless I need to have an in-memory list that can be used for
# lookups.  But that's best for debugging, so keep it this way for now.
#
# Also Note: the first node is a dummy that can and should be tossed
# now.

sub moveTopToFront($) {
    my ( $firstRef ) = @_;

    my $firstChild = ${$firstRef};
    ${$firstRef} = 0;
    my @lastSub;

    if ( $firstChild > 0 ) {
        # remove the last (the root) subarray
        @lastSub = splice( @gNodes, $firstChild );
    } else {
        die "there should be no words!!" if $gWordCount != 0;
    }
    # remove the first (garbage) node
    shift @gNodes;

    my $diff;
    if ( $firstChild > 0 ) {
        # -1 because all move down by 1; see prev line
        $diff = @lastSub - 1;
        die "something wrong with len\n" if $diff < 0;
    } else {
        $diff = 0;
    }

    # stick it on the front
    splice( @gNodes, 0, 0, @lastSub);

    # We add $diff to everything. There's no subtracting because
    # nobody had any refs to the top list.

    for ( my $i = 0; $i < @gNodes; ++$i ) {
        my $fco = TrieNodeGetFirstChildOffset( $gNodes[$i] );
        if ( $fco != 0 ) {      # 0 means NONE, not 0th!!
            TrieNodeSetFirstChildOffset( \$gNodes[$i], $fco+$diff );
        }
    }
} # moveTopToFront


sub buildNode {
    my ( $depth ) = @_;

    if ( @gCurrentWord == $depth ) {
        # End of word reached. If the next word isn't a continuation
        # of the current one, then we've reached the bottom of the
        # recursion tree.
        readNextWord();
        if ($gFirstDiff < $depth || $gDone) {
            return 0;
        }
    }

    my @newedges;

    do {
        my $letter = $gCurrentWord[$depth];
        my $isTerminal = @gCurrentWord - 1 == $depth ? 1:0;

        my $nodeOffset = buildNode($depth+1);
        my $newNode = MakeTrieNode($letter, $isTerminal, $nodeOffset);
        push( @newedges, $newNode );

    } while ( ($gFirstDiff == $depth) && !$gDone);

    TrieNodeSetIsLastSibling( \@newedges[@newedges-1], 1 );

    return addNodes( \@newedges );
} # buildNode

sub addNodes {
    my ( $newedgesR ) = @_;

    my $found = findSubArray( $newedgesR );

    if ( $found >= 0 ) {
        die "0 is an invalid match!!!" if $found == 0;
        return $found;
    } else {

        my $firstFreeIndex = @gNodes;

        print STDERR "adding...\n" if $debug;
        printNodes( $newedgesR ) if $debug;

        push @gNodes, (@{$newedgesR});

        registerSubArray( $newedgesR, $firstFreeIndex );
        return $firstFreeIndex;
    }
} # addNodes

sub printNode {
    my ( $index, $node ) = @_;

    print STDERR "[$index] ";

    my $letter = TrieNodeGetLetter($node);
    printf( STDERR
            "letter=%d(%s); isTerminal=%d; isLastSib=%d; fco=%d;\n",
            $letter, "" . $gRevMap[$letter],
            TrieNodeGetIsTerminal($node),
            TrieNodeGetIsLastSibling($node),
            TrieNodeGetFirstChildOffset($node));
} # printNode

sub printNodes {
    my ( $nodesR, $name ) = @_;

    my $len = @{$nodesR};
    # print "printNodes($name): len = $len\n";

    for ( my $i = 0; $i < $len; ++$i ) {
        my $node = ${$nodesR}[$i];
        printNode( $i, $node );
    }

}


# Hashing.  We'll keep a hash of offsets into the existing nodes
# array, and as the key use a string that represents the entire sub
# array.  Since the key is what we're matching for, there should never
# be more than one value per hash and so we don't need buckets.
# Return -1 if there's no match.

sub findSubArray {
   my ( $newedgesR ) = @_;

	my $key = join('', @{$newedgesR});

    if ( exists( $gSubsHash{$key} ) ) {
        return $gSubsHash{$key};
    } else {
        return -1;
    }
} # findSubArray

# add to the hash
sub registerSubArray {
    my ( $edgesR, $nodeLoc ) = @_;

    my $key = join( '', @{$edgesR} );

    if ( exists $gSubsHash{$key} ) {
        die "entry for key shouldn't exist!!";
    } else {
        $gSubsHash{$key} = $nodeLoc;
    }

} # registerSubArray

sub toWord($) {
    my ( $tileARef ) = @_;
    my $word = "";

    foreach my $tile (@$tileARef) {
        foreach my $letter (keys (%gTableHash) ) {
            if ( $tile == $gTableHash{$letter} ) {
                $word .= $letter;
                last;
            }
        }
    }

    return $word;
}

sub readNextWord() {
    my @word;

    if ( !$gDone ) {
        $gDone = @gInputStrings == 0;
        if ( !$gDone ) {
            @word = @{shift @gInputStrings};
        } else {
            print STDERR "gDone set to true\n" if $debug;
        }

        print STDERR "got word: ", join(',',@word), "\n" if $debug;
    }
    my $numCommonLetters = 0;
    my $len = @word;
    if ( @gCurrentWord < $len ) {
        $len = @gCurrentWord;
    }

    while ( @gCurrentWord[$numCommonLetters] eq @word[$numCommonLetters]
            && $numCommonLetters < $len) {
        ++$numCommonLetters;
    }

    $gFirstDiff = $numCommonLetters;
    if ( #$debug && 
         @gCurrentWord > 0 && @word > 0
         && !firstBeforeSecond( \@gCurrentWord, \@word ) ) {
        die "words ", join(",",@gCurrentWord), " (" . toWord(\@gCurrentWord) .
            ") and " . join(",", @word) . " (" . toWord(\@word) .
            ") out of order";
    }
    @gCurrentWord = @word;
} # readNextWord

sub firstBeforeSecond {
    my ( $firstR, $secondR ) = @_;
  
    for ( my $i = 0; ; ++$i ) {

        # if we reach the end of the first word/list, we're done.
        if ( $i == @{$firstR} ) {  
            die "duplicate!!!" if $i == @{$secondR};
            return 1;
            # but if we reach the second end first, we've failed
        } elsif ( $i == @{$secondR} ) {
            return 0;
        }

        my $diff = ${$firstR}[$i] <=> ${$secondR}[$i];

        if ( $diff == 0 ) {
            next;
        } else {
            return $diff < 0;
        }
    }
} # firstBeforeSecond

# passed to sort.  Should remain unprototyped for effeciency's sake

sub cmpWords {

    my $lenA = @{$a};
    my $lenB = @{$b};
    my $min = $lenA > $lenB? $lenB: $lenA;

    for ( my $i = 0; $i < $min; ++$i ) {
        my $ac = ${$a}[$i];
        my $bc = ${$b}[$i];

        my $res = $ac <=> $bc;

        if ( $res != 0 ) {
            return $res;        # we're done
        }
    }

    # If we got here, they match up to their common length.  Longer is
    # greater.
    my $res = @{$a} <=> @{$b};
    return $res; # which is longer?
} # cmpWords

sub parseAndSort() {
    my ( $infile ) = @_;

    my @wordlist;
    my @word;

    my $lastWord;
  WORDLOOP:
    for ( ; ; ) {

        my $dropWord = 0;
        splice @word;           # empty it

        # for each byte
        for ( ; ; ) {
            my $byt = getc($infile);

            if ( $byt eq undef ) {
                last WORDLOOP;
            } elsif ( $byt eq $gTermChar ) {
                if ( !$dropWord ) {
                    push @wordlist, [ @word ];
                    ++$gWordCount;
                }
                $lastWord = "";
                next WORDLOOP;
            } elsif ( exists( $gTableHash{$byt} ) ) {
                if ( !$dropWord ) {
                    push @word, $gTableHash{$byt};
                    die "word too long" if @word > 15;
                    if ( $gKillIfMissing ) {
                        $lastWord .= $byt;
                    }
                }
            } elsif ($gKillIfMissing) {
                die "$0: chr $byt (", $byt+0, ") not in map file $gTableFile\n"
                    . "last word was $lastWord\n";
            } else {
                $dropWord = 1;
                splice @word;     # lose anything we already have
            }
        }
    }

    if ( $gNeedsSort && ($gWordCount > 0)  ) {
        print STDERR "starting sort...\n" if $debug;
        @wordlist = sort cmpWords @wordlist;
        print STDERR "sort finished\n" if $debug;
    }

    print STDERR "length of list is ", @wordlist + 0, ".\n" if $debug;

    return @wordlist;
} # parseAndSort

# Print binary representation of trie array.  This isn't used yet, but
# eventually it'll want to dump to multiple files appropriate for Palm
# that can be catenated together on other platforms.  There'll need to
# be a file giving the offset of the first node too.  Also, might want
# to move to 4-byte representation when the input can't otherwise be
# handled.

sub dumpNodes {

    for ( my $i = 0; $i < @gNodes; ++$i ) {
        my $node = $gNodes[$i];
        my $bstr = pack( "I", $node );
        print STDOUT $bstr;
    }
}

##############################################################################
# Little node-field setters and getters to hide what bits represent
# what.
#
# high bit (31) is ACCEPTING bit
# next bit (30) is LAST_SIBLING bit
# next 6 bits (29-24) are tile bit (allowing alphabets of 64 letters)
# final 24 bits (23-0) are the index of the first child (fco)
#
##############################################################################

sub TrieNodeSetIsTerminal {
    my ( $nodeR, $isTerminal ) = @_;

    if ( $isTerminal ) {
        ${$nodeR} |= (1 << 31);
    } else {
        ${$nodeR} &= ~(1 << 31);
    }
}

sub TrieNodeGetIsTerminal {
    my ( $node ) = @_;
    return ($node & (1 << 31)) != 0;
}

sub TrieNodeSetIsLastSibling {
    my ( $nodeR, $isLastSibling ) = @_;
    if ( $isLastSibling ) {
        ${$nodeR} |= (1 << 30);
    } else {
        ${$nodeR} &= ~(1 << 30);
    }
}

sub TrieNodeGetIsLastSibling {
    my ( $node ) = @_;
    return ($node & (1 << 30)) != 0;
}

sub TrieNodeSetLetter {
    my ( $nodeR, $letter ) = @_;

    die "$0: letter ", $letter, " too big" if $letter >= 64;

    my $mask = ~(0x3F << 24);
    ${$nodeR} &= $mask;                         # clear all the bits
    ${$nodeR} |= ($letter << 24);          # set new ones
}

sub TrieNodeGetLetter {
    my ( $node ) = @_;
    $node >>= 24;
    $node &= 0x3F;              # is 3f ok for 3-byte case???
    return $node;
}

sub TrieNodeSetFirstChildOffset {
    my ( $nodeR, $fco ) = @_;

    die "$0: $fco larger than 24 bits" if ($fco & 0xFF000000) != 0;

    my $mask = ~0x00FFFFFF;
    ${$nodeR} &= $mask;                   # clear all the bits
    ${$nodeR} |= $fco;                    # set new ones
}

sub TrieNodeGetFirstChildOffset {
    my ( $node ) = @_;
    $node &= 0x00FFFFFF;                  # 24 bits
    return $node;
}


sub MakeTrieNode {
    my ( $letter, $isTerminal, $firstChildOffset, $isLastSibling ) = @_;
    my $result = 0;

    TrieNodeSetIsTerminal( \$result, $isTerminal );
    TrieNodeSetIsLastSibling( \$result, $isLastSibling );
    TrieNodeSetLetter( \$result, $letter );
    TrieNodeSetFirstChildOffset( \$result, $firstChildOffset );

    return $result;
} # MakeTrieNode

# Caller may need to know the offset of the first top-level node.
# Write it here.
sub writeOutStartNode {
    my ( $startNodeOut, $firstRootChildOffset ) = @_;

    open NODEOUT, ">$startNodeOut";
    print NODEOUT pack( "N", $firstRootChildOffset );
    close NODEOUT;
} # writeOutStartNode

# build the hash for translating.  I'm using a hash assuming it'll be
# fast.  Key is the letter; value is the 0..31 value to be output.
sub makeTableHash {
    my $i;
    open TABLEFILE, "< $gTableFile";

    splice @gRevMap;            # empty it

    for ( $i = 0; ; ++$i ) {
        my $ch = getc(TABLEFILE);
        if ( $ch eq undef ) {
            last;
        }

        if ( $gUseUnicode ) {   # skip the first byte each time: tmp HACK!!!
            $ch = getc(TABLEFILE);
        }
        if ( $ch eq undef ) {
            last;
        }

        push @gRevMap, $ch;

        if ( ord($ch) == 0 ) {	# blank
            $gBlankIndex = $i;
            next;       # we want to increment i when blank seen since 
                        # it is a tile value
        }

        die "$0: $gTableFile too large\n" if $i > 64;
        die "$0: only blank (0) can be 64th char\n" if ($i == 64 && $ch != 0);

        $gTableHash{$ch} = $i;
    }

    close TABLEFILE;
} # makeTableHash

# emitNodes. "input" is $gNodes.  From it we write up to
# $nBytesPerOutfile to files named $outFileBase0..n, mapping the
# letter field down to 5 bits with a hash built from $tableFile.  If
# at any point we encounter a letter not in the hash we fail with an
# error.

sub emitNodes($$) {
    my ( $nBytesPerOutfile, $outFileBase ) = @_;

    # now do the emit.

    # is 17 bits enough?
    printf STDERR ("There are %d (0x%x) nodes in this DAWG.\n",
                   0 + @gNodes, 0 + @gNodes );
    my $nTiles = 0 + keys(%gTableHash); # blank is not included in this count!
    if ( @gNodes > 0x1FFFF || $gForceFour || $nTiles > 32 ) {
        $gNBytesPerNode = 4;
    } elsif ( $nTiles < 32 ) {
        $gNBytesPerNode = 3;
    } else {
        if ( $gBlankIndex == 32 ) { # blank
            print STDERR "blank's at 32; 3-byte-nodes still ok\n";
            $gNBytesPerNode = 3;
        } else {
            die "$0: move blank to last position in info.txt for smaller DAWG";
        }
    }

    my $nextIndex = 0;
    my $nextFileNum = 0;

    for ( $nextFileNum = 0; ; ++$nextFileNum ) {

        if ( $nextIndex >= @gNodes ) {
            last;	# we're done
        }
        
        die "Too many outfiles; infinite loop?" if $nextFileNum > 99;

        my $outName = sprintf("${outFileBase}_%03d.bin", $nextFileNum);
        open OUTFILE, "> $outName";
        binmode( OUTFILE );
        my $curSize = 0;

        while ( $nextIndex < @gNodes ) {

            # scan to find the next terminal
            my $i;
            for ( $i = $nextIndex; 
                  !TrieNodeGetIsLastSibling($gNodes[$i]);
                  ++$i ) {

                # do nothing but a sanity check
                if ( $i >= @gNodes) {
                    die "bad trie format: last node not last sibling" ;
                }

            }
            ++$i;	# move beyond the terminal
            my $nextSize = ($i - $nextIndex) * $gNBytesPerNode;
            if ($curSize + $nextSize > $nBytesPerOutfile) {
                last;
            } else {
                # emit the subarray
                while ( $nextIndex < $i ) {
                    outputNode( $gNodes[$nextIndex], $gNBytesPerNode,
                                \*OUTFILE );
                    ++$nextIndex;
                }
                $curSize += $nextSize;
            }
        }

        close OUTFILE;
    }

} # emitNodes

sub printWord {
    my ( $str ) = @_;

    print STDERR "$str\n";
}

# print out the entire dictionary, as text, to STDERR.

sub printOneLevel {

    my ( $index, $str ) = @_;

    for ( ; ; ) {

        my $newStr = $str;
        my $node = $gNodes[$index++];

        my $lindx = $gRevMap[TrieNodeGetLetter($node)];

        if ( ord($lindx) >= 0x20 ) {
            $newStr .= "$lindx";
        } else {
            print STDERR "sub space" if $debug;
            $newStr .= "\\" . chr('0'+$lindx);
        }

        if ( TrieNodeGetIsTerminal($node) ) {
            printWord( $newStr );
        } 

        my $fco = TrieNodeGetFirstChildOffset( $node );
        if ( $fco != 0 ) {
            printOneLevel( $fco, $newStr );
        }

        if ( TrieNodeGetIsLastSibling($node) ) {
            last;
        }
    }
}

sub outputNode ($$$) {
    my ( $node, $nBytes, $outfile ) = @_;

    my $fco = TrieNodeGetFirstChildOffset($node);
    my $fourthByte;

    if ( $nBytes == 4 ) {
        $fourthByte = $fco >> 16;
        die "$0: fco too big" if $fourthByte > 0xFF;
        $fco &= 0xFFFF;
    }

    # Formats are different depending on whether it's to have 3- or
    # 4-byte nodes.

    # Here's what the three-byte node looks like.  16 bits plus one
    # burried in the last byte for the next node address, five for a
    # character/tile and one each for accepting and last-edge.

    # 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
    # |-------- 16 bits of next node address -------|  |  |  |  |-tile indx-|
    #                                                  |  |  |    
    #                                accepting bit  ---+  |  |
    #                                 last edge bit ------+  |
    #         ---- last bit (17th on next node addr)---------+

    # The four-byte format adds a byte at the right end for
    # addressing, but removes the extra bit (5) in order to let the
    # chars field be six bits.  Bits 7 and 6 remain the same.

    # write the fco (less that one bit).  We want two bytes worth
    # in three-byte mode, and three in four-byte mode

    # first two bytes are low-word of fco, regardless of format
    for ( my $i = 1; $i >= 0; --$i ) {
        my $tmp = ($fco >> ($i * 8)) & 0xFF;
        print $outfile pack( "C", $tmp );
    }
    $fco >>= 16;                # it should now be 1 or 0
    die "fco not 1 or 0" if $fco > 1;

    my $chIn5 = TrieNodeGetLetter($node);
    my $bits = $chIn5;
    die "$0: char $bits too big" if $bits > 0x1F && $nBytes == 3;

    if ( TrieNodeGetIsLastSibling($node) ) {
        $bits |= 0x40;
    }
    if ( TrieNodeGetIsTerminal($node) ) {
        $bits |= 0x80;
    }

    # We set the 17th next-node bit only in 3-byte case (where char is
    # 5 bits)
    if ( $nBytes == 3 && $fco != 0 ) {
        $bits |= 0x20;
    }
    print $outfile pack( "C", $bits );

    # the final byte, if in use
    if ( $nBytes == 4 ) {
        print $outfile pack( "C", $fourthByte );
    }
} # outputNode

sub usage {
    print STDERR "usage: $0 \n"
        . "\t[-b    bytesPerFile] (default = 0xFFFFFFFF)\n"
        . "\t-m     mapFile\n"
        . "\t-mn    mapFile (unicode)\n"
        . "\t-ob    outFileBase\n"
        . "\t-sn    start node out file\n"
        . "\t[-if   input file name]  -- default = stdin\n"
        . "\t[-term ch] (word terminator -- default = '\\0'\n"
        . "\t[-nosort] (input already sorted in accord with -m; " .
        " default=sort'\n"
        . "\t[-dump]  (write dictionary as text to STDERR for testing)\n"
        . "\t[-force4](use 4 bytes per node regardless of need)\n"
        . "\t[-r]     (reject words with letters not in mapfile)\n"
        . "\t[-k]     (kill if any letters no in mapfile -- default)\n"
        . "\t[-debug] (print a bunch of stuff)\n"
        ;

} # usage

sub parseARGV {

    my $arg;
    while ( my $arg = shift(@ARGV) ) {

      SWITCH: {
          if ($arg =~ /-b/) {$gNBytesPerOutfile = shift(@ARGV), last SWITCH;}
          if ($arg =~ /-mn/) {$gTableFile = shift(@ARGV);
                              $gUseUnicode = 1;
                              last SWITCH;}
          if ($arg =~ /-m/) {$gTableFile = shift(@ARGV); last SWITCH;}
          if ($arg =~ /-ob/) {$gOutFileBase = shift(@ARGV), last SWITCH;}
          if ($arg =~ /-sn/) {$gStartNodeOut = shift(@ARGV), last SWITCH;}
          if ($arg =~ /-if/) {$gInFileName = shift(@ARGV), last SWITCH;}
          if ($arg =~ /-r/) {$gKillIfMissing = 0; last SWITCH;}
          if ($arg =~ /-k/) {$gKillIfMissing = 1; last SWITCH;}
          if ($arg =~ /-term/) {$gTermChar = chr(shift(@ARGV)); last SWITCH;}
          if ($arg =~ /-dump/) {$gDumpText = 1; last SWITCH;}
          if ($arg =~ /-nosort/) {$gNeedsSort = 0; last SWITCH;}
          if ($arg =~ /-wc/) {$gCountFile = shift(@ARGV); last SWITCH;}
          if ($arg =~ /-ns/) {$gBytesPerNodeFile = shift(@ARGV); last SWITCH;}
          if ($arg =~ /-force4/) {$gForceFour = 1; last SWITCH;}
          # accept -fsize for compatibility with c++ version (but drop it)
          if ($arg =~ /-fsize/) {shift(@ARGV); last SWITCH;}
          if ($arg =~ /-debug/) {$debug = 1; last SWITCH;}
          die "unexpected arg $arg\n";
      }
    }


    print STDERR "gNBytesPerOutfile=$gNBytesPerOutfile\n" if $debug;
    print STDERR "gTableFile=$gTableFile\n" if $debug;
    print STDERR "gOutFileBase=$gOutFileBase\n" if $debug;
    print STDERR "gStartNodeOut=$gStartNodeOut\n" if $debug;
    printf STDERR "gTermChar=%s(%d)\n", $gTermChar, ord($gTermChar) if $debug;

    return $gTableFile;
    
} # parseARGV
