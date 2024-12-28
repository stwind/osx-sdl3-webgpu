/**
 * This is an extension of SDL3 for WebGPU, abstracting away the details of
 * OS-specific operations.
 *
 * This file is part of the "Learn WebGPU for C++" book.
 *   https://eliemichel.github.io/LearnWebGPU
 *
 * Most of this code comes from the wgpu-native triangle example:
 *   https://github.com/gfx-rs/wgpu-native/blob/master/examples/triangle/main.c
 *
 * MIT License
 * Copyright (c) 2022-2024 Elie Michel and the wgpu-native authors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "sdl3webgpu.h"

#include <webgpu.h>

#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>

#include <SDL3/SDL.h>

WGPUSurface SDL_GetWGPUSurface(WGPUInstance instance, SDL_Window* window) {
        SDL_PropertiesID props = SDL_GetWindowProperties(window);
        {
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
}