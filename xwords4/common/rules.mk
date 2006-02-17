# -*- mode: makefile -*-
# Copyright 2002 by Eric House
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


all: $(TARGET)

# Rule for (xplatform) objfiles in this directory
$(COMMONOBJDIR)/%.o: ../common/%.c 
	mkdir -p $(COMMONOBJDIR)
	$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) -DPLATFORM=$(PLATFORM) \
		 $< -o $@

# Rule for single-platform objfiles in directory of including Makefile
$(PLATFORM)/%.o: %.c
	mkdir -p $(PLATFORM)
	$(CC) -c -dD $(CFLAGS) $(INCLUDES) $(DEFINES) -DPLATFORM=$(PLATFORM) $< -o $@

