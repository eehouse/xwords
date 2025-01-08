# -*- mode: makefile; compile-command: "make -f Makefile.CSW12"; -*-
# Copyright 2002-2022 by Eric House (xwords@eehouse.org).  All rights
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

XWLANG=CSW24
LANGCODE=en_US
TARGET_TYPE=WINCE
DICTNOTE = "User-contributed file augmented with \"Complete New Words in CSW24\""

include ../Makefile.langcommon

SOURCEDICT ?= $(XWDICTPATH)/English/CSW21.txt

# I build CSW24 by merging in the user-supplied list of additions to
# CSW21. No point in making a new wordlist to track.
ADDS = $(XWDICTPATH)/English/CSW24_adds.txt

$(XWLANG)Main.dict.gz: $(SOURCEDICT) $(ADDS)
	cat $^ | tr -d '\r' | sort -u | \
		sed 's/[[:lower:]]*/\U&/' | \
		grep -e "^[A-Z]\{2,15\}$$" | \
		gzip -c > $@

# Everything but creating of the Main.dict file is inherited from the
# "parent" Makefile.langcommon in the parent directory.

clean: clean_common
	rm -f $(XWLANG)Main.dict.gz *.bin $(XWLANG)*.pdb $(XWLANG)*.seb 
