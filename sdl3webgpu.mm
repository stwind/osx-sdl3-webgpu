#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>
#include <SDL3/SDL.h>
#include <webgpu.h>
#include "sdl3webgpu.h"

WGPUSurface SDL_GetWGPUSurface(WGPUInstance instance, SDL_Window* window) {
        SDL_PropertiesID props = SDL_GetWindowProperties(window);
        id metal_layer = NULL;
        NSWindow* ns_window = (__bridge NSWindow*)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
        if (!ns_window) return NULL;
        [ns_window.contentView setWantsLayer : YES] ;
        metal_layer = [CAMetalLayer layer];
        [ns_window.contentView setLayer : metal_layer] ;

        WGPUSurfaceDescriptorFromMetalLayer fromMetalLayer;
        fromMetalLayer.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;

        fromMetalLayer.chain.next = NULL;
        fromMetalLayer.layer = metal_layer;

        WGPUSurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &fromMetalLayer.chain;
        surfaceDescriptor.label = NULL;

        return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
}