# -*- mode: Makefile; -*-

PLATFORM = emulatorDbg
TARGET = $(PLATFORM)/xwords4.exe

LIBS = commctrl.lib coredll.lib corelibc.lib winsock.lib
RSRC = $(PLATFORM)/xwords4.res

MAKE = make -f simrel.mk

include ../common/config.mk

include ./shared.mk

C_CMD = \
	mkdir -p $(dir $@) && \
	cl.exe /nologo /W3 /Zi /Od \
		/I "$(WINCE_DEV_PATH)\Include\Emulator" \
		/I "..\common" /I "." /D "DEBUG" /D "_i386_" /D UNDER_CE=420 \
		/D _WIN32_WCE=420 /D "WIN32_PLATFORM_PSPC=400" /D "i_386_" \
		/D "UNICODE" /D "_UNICODE" /D "_X86_" /D "x86" $(XW_C_DEFINES) \
		/Fo$@ /Gs8192 /GF /c $<

$(PLATFORM)/StdAfx.o: StdAfx.cpp
	$(C_CMD)

$(PLATFORM)/%.o: %.c
	$(C_CMD)

../common/$(PLATFORM)/%.o: ../common/%.c
	$(C_CMD)

$(RSRC): xwords4.rc
	rc.exe /l 0x409 /fo$@ \
		/i "$(WINCE_DEV_PATH)\Include\Emulator" \
		/d "WIN32_PLATFORM_PSPC=400" /d UNDER_CE=420 /d _WIN32_WCE=420 \
		/d "UNICODE" /d "_UNICODE" /d "DEBUG" /d "_X86_" /d "x86" /d "_i386_" \
		$(XW_RES_DEFINES) \
		/r $<

test:
	echo $(basename $(TARGET)).pdb

$(TARGET): $(COMMONOBJ) $(PLATOBJ) $(RSRC)
	link.exe $(LIBS) /nologo /base:"0x00010000" /stack:0x10000,0x1000 \
		/entry:"WinMainCRTStartup" /incremental:yes \
		/pdb:$(basename $(TARGET)).pdb \
		/debug /nodefaultlib:"OLDNAMES.lib" /nodefaultlib:libc.lib \
		/nodefaultlib:libcd.lib /nodefaultlib:libcmt.lib \
		/nodefaultlib:libcmtd.lib /nodefaultlib:msvcrt.lib \
		/nodefaultlib:msvcrtd.lib /out:$@ \
		/libpath:"$(WINCE_DEV_PATH)\Lib\emulator" \
		/subsystem:windowsce,4.20 /MACHINE:IX86 \
		$(COMMONOBJ) $(PLATOBJ) $(RSRC)

clean:
	rm -f $(COMMONOBJ) $(PLATOBJ) $(TARGET) $(RSRC) $(PLATFORM)/*.pdb
