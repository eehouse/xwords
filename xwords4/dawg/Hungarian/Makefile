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

DICTNOTE = "Derived from szotar/alap/ in https://github.com/laszlonemeth/magyarispell.git"

LANG_SPECIAL_INFO = \
		"CS Cs cS cs" /dev/null /dev/null \
		"GY Gy gY gy" /dev/null /dev/null \
		"LY Ly lY ly" /dev/null /dev/null \
		"NY Ny nY ny" /dev/null /dev/null \
		"SZ Sz sZ sz" /dev/null /dev/null \
		"TY Ty tY ty" /dev/null /dev/null \
		"ZS Zs zS zs" /dev/null /dev/null \

include ../Makefile.langcommon

# Filtering wordlist: When a word can be spelled with either a
# double-letter tile or two single-letter tiles, it's in the list with
# both spellings. That's what the longer sed expressions are doing
# (emitting two words)
$(XWLANG)Main.dict.gz:
	cat hungarian_wordlist.txt \
	| tr -d '\r' \
	| tr [aábcdeéfghiíjklmnnyoóöőprtuúüűvzs] [AÁBCDEÉFGHIÍJKLMNNYOÓÖŐPRTUÚÜŰVZS] \
	| grep -v '1\|2\|3\|4\|5\|6\|7' \
	| sed -e 's,^\(.*\)CS\(.*\)$$,\11\2\n\1CS\2,g' \
	| sed -e 's,GY,2,g' \
	| sed -e 's,LY,3,g' \
	| sed -e 's,NY,4,g' \
	| sed -e 's,^\(.*\)SZ\(.*\)$$,\15\2\n\1SZ\2,g' \
	| sed -e 's,TY,6,g' \
	| sed -e 's,^\(.*\)ZS\(.*\)$$,\17\2\n\1ZS\2,g' \
	| grep '^[1-7AÁBCDEÉFGHIÍJKLMNOÓÖŐPRSTUÚÜŰVZ]*$$' \
	| tr '1234567' '\001\002\003\004\005\006\007' \
	| gzip -c > $@

# Everything but creating of the Main.dict file is inherited from the
# "parent" Makefile.langcommon in the parent directory.

clean: clean_common
	rm -f $(XWLANG)Main.dict.gz *.bin $(XWLANG)*.pdb $(XWLANG)*.seb
