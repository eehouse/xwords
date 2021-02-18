# -*- mode: Makefile -*-

SRCS = $(wildcard ./app/src/main/res/xml/prefs*.xml)
TARGET = ./app/src/main/java/org/eehouse/android/xw4/gen/PrefsWrappers.java
PREFS_WRAPPER_GEN = ./scripts/genPrefsWrapper.sh

.PHONY: all clean

RES_SRC = ./res_src
RES_DEST = ./app/src/main/res

all: $(TARGET)

$(TARGET): $(SRCS)
	$(PREFS_WRAPPER_GEN) $^ > $@

clean:
	rm -f $(TARGET)
