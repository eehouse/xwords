# -*-mode: Makefile -*-
#
# Copyright 2007 by Eric House (xwords@eehouse.org)
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

all: byodfiles.tgz

byodfiles.tgz: byodfiles.tar
	gzip -c $< > $@

byodfiles.tar:
	rm -f $@ langinfo
	tar cvf $@ ./dict2dawg.cpp ./par.pl ./xloc.pl ./xloc.pm
	for dir in $$(ls .); do \
		if [ -f $$dir/info.txt ] && [ -f $$dir/Makefile ]; then \
			mkdir -p langinfo/$$dir; \
			(cd $$dir; make TARGET_TYPE=PALM table.bin values.bin palmvalues.bin); \
			(cd $$dir; make TARGET_TYPE=FRANK frankspecials.bin); \
			cp $$dir/table.bin $$dir/values.bin $$dir/palmvalues.bin $$dir/frankspecials.bin $$dir/info.txt langinfo/$$dir; \
			tar rvf $@ langinfo/$$dir/table.bin langinfo/$$dir/values.bin langinfo/$$dir/palmvalues.bin langinfo/$$dir/frankspecials.bin langinfo/$$dir/info.txt; \
		fi \
	done
	rm -rf langinfo

clean:
	rm -f byodfiles.tgz byodfiles.tar dict2dawg

dict2dawg: dict2dawg.cpp
	$(CXX) $< -o $@

PHONY: test

test:
	for f in $$(ls); do \
		test -d $$f || continue; \
		test -e $$f/Makefile || continue; \
		(cd $$f && make clean && make); \
	done \

help:
	@echo make \# makes byod tarball
	@echo make test \# builds dicts in a bunch of subdirectories
