# -*- mode: Makefile; compile-command: "/usr/bin/make -f bldwin.mk" -*-

SERIES ?= 80
TARGET ?= WINS

#U1 = 1000007a
U1 = 10000079
U2 = 100039ce
U3 = 10206D64

# User should define EPOC_80 and/or EPOC_60 in the environment
EPOC = $(EPOC_$(SERIES))
NAME = xwords_$(SERIES)
DESTDIR = $(EPOC)/wins/c/system/Apps/$(NAME)
DICT = ../../dawg/English/BasEnglish2to8.xwd
EDLL_LIB = $(EPOC)/release/wins/udeb/edll.lib

#CPP = $(EPOC)/gcc/bin/gcc -E
CPP = $(EPOC)/gcc/arm-epoc-pe/bin/gcc.exe -E -
RC = rcomp
GA = genaif
PT = $(EPOC)/tools/petran
LIB = lib.exe
PERL = c:/activePerl/bin/perl
MAKEDEF = makedef.pl
EPOCRC = epocrc.pl
ECOPYFILE = ecopyfile.pl
LINK = $(MSVC_DIR)/Bin/link.exe
DUMPBIN = $(MSVC_DIR)/Bin/dumpbin.exe
LIB = $(MSVC_DIR)/Bin/lib.exe
ECHO = /usr/bin/echo

#STANDALONE_ONLY = -DXWFEATURE_STANDALONE_ONLY 

COMMON_FLAGS = \
	-D$(SYMARCH) -D__LITTLE_ENDIAN -DKEYBOARD_NAV \
	-DKEY_SUPPORT -DFEATURE_TRAY_EDIT -DNODE_CAN_4 \
	-DOS_INITS_DRAW $(STANDALONE_ONLY)

RSS_DEFS = $(COMMON_FLAGS)

##################################################
# WINS- vs ARMI-specific settings
##################################################
ifeq ($(TARGET),WINS)
#########################
# WINS
#########################
CC = $(VSDIR)/VC98/Bin/CL.EXE
CL_FLAGS = \
	/MDd /Zi /Yd /Od /nologo /Zp4 /GF /QIfist /X /W4 \
	/D _DEBUG /D _UNICODE /D UNICODE /D "__SYMBIAN32__" \
	/D "__VC32__" /D "__WINS__" \

CFLAGS += $(CL_FLAGS) -I. -DUID3=0x$(U3) $(DEBUG_FLAGS) \
	-D$(SYMARCH) $(COMMON_FLAGS) \
	-DSYM_WINS \
	$(INCDIR)

else
ifeq ($(TARGET),ARMI)
#########################
# ARMI (incomplete; build 
# with linux. :-)
#########################

CC = $(EPOC)/gcc/bin/g++.exe

endif
endif

BMCONV = bmconv

COMMONDIR = ../../common
PLATFORM = SYMB_$(SERIES)_$(TARGET)
XWORDS_DIR = \"xwords_$(SERIES)\"

include $(COMMONDIR)/config.mk

EPOCTRGREL = $(EPOC)/release/wins/udeb

LIBS_ALLSERIES = \
	$(EPOCTRGREL)/euser.lib \
	$(EPOCTRGREL)/apparc.lib \
	$(EPOCTRGREL)/cone.lib \
	$(EPOCTRGREL)/gdi.lib \
	$(EPOCTRGREL)/eikcoctl.lib \
	$(EPOCTRGREL)/eikcore.lib \
	$(EPOCTRGREL)/bafl.lib \
	$(EPOCTRGREL)/egul.lib \
	$(EPOCTRGREL)/estlib.lib \
	$(EPOCTRGREL)/flogger.lib  \
	$(EPOCTRGREL)/commonengine.lib \
	$(EPOCTRGREL)/eikdlg.lib \
	$(EPOCTRGREL)/fbscli.lib \
	$(EPOCTRGREL)/efsrv.lib \
	$(EPOCTRGREL)/estor.lib \
	$(EPOCTRGREL)/ws32.lib \
	$(EPOCTRGREL)/insock.lib \
	$(EPOCTRGREL)/esock.lib \

LIBS_60 = \
	$(EPOCTRGREL)/eikcore.lib \
	$(EPOCTRGREL)/avkon.lib \
	$(EPOCTRGREL)/eikcdlg.lib \

LIBS_80 = \
	$(EPOCTRGREL)/ckndlg.lib \
	$(EPOCTRGREL)/ckncore.lib \
	$(EPOCTRGREL)/eikfile.lib \
	$(EPOCTRGREL)/eikctl.lib \
	$(EPOCTRGREL)/bitgdi.lib \

LIBS = $(LIBS_ALLSERIES) $(LIBS_$(SERIES))

STAGE_BOTH_LINK_FLAGS = \
	$(EDLL_LIB) $(LIBS) /nologo \
	/include:"?_E32Dll@@YGHPAXI0@Z" /nodefaultlib \
	/entry:"_E32Dll" /subsystem:windows /dll \
	/out:$(NAME).app \
	/machine:IX86 \

STAGE1_LINK_FLAGS = $(STAGE_BOTH_LINK_FLAGS) \
	/incremental:no \

STAGE2_LINK_FLAGS = $(STAGE_BOTH_LINK_FLAGS) \
	$(NAME).exp /debug \

ARCH = series$(SERIES)
SYMARCH = SERIES_$(SERIES)

INC = ../inc
SRCDIR = ../src
UID_CPP = $(SRCDIR)/$(NAME).UID.cpp
INCDIR = -I $(EPOC)/include -I $(EPOC)/include/libc -I$(INC) \
	$(subst ../,../../,$(COMMON_INCS))

LCLSRC = \
	$(SRCDIR)/xwmain.cpp \
	$(UID_CPP) \
	$(SRCDIR)/xwapp.cpp \
	$(SRCDIR)/symaskdlg.cpp \
	$(SRCDIR)/symdraw.cpp \
	$(SRCDIR)/xwappview.cpp \
	$(SRCDIR)/symdict.cpp \
	$(SRCDIR)/symutil.cpp \
	$(SRCDIR)/xwappui.cpp \
	$(SRCDIR)/xwdoc.cpp \
	$(SRCDIR)/symgmmgr.cpp \
	$(SRCDIR)/symgmdlg.cpp \
	$(SRCDIR)/symblnk.cpp \
	$(SRCDIR)/symgamdl.cpp \
	$(SRCDIR)/symgamed.cpp \
	$(SRCDIR)/symssock.cpp \

IMG_SRC = ../bmps/downarrow_80.bmp \
	../bmps/rightarrow_80.bmp \
	../bmps/star_80.bmp \
	../bmps/turnicon_80.bmp \
	../bmps/turniconmask_80.bmp \
	../bmps/robot_80.bmp \
	../bmps/robotmask_80.bmp \

INCLUDES = \
	$(NAME).rsg \
	$(NAME).mbg \
	$(INC)/symaskdlg.h \
	$(INC)/symblnk.h \
	$(INC)/symdict.h \
	$(INC)/symdraw.h \
	$(INC)/symgamdl.h \
	$(INC)/symgamed.h \
	$(INC)/symgmdlg.h \
	$(INC)/symgmmgr.h \
	$(INC)/symutil.h \
	$(INC)/xptypes.h \
	$(INC)/xwapp.h \
	$(INC)/xwappui.h \
	$(INC)/xwappview.h \
	$(INC)/xwdoc.h \
	$(INC)/xwords.hrh \
	$(INC)/LocalizedStrIncludes.h \

AIF = ../aif
ICON_SRC = \
	$(AIF)/lrgicon.bmp \
	$(AIF)/lrgiconmask.bmp \

OBJDIR = $(SRCDIR)/$(PLATFORM)

OBJECTS = $(patsubst $(SRCDIR)/%,$(OBJDIR)/%,$(LCLSRC:.cpp=.o)) $(COMMONOBJ)

MAJOR = 4
MINOR = 1
PKGVERS = $(MAJOR),$(MINOR)

MBG = $(NAME).mbg 

#PKGFILES=$(THEAPP) $(NAME).aif $(NAME).rsc $(NAME).mbm BasEnglish2to8.xwd
PKGFILES = $(NAME).app $(NAME).rsc $(NAME).mbm $(NAME).pdb $(DICT)

DEBUG_FLAGS = -DDEBUG -DMEM_DEBUG

CPFLAGS = $(CFLAGS) -DCPLUS

# Following is used for the resource file
CPPFLAGS = -I$(EPOC)/include  -I../inc

ifdef VERBOSE
else
AMP = @
endif

ifeq ($(TARGET),WINS)
all: wins
else
ifeq ($(TARGET),ARMI)
all: armi
else
all: define_TARGET_please
endif
endif

wins: _sanity $(PKGFILES)
	@mkdir -p $(DESTDIR)
	cp $(PKGFILES) $(DESTDIR)

_sanity:
	@if [ "$(EPOC_$(SERIES))" = "" ]; then \
		$(ECHO) " ---> ERROR: EPOC_$(SERIES) undefined in env"; \
		exit 1; \
	fi

icon.$(ARCH).mbm: $(ICON_SRC)
	$(BMCONV) $@ $(subst ..,/c8..,$^)

$(NAME).aifspec: icon.$(ARCH).mbm
	@$(ECHO) "mbmfile=$<" > $@
	@$(ECHO) "ELangEnglish=$(NAME)" >> $@

# I'm adding my own rules here because I can't figure out how to use
# the default ones when src and obj live in different directories.
$(COMMONOBJDIR)/%.o: $(COMMONDIR)/%.c
	$(AMP)mkdir -p $(COMMONOBJDIR)
	$(AMP)$(CC) $(CFLAGS) /c /Fo$@ $<

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(INCLUDES)
	$(AMP)mkdir -p $(OBJDIR)
	$(AMP)$(CC) $(CPFLAGS) /c /Fo$@ $<

$(NAME).mbg $(NAME).mbm: $(IMG_SRC)
	$(BMCONV) /h$(NAME).mbg $(NAME).mbm $(subst ..,/2..,$(IMG_SRC))

$(NAME).rss: xwords.rss
	cp $< $@

clean:
	rm -rf $(GENERATED) $(NAME).aifspec $(OBJECTS) $(MBG) *.mbm *.rpp *.rsc \
		*.rsg *.app $(UID_CPP)

# remove saved games and data file
clean_state: 
	rm -rf $(EPOC)/wins/c/system/Apps/$(NAME)/*
	rm -rf $(EPOC)/wins/c/system/Apps/$(NAME)
	rm -rf $(EPOC)/release/wins/udeb/z/system/apps/$(NAME)/*
	rm -rf $(EPOC)/release/wins/udeb/z/system/apps/$(NAME)

#############################################################################
# from here down added from the linux build system or stolen from
# makefiles generated by the symbian system
#############################################################################

$(UID_CPP): ./bldwin.mk
	rm -f $@
	$(ECHO) "// Make-generated uid source file" >> $@
	$(ECHO) "#include <E32STD.H>" >> $@
	$(ECHO) "#pragma data_seg(\".E32_UID\")" >> $@
	$(ECHO) "__WINS_UID(0x$(U1),0x$(U2),0x$(U3))" >> $@
	$(ECHO) "#pragma data_seg()" >> $@

%.rsc %.rsg: %.rss
	$(PERL) -S $(EPOCRC) $(RSS_DEFS) -I "." -I "..\inc" \
		$(subst ../,../../,$(COMMON_INCS)) -I- -I $(EPOC)/include \
		-DLANGUAGE_SC -u $< -o$*.rsc  -h$*.rsg -l./

%.aif: %.aifspec
	@$(ECHO) "[GENAIF] $*"
	$(AMP)$(GA) -u 0x$(U3) $< $@

$(NAME).app : $(OBJECTS) $(EDLL_LIB) $(LIBS)
	@$(ECHO) building $@
	$(AMP)$(LINK) $(STAGE1_LINK_FLAGS) $(OBJECTS)
	$(AMP)rm -f $@ $(NAME).exp
	$(AMP)$(DUMPBIN) /exports /out:$(NAME).inf $(NAME).lib
	$(AMP)rm -f $(NAME).lib
	$(AMP)$(PERL) -S $(MAKEDEF) -Inffile $(NAME).inf \
		-1 ?NewApplication@@YAPAVCApaApplication@@XZ $(NAME).def
	$(AMP)rm $(NAME).inf
	$(AMP)$(LIB) /nologo /machine:i386 /nodefaultlib \
		/name:$@ /def:$(NAME).def /out:$(NAME).lib
	$(AMP)rm -f $(NAME).lib
	$(AMP)$(LINK)	$(STAGE2_LINK_FLAGS) $(OBJECTS)
	$(AMP)rm -f $(NAME).exp
