EPOC=/usr/local/symbian
PATH=$(EPOC)/bin:/local/bin:/usr/bin:/bin

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


# fntstr.lib \
# 	$(EPOCTRGREL)/bitgdi.lib \

NAME = xwords
ARCH = series60
SYMARCH = SERIES_60

SRCDIR = ../src
INCDIR = -I $(EPOC)/include -I $(EPOC)/include/libc -I../inc -I../../common
LCLSRC = \
	$(SRCDIR)/$(NAME).cpp \
	$(SRCDIR)/symaskdlg.cpp \
	$(SRCDIR)/symdraw.cpp \
	$(SRCDIR)/xwapp.cpp \
	$(SRCDIR)/xwappview.cpp \
	$(SRCDIR)/symdict.cpp \
	$(SRCDIR)/symutil.cpp \
	$(SRCDIR)/xwappui.cpp \
	$(SRCDIR)/xwdoc.cpp \

OBJECTS = $(LCLSRC:.cpp=.o) $(COMMONOBJ)

TARGET=$(NAME).app
MAJOR=2
MINOR=4
PKGVERS=$(MAJOR),$(MINOR)

PKGFILES=$(NAME).app $(NAME).aif $(NAME).rsc $(NAME).exe

U1 = 1000007a
U2 = 100039ce
U3 = 10206D64

CFLAGS = -O -I. -DUID3=0x$(U3) -DDEBUG -DMEM_DEBUG \
	-D__LITTLE_ENDIAN -DKEYBOARD_NAV \
	-DKEY_SUPPORT -DFEATURE_TRAY_EDIT -DNODE_CAN_4 \
	-DXWFEATURE_STANDALONE_ONLY -D$(SYMARCH) \
	$(INCDIR)

# This violates the no-data rule.  Don't allow it for ARMI build.
# It's ok for WINS builds since the rules are relaxed there.

# CFLAGS += -DSTUBBED_DICT

CPFLAGS = $(CFLAGS) -DCPLUS

# Following is used for the resource file
CPPFLAGS += -D_EPOC32_6 -DCPLUS -I../inc

all:$(PKGFILES) $(NAME).sis
	mv $(NAME).sis $(NAME)-$(MAJOR).$(MINOR)-$(ARCH).sis

$(TARGET): $(NAME).rsc $(OBJECTS)

$(NAME).aifspec:
	@echo "mbmfile=icon.$(ARCH).mbm" > $(NAME).aifspec
	@echo "ELangEnglish=$(NAME)" >> $(NAME).aifspec

$(COMMONOBJDIR)/%.o: $(COMMONDIR)/%.c
	mkdir -p $(COMMONOBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CCC) $(CPFLAGS) -c -o $@ $<

clean:
	rm -f $(GENERATED) $(NAME).aifspec $(OBJECTS)
