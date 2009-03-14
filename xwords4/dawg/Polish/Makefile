# -*- coding: iso-8859-2; mode: Makefile; -*-
# Copyright 2002 - 2009 by Eric House (xwords@eehouse.org).  All
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

XWLANG=Polish
LANGCODE=pl_PL
ENC = ISO-8859-2

# DICT2DAWGARGS = -lang $(LANGCODE)
# DICT2DAWGARGS = -debug

TARGET_TYPE ?= WINCE

include ../Makefile.2to8

include ../Makefile.langcommon

SOURCEDICT ?= $(XWDICTPATH)/Polish/iso-8859-2/slowa.txt.gz

$(XWLANG)Main.dict.gz: $(SOURCEDICT)
	zcat $< | tr -d '\r' \
	| LANG=$(LANGCODE):$(ENC) tr [aąbcćdeęfghijklłmnńoóprsśtuwyzźż] [AĄBCĆDEĘFGHIJKLŁMNŃOÓPRSŚTUWYZŹŻ] \
    | LANG=$(LANGCODE):$(ENC) grep '^[AĄBCĆDEĘFGHIJKLŁMNŃOÓPRSŚTUWYZŹŻ]*$$' \
    | gzip > $@

# Everything but creating of the Main.dict file is inherited from the
# "parent" Makefile.langcommon in the parent directory.

clean: clean_common
	rm -f $(XWLANG)Main.dict.gz *.bin $(XWLANG)*.pdb $(XWLANG)*.seb
