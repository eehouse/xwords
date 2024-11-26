# -*-mode: Makefile; compile-command: "make TARGET_TYPE=WINCE"; -*-
# Copyright 2002-2024 by Eric House (xwords@eehouse.org).  All rights
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

XWLANG=Basque
LANGCODE=eu
TARGET_TYPE ?= WINCE
ENC = UTF-8

DICTNOTE = "List provided by Tonino Licciardello (Spain)"

# not used yet
# DICTREV = 8dea44bb695f8e18dd64a24d96281e1dd2bb56ba

# Needs only to be defined
ALLOWS_DUPLICATES = TRUE

LANG_SPECIAL_INFO = \
		"RR Rr rr rR" /dev/null /dev/null \
		"TS Ts ts tS" /dev/null /dev/null \
		"TX Tx tx tX" /dev/null /dev/null \
		"TZ Tz tz tZ" /dev/null /dev/null \

include ../Makefile.langcommon

# This list is private: not to be released
SOURCEDICT ?= $(XWDICTPATH)/Basque/palabrascon_pc_euskera.txt

$(XWLANG)Main.dict.gz: $(SOURCEDICT) $(MAKEFILE)
	cat $< \
	| grep '^[abdefghijklmnñoprstuxz]*$$' \
	| tr 'ñ' 'n' \
	| sed -e 's/[[:lower:]]*/\U&/' \
	| sed 's/RR/1/g' \
	| sed 's/TS/2/g' \
	| sed 's/TX/3/g' \
	| sed 's/TZ/4/g' \
	| tr '1234' '\001\002\003\004' \
	| gzip - > $@

clean: clean_common
	rm -rf *.saved
