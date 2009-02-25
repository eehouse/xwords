CEOPT_ROOT ?= /opt

CE_ARCH = arm-mingw32ce
CC = $(CE_ARCH)-gcc

LFLAGS += -Wl,--major-subsystem-version,4,--minor-subsystem-version,20
CFLAGS += -D_WIN32_WCE=0x0400 -D__W32API_USE_DLLIMPORT__
CELIBS = -L${CEOPT_ROOT}/${CE_ARCH}/lib/ -lws2

messagebox.exe: messagebox.c
	$(CC) $(CFLAGS) $(LFLAGS) $^ $(CELIBS) -o $@


clean:
	rm -f messagebox.exe
