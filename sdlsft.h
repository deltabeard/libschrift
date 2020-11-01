#pragma once

#include <SDL.h>

typedef struct sdlsft sdlsft;

sdlsft *sdlsft_init(Uint32 em, const void *font_mem, unsigned long size);
SDL_Surface *sdlsft_render(sdlsft *ctx, const Uint32 *str);
void sdlsft_exit(sdlsft *ctx);
