# -*- mode: Makefile; compile-command: "make -f armrel.mk"; -*-

# This is the makefile for non-debug (release) ARM builds.  It was
# built by hand-cribbing commands from the build-time log file EVC
# creates (by default) in a .vcl file in the build directory.

PLATFORM = ARMV4Rel
TARGET = $(PLATFORM)/xwords4.exe

LIBS = commctrl.lib coredll.lib winsock.lib aygshell.lib
RSRC = $(PLATFORM)/xwords4.res

MAKE = make -f armrel.mk

CC = $(WCE420)/bin/clarm.exe
LINK = $(WCE420)/bin/link.exe
RC = $(VSDIR)/Common/MSDev98/Bin/rc.exe

# CC_OPT += /Os
# CC_OPT += /O1
# CC_OPT += /Og

include ../common/config.mk

include ./shared.mk


# Since three rules use exactly the same command, I'm writing it out
# only once
C_CMD = \
	$(CC) /nologo $(CC_OPT) /W3 /I "$(WINCE_PATH)\Include\Armv4" \
		/I"..\common" /I"..\relay" /I"." /D _WIN32_WCE=420 /D "WIN32_PLATFORM_PSPC=400" \
		/D "ARM" /D "_ARM_" /D "ARMV4" /D UNDER_CE=420 /D "UNICODE" \
		/D "_UNICODE" /D "NDEBUG" $(XW_C_DEFINES) \
		/Fo$@ /MC /c $<

$(PLATFORM)/StdAfx.o: StdAfx.cpp
	mkdir -p $(dir $@)
	$(C_CMD)

$(PLATFORM)/%.o: %.c
	mkdir -p $(dir $@)
	$(C_CMD)

../common/$(PLATFORM)/%.o: ../common/%.c
	mkdir -p $(dir $@)
	$(C_CMD)

$(RSRC): xwords4.rc
	mkdir -p $(PLATFORM)
	$(RC) /l 0x409 /fo$@ \
		/i "$(WINCE_PATH)\Include\Armv4" \
		/d UNDER_CE=420 /d _WIN32_WCE=420 /d "NDEBUG" /d "UNICODE" \
		/d "_UNICODE" /d "WIN32_PLATFORM_PSPC=400" /d "ARM" /d "_ARM_" \
		/d "ARMV4" $(XW_RES_DEFINES) \
		/r $<

$(TARGET): $(COMMONOBJ) $(PLATOBJ) $(RSRC)
	$(LINK) $(LIBS) /nologo /base:"0x00010000" \
		/stack:0x10000,0x1000 /entry:"WinMainCRTStartup" /incremental:no \
		/pdb:$(basename $(TARGET)).pdb /nodefaultlib:"libc.lib" \
		/nodefaultlib:libcd.lib /nodefaultlib:libcmt.lib \
		/nodefaultlib:libcmtd.lib /nodefaultlib:msvcrt.lib \
		/nodefaultlib:msvcrtd.lib /out:$@ \
		/libpath:"$(PPC_SDK_PPC)\Lib\Armv4" \
		/subsystem:windowsce,4.20 /align:"4096" /MACHINE:ARM \
		$^

clean:
	rm -f $(COMMONOBJ) $(PLATOBJ) $(TARGET) $(RSRC) $(PLATFORM)/*.pdb

test:
	echo $(FIXED_OPT_COMMON)