# Copyright 2002 by Eric House (xwords@eehouse.org).  All rights reserved.
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

XWLANG = Hex
LANGCODE = hex

TARGET_TYPE = WINCE

include ../Makefile.2to8

include ../Makefile.langcommon

# Pass in your own dict here by setting DICT
DICT ?= $(XWDICTPATH)/English/SOWPODS_official.txt.gz

# Feel free to base this on whatever dictionary you have at hand.  I'm
# using CollegeEng for no particular reason.
$(XWLANG)Main.dict.gz: $(DICT)
	@echo "building $@ from $<"
	zcat $< | tr [a-f] [A-F] | grep -e '^[A-F]\{2,8\}$$' | \
		echo CAFEBABE DEADBEEF $$(cat -) | \
		tr ' ' '\n' | sort | gzip > $@

# Everything but creating of the Main.dict file is inherited from the
# "parent" Makefile.langcommon in the parent directory.

clean: clean_common
	rm -f $(XWLANG)Main.dict.gz *.bin $(XWLANG)*.pdb $(XWLANG)*.seb
