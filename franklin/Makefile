# -*- mode: Makefile; compile-command: "make -k ARCH=i686"; -*-
# ©2001 Franklin Electronic Publishers, Inc. Burlington, NJ.
#	File: gui/test/Makefile
#
PLATFORM = franklin
COMMON = ../common

ifndef ARCH
$(error "Franklin builds require the environment variable ARCH. ")
endif

XW_DEFINES = -DPLATFORM_EBOOK -DPOINTER_SUPPORT -DKEY_SUPPORT -DSHOW_PROGRESS \
	 -DXWFEATURE_STANDALONE_ONLY -DFEATURE_TRAY_EDIT -DXWFEATURE_SEARCHLIMIT \
	 -DNODE_CAN_4 -DUSE_PATTERNS -D__LITTLE_ENDIAN
include ../common/config.mk

APPNAME = xwords4

# Define the list of source files
SRCS_CPP = frankdraw.cpp \
	frankmain.cpp \
	franksavedgames.cpp \
	frankask.cpp \
	frankgamesdb.cpp \
	frankpasswd.cpp \
	frankshowtext.cpp \
	frankdict.cpp \
	frankletter.cpp	\
	frankplayer.cpp \
	frankdlist.cpp

SRCS_C =
SRCS_S =

# Program parameters (ebo_locate needs 12K of stack)
#HEAPSPACE       = 98304
HEAPSPACE       = 409600 # 0x64000
MAINSTACKSPACE  = 16384
TIMERSTACKSPACE = 4096

# Environment: Use "yes" or "no"
GUI_LOCAL_COPY  = no
OS_LOCAL_COPY   = no
LANG_LOCAL_COPY = no
GUI_LANGUAGE    = GUI_Amr.pkg
OS_BASEDIR      = ../../../os
GUI_BASEDIR     = ../source

#To prevent warnings from stopping the compilation, change the following line
#to "ERROR=" or start the make using "make ERROR=".
ERROR = -Werror

OBJS = ${COMMONOBJ}

# Include the standard Makefile targets
ifeq (x$(EBSDK_LIBS)x,xx)
# Normal: binary release version
	include ${EBOOKMAN_SDK}/ebsdk.uses
	include ${ESDK_TARGET_INC}/Makefile.GUI
else
# Special: building the SDK
	include ${EBSDK_LIBS}/ebookman_libs.uses
	include ../source/Makefile.GUI
endif


#
# At this point in the Makefile, you can put overrides for variables that
# were set in "Makefile.GUI".
#

#Create linkable version of VoiceAge file
#${OBJDIR}/objects.link: welcome.voiceage

#welcome.voiceage: welcome.seb
#	tail -c+4097 <welcome.seb >welcome.voiceage

#CLEANLIST += welcome.voiceage


#Need this for an os1 link of a program that uses floating-point
#GCC_LIB = ${ESDK_LIBGCC_INSTALL_LIBFILE_FLOAT}

INCFLAGS += -I$(COMMON) -I../relay -I./
CFLAGS += ${XW_DEFINES} -DCPLUS
CFL_NOCPLUS = $(subst -DCPLUS,,${CFLAGS})

CLEANLIST += ${COMMONOBJ}

# dependencies for image generation 
BMPS = ./bmps
frankmain.cpp: bmps_includes.h 

$(BMPS)/%.cpp: $(BMPS)/%.pbitm 
	./pbitm2frank.pl $< $* > $@ 

GENIMGS = $(BMPS)/flip.cpp \
	$(BMPS)/lightbulb.cpp \
	$(BMPS)/valuebutton.cpp \
	$(BMPS)/undo.cpp \

# listing the .cpp files here as dependencies should be all that's 
# needed to get them generated. 
bmps_includes.h: $(GENIMGS) 
	@echo "/* This file generated in Makefile; do not edit!!! */" > $@ 
	for f in $^; do echo "#include \"$$f\"" >> $@; done 



#
# Generic rules for invoking the compiler and assembler.  Stolen from
# Makefile.GUI
#

${COMMONOBJDIR}/%.s :: ${COMMON}/%.c
	${ESDK_GCC32_EXE} -b sneak32 -g $(CFL_NOCPLUS) -mclist -S -o $@ -c $<

${COMMONOBJDIR}/%.i :: ${COMMON}/%.c
	${ESDK_GCC32_EXE} -b sneak32 -g $(CFL_NOCPLUS) -mclist -E -o $@ -c $<

${COMMONOBJDIR}/%.i :: ${COMMON}/%.cpp
	${ESDK_GXX32_EXE} -b sneak32 -g $(CFLAGS) -mclist -E -o $@ -c $<

${COMMONOBJDIR}/%.s :: ${COMMON}/%.cpp
	${ESDK_GXX32_EXE} -b sneak32 -g $(CFLAGS) -mclist -S -o $@ -c $<

${COMMONOBJDIR}/%.o :: ${COMMON}/%.cpp
	${ESDK_GXX32_EXE} -b sneak32 -g $(CFLAGS) -o $@ -c $<

${COMMONOBJDIR}/%.o :: ${COMMON}/%.c
	${ESDK_GCC32_EXE} -b sneak32 -g $(CFL_NOCPLUS) -o $@ -c $<

${COMMONOBJDIR}/%.o :: ${COMMONOBJDIR}/%.s
	${ESDK_ASM32_EXE} -case ${INCFLAGS} -po ${COMMONOBJDIR} -o $<

# Targets to allow build without copying lots of files in.  First has
# hack to force directory creation
os.link os.def standalone.link:
	mkdir -p ../common/franklin
	ln -s $$(find $${EBOOKMAN_SDK} -follow -name $@ | grep gui) $@

# get rid of the welcome crap
libraries.link:
	cat $$(find $${EBOOKMAN_SDK} -follow -name $@ | grep gui) | \
		grep -v Welcome > $@



#
# The following targets are usable only during SDK creation.  They are for
# copying this directory to samples/gui/test.
#

build: os sa

configure:

do_install_out: build
	mkdir -p ${ELIB_GUI_INSTALL_SAMPLE_TESTDIR}
	(tar cf - . | (cd ${ELIB_GUI_INSTALL_SAMPLE_TESTDIR} && tar xf -))

memdebug:
	$(MAKE) "XW_DEFINES = ${XW_DEFINES} -DMEM_DEBUG -DDEBUG"

test:
	echo ${OBJS}
