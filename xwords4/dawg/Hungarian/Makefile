# -*-mode: Makefile; -*-
# Copyright 2002-2020 by Eric House (xwords@eehouse.org).  All rights
# reserved.
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

XWLANG=Hungarian
LANGCODE=hu_HU
ENC = UTF-8
TARGET_TYPE ?= WINCE

DICTNOTE = "This wordlist contains the tile information for Hungarian but no words."

LANG_SPECIAL_INFO = \
		"CS Cs cS cs" /dev/null /dev/null \
		"GY Gy gY gy" /dev/null /dev/null \
		"LY Ly lY ly" /dev/null /dev/null \
		"NY Ny nY ny" /dev/null /dev/null \
		"SZ Sz sZ sz" /dev/null /dev/null \
		"TY Ty tY ty" /dev/null /dev/null \
		"ZS Zs zS zs" /dev/null /dev/null \

include ../Makefile.langcommon

# Empty dict
$(XWLANG)Main.dict.gz:
	echo -n "" | gzip -c > $@

# Everything but creating of the Main.dict file is inherited from the
# "parent" Makefile.langcommon in the parent directory.

clean: clean_common
	rm -f $(XWLANG)Main.dict.gz *.bin $(XWLANG)*.pdb $(XWLANG)*.seb
