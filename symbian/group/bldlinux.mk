EPOC=/usr/local/symbian
PATH=$(EPOC)/bin:/local/bin:/usr/bin:/bin

# Get bmconv from http://symbianos.org/download/bmconv_1.1.0-2.tar.gz
# or try gnupoc.sf.net.  wine can't run bmconv.exe best as I can tell.
BMCONV = $(EPOC)/bin/bmconv

include $(EPOC)/lib/makerules/eikon

COMMONDIR = ../../common
PLATFORM = SYMB


include ../../common/config.mk

LIBS = \
	$(EPOCTRGREL)/euser.lib \
	$(EPOCTRGREL)/apparc.lib \
	$(EPOCTRGREL)/cone.lib \
	$(EPOCTRGREL)/eikcore.lib \
	$(EPOCTRGREL)/eikdlg.lib \
	$(EPOCTRGREL)/gdi.lib \
	$(EPOCTRGREL)/eikcoctl.lib \
	$(EPOCTRGREL)/bafl.lib \
	$(EPOCTRGREL)/egul.lib \
	$(EPOCTRGREL)/estlib.lib \
	$(EPOCTRGREL)/flogger.lib  \
	$(EPOCTRGREL)/commonengine.lib \
	$(EPOCTRGREL)/efsrv.lib \
	$(EPOCTRGREL)/avkon.lib \
	$(EPOCTRGREL)/eikcore.lib \
	$(EPOCTRGREL)/eikcdlg.lib \
	$(EPOCTRGREL)/estor.lib \
	$(EPOCTRGREL)/fbscli.lib \
	$(EPOCTRGREL)/ws32.lib \

# fntstr.lib \
# 	$(EPOCTRGREL)/bitgdi.lib \

NAME = xwords
ARCH = series60
SYMARCH = SERIES_60

SRCDIR = ../src
INCDIR = -I $(EPOC)/include -I $(EPOC)/include/libc -I../inc -I../../common
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

OBJECTS = $(LCLSRC:.cpp=.o) $(COMMONOBJ)

THEAPP=$(NAME).app
MAJOR=2
MINOR=4
PKGVERS=$(MAJOR),$(MINOR)

MBG = $(NAME).mbg 

PKGFILES=$(THEAPP) $(NAME).aif $(NAME).rsc $(NAME).mbm

U1 = 1000007a
U2 = 100039ce
U3 = 10206D64

DEBUG_FLAGS = -DDEBUG -DMEM_DEBUG
CFLAGS = -O -I. -DUID3=0x$(U3) $(DEBUG_FLAGS) \
	-D__LITTLE_ENDIAN -DKEYBOARD_NAV \
	-DKEY_SUPPORT -DFEATURE_TRAY_EDIT -DNODE_CAN_4 \
	-DXWFEATURE_STANDALONE_ONLY -D$(SYMARCH) \
	-DSYM_ARMI \
	$(INCDIR)

# This violates the no-data rule.  Don't allow it for ARMI build.
# It's ok for WINS builds since the rules are relaxed there.

# CFLAGS += -DSTUBBED_DICT

CPFLAGS = $(CFLAGS) -DCPLUS

# Following is used for the resource file
CPPFLAGS += -D_EPOC32_6 -DCPLUS -I../inc -DSERIES_60

all:$(PKGFILES) $(NAME).sis
	mv $(NAME).sis $(NAME)-$(MAJOR).$(MINOR)-$(ARCH).sis

$(THEAPP): $(NAME).rsc $(MBG) $(OBJECTS)

icon.$(ARCH).mbm: $(ICON_SRC)
	$(BMCONV) $@ $(subst ..,/c8..,$^)

$(NAME).aifspec: icon.$(ARCH).mbm
	@echo "mbmfile=$<" > $@
	@echo "ELangEnglish=$(NAME)" >> $@

# I'm adding my own rules here because I can't figure out how to use
# the default ones when src and obj live in different directories.
$(COMMONOBJDIR)/%.o: $(COMMONDIR)/%.c
	mkdir -p $(COMMONOBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CCC) $(CPFLAGS) -c -o $@ $<

$(NAME).mbg $(NAME).mbm: $(IMG_SRC)
	$(BMCONV) /h$(NAME).mbg $(NAME).mbm $(subst ..,/2..,$(IMG_SRC))

clean:
	rm -f $(GENERATED) $(NAME).aifspec $(OBJECTS) $(MBG) *.mbm
