#!/usr/bin/perl

use strict;

my @counts;
my $nGames = 0;

while ( <> ) {
    chomp;
    if ( m|^([A-Z]+) \[| ) {
        my $len = length($1);
        ++$counts[$len];
    } elsif ( m|^1:1| ) {
        ++$nGames;
    }
}

print "****** out of $nGames games: *****\n";
print "length         num played\n";
for ( my $i = 2; $i <= 15; ++$i ) {
    printf( "%3d           %8d\n", $i, $counts[$i] );
}

