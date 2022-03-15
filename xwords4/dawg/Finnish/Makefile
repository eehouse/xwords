# -*-mode:  Makefile; compile-command:  "make TARGET_TYPE=WINCE";  -*-
# Copyright 2002 - 2022 by Eric House (xwords@eehouse.org).  All
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

XWLANG=Finish
LANGCODE=fi_FI
TARGET_TYPE ?= WINCE
ENC = UTF-8
DICTNOTE = "Built from https://kaino.kotus.fi/sanat/nykysuomi"

include ../Makefile.langcommon

SOURCEDICT ?= $(XWDICTPATH)/Finnish/Kotus_2007_0to15.txt

$(XWLANG)Main.dict.gz: $(SOURCEDICT)
	cat $< \
	| tr -d '\r' \
	| sed 's/[[:lower:]]*/\U&/' \
	| grep '^[AÄBCDEFGHIJKLMNOÖPRSTUVWY]*$$' \
	| sort -u \
	| gzip - > $@

clean: clean_common
	rm -rf *.saved $(XWLANG)Main.dict.gz
