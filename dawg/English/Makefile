# -*-mode: Makefile -*-
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

TARGET_TYPE ?= FRANK

include ../Makefile.langcommon

# This represents the default -- for now
COMMAND = -f Makefile.BasEnglish TARGET_TYPE=FRANK

alleng:
	for mfile in Makefile.BasEnglish Makefile.OSW Makefile.TWL98 Makefile.CollegeEng; do \
		$(MAKE) -f $$mfile TARGET_TYPE=$(TARGET_TYPE); \
	done

%:
	$(MAKE) $(COMMAND) $@

all:
	$(MAKE) $(COMMAND)

clean:
	$(MAKE) $(COMMAND) clean

help:
	@echo "try make -f Makefile.[BasEnglish|CollegeEng] \\"
	@echo "    TARGET_TYPE=[PALM|FRANK]"

