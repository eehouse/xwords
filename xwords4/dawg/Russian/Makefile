# -*- coding: utf-8; -*-
# Copyright 2002-2007 by Eric House (xwords@eehouse.org).  All rights reserved.
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

XWLANG = Russian
LANGCODE = ru_RU
ENC = UTF-8
DICT2DAWGARGS = -r

TARGET_TYPE ?= WINCE

include ../Makefile.langcommon

SOURCEDICT ?= $(XWDICTPATH)/Russian/RU5000.txt.gz

$(XWLANG)Main.dict.gz: $(SOURCEDICT) Makefile
	zcat $< | tr -d '\r' | \
		iconv -f ISO_8859-2 -t utf8 | \
		sed 's,.,\U\0,g' | \
		grep '^[ŔÁÂĂÄĹĆÇČÉĘËĚÍÎĎĐŃŇÓÔŐÖ×ŘŮÚŰÜÝŢß]*$$' | \
		gzip -c > $@

# Everything but creating of the Main.dict file is inherited from the
# "parent" Makefile.langcommon in the parent directory.

clean: clean_common
	rm -f $(XWLANG)Main.dict.gz *.bin $(XWLANG)*.pdb $(XWLANG)*.seb

