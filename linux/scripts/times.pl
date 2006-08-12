#!/usr/bin/perl

# Taking the output of time on stdin, sum all of the entries and print
# out averages.

use strict;

#my %nSeen;
my %nSecs;
my $nDiscard = 0;

use constant PATS         => ("real", "sys", "user");

map { $nSecs{$_} = [];} PATS;

my $parm = shift( @ARGV );
if ( $parm eq "--discard" ) {
    $nDiscard = shift( @ARGV );
} elsif ( defined $parm ) {
    print STDERR "usage: $0 [--discard num_high_and_low] < output_from_time\n";
    exit 0;
}

while ( <> ) {
    chomp;
    foreach my $pat (PATS) {
        tryOne( $pat, $_ );
    }
}

print "results:\n";
foreach my $pat (PATS) {
    my $ref = $nSecs{$pat};
    my @locList = sort { $a <=> $b; } @$ref;

    # discard first and last from sorted list
    splice @locList, 0, $nDiscard;
    splice @locList, -$nDiscard;

    my $count = @locList;
    if ( $count > 0 ) {
        my $sum;
        map { $sum += $_ } @locList;
        printf "$pat: average for $count runs: %.3f\n", $sum/$count;
    }
}


sub tryOne($$) {
    my ( $pat, $str ) = @_;

    if ( $str =~ m|^$pat\s+(\d+)m(\d+\.\d+)s| ) {
        push @{$nSecs{$pat}}, ($1*60) + $2;
    }


}
