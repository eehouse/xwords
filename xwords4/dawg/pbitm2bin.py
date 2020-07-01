#!/usr/bin/env python3
#
# Copyright  2001-2020  by  Eric  House (xwords@eehouse.org).   All
# rights reserved.
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
#
#
# Given a pbitm on stdin, a text bitmap file where '#' indicates a set
# bit and '-' indicates a clear bit, convert into binary form (on
# stdout) where there's one bit per bit plus a byte each for the width
# and height.  Nothing for bitdepth at this point.  And no padding: if
# the number of bits in a row isn't a multiple of 8 then one byte will
# hold the last bits of one row and the first of another.

import os, struct, sys

lines = [ line.strip() for line in sys.stdin ]
nRows = len(lines)
nCols = 0
bits = ''
    
# first gather information and sanity-check the data

for line in lines:
    lineLen = len(line)
    if nCols == 0:
        nCols = lineLen
    else:
        assert nCols == lineLen, 'line of inconsistent length'
    bits += line

with os.fdopen(sys.stdout.fileno(), "wb", closefd=False) as stdout:
    stdout.write(struct.pack('B', nCols))

    # if we've been given an empty file, print out a single null byte
    # and be done.  That'll be the convention for "non-existant
    # bitmap".

    if nCols > 0:
        stdout.write(struct.pack( 'B', nRows ) )
        print( 'emitting {}x{} bitmap'.format(nCols, nRows), file=sys.stderr)

        while bits:
            cur = bits[:8]
            bits = bits[8:]

            byt = 0
            for indx in range(len(cur)):
                ch = cur[indx]
                assert ch == '-' or ch == '#', "char {} neither '#' nor '-'".format(ch)
                if ch == '#': byt |= 1 << (7 - indx)

            stdout.write(struct.pack( 'B', byt ))

    stdout.flush()
