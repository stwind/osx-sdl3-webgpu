#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>
#include <SDL3/SDL.h>
#include <webgpu.h>
#include "sdl3webgpu.h"

WGPUSurface SDL_GetWGPUSurface(WGPUInstance instance, SDL_Window* window) {
        NSWindow* ns_window = (__bridge NSWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
        if (!ns_window) return NULL;
        [ns_window.contentView setWantsLayer : YES];
        [ns_window.contentView setLayer : [CAMetalLayer layer]];

        WGPUSurfaceDescriptorFromMetalLayer fromMetalLayer;
        fromMetalLayer.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;
        fromMetalLayer.chain.next = NULL;
        fromMetalLayer.layer = ns_window.contentView.layer;

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromMetalLayer.chain;

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
}