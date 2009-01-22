# -*-mode: Makefile; compile-command: "make TARGET_TYPE=WINCE"; -*-
# Copyright 2002-2009 by Eric House (xwords@eehouse.org).  All rights reserved.
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

XWLANG=DANOSC
LANGCODE=ca_ES
TARGET_TYPE ?= WINCE
ENC = UTF-8

ifeq ($(TARGET_TYPE),PALM)
PBITMS = ./bmps/palm
else
ifeq ($(TARGET_TYPE),FRANK)
PBITMS = ./bmps/franklin
else
ifeq ($(TARGET_TYPE),WINCE)
PBITMS = ./bmps/franklin
endif
endif
endif

LANG_SPECIAL_INFO = \
		"L-L" $(PBITMS)/large_ll.pbitm $(PBITMS)/small_ll.pbitm \
		"NY" $(PBITMS)/large_ny.pbitm $(PBITMS)/small_ny.pbitm \
		"QU" $(PBITMS)/large_qu.pbitm $(PBITMS)/small_qu.pbitm \

include ../Makefile.2to8

include ../Makefile.langcommon

SOURCEDICT ?= $(XWDICTPATH)/Catalan/DANOSC.txt.gz

$(XWLANG)Main.dict.gz: $(SOURCEDICT) Makefile
	zcat $< \
	| tr -d '\r' \
	| tr [a-zç] [A-ZÇ] \
	| sed 's/L·L/1/g' \
	| sed 's/L\.L/1/g' \
	| sed 's/L-L/1/g' \
	| sed 's/NY/2/g' \
	| sed 's/QU/3/g' \
	| grep '^[[1-3ÇA-JL-PR-VXZ]*$$' \
	| tr '123' '\001\002\003' \
	| gzip - > $@

clean: clean_common
	rm -rf *.saved