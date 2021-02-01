
INPUTS = main.c

all: main.html

main.html: ${INPUTS}
	emcc -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='["png"]' $^ -o $@


