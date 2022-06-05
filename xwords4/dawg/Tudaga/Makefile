# -*-mode: Makefile -*-
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

XWLANG=Tudaga
LANGCODE=tuq

TARGET_TYPE ?= WINCE

include ../Makefile.langcommon

LANG_SPECIAL_INFO = \
		"NJ nj Nj nJ" /dev/null /dev/null

# Sed works to upper-case ŋ where tr doesn't. Sigh
$(XWLANG)Main.dict.gz: $(XWDICTPATH)/Tudaga/Tudaga.txt
	cat $< | \
		sed 's/[[:lower:]]*/\U&/' | \
		sed 's/NJ/1/g' | \
		grep '^[1ABÇDEGHIKLMNŊOPRSTUWYZ]*$$' | \
		tr '1' '\001' | \
		gzip > $@

clean: clean_common
	rm -f $(XWLANG)Main.dict.gz *.bin $(XWLANG)*.pdb $(XWLANG)*.seb
