# -*- mode: makefile; -*-

# The braindead build system doesn't remove object files from these
# dirs.  So add to the clean target manually
# COMMON_OBJ_DIR = $(EPOC)/BUILD/CYGWIN$(shell pwd)/xwords/symbian/group/common/armi/urel
# XWORDS_OBJ_DIR = $(EPOC)/BUILD/CYGWIN$(shell pwd)/xwords/symbian/group/XWORDS/armi/urel

ifdef VERB
BLDMAKE_FLAGS = -v
endif

# There are no rules for building these puppies -- the symbian build
# system does that.  But they'd better exist by the time we get to the
# .pkg target.
SISFILES = \
	$(EPOC)/release/armi/urel/xwords.APP \
	$(EPOC)\\data/z/system/apps/xwords/xwords.rsc \
	$(EPOC)\\data/z/system/apps/xwords/xwords.aif \
	$(EPOC)\\data/z/system/apps/xwords/xwords.mbm \
	../../dawg/English/BasEnglish2to8.xwd \

all: wins

ABLD.BAT:
	bldmake $(BLDMAKE_FLAGS) bldfiles

wins: ABLD.BAT
	$< build $(BLDMAKE_FLAGS) wins udeb

armi: ABLD.BAT
	$< build $(BLDMAKE_FLAGS) armi urel

# depends on wins because that's where the .aif file gets built
sis: wins armi xwords.pkg
	makesis -v xwords.pkg

# If this xwords.pkg target generates errors -- especially the &EN
# part -- you're probably running symbian's make instead of cygwin's.
# Try invoking this Makefile with /usr/bin/make explicitly:
# /usr/bin/make sis
xwords.pkg: Makefile $(SISFILES)
	rm -f $@
	@echo "&EN" | tr -d '\r' >> $@
	@echo '#{"Crosswords"},(0x10206D64),1,0,0' | tr -d '\r' >> $@
	@for f in $(SISFILES); do \
		bname=$$(basename "$$f"); \
		echo \"$$f\"-\"C:\\system\\apps\\xwords\\$$bname\" >> $@; \
	done

# build project files for M$ VC++ version 6, e.g. for source-level
# debugging
vc6:
	makmake.exe xwords vc6

clean:
	rm -f xwords.SIS xwords.pkg
	rm -f $(COMMON_OBJ_DIR)/*.o
	rm -f $(XWORDS_OBJ_DIR)/*.o
	if [ -f abLD.BAT ]; then \
		./abLD.BAT clean wins udeb; \
		./abLD.BAT clean armi udeb; \
	fi
	bldmake $(BLDMAKE_FLAGS) clean
