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
    print STDERR "$0 \\\n" .
        "\t[-oh out.h] \\\n" .
        "\t[-oc out.c] \\\n" .
        "\t[-file palmheader.h|palm_header_dir] (can repeat) \\\n" .
        "\t[-func func_name_or_list_file] (can repeat)\n";

    exit 1;
}

# Given the name of a palm function, or of a file containing a list of
# such, return a list of function names.  In the first case it'll be a
# singleton.
sub makeFuncList($) { 
    my ( $nameOrPath ) = @_; 
    my @result;

    if ( open LIST, "< $nameOrPath" ) {
        while ( <LIST> ) {
            chomp;

            # comments?
            s/\#.*$//;
            # white space
            s/\s*(\w+)\s*/$1/;
            next if ! length;

            push( @result, $_ );
        }
        close LIST;
    } else {
        # must be a simple function name
        push( @result, $nameOrPath );
    }
    return @result;
} # makeFuncList

sub makeFileList($) {
    my ( $fileOrDir ) = @_;
    my @result;
    push @result, $fileOrDir;
    return @result;
} # makeFileList

my @funcList;
my @pathList;
my $dot_c;
my $dot_h;

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
                "Coord*" => { "size" => 4, "a0" => 0, "autoSwap" => 2 },
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
                "ExgSocketType*" => { "size" => 4, "a0" => 1, "autoSwap" => -1 },
                "FieldAttrPtr" => { "size" => 4, "a0" => 1 },
                "FieldType*" => { "size" => 4, "a0" => 1 },
                "FileInfoType*" => { "size" => 4, "a0" => 1, "autoSwap" => -1 },
                "FileRef" => { "size" => 4, "a0" => 0 }, # UInt32
                "FileRef*" => { "size" => 4, "a0" => 1, "autoSwap" => 4 },
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
                "RectangleType*" => { "size" => 4, "a0" => 1,
                                      "autoSwap" => -1 },
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
                "const CustomPatternType*" => { "size" => 4, "a0" => 1 }, # UInt8[8]; no need to translate
                "const EventType*" => { "size" => 4, "a0" => 1 },
                "const FieldAttrType*" => { "size" => 4, "a0" => 1 },
                "const FieldType*" => { "size" => 4, "a0" => 1 },
                "const FormType*" => { "size" => 4, "a0" => 1 },
                "const ListType*" => { "size" => 4, "a0" => 1 },
                "const RGBColorType*" => { "size" => 4, "a0" => 1 },
                "const RectangleType*" => { "size" => 4, "a0" => 1,
                                        "autoSwap" => -1 },
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

sub nameToBytes($) {
    my ( $nam ) = @_;
    return "\"'" . join( "','", split( //, $nam ) ) . "'\"";
}

sub makeSwapStuff($$$$$) {
    my ( $params, $varDecls, $swapIns, $pushes, $swapOuts ) = @_;
    my $sizeSum = 0;
    my $vcount = 0;

    $$varDecls .= "    /* var decls */\n";
    $$pushes   .= "    /* pushes */\n";
    $$swapOuts .= "    /* swapOuts */\n";
    $$swapIns  .= "    /* swapIns */\n";

    foreach my $param ( @$params ) {
        # each param has "type" and "name" fields
        my $type = $param->{"type"};
        my $info = $typeInfo{$type};
        my $size = $info->{'size'};
        my $name = $param->{'name'};
        my $pushName = $name;

        # If type requires swapping, just swap in and out.  If it's
        # RECT (or later, other things) declare a tmp var and swap in
        # and out.  If it's const, only swap in.

        my $swapInfo = $info->{"autoSwap"};
        if ( defined $swapInfo ) {

            if ( $swapInfo == -1 ) {
                my $typeNoStar = $type;
                $typeNoStar =~ s/\*$//;
                die "no star found" if $typeNoStar eq $type;
                my $isConst = $typeNoStar =~ m|^const|;
                $typeNoStar =~ s/^const\s+//;
                my $vName = "${typeNoStar}_68K${vcount}";
                $pushName = "&$vName";
                $$varDecls .= "    $typeNoStar $vName;\n";
                $typeNoStar = uc($typeNoStar);
                $$swapIns .= "    SWAP_${typeNoStar}_ARM_TO_68K( &$vName, $name );\n";
                if ( ! $isConst ) {
                    $$swapOuts .= "    SWAP_${typeNoStar}_68K_TO_ARM( $name, &$vName );\n";
                }

            } else {
                if ( $swapInfo >= 1 && $swapInfo <= 4 ) {
                    $$swapIns .= "    SWAP${swapInfo}_NON_NULL_IN($name);\n";
                    $$swapOuts .= "    SWAP${swapInfo}_NON_NULL_OUT($name);\n";
                } else {
                    die "unknown swapInfo $swapInfo\n";
                }
            }
        }

        $$pushes .= "    ADD_TO_STACK$size(stack, $pushName, $sizeSum);\n";

        $sizeSum += $size;
        if ( $size == 1 ) {
            ++$sizeSum;
        }
        ++$vcount;
    }
    return $sizeSum;
} # makeSwapStuff


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

    my ( $varDecls, $swapIns, $pushes, $swapOuts );
    $offset = makeSwapStuff( $params, \$varDecls, 
                             \$swapIns, \$pushes, \$swapOuts );

    $result .= $varDecls . $swapIns;

    $result .= "    {\n";
    $result .= "    PNOState* sp = GET_CALLBACK_STATE();\n";

    $result .= "    STACK_START(unsigned char, stack, $offset);\n"
        . $pushes 
        . "    STACK_END(stack);\n";

    my $info = $typeInfo{$returnType};
    if ( $info->{'a0'} ) {
        $offset = "kPceNativeWantA0 | $offset";
    }

    if ( $trapType eq "VFSMGR_TRAP" ) {
        $result .= "    SET_SEL_REG($trapSel, sp);\n";
        $trapSel = "sysTrapFileSystemDispatch";
    } elsif( $trapType eq "GRF_TRAP" || $trapType eq "SYS_TRAP"  ) {
        # they're the same according to Graffiti.h
    } else {
        die "unknown dispatch type";
    }

    $result .= "    ";
    if ( $notVoid ) {
        $result .= "result = ($returnType)";
    }

    $result .= "(*sp->call68KFuncP)( sp->emulStateP, \n"
        . "                               PceNativeTrapNo($trapSel),\n"
        . "                               stack, $offset );\n";

    $result .= $swapOuts;

    $result .= "    }\n    FUNC_TAIL($name);\n";
    $result .= "    EMIT_NAME(\"$name\"," . nameToBytes($name) . ");\n";
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


while ( my $arg = shift(@ARGV) ) {
    if ( $arg eq "-oh" ) {
        $dot_h = shift(@ARGV);        
    } elsif ( $arg eq "-oc" ) {
        $dot_c = shift(@ARGV);
    } elsif ( $arg eq "-file" ) {
        push( @pathList, makeFileList(shift(@ARGV)) );
    } elsif ( $arg eq "-func" ) {
        push( @funcList, makeFuncList(shift(@ARGV)) );
    } else {
        usage();
    }
}

foreach my $func (@funcList) {

    print STDERR "looking for $func\n";

    my $found = 0;
    foreach my $path (@pathList) {
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
}

my $outRef;

if ( $dot_c ) {
    if ( $dot_c eq "-" ) {
        $outRef = *STDOUT{IO};
    } else {
        open DOT, "> $dot_c";
        $outRef = *DOT{IO};
    }
    print $outRef "/********** this file is autogenerated by $0 "
        . "***************/\n\n";

    print $outRef "\n";

    print $outRef <<EOF;

\#include "pnostate.h"
\#include "pace_gen.h"
\#include "pace_man.h"

EOF

    foreach my $key (keys %funcInfo) {
        my $ref = $funcInfo{$key};
        my $type = $${ref}{"type"};
    
        $type =~ s/extern\s*//;  # "extern" doesn't belong in implementation

        $type =~ s/(\S+)\s*$/$1/;   # trailing whitespace
        $type =~ s/^\s*(.*)/$1/;    # leading ws

        my $funcstr = print_func_impl( $type,
                                       $key,
                                       $$ref{'params'},
                                       $$ref{'file'},
                                       $$ref{'sel'},
                                       $$ref{'trapType'});
        print $outRef $funcstr;
    }

    if ( $dot_c ne "-" ) {
        close DOT;
    }
}

if ( $dot_h ) {
    open DOT, "> $dot_h";
    print DOT "/********** this file is autogenerated by $0 "
        . "***************/\n\n";
    
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
}


exit 0;
