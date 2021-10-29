#include <draw.h>

SDL_Event event;
SDL_Renderer *renderer;
SDL_Window *window;

color jet(double v){
    color c = {1., 1., 1.};
    double dv, vmax = 1., vmin = -1.;
    if (v < vmin)
        v = vmin;
    if (v > vmax)
        v = vmax;
    dv = vmax - vmin;

    if (v < (vmin + 0.25 * dv)) {
        c.r = 0;
        c.g = 4 * (v - vmin) / dv;
    } else if (v < (vmin + 0.5 * dv)) {
        c.r = 0;
        c.b = 1 + 4 * (vmin + 0.25 * dv - v) / dv;
    } else if (v < (vmin + 0.75 * dv)) {
        c.r = 4 * (v - vmin - 0.5 * dv) / dv;
        c.b = 0;
    } else {
        c.g = 1 + 4 * (vmin + 0.75 * dv - v) / dv;
        c.b = 0;
    }

   return(c);
}

void init_sdl (){
    SDL_Init (SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer (WIDTH, HEIGHT, 0, &window, &renderer);
    SDL_SetRenderDrawColor (renderer, 0, 0, 0, 0);
    SDL_RenderClear (renderer);
}

void draw_field(EM_field* f){
    #if defined(USE_CUDA)
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom (f->out, WIDTH, HEIGHT, 32, WIDTH * 4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    SDL_Texture *texture = SDL_CreateTextureFromSurface (renderer, surface);
    SDL_RenderCopy (renderer, texture, NULL, NULL);
    SDL_RenderPresent (renderer);
    SDL_FreeSurface (surface);
    SDL_DestroyTexture (texture);
    #else
    for (int i = 0; i < WIDTH; i++){
        for (int j = 0; j < HEIGHT; j++){
            color c = jet(f->H[i][j]);
            SDL_SetRenderDrawColor(renderer, c.r * 255, c.g * 255, c.b * 255, 255);
            SDL_RenderDrawPoint (renderer, i, j);
        }
    }
    SDL_RenderPresent (renderer);
    #endif
}

void stop_sdl (){
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

bool poll_quit (){
    return SDL_PollEvent(&event) && event.type == SDL_QUIT;
}
