#!/usr/bin/perl

# Script for turning the Crosswords executable into a cab, along with
# a shortcut in the games menu

use strict;

sub main() {
    my $provider = "\"Crosswords project\"";

    usage() if 1 != @ARGV;

    my $path = shift @ARGV;

    usage() unless $path =~ m|.dll$|;

    die "$path not found\n" unless -f $path;

    my $cabname = `basename $path`;
    chomp $cabname;

    # Create a link.  The format, says Shaun, is
    # <number of characters>#command line<no carriage return or line feed>

    my $userName = $cabname;

    $cabname =~ s/.dll$//;

    my $fname = "/tmp/file$$.list";

# see this url for %CE5% and other definitions:
# http://msdn.microsoft.com/library/default.asp?url=/library/en-us/DevGuideSP/html/sp_wce51consmartphonewindowscestringsozup.asp

    open FILE, "> $fname";

    my $tmpfile = "/tmp/$userName";
    `cp $path $tmpfile`;
    print FILE "$tmpfile ";
    print FILE '%CE1%\\\\Crosswords', "\n";

    close FILE;

    my $appname = $cabname;
    $cabname .= ".cab";

    my $cmd = "pocketpc-cab -p $provider -a $appname "
        . "$fname $cabname";
    print( STDERR $cmd, "\n");
    `$cmd`;

    unlink $tmpfile;
}

sub usage() {
    print STDERR "usage: $0 path/to/xwords4_language.dll\n";
    exit 2;
}

main();
