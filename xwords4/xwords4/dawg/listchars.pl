#!/usr/bin/perl

# Copyright 2001 by Eric House
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

use strict;

my %lettersHash;


while ( <> ) {
    chomp;
    foreach my $byte (split //) {
        ++$lettersHash{$byte};
    }
}

foreach my $key (sort keys(%lettersHash)) {
    my $count = $lettersHash{$key};
    if ( $count ) {
        printf( "%.3d: %s: %.7d\n", ord($key), $key, $count );
    }
}
