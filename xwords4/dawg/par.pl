#!/usr/bin/perl

# Copyright 2002 by Eric House (xwords@eehouse.org)   All rights reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

# Only enough of par's features to support building a crosswords dict
# pdb

use strict;

my $debug = 0;


# stolen from par source
my $PRC_FLAGS_RESOURCE =    (0x1<<0);
my $PRC_FLAGS_READONLY =   (0x1<<1);
my $PRC_FLAGS_DIRTY    =   (0x1<<2);
my $PRC_FLAGS_BACKUP   =   (0x1<<3);
my $PRC_FLAGS_NEWER    =   (0x1<<4);
my $PRC_FLAGS_RESET    =   (0x1<<5);
my $PRC_FLAGS_COPYPREVENT = (0x1<<6);
my $PRC_FLAGS_STREAM   =   (0x1<<7);
my $PRC_FLAGS_HIDDEN   =   (0x1<<8);
my $PRC_FLAGS_LAUNCHABLE = (0x1<<9);
my $PRC_FLAGS_RECYCLABLE = (0x1<<10);
my $PRC_FLAGS_BUNDLE   =   (0x1<<11);
my $PRC_FLAGS_OPEN     =   (0x1<<15);


my $gAttrs = 0;
my $gVersion = 1;               # par defaults this to 1

my $cmd = shift( @ARGV );
die "only 'c' supported now" if $cmd ne "c" && $cmd ne "-c";

readHOptions( \@ARGV );

my $dbfile = shift( @ARGV );
my $name = shift( @ARGV );
die "name $name too long" if length($name) > 31;
my $type = shift( @ARGV );
die "type $type must be of length 4" if length($type) != 4;
my $cid = shift( @ARGV );
die "cid $cid must be of length 4" if length($cid) != 4;

my @fileNames;
my @fileLengths;

my $nFiles = 0;

while ( @ARGV > 0 ) {
    my $filename = shift( @ARGV );
    push @fileNames, $filename;
    push @fileLengths, -s $filename;
    ++$nFiles;
}

# from par's prcp.h; thanks djw!
# typedef struct prc_file_t {
#     prc_byte_t name[32];
#     prc_byte_t flags[2];
#     prc_byte_t version[2];
#     prc_byte_t ctime[4];
#     prc_byte_t mtime[4];
#     prc_byte_t btime[4];
#     prc_byte_t modnum[4];
#     prc_byte_t appinfo[4];
#     prc_byte_t sortinfo[4];
#     prc_byte_t type[4];
#     prc_byte_t cid[4];
#     prc_byte_t unique_id_seed[4];
#     prc_byte_t next_record_list[4];
#     prc_byte_t nrecords[2];
# } prc_file_t;

my $str;
my $offset = 0;

open OUTFILE, "> $dbfile" or die "couldn't open outfile $dbfile for writing";

# print the string, then pad with 0s
$offset = length($name);
print OUTFILE $name;
while ( $offset < 32 ) {
    print OUTFILE pack("c", 0);
    ++$offset;
}

$str = pack("n", $gAttrs);     # flags
print OUTFILE $str;
$offset += length($str);

$str = pack("n", $gVersion);     # version
print OUTFILE $str;
$offset += length($str);

my $time = time() + 2082844800;
$str = pack("NNN", $time, $time, 0);     # ctime, mtime, btime
print OUTFILE $str;
$offset += length($str);

$str = pack("N", 0 );    # mod num
print OUTFILE $str;
$offset += length($str);

$str = pack("N", 0 );    # appinfo
print OUTFILE $str;
$offset += length($str);

$str = pack("N", 0 );    # sortinfo
print OUTFILE $str;
$offset += length($str);


print OUTFILE $type;            # type
print OUTFILE $cid;             # cid
$offset += 8;

$str = pack("NN", 0, 0 ); # unique_id_seed, next_record_list
print OUTFILE $str;
$offset += length($str);

$str = pack("n", $nFiles );     # nrecords
print OUTFILE $str;
$offset += length($str);

$offset += $nFiles * 8;
$offset += 2;                   # djw adds 2 bytes after size list; see below
foreach my $len ( @fileLengths ) {
    print OUTFILE pack( "N", $offset );
    print OUTFILE pack( "N", 0 );
    $offset += $len;
}

print OUTFILE pack( "n", 0 );   # djw does this sans comment: flush.c, line 87

foreach my $file ( @fileNames ) {
    open INFILE, "<$file" or die "couldn't open infile $file\n";
    my $buffer;
    while ( read INFILE, $buffer, 1024 ) {
        print OUTFILE $buffer;
    }
    close INFILE;
}


close OUTFILE;

exit 0;

##############################################################################
# Subroutines
##############################################################################

sub readHOptions {

    my ( $argvR ) = @_;
    
    for ( ; ; ) {
        my $opt = ${$argvR}[0];
    
        if ( $opt !~ /^-/ ) {
            last;
        }

        # it starts with a '-': use it; else don't consume anything
        shift @{$argvR};

        if ( $opt eq "-a" ) {
            my $attrs = shift @{$argvR};
            processAttrString( $attrs );
        } elsif ( $opt eq "-v" ) {
            $gVersion = shift @{$argvR};
        } else {
            die "what's with \"$opt\": -a and -v are the only hattrs supported";
        }
    }

} # readHOptions

sub processAttrString {

    my ( $attrs ) = @_;

    foreach my $flag ( split /\|/, $attrs ) {

        print STDERR "looking at flag $flag\n" if $debug;

        if ( $flag =~ /resource/ ) {
            $gAttrs |= $PRC_FLAGS_RESOURCE;
            die "resource attr not supported";
        } elsif ( $flag =~ /readonly/ ) {
            $gAttrs |= $PRC_FLAGS_READONLY;
        } elsif ( $flag =~ /dirty/ ) {
            $gAttrs |= $PRC_FLAGS_DIRTY;
        } elsif ( $flag =~ /backup/ ) {
            $gAttrs |= $PRC_FLAGS_BACKUP;
        } elsif ( $flag =~ /newer/ ) {
            $gAttrs |= $PRC_FLAGS_NEWER;
        } elsif ( $flag =~ /reset/ ) {
            $gAttrs |= $PRC_FLAGS_RESET;
        } elsif ( $flag =~ /copyprevent/ ) {
            $gAttrs |= $PRC_FLAGS_COPYPREVENT;
        } elsif ( $flag =~ /stream/ ) {
            $gAttrs |= $PRC_FLAGS_STREAM;
            die "stream attr not supported";
        } elsif ( $flag =~ /hidden/ ) {
            $gAttrs |= $PRC_FLAGS_HIDDEN;
        } elsif ( $flag =~ /launchable/ ) {
            $gAttrs |= $PRC_FLAGS_LAUNCHABLE;
        } elsif ( $flag =~ /recyclable/ ) {
            $gAttrs |= $PRC_FLAGS_RECYCLABLE;
        } elsif ( $flag =~ /bundle/ ) {
            $gAttrs |= $PRC_FLAGS_BUNDLE;
        } elsif ( $flag =~ /open/ ) {
            $gAttrs |= $PRC_FLAGS_OPEN;
        } else {
            die "flag $flag not supportd";
        }
    }
} # processAttrString
