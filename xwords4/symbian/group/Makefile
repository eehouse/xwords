# -*- mode: makefile; -*-

# The braindead build system doesn't remove object files from these
# dirs.  So add to the clean target manually
COMMON_OBJ_DIR = $(EPOC)/BUILD/CYGWIN$(shell pwd)/xwords/symbian/group/common/armi/urel
XWORDS_OBJ_DIR = $(EPOC)/BUILD/CYGWIN$(shell pwd)/xwords/symbian/group/XWORDS/armi/urel


# SRC = \
# 	../src/helloworldbasic.cpp \
# 	../src/helloworldbasicapplication.cpp \
# 	../src/helloworldbasicappui.cpp \
# 	../src/helloworldbasicappview.cpp \
# 	../src/helloworldbasicdocument.cpp \

# INC = \
# 	../inc/helloworldbasic.hrh \
# 	../inc/helloworldbasic.pan \
# 	../inc/helloworldbasicapplication.h \
# 	../inc/helloworldbasicappui.h \
# 	../inc/helloworldbasicappview.h \
# 	../inc/helloworldbasicdocument.h \

# BLDMAKE_FLAGS = -v

all: wins

wins:
	bldmake $(BLDMAKE_FLAGS) bldfiles
	abLD.BAT build $(BLDMAKE_FLAGS) wins udeb

armi_bld:
	bldmake $(BLDMAKE_FLAGS) bldfiles
	abLD.BAT build $(BLDMAKE_FLAGS) armi urel

# armi:
# 	bldmake $(BLDMAKE_FLAGS) bldfiles
# 	abLD.BAT build $(BLDMAKE_FLAGS) armi udeb

# depends on wins because that's where the .aif file gets built
sis: wins armi_bld xwords.pkg
	makesis xwords.pkg

xwords.pkg:
	@echo '; xwords.pkg' | tr -d '\r' > $@
	@echo ';' | tr -d '\r' >> $@
	@echo ';Language - standard language definitions' | tr -d '\r' >> $@
	@echo '&EN' | tr -d '\r' >> $@
	@echo '' | tr -d '\r' >> $@
	@echo '; standard SIS file header' | tr -d '\r' >> $@
	@echo '#{"Crosswords"},(0x10206D64),1,0,0' | tr -d '\r' >> $@
	@echo '' | tr -d '\r' >> $@
	@echo ';' | tr -d '\r' >> $@
	@echo \"$(EPOC)\\release\\armi\\urel\\xwords.APP\"-\"C:\\system\\apps\\xwords\\xwords.app\" | tr '/' '\\' >> $@
	@echo \"$(EPOC)data/z/system/apps/xwords/xwords.rsc\"-\"C:\\system\\apps\\xwords\\xwords.rsc\" | tr '/' '\\' >> $@
	@echo \"../../../dawg/English/BasEnglish2to8.xwd\"-\"C:\\system\\apps\\xwords\\BasEnglish2to8.xwd\" | tr '/' '\\' >> $@

#"..\..\..\epoc32\data\z\system\apps\HelloWorldBasic\HelloWorldBasic.rsc"-"C:\system\apps\HelloWorldBasic\HelloWorldBasic.rsc"


# build project files for M$ VC++ version 6, e.g. for source-level
# debugging
vc6:
	makmake.exe xwords vc6

clean:
	rm -f xwords.SIS
	rm -f $(COMMON_OBJ_DIR)/*.o
	rm -f $(XWORDS_OBJ_DIR)/*.o
	abLD.BAT clean wins udeb
	abLD.BAT clean armi udeb
	bldmake $(BLDMAKE_FLAGS) clean

# all: ./ABLD.BAT $(SRC)
# 	(PATH=$$(echo $$(echo $$PATH | sed -e 's/\/usr\/local\/bin:\/usr\/bin:\/usr\/bin://'):/usr/local/bin:/usr/bin:/usr/bin); \
# 		./ABLD.BAT build wins udeb)


# ./ABLD.BAT: helloworldbasic.mmp Makefile
# 	(PATH=$$(echo $$(echo $$PATH | sed -e 's/\/usr\/local\/bin:\/usr\/bin:\/usr\/bin://'):/usr/local/bin:/usr/bin:/usr/bin); \
# 		bldmake bldfiles)
