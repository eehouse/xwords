#!/usr/bin/perl
#  Copyright 2004 by Eric House (fixin@peak.org).  All rights reserved.
# 
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

use strict;
use File::Basename;

my %funcInfo;
my %fileNames;
my %contents;

sub usage() {
    print STDERR "$0 funcList path .c_file .h_file\n"
}

my $funcList = shift(@ARGV);
my $pathList = shift(@ARGV);
my $dot_c = shift(@ARGV);
my $dot_h = shift(@ARGV);

usage() if ! defined($funcList) || !defined( $pathList )
    || ! defined($dot_c) || !defined( $dot_h );

# A list of types seen in the header files.  The idea is to use these
# to determine the size of a type and whether it's returned in d0 or
# a0.  The user function should print out types it doesn't find here
# and they should be added manually;

my %typeInfo = (
                "UInt32" => { "size" => 4, "a0" => 0 },
                "UInt16" => { "size" => 2, "a0" => 0 },
                "void*" => { "size" => 4, "a0" => 1 },
                "void" => { "size" => 0, "a0" => 0 },
                "BitmapPtr" => { "size" => 4, "a0" => 1 },
                "Boolean" => { "size" => 1, "a0" => 0 },
                "Boolean*" => { "size" => 4, "a0" => 0, "autoSwap" => 1 },
                "Char const*" => { "size" => 4, "a0" => 1 },
                "Char*" => { "size" => 4, "a0" => 1 },
                "Char**" => { "size" => 4, "a0" => 1 },
                "ControlType*" => { "size" => 4, "a0" => 1 },
                "Coord" => { "size" => 2, "a0" => 0 }, # ???
                "Coord*" => { "size" => 4, "a0" => 0 },
                "DateFormatType" => { "size" => 1, "a0" => 0 }, # enum
                "DateTimeType*" => { "size" => 4, "a0" => 0 },
                "DmOpenRef" => { "size" => 4, "a0" => 1 },
                "DmResID" => { "size" => 2, "a0" => 0 },
                "DmResType" => { "size" => 4, "a0" => 0 },
                "DmSearchStatePtr" => { "size" => 4, "a0" => 0 },
                "Err" => { "size" => 2, "a0" => 0 },
                "Err*" => { "size" => 4, "a0" => 1 },
                "EventPtr" => { "size" => 4, "a0" => 1 },
                "EventType*" => { "size" => 4, "a0" => 1 },
                "ExgDBWriteProcPtr" => { "size" => 4, "a0" => 1 },
                "ExgSocketType*" => { "size" => 4, "a0" => 1 },
                "FieldAttrPtr" => { "size" => 4, "a0" => 1 },
                "FieldType*" => { "size" => 4, "a0" => 1 },
                "FileInfoType*" => { "size" => 4, "a0" => 1 },
                "FileRef" => { "size" => 4, "a0" => 0 }, # UInt32
                "FileRef*" => { "size" => 4, "a0" => 1 },
                "FontID" => { "size" => 1, "a0" => 0 }, # enum
                "FormEventHandlerType*" => { "size" => 4, "a0" => 1 },
                "FormType*" => { "size" => 4, "a0" => 1 },
                "FrameType" => { "size" => 2, "a0" => 0 },
                "IndexedColorType" => { "size" => 1, "a0" => 0 }, # UInt8
                "Int16" => { "size" => 2, "a0" => 0 },
                "Int32" => { "size" => 4, "a0" => 0 },
                "Int8" => { "size" => 1, "a0" => 0 },
                "ListDrawDataFuncPtr" => { "size" => 4, "a0" => 1 },
                "ListType*" => { "size" => 4, "a0" => 1 },
                "LocalID" => { "size" => 4, "a0" => 0 },
                "LocalID*" => { "size" => 4, "a0" => 1, "autoSwap" => 4 },
                "MemHandle" => { "size" => 4, "a0" => 1 },
                "MemHandle*" => { "size" => 4, "a0" => 1 },
                "MemPtr" => { "size" => 4, "a0" => 1 },
                "MenuBarType*" => { "size" => 4, "a0" => 1 },
                "RectangleType*" => { "size" => 4, "a0" => 1 },
                "ScrollBarType*" => { "size" => 4, "a0" => 1 },
                "SndSysBeepType" => { "size" => 1, "a0" => 0 },
                "SysNotifyProcPtr" => { "size" => 4, "a0" => 1 },
                "TimeFormatType" => { "size" => 1, "a0" => 0 }, # enum
                "UInt16*" => { "size" => 4, "a0" => 1, "autoSwap" => 2 },
                "UInt32*" => { "size" => 4, "a0" => 1, "autoSwap" => 4 },
                "UInt8" => { "size" => 1, "a0" => 0 },
                "WinDirectionType" => { "size" => 1, "a0" => 0 }, # enum
                "WinDrawOperation" => { "size" => 1, "a0" => 0 }, # enum
                "WinHandle" => { "size" => 4, "a0" => 1 },
                "WinScreenModeOperation" => { "size" => 1, "a0" => 0 }, # enum
                "WindowFormatType" => { "size" => 1, "a0" => 0 }, # enum
                "char*" => { "size" => 4, "a0" => 1 },
                "const Char*" => { "size" => 4, "a0" => 1 },
                "const ControlType*" => { "size" => 4, "a0" => 1 },
                "const CustomPatternType*" => { "size" => 4, "a0" => 1 },
                "const EventType*" => { "size" => 4, "a0" => 1 },
                "const FieldAttrType*" => { "size" => 4, "a0" => 1 },
                "const FieldType*" => { "size" => 4, "a0" => 1 },
                "const FormType*" => { "size" => 4, "a0" => 1 },
                "const ListType*" => { "size" => 4, "a0" => 1 },
                "const RGBColorType*" => { "size" => 4, "a0" => 1 },
                "const RectangleType*" => { "size" => 4, "a0" => 1 },
                "const char*" => { "size" => 4, "a0" => 1 },
                "const void*" => { "size" => 4, "a0" => 1 },
                "FormObjectKind" => { "size" => 1, "a0" => 0 }, # enum
                "_Palm_va_list" => { "size" => 4, "a0" => 1 },  # it's a char*, likely never returned
                );

sub name_compact($) {
    my ( $name ) = @_;

    $name =~ s/^\s*//;
    $name =~ s/\s*$//;
    return $name;
}

sub type_compact($) {
    my ( $type ) = @_;
    #print STDERR "1. $type\n";
    $type = name_compact( $type );
    $type =~ s/\s*(\*+)$/$1/;      # remove spaces before any trailing *
    #print STDERR "2. $type\n";
    return $type;
}

# Split a string like "( type foo, type* bar, type *bazz, const type
# buzz )" into a list of hashes each with "type" and "name"
sub params_parse($) {
    my ( $params ) = @_;
    my @plist;

    #print STDERR "1. $params\n";

    # strip leading and training ws and params
    $params =~ s/^\s*\(//;
    $params =~ s/\)\s*$//;
    $params =~ s/^void$//;      # if just (void), nuke it

    #split based on commas
    my @params = split ",", $params;

    @params = map {
        s/^\s*//;               # leading ws
        s/\s*$//;               # trailing ws
        m|^([\w\s]+[\s\*]+)(\w+)$|; # split on space-or-star
        { "type" => type_compact($1), "name" => name_compact($2) };
    } @params;

#     foreach my $param (@params) {
#         print STDERR $$param{"type"}, "\n";
#         print STDERR $$param{"name"}, "\n";
#     }

#     print STDERR "got ", 0 + @params, "\n";
    return \@params;
}

sub clean_type($) {
    my ( $type ) = @_;
    # trip white space off the type
    $type =~ s/\s*\*/\*/;
    $type =~ s/^\s*//;
    $type =~ s/\s*$//;
    $type =~ s/^\s*(\S*)\s*$/$1/;

    my @out;
    my @terms = split( '\s+', $type );
    foreach my $term (@terms) {
        if ( defined( $typeInfo{$term} ) ) {
        } elsif ( "const" eq $term ) {
        } elsif ( "extern" eq $term ) {
        } else {
            next;
        }
        push( @out, $term );
    }
    $type = join( " ", @out );
    # print STDERR "clean_type => $type\n";

    return $type;
}


sub searchOneFile($$) {
    my ( $file, $function ) = @_;

    my $base = basename($file);
    my $contents = $contents{$base};

    if ( ! $contents ) {

        # read the file into a single long line, stripping
        # (imperfectly) comments.
        open FILE, "< $file";
        # print STDERR "looking at file $file\n";
        while( <FILE> ) {
            chomp;
            # remove // to eol comments
            s|//.*$||;
            $contents .= "$_ ";
        }

        close FILE;
        # strip /*....*/ comments
        $contents =~ s|/\*[^*]*\*+([^/*][^*]*\*+)*/||g;
        $contents{$base} = $contents;
        # print STDERR "$contents\n";
    }

    # a word-token, followed by at least ' ' or *, and another token.

    # This one strips 'const' in 'const type func(params)'
    # $contents =~ m/(\w+)([\s\*]+)$function\s*(\([^)]*\))/;

    if ( $contents =~ m/([\w\s]+)([\s\*]+)$function\s*(\([^)]*\))[^V]*(VFSMGR_TRAP)\(([\w]+)\);/ 
        || $contents =~ m/([\w\s]+)([\s\*]+)$function\s*(\([^)]*\))[^S]*(SYS_TRAP)\(([\w]+)\);/
        || $contents =~ m/([\w\s]+)([\s\*]+)$function\s*(\([^)]*\))[^G]*(GRF_TRAP)\(([\w]+)\);/ ) {

        # print STDERR "found something\n";

        my ( $type, $params, $trapType, $trapSel ) = ( "$1$2", $3, $4, $5 );
        # my ( $type, $params, $trapSel ) = ( $1, $2, $3 );
        if ( $type && $params ) {

            $type = clean_type($type);

            my $found = "$type<->$function<->$params<->$trapSel<->$trapType";
            print STDERR "$found\n";

            $params = params_parse($params);
            $funcInfo{$function} = { 'type' => $type,
                                     'params' => $params,
                                     'file' => $base,
                                     'sel' => $trapSel,
                                     'trapType' => $trapType,
                                 };
            $fileNames{$base} = 1;
            return 1;
        }
    }
    return 0;
} # searchOneFile

sub searchOneDir($$) {
    my ( $dir, $function ) = @_;

#    print STDERR "checking dir $dir\n";

    opendir(DIR, $dir) || die "can't opendir $dir: $!";
    my @files = readdir(DIR);
    closedir DIR;

    foreach my $file (@files) {
        if ( $file =~ m|^\.| ) {
            # skip if starts with .
        } elsif ( -d "$dir/$file" ) {
            searchOneDir( "$dir/$file", $function );
        } elsif ( $file =~ m|\.h$| ) {
            if ( searchOneFile( "$dir/$file", $function ) ) {
                last;
            }
        }
    }
} # searchOneDir

sub print_params_list($) {
    my ( $params ) = @_;
    my $result = "( ";

    foreach my $param ( @$params ) {
        $result .= $$param{"type"} . " ";
        $result .= $$param{'name'} . ", ";
    }

    $result =~ s/, $//;
    $result .= " )";

    return $result;
}

sub push_param($$$) {
    my ( $typ, $name, $offset ) = @_;
    my $result;
    my $info = $typeInfo{$typ};

    if ( ! $info ) {
        die "type \"$typ\" (name: $name) not listed\n";
    } else {

        my $size = $$info{'size'};

        $result .= "    ADD_TO_STACK$size($name, $$offset),\n";
        if ( $size == 1 ) {
            $$offset += 2;
        } else {
            $$offset += $size;
        }
    }

    return "$result";
}

sub swap_param($$$) {
    my ( $type, $name, $out ) = @_;
    my $result = "";

    my $info = $typeInfo{$type};
    die "unknown type $type\n" if !defined $info;

    my $size = $info->{'autoSwap'};
    if ( $size ) {
        die "not sure what to do swapping struct\n" if $size > 4;
        $result .= "    SWAP${size}_NON_NULL_";
        $result .= $out? "OUT" : "IN";
        $result .= "($name);\n";
    }

    return $result;
}

sub print_body($$$$$) {
    my ( $name, $returnType, $params, $trapSel, $trapType ) = @_;
    my $result;
    my $offset = 0;
    my $notVoid = $returnType !~ m|void$|;

    $result .= "{\n";
    if ( $notVoid ) {
        $result .= "    $returnType result;\n";
    }
    $result .= "    FUNC_HEADER($name);\n";

    foreach my $param ( @$params ) {
        $result .= swap_param( $$param{"type"}, $$param{"name"}, 0 );
    }
    $result .= "   {\n";
    $result .= "    PNOState* sp = GET_CALLBACK_STATE();\n";
    $result .= "    unsigned char stack[] = {\n";

    foreach my $param ( @$params ) {
        $result .= push_param( $$param{"type"}, 
                                $$param{"name"},
                                \$offset );
    }
    $result .= "    0 };\n";

    my $info = $typeInfo{$returnType};
    if ( $info->{'a0'} ) {
        $offset = "kPceNativeWantA0 | $offset";
    }

    if ( $trapType eq "VFSMGR_TRAP" ) {
        $result .= "    SET_SEL_REG($trapSel, sp);\n";
        $trapSel = "sysTrapFileSystemDispatch";
    } else {
    }

    $result .= "    ";
    if ( $notVoid ) {
        $result .= "result = ($returnType)";
    }

    $result .= "(*sp->call68KFuncP)( sp->emulStateP, \n"
        . "                               PceNativeTrapNo($trapSel),\n"
        . "                               stack, $offset );\n";

    foreach my $param ( @$params ) {
        $result .= swap_param( $$param{"type"}, $$param{"name"}, 1 );
    }

    $result .= "    }\n    FUNC_TAIL($name);\n";
    if ( $notVoid ) {
        $result .= "    return result;\n";
    }
    $result .= "} /* $name */\n";

    return $result;
}

sub print_func_impl($$$$$$) {
    my ( $returnType, $name, 
         $params,       # ref-to-list created by params_parse above
         $file, $trapSel, $trapType ) = @_;
    my $result;
    
    $result .= "\n/* from file $file */\n";
    $result .= "$returnType\n";
    $result .= "$name" . print_params_list($params) . "\n";
    $result .= print_body( $name, $returnType, $params, $trapSel, $trapType );
         
    return $result;
} # print_func_impl


###########################################################################
# Main
###########################################################################
open LIST, "<$funcList";
while ( <LIST> ) {
    chomp;

    # comments?
    s/\#.*$//;
    # white space
    s/\s*(\w+)\s*/$1/;
    next if ! length;

    my $func = $_;
    print STDERR "looking for $func\n";

    my $found = 0;
    open PATHS, "< $pathList";
    while ( <PATHS> ) {
        chomp;
        my $path = $_;
        if ( -d $path ) {
            searchOneDir( $path, $func );
        } elsif ( -e $path ) {
            $found = searchOneFile( $path, $func );
            if ( $found ) {
                last;
            }
        }
    }
    close PATHS;

#    if ( !$found ) {
#        print STDERR "ERROR $func not found\n";
#    }
}
close LIST;



open DOT, "> $dot_c";
print DOT "/********** this file is autogenerated by $0 ***************/\n\n";

#map { print DOT "#include <$_>\n"; } keys(%fileNames);

print DOT "\n";

print DOT <<EOF;

\#include "pnostate.h"
\#include "pace_gen.h"
\#include "pace_man.h"

EOF

foreach my $key (keys %funcInfo) {
    my $ref = $funcInfo{$key};
    my $type = $${ref}{"type"};
    
    $type =~ s/extern\s*//;        # "extern" doesn't belong in implementation

    $type =~ s/(\S+)\s*$/$1/;   # trailing whitespace
    $type =~ s/^\s*(.*)/$1/;    # leading ws

    my $funcstr = print_func_impl( $type,
                                   $key,
                                   $$ref{'params'},
                                   $$ref{'file'},
                                   $$ref{'sel'},
                                   $$ref{'trapType'});
    print DOT $funcstr;
}

print DOT "#include \"pace_man.c\"\n"; 


close DOT;



open DOT, "> $dot_h";
print DOT "/********** this file is autogenerated by $0 ***************/\n\n";

my $def = "_" . uc($dot_h) . "_";
$def =~ s/\./_/;

print DOT "#ifndef $def\n";
print DOT "#define $def\n";

map { print DOT "#include <$_>\n"; } keys(%fileNames);

foreach my $key (keys %funcInfo) {
    my $ref = $funcInfo{$key};
    print DOT $${ref}{"type"}, " "
        . $key . print_params_list($$ref{'params'}) . ";\n"; 
}

print DOT "\n#include \"pace_man.h\"\n"; 
print DOT "#endif /* $def */\n";
close DOT;
