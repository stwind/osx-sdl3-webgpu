#ifndef _sdl3_webgpu_h_
#define _sdl3_webgpu_h_

#include <webgpu.h>
#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

  WGPUSurface SDL_GetWGPUSurface(WGPUInstance instance, SDL_Window* window);

#ifdef __cplusplus
}
#endif

#endif // _sdl3_webgpu_h_