# -*-mode: Makefile; coding: iso-8859-1; -*-
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

XWLANG=Swedish
LANGCODE=sv_SE
ENC = ISO-8859-1

# Swedish has too many chars for the old format.
NEWDAWG=whatever

TARGET_TYPE ?= WINCE

include ../Makefile.2to8

include ../Makefile.langcommon

SOURCEDICT ?= $(XWDICTPATH)/Swedish/swedish15.dict.gz

# Q and W are not available as tiles, but I'm told there's a custom in
# Swedish play of allowing blanks to stand for those letters as well.
# So we don't exclude words with those letters from the dictionary.
$(XWLANG)Main.dict.gz: $(SOURCEDICT) Makefile
	zcat $< | tr [a-zäåæöü] [A-ZÄÅÆÖÜ] | \
		LANG=$(LANGCODE):$(ENC) grep '^[A-ZÄÅÆÖÜ]\{2,15\}$$' | \
		gzip -c > $@

# Everything but creating of the Main.dict file is inherited from the
# "parent" Makefile.langcommon in the parent directory.

clean: clean_common
	rm -f $(XWLANG)Main.dict.gz *.bin $(XWLANG)*.pdb $(XWLANG)*.seb

