# -*- mode: Makefile; compile-command: "make -f bldlinux.mk" -*-

SERIES ?= 80
DEBUG ?= TRUE

# User should define EPOC_80 and/or EPOC_60 in the environment
EPOC = $(EPOC_$(SERIES))


PATH = $(EPOC)/bin:/local/bin:/usr/bin:/bin

BMCONV = bmconv

include $(EPOC)/lib/makerules/eikon

COMMONDIR = ../../common
PLATFORM = SYMB_$(SERIES)
XWORDS_DIR = \"xwords_$(SERIES)\"

include ../../common/config.mk

#STANDALONE_ONLY ?= -DXWFEATURE_STANDALONE_ONLY
BEYOND_IR = -DBEYOND_IR

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

# fntstr.lib \
# 	$(EPOCTRGREL)/bitgdi.lib \

NAME = xwords_$(SERIES)
USERNAME = Crosswords
ARCH = series$(SERIES)
SYMARCH = SERIES_$(SERIES)

SRCDIR = ../src
INCDIR = -I. -I $(EPOC)/include -I $(EPOC)/include/libc -I../inc \
	$(subst ../,../../,$(COMMON_INCS))
LCLSRC = \
	$(SRCDIR)/xwmain.cpp \
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

AIF = ../aif
ICON_SRC = \
	$(AIF)/lrgicon.bmp \
	$(AIF)/lrgiconmask.bmp \

OBJDIR = $(SRCDIR)/$(PLATFORM)

OBJECTS = $(patsubst $(SRCDIR)/%,$(OBJDIR)/%,$(LCLSRC:.cpp=.o)) $(COMMONOBJ)

THEAPP = $(NAME).app
MAJOR = 4
MINOR = 1
PKGVERS = $(MAJOR),$(MINOR)
SISNAME = $(NAME)-$(MAJOR).$(MINOR)-$(ARCH).sis

MBG = $(NAME).mbg 

PKGFILES=$(THEAPP) $(NAME).aif $(NAME).rsc $(NAME).mbm BasEnglish2to8.xwd

U1 = 1000007a
U2 = 100039ce
U3 = 10206D64

ifeq ($(DEBUG),TRUE)
DEBUG_FLAGS = -DDEBUG -DMEM_DEBUG
else
OPT = -O2 -fomit-frame-pointer
endif

CFLAGS += $(OPT) -I. -DUID3=0x$(U3) $(DEBUG_FLAGS) \
	-DXWORDS_DIR=$(XWORDS_DIR) \
	-D__LITTLE_ENDIAN -DKEYBOARD_NAV \
	-DKEY_SUPPORT -DFEATURE_TRAY_EDIT -DNODE_CAN_4 \
	$(STANDALONE_ONLY) -D$(SYMARCH) \
	-DSYM_ARMI -DOS_INITS_DRAW $(BEYOND_IR) \
	$(INCDIR)

# This violates the no-data rule.  Don't allow it for ARMI build.
# It's ok for WINS builds since the rules are relaxed there.

# CFLAGS += -DSTUBBED_DICT

CPFLAGS = $(CFLAGS) -DCPLUS

# Following is used for the resource file
CPPFLAGS += -D_EPOC32_6 -DCPLUS -I../inc -D$(SYMARCH) \
	$(subst ../,../../,$(COMMON_INCS))

all: _sanity $(PKGFILES) $(NAME).sis
	mv $(NAME).sis $(SISNAME)
ifdef XW_UPLOAD_CMD
	$(XW_UPLOAD_CMD) $(SISNAME)
endif

_sanity:
	@if [ "$(EPOC_$(SERIES))" = "" ]; then \
		echo " ---> ERROR: EPOC_$(SERIES) undefined in env"; \
		exit 1; \
	fi

$(THEAPP): $(NAME).rsc $(MBG) $(OBJECTS)

icon.$(ARCH).mbm: $(ICON_SRC)
	$(BMCONV) $@ $(subst ..,/c8..,$^)

$(NAME).aifspec: icon.$(ARCH).mbm
	@echo "mbmfile=$<" > $@
	@echo "ELangEnglish=$(USERNAME)" >> $@

# I'm adding my own rules here because I can't figure out how to use
# the default ones when src and obj live in different directories.
$(COMMONOBJDIR)/%.o: $(COMMONDIR)/%.c
	mkdir -p $(COMMONOBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	mkdir -p $(OBJDIR)
	$(CCC) $(CPFLAGS) -c -o $@ $<

$(NAME).mbg $(NAME).mbm: $(IMG_SRC)
	$(BMCONV) /h$(NAME).mbg $(NAME).mbm $(subst ..,/2..,$(IMG_SRC))

# temporary hack until I get 'round to breaking the .rss file into
# common, 60 and 80 (with the latter two including the first).
$(NAME).rss: xwords.rss
	ln -s $< $@

BasEnglish2to8.xwd: ../../dawg/English/BasEnglish2to8.xwd
	ln -s $< $@ 

clean:
	rm -f $(GENERATED) $(NAME).aifspec $(OBJECTS) $(MBG) *.mbm
