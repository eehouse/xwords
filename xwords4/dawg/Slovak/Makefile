# -*-mode: Makefile; coding: utf-8; -*-
# Copyright 2002-2010 by Eric House (xwords@eehouse.org).  All rights reserved.
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

XWLANG = Slovak
LANGCODE = sk_SK
ENC = UTF-8
DICTNOTE = "compiled by a user from Slovak Scrabble Association dictionaries (Slovenský Spolok Scrabble) at www.hramescrabble.sk"

TARGET_TYPE ?= WINCE

include ../Makefile.langcommon

SOURCEDICT ?= $(XWDICTPATH)/Slovak/Scrabble_SK.dict

$(XWLANG)Main.dict.gz: $(SOURCEDICT) Makefile
	cat $< | tr -d '\r' | \
		sed 's/[[:lower:]]*/\U&/' | \
		sed 's,.,\U\0,g' | \
		grep '^[AÁÄBCČDĎEÉFGHIÍJKLĹĽMNŇOÔÓPRŔSŠTŤUÚVXYÝZŽ]*$$' | \
		gzip -c > $@

# Everything but creating of the Main.dict file is inherited from the
# "parent" Makefile.langcommon in the parent directory.

clean: clean_common
	rm -f $(XWLANG)Main.dict.gz *.bin $(XWLANG)*.pdb $(XWLANG)*.seb

help:
	@echo 'make [SOURCEDICT=$(XWDICTPATH)/$(XWLANG)/czech2_5.dict.gz]'
