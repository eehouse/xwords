# -*- mode: makefile; compile-command: "make -j3 MEMDEBUG=TRUE install"; -*-
# Copyright 2021 by Eric House (xwords@eehouse.org).  All rights
# reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

include ../common/config.mk

DEBS = cmake

INPUTS = main.c wasmdict.c wasmutls.c wasmdraw.c wasmutil.c wasmdutil.c wasmasm.c ${COMMONSRC}

DEFINES += -DXWFEATURE_CHAT
DEFINES += -DXWFEATURE_BONUSALL
DEFINES += -DMAX_ROWS=32
DEFINES += -DCOMMON_LAYOUT
DEFINES += -DDROP_BITMAPS
DEFINES += -D__LITTLE_ENDIAN
DEFINES += -DXWFEATURE_RELAY
# DEFINES += -DXWFEATURE_DEVID
DEFINES += -D__WORDSIZE=32
# DEFINES += -DCOMMS_CHECKSUM
DEFINES += -DENABLE_LOGGING
DEFINES += -DKEYBOARD_NAV
DEFINES += -DDISABLE_TILE_SEL
DEFINES += -DKEY_SUPPORT
DEFINES += -DSTREAM_VERS_HASHSTREAM
DEFINES += -DXWFEATURE_KNOWNPLAYERS
DEFINES += -DXWFEATURE_DICTSANITY
DEFINES += -DPOINTER_SUPPORT
DEFINES += -DPLATFORM_WASM
DEFINES += -DXWFEATURE_CROSSHAIRS
DEFINES += -DXWFEATURE_STREAMREF
DEFINES += -DXWFEATURE_TURNCHANGENOTIFY
DEFINES += -DNATIVE_NLI
DEFINES += -DDEBUG_REF
DEFINES += -Wno-switch
DEFINES += -DGITREV=\"$(shell git describe --tags --dirty)\"

ifeq ($(MEMDEBUG),TRUE)
	DEFINES += -DMEM_DEBUG -DDEBUG -O0
else
	DEFINES += -O3
endif

OUTPUTS = main.html main.js main.js.mem main.data main.html.mem main.wasm

all: main.js

# PHONY: install

# main.js: ${INPUTS} Makefile
# 	emcc $(DEFINES) -O2 -I . -I ../common -I ../relay -s USE_SDL=2 \
# 		--preload-file assets_dir --memory-init-file 1 \
# 		-s USE_SDL_TTF=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='["png"]' \
# 		-s EXPORTED_FUNCTIONS='["_mainf", "_button", "_newgame", "_gotMQTTMsg"]' -s WASM=0 \
# 		-s "EXTRA_EXPORTED_RUNTIME_METHODS=['ccall']" \
# 		${INPUTS} -o $@

# This isn't a thing any more
main.html: ${INPUTS} Makefile shell_minimal.html
	emcc $(DEFINES) -I . -I ../common -I ../relay -s USE_SDL=2 \
		--preload-file assets_dir --shell-file shell_minimal.html \
		-s USE_SDL_TTF=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='["png"]' \
		-s "EXTRA_EXPORTED_RUNTIME_METHODS=['ccall']" \
		-s EXPORTED_FUNCTIONS='["_main", "_gotMQTTMsg", "_cbckVoid", "_cbckString", "_MQTTConnectedChanged", "_onNewGame"]' \
		-s WASM=1 \
		${INPUTS} -o $@

install: main.html
	if [ -n "${XW_WASM_DEST}" ]; then \
		for FILE in ${OUTPUTS} paho-mqtt.js xwutils.js; do \
			[ -f $$FILE ] && scp $$FILE ${XW_WASM_DEST}; \
			echo "$$FILE copied"; \
		done; \
	fi

clean:
	rm -f ${OUTPUTS}

debs_install:
	sudo apt-get install ${DEBS}
