#!/usr/bin/perl

# Script for turning Crosswords .xwd files into .cab files that, when
# run on a PPC device, will install the enclosed .xwd file into a
# Crosswords directory in "c:\Program Files\Crosswords".

use strict;

my $provider = "\"Crosswords project\"";

usage() if 0 == @ARGV;

while ( my $path = shift @ARGV ) {

    unless ( $path =~ m|.xwd$| ) {
        print STDERR "skipping $path: doesn't end in .xwd\n";
        next;
    }
    unless (-f $path ) {
        print STDERR "skipping $path: not found\n";
        next;
    }

    my $fname = "/tmp/file$$.list";

    # see this url for %CE5% and other definitions:
    # http://msdn.microsoft.com/library/default.asp?url=/library/en-us/DevGuideSP/html/sp_wce51consmartphonewindowscestringsozup.asp

    open FILE, "> $fname";
    print FILE "$path ";
    print FILE '%CE1%\\\\Crosswords', "\n";
    close FILE;

    my $cabname = `basename $path`;
    chomp $cabname;
    $cabname =~ s/.xwd$//;
    my $appname = $cabname;
    $cabname .= "_xwd.cab";

    `./scripts/pocketpc-cab -p $provider -a $appname $fname $cabname`;

    print STDERR "$cabname done\n";
    unlink $fname;
}

sub usage() {
    print STDERR "usage: $0 xwdfile[ xwdfile]*\n";
    exit 2;
}
