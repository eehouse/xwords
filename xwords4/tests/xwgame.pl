#!/usr/bin/perl

# Play a relay game.  Params are CookieName and device count.

use strict;

my $xwdir = "../linux/linux";

my $cookie = "COOKIE";
my $nPlayers = 2;
my $port = 10999;
my $dict = "~/personal/dicts/SOWPODS2to15.xwd";
my $quit = "";

my @names = (
    "Fred",
    "Barney",
    "Wilma",
    "Betty",
);

while ( my $param = shift( @ARGV ) ) {
    print STDERR "param $param\n";

    if ( $param =~ m|-C| ) {
        $cookie = shift( @ARGV );
        next;
    } elsif ( $param =~ m|-nplayers| ) {
        $nPlayers = shift( @ARGV );
    } elsif ( $param =~ m|-dict| ) {
        $dict = shift( @ARGV );
    } elsif ( $param =~ m|-port| ) {
        $port = shift( @ARGV );
    } elsif ( $param =~ m|-quit| ) {
        $quit = " -q ";
    } else {
        usage();
        exit 1;
    }
}

my $cmdBase = "cd $xwdir && ./xwords -d $dict -C $cookie $quit -p $port";

for ( my $p = 0; $p < $nPlayers; ++$p ) {
    print STDERR "p = $p\n";
    my $cmd = $cmdBase;
    if ( $p == 0 ) {            # server case
        $cmd .= " -s ";
        
        for ( my $i = 1; $i < $nPlayers; ++$i ) {
            $cmd .= " -N ";
        }
    }
    $cmd .= " -r $names[$p]";

    my $pid = fork;
    print STDERR "fork => $pid.\n";
    if ( $pid ) {
        # parent; give child chance to get ahead
        sleep 1;
    } else {
        exec "$cmd";
        last;
    }
} 

sub usage()
{
    print STDERR <<EOF
    usage : $0 [-nplayers n] [-dict dname] [-port pnum] [-quit]
EOF
}
