# -*- mode: makefile; -*-
# Copyright 2002-2009 by Eric House (xwords@eehouse.org).  All rights
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

XWLANG = Hex
LANGCODE = hex
ENC = UTF-8

TARGET_TYPE = WINCE

include ../Makefile.2to8

include ../Makefile.langcommon

# Pass in your own dict here by setting DICT
# DICT ?= $(XWDICTPATH)/English/CSW.dict.gz
DICT ?= $(XWDICTPATH)/English/COSD.dict.gz

# tr 'AE' 'ÄË' doesn't work, so use sed.  Note that although we're
# pulling words up to 15 letters in length there are none longer than
# seven in this dict.  So all three dicts will be the same length and
# only one is worth publishing.
$(XWLANG)Main.dict.gz: $(DICT)
	@echo "building $@ from $<"
	zcat $< | tr [a-f] [A-F] | grep -e '^[A-F]\{2,15\}$$' | \
		echo CAFEBABE DEADBEEF $$(cat -) | \
		sed 's/A/Ä/g' | \
		sed 's/E/Ë/g' | \
		tr ' ' '\n' | sort | gzip > $@

# Everything but creating of the Main.dict file is inherited from the
# "parent" Makefile.langcommon in the parent directory.

clean: clean_common
	rm -f $(XWLANG)Main.dict.gz *.bin $(XWLANG)*.pdb $(XWLANG)*.seb
