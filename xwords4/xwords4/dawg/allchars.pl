#!/usr/bin/perl

use strict;

for (my $i = 1; $i < 255; ++$i ) {
    printf( "%d: %s (0x%x)\n", $i, chr($i), $i );
}
