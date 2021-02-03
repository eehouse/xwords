
include ../common/config.mk

INPUTS = main.c wasmdict.c wasmutls.c wasmdraw.c wasmutil.c wasmdutil.c ${COMMONSRC}

DEFINES += -DMEM_DEBUG -DDEBUG
DEFINES += -DXWFEATURE_CHAT
DEFINES += -DXWFEATURE_BONUSALL
DEFINES += -DMAX_ROWS=32
DEFINES += -DCOMMON_LAYOUT
DEFINES += -DDROP_BITMAPS
DEFINES += -D__LITTLE_ENDIAN
DEFINES += -DXWFEATURE_RELAY
DEFINES += -DXWFEATURE_DEVID
DEFINES += -D__WORDSIZE=32
DEFINES += -DCOMMS_CHECKSUM
DEFINES += -DENABLE_LOGGING
DEFINES += -DKEYBOARD_NAV
DEFINES += -DDISABLE_TILE_SEL
DEFINES += -DKEY_SUPPORT
DEFINES += -DSTREAM_VERS_HASHSTREAM
DEFINES += -DXWFEATURE_KNOWNPLAYERS
DEFINES += -DXWFEATURE_DICTSANITY
DEFINES += -DPOINTER_SUPPORT
DEFINES += -O2

OUTPUTS = main.html main.js main.js.mem main.data

all: main.js

# PHONY: upload

main.js: ${INPUTS} Makefile
	emcc $(DEFINES) -I . -I ../common -I ../relay -s USE_SDL=2 \
		--preload-file assets_dir --memory-init-file 1 \
		-s USE_SDL_TTF=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='["png"]' \
		-s EXPORTED_FUNCTIONS='["_mainf", "_button"]' -s WASM=0 \
		-s "EXTRA_EXPORTED_RUNTIME_METHODS=['ccall']" \
		${INPUTS} -o $@

upload: main.js
	scp ${OUTPUTS} pi4:/tmp
