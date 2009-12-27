#!/usr/bin/perl

# Script for turning the Crosswords executable into a cab, along with
# a shortcut in the games menu

use strict;

my $userName = "Crosswords.exe";

sub main() {
    my $provider = "\"Crosswords project\"";

    usage() if 1 > @ARGV;

    my $path = shift @ARGV;
    my $dict = shift @ARGV;
    if ( ! $dict ) {
        $dict = "../../../dawg/English/BasEnglish2to8.xwd";
    }

    usage() unless $path =~ m|.exe$|;

    die "$path not found\n" unless -f $path;

    my $cabname = `basename $path`;
    chomp $cabname;

    # Create a link.  The format, says Shaun, is
    # <number of characters>#command line<no carriage return or line feed>

    $userName = $cabname unless $userName;
    my $cmdline = "\"\\Program Files\\Crosswords\\" . $userName . "\"";
    my $cmdlen = length( $cmdline );

    $cabname =~ s/.exe$//;
    my $linkName = "Crosswords.lnk";
    open LINKFILE, "> $linkName";
    print LINKFILE $cmdlen, "#", $cmdline;
    close LINKFILE;

    my $fname = "/tmp/file$$.list";

# see this url for %CE5% and other definitions:
# http://msdn.microsoft.com/library/default.asp?url=/library/en-us/DevGuideSP/html/sp_wce51consmartphonewindowscestringsozup.asp

    open FILE, "> $fname";

    my $tmpfile = "/tmp/$userName";
    `cp $path $tmpfile`;
    print FILE "$tmpfile ";
    print FILE '%CE1%\\\\Crosswords', "\n";

    print FILE "$dict ";
    print FILE '%CE1%\\\\Crosswords', "\n";

#     print FILE "$ENV{'CEOPT_ROOT'}/opt/mingw32ce/arm-wince-mingw32ce/bin/mingwm10.dll ";
#     print FILE '%CE2%\\mingwm10.dll', "\n";

    print FILE "$linkName ";
    print FILE '%CE14%', "\n";

    close FILE;

    my $appname = $cabname;
    $cabname .= ".cab";

    my $cmd = "pocketpc-cab -p $provider -a $appname "
        . "$fname $cabname";
    print( STDERR $cmd, "\n");
    `$cmd`;

    unlink $linkName, $tmpfile;
}

sub usage() {
    print STDERR "usage: $0 path/to/xwords4.exe [path/to/dict.xwd]\n";
    exit 2;
}

main();
