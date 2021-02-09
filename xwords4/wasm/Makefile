
include ../common/config.mk

DEBS = cmake

INPUTS = main.c wasmdict.c wasmutls.c wasmdraw.c wasmutil.c wasmdutil.c ${COMMONSRC}

DEFINES += -DMEM_DEBUG -DDEBUG
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
DEFINES += -DNATIVE_NLI
DEFINES += -DDEBUG_REF
OUTPUTS = main.html main.js main.js.mem main.data main.html.mem main.wasm

all: main.js

# PHONY: upload

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
		-s EXPORTED_FUNCTIONS='["_main", "_button", "_newgame", "_gotMQTTMsg"]' -s WASM=0 \
		-s WASM=1 \
		${INPUTS} -o $@

upload: main.html
	if [ -n "${XW_WASM_DEST}" ]; then \
		for FILE in ${OUTPUTS} paho-mqtt.js; do \
			[ -f $$FILE ] && scp $$FILE ${XW_WASM_DEST}; \
			echo "$$FILE copied"; \
		done; \
	fi

clean:
	rm -f ${OUTPUTS}

debs_install:
	sudo apt-get install ${DEBS}
