#!/usr/bin/perl

# Script for wrapping an xwd file as a CAB to ease installation.

use strict;
use File::Basename;

my $force = 0;
my $path;

sub usage() {
    print STDERR "Usage: $0 [-f] dictName.xwd\n";
    exit 1;
}

my $cabwiz = $ENV{"CABWIZPATH"};
if ( ! -x $cabwiz ) {
    print STDERR <<EOF;
******************************************************************************
* Unable to locate Cabwiz.exe.  If you are on a windoze system and
* that file exists, please define the environment variable CABWIZPATH
* to its complete pathname.  If you do not have that binary and don\'t
* want to see this warning set CABWIZPATH to something like /bin/true.
*
* This error is non-fatal, but no .CAB file will be built.
******************************************************************************
EOF
    exit 0;
}

while ( my $parm = shift( @ARGV ) ) {
    if ( $parm eq "-f" ) { $force = 1; next; }
    else { $path = $parm; }
}

usage() if !$path;

my $baseName = basename($path);
my $dirName = dirname($path);

$baseName =~ m|^(.*)\.xwd$|;
my $name = $1;
usage() if !$name;

my $infname = $name . ".inf";
if ( -f $infname && !$force ) {
    print STDERR "file $infname already exists and -f (clobber) flag "
        . "not passed\n";
    exit 1;
}

# build the .inf file
open INFFILE, "> $infname";

print INFFILE <<EOF;
[CEStrings]
InstallDir=%CE1%\\%AppName%
AppName="Crosswords"

[Strings]
CompanyName="Fixed Innovations"    

[Version]
Signature="\$Chicago\$"
CESignature="\$Windows CE\$"
Provider=%CompanyName%

[SourceDisksNames]
1=,"FILES",,$dirName

[SourceDisksFiles]
$baseName=1

[Files]
$baseName

[DefaultInstall]
CopyFiles=Files

[DestinationDirs]
Files=,%InstallDir%
EOF

close INFFILE;

# Then run it
system( "$cabwiz", $infname );
