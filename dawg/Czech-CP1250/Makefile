# -*-mode: Makefile; coding: windows-1250; -*-
# Copyright 2002-2008 by Eric House (xwords@eehouse.org).  All rights reserved.
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

XWLANG=Czech-CP1250
LANGCODE=cs_CS

TARGET_TYPE ?= PALM

include ../Makefile.2to8

include ../Makefile.langcommon

SOURCEDICT ?= $(XWDICTPATH)/$(XWLANG)/czech2_5.dict.gz

$(XWLANG)Main.dict.gz: $(SOURCEDICT) Makefile
	export LC_ALL=$(LANGCODE); \
		 zcat $< | \
			tr [aábcčdďeéěfghiíjklmnňoóprřsštťuúůvxyýzž] [AÁBCČDĎEÉĚFGHIÍJKLMNŇOÓPRŘSŠTŤUÚŮVXYÝZŽ] | \
			grep '^[AÁBCČDĎEÉĚFGHIÍJKLMNŇOÓPRŘSŠTŤUÚŮVXYÝZŽ]\+$$' | \
			gzip -c > $@

# Everything but creating of the Main.dict file is inherited from the
# "parent" Makefile.langcommon in the parent directory.

clean: clean_common
	rm -f $(XWLANG)Main.dict.gz *.bin $(XWLANG)*.pdb $(XWLANG)*.seb

help:
	@echo 'make [SOURCEDICT=$(XWDICTPATH)/$(XWLANG)/czech2_5.dict.gz]'
