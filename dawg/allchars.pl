#!/usr/bin/perl

# Print all ascii characters (for pasting into Makefiles etc. when you
# don't know what key combination produces them)

use strict;

for ( my $i = int(' '); $i <= 255; ++$i ) {
    printf "%.3d: %c\n", $i, $i;
}
