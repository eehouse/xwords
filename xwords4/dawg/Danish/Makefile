# -*- mode: makefile -*-
# Copyright 2002-2005 by Eric House (xwords@eehouse.org).  All rights reserved.
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

LANG=Danish
LANGCODE=da_DK

TARGET_TYPE ?= PALM

include ../Makefile.2to8

include ../Makefile.langcommon

SOURCEDICT ?= $(XWDICTPATH)/$(LANG)/LarsDanish.dict.gz

$(LANG)Main.dict.gz: $(SOURCEDICT) Makefile
	zcat $< | tr -d '\r' | tr [a-zåæø] [A-ZÅÆØ] | \
		grep '[AEIOUÅÆØ]' | \
		grep '^[A-PR-VX-ZÅÆØ]\+$$' | sort -u | \
		gzip -c > $@

# Everything but creating of the Main.dict file is inherited from the
# "parent" Makefile.langcommon in the parent directory.

clean: clean_common
	rm -f $(LANG)Main.dict.gz *.bin $(LANG)*.pdb $(LANG)*.seb

help:
	@echo 'make [SOURCEDICT=LarsDanish.dict.gz] [TARGET_TYPE=WINCE|PALM]'
