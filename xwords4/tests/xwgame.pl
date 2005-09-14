#!/usr/bin/perl

# Play a relay game.  Params are CookieName and device count.

use strict;

my $xwdir = "../linux/linux";

my @cookies = ( "COOKIE" );
my $nPlayers = 2;
my $port = 10999;
my $dict = "~/personal/dicts/SOWPODS2to15.xwd";
my $quit = "";
my $gettingCookies = 0;

my @names = (
    "Fred",
    "Barney",
    "Wilma",
    "Betty",
);

while ( my $param = shift( @ARGV ) ) {
    print STDERR "param $param\n";

    if ( $param =~ m|-C| ) {
        $cookies[0] = shift( @ARGV );
        $gettingCookies = 1;
        next;
    } elsif ( $param =~ m|-nplayers| ) {
        $nPlayers = shift( @ARGV );
    } elsif ( $param =~ m|-dict| ) {
        $dict = shift( @ARGV );
    } elsif ( $param =~ m|-port| ) {
        $port = shift( @ARGV );
    } elsif ( $param =~ m|-quit| ) {
        $quit = " -q ";
    } elsif ( $gettingCookies ) {
        push @cookies, $param;
        next;
    } else {
        usage();
        exit 1;
    }
    $gettingCookies = 0;
}

foreach my $cookie (@cookies) {

    my $cmdBase = "cd $xwdir && ./xwords -d $dict -C $cookie $quit -p $port";
    print "$cmdBase\n";

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
#            sleep 1;
        } else {
            exec "$cmd 2>/dev/null &";
            last;
        }
    } 
}

sub usage()
{
    print STDERR <<EOF
    usage : $0 [-nplayers n] [-dict dname] [-port pnum] [-quit]
EOF
}
