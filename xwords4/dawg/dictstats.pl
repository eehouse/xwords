#!/usr/bin/perl

# print stats about in input stream that's assumed to be a dictionary.
# Counts and percentages of each letter, as well as total numbers of
# words.  This is not part of the dictionary build process.  I use it
# for creating info.txt files for new languages and debugging the
# creation of dictionaries from new wordlists.
#
# Something like this might form the basis for choosing counts and
# values for tiles without using the conventions established by
# Scrabble players.  This isn't enough, though: the frequency of
# letter tuples and triples -- how often letters appear together -- is
# a better indicator than just letter count.

use strict;

my @wordSizeCounts;
my %letterCounts;
my $wordCount;
my $letterCount;
my $enc = "utf8";               # this could be a cmdline arg....

if ( $enc ) {
    binmode( STDOUT, ":encoding($enc)" ) ;
    binmode( STDIN, ":encoding($enc)" ) ;
}

while (<>) {

    chomp;

    ++$wordSizeCounts[length];
    ++$wordCount;

    foreach my $letter (split( / */ ) ) {
        my $ii = ord($letter);
        # special-case the bogus chars we add for "specials"
        die "$0: this is a letter?: $ii" if $ii <= 32 && $ii >= 4 && $ii != 0; 
        ++$letterCounts{$letter};
        ++$letterCount;
    }
}

print "Number of words: $wordCount\n";
print "Number of letters: $letterCount\n\n";


print "**** word sizes ****\n";
print "SIZE  COUNT   PERCENT\n";
for ( my $i = 1 ; $i <= 99; ++$i ) {
    my $count = $wordSizeCounts[$i];
    if ( $count > 0 ) {
        my $pct = (100.00 * $count)/$wordCount;
        printf "%2d    %5d    %.2f\n", $i, $count, $pct;
    }
} 



print "\n\n**** Letter counts ****\n";
print "     ASCII ORD  HEX     PCT (of $letterCount)\n";
my $lineNo = 1;
foreach my $key (sort keys %letterCounts) {
    my $count = $letterCounts{$key};
    my $pct = (100.00 * $count) / $letterCount;
    printf( "%2d: %3s   %3d  %x    %5.2f (%d)\n",
            $lineNo, $key, ord($key), ord($key), $pct, $count );
    ++$lineNo;
}

print "\n";
