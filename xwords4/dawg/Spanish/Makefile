# -*-mode: Makefile; compile-command: "make all"; coding: utf-8; -*-
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

XWLANG = SpanishFAA41
LANGCODE = es_ES
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
		"CH ch Ch cH" $(PBITMS)/large_ch.pbitm $(PBITMS)/small_ch.pbitm \
		"LL ll Ll lL" $(PBITMS)/large_ll.pbitm $(PBITMS)/small_ll.pbitm \
		"RR rr Rr rR" $(PBITMS)/large_rr.pbitm $(PBITMS)/small_rr.pbitm \

include ../Makefile.langcommon

#$(LANG)Main.dict.gz: SpanishMain.dict.gz
#	ln -s $< $@

SOURCEDICT ?= $(XWDICTPATH)/Spanish/FAA_4.1.txt

$(XWLANG)Main.dict.gz: $(SOURCEDICT) Makefile
	cat $< \
	| tr -d '\r' \
	| sed 's/[[:lower:]]*/\U&/' \
	| LANG=$(LANGCODE):$(ENC) grep '^[A-JL-VX-ZÑ]*$$' \
	| sed 's/CH/1/g' \
	| sed 's/LL/2/g' \
	| sed 's/RR/3/g' \
	| tr '123' '\001\002\003' \
	| gzip - > $@

clean: clean_common
	rm -rf *.saved
