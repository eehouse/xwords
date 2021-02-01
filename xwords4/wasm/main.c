// Copyright 2011 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <emscripten.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

int main(int argc, char** argv)
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window;
    SDL_Renderer* renderer;

    SDL_CreateWindowAndRenderer(600, 400, 0, &window, &renderer);

    int result = 0;

    /**
     * Set up a white background
     */
    SDL_SetRenderDrawColor(renderer, 255, 255, 50, 50);
    SDL_RenderClear(renderer);

    /**
     * Show what is in the renderer
     */
    SDL_RenderPresent(renderer);

    printf("you should see an image.\n");

    return 0;
}
