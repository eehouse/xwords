# -*- mode: makefile; -*-

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
	abLD.BAT build wins udeb

armi:
	bldmake $(BLDMAKE_FLAGS) bldfiles
	abLD.BAT build armi udeb

# build project files for M$ VC++ version 6, e.g. for source-level
# debugging
vc6:
	makmake.exe xwords vc6

clean:
	abLD.BAT clean wins udeb
	abLD.BAT clean armi udeb
	bldmake $(BLDMAKE_FLAGS) clean

# all: ./ABLD.BAT $(SRC)
# 	(PATH=$$(echo $$(echo $$PATH | sed -e 's/\/usr\/local\/bin:\/usr\/bin:\/usr\/bin://'):/usr/local/bin:/usr/bin:/usr/bin); \
# 		./ABLD.BAT build wins udeb)


# ./ABLD.BAT: helloworldbasic.mmp Makefile
# 	(PATH=$$(echo $$(echo $$PATH | sed -e 's/\/usr\/local\/bin:\/usr\/bin:\/usr\/bin://'):/usr/local/bin:/usr/bin:/usr/bin); \
# 		bldmake bldfiles)
