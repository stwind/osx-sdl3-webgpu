#pragma once

#include <iostream>
#include <SDL3/SDL.h>
#include <webgpu.h>
#include <wgpu.h>
#include "sdl3webgpu.h"

void LogOutputFunction(void* userdata, int category, SDL_LogPriority priority, const char* message) {
  const char* priority_name = NULL;

  switch (priority) {
  case SDL_LOG_PRIORITY_VERBOSE: priority_name = "VERBOSE"; break;
  case SDL_LOG_PRIORITY_DEBUG: priority_name = "DEBUG"; break;
  case SDL_LOG_PRIORITY_INFO: priority_name = "INFO"; break;
  case SDL_LOG_PRIORITY_WARN: priority_name = "WARN"; break;
  case SDL_LOG_PRIORITY_ERROR: priority_name = "ERROR"; break;
  case SDL_LOG_PRIORITY_CRITICAL: priority_name = "CRITICAL"; break;
  default: priority_name = "UNKNOWN"; break;
  }

  char time_buffer[9];
  time_t now = time(NULL);
  strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", localtime(&now));

  fprintf(stderr, "%s [%s]: %s\n", time_buffer, priority_name, message);
}

WGPUAdapter requestAdapter(WGPUSurface surface, WGPUInstance instance) {
  WGPUAdapter adapter;
  wgpuInstanceRequestAdapter(instance, new WGPURequestAdapterOptions{
  .compatibleSurface = surface,
  .powerPreference = WGPUPowerPreference_HighPerformance,
  .forceFallbackAdapter = false,
  .backendType = WGPUBackendType_Undefined,
    }, [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* userdata) {
      if (status == WGPURequestAdapterStatus_Success) *(WGPUAdapter*)(userdata) = adapter;
      else throw std::runtime_error(message);
    }, &adapter);
  return adapter;
};

WGPUDevice requestDevice(WGPUAdapter adapter) {
  WGPUDevice device;
  WGPUSupportedLimits supportedLimits;
  wgpuAdapterGetLimits(adapter, &supportedLimits);
  wgpuAdapterRequestDevice(adapter, new WGPUDeviceDescriptor{
    .requiredFeatureCount = 3,
    .requiredFeatures = new WGPUFeatureName[3]{
      WGPUFeatureName_Float32Filterable,
      WGPUFeatureName_TimestampQuery,
      (WGPUFeatureName)WGPUNativeFeature_TextureAdapterSpecificFormatFeatures,
    },
    .requiredLimits = new WGPURequiredLimits{
      .limits = supportedLimits.limits
    },
    }, [](WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* userdata) {
      if (status == WGPURequestDeviceStatus_Success) *(WGPUDevice*)(userdata) = device;
      else throw std::runtime_error(message);
    }, &device);
  return device;
}

void configureSurface(SDL_Window* window, WGPUSurface surface, WGPUDevice device, WGPUTextureFormat surfaceFormat) {
  int bbwidth, bbheight;
  SDL_GetWindowSizeInPixels(window, &bbwidth, &bbheight);
  wgpuSurfaceConfigure(surface, new WGPUSurfaceConfiguration{
    .device = device,
    .format = surfaceFormat,
    .usage = WGPUTextureUsage_RenderAttachment,
    .viewFormatCount = 1,
    .viewFormats = &surfaceFormat,
    .alphaMode = WGPUCompositeAlphaMode_Auto,
    .presentMode = WGPUPresentMode_Fifo,
    .width = (uint32_t)bbwidth,
    .height = (uint32_t)bbheight,
    });
}

class Application {
public:
  Application(WGPUTextureFormat surfaceFormat = WGPUTextureFormat_BGRA8UnormSrgb) : surfaceFormat(surfaceFormat) {
    SDL_SetLogOutputFunction(LogOutputFunction, NULL);
    if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error("SDL_Init failed");

    window = SDL_CreateWindow("Window", 1280, 720, SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) throw std::runtime_error("SDL_CreateWindow failed");

    WGPUInstance instance = wgpuCreateInstance(new WGPUInstanceDescriptor{});
    surface = SDL_GetWGPUSurface(instance, window);
    WGPUAdapter adapter = requestAdapter(surface, instance);
    wgpuInstanceRelease(instance);
    device = requestDevice(adapter);
    wgpuAdapterRelease(adapter);
    configureSurface(window, surface, device, surfaceFormat);
    queue = wgpuDeviceGetQueue(device);
  }

  ~Application() {
    wgpuDeviceRelease(device);
    wgpuSurfaceUnconfigure(surface);
    wgpuSurfaceRelease(surface);

    SDL_DestroyWindow(window);
  }

  void present() {
    wgpuSurfacePresent(surface);
  }

  SDL_Window* window;
  WGPUSurface surface;
  WGPUDevice device;
  WGPUQueue queue;
  WGPUTextureFormat surfaceFormat;
};
