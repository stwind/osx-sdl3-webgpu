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

class WGPU {
public:
  SDL_Window* window;
  WGPUSurface surface;
  WGPUDevice device;
  WGPUQueue queue;
  WGPUSurfaceTexture surfaceTexture;
  WGPUTextureFormat surfaceFormat;

  WGPU(int w, int h, WGPUTextureFormat surfaceFormat = WGPUTextureFormat_BGRA8UnormSrgb) : surfaceFormat(surfaceFormat) {
    SDL_SetLogOutputFunction(LogOutputFunction, NULL);
    if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error("SDL_Init failed");

    window = SDL_CreateWindow("Window", w, h, SDL_WINDOW_METAL);
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

  ~WGPU() {
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuSurfaceUnconfigure(surface);
    wgpuSurfaceRelease(surface);

    SDL_DestroyWindow(window);
  }

  WGPUBuffer createBuffer(const WGPUBufferDescriptor* descripter) {
    return wgpuDeviceCreateBuffer(device, descripter);
  }

  void writeBuffer(WGPUBuffer buffer, uint64_t offset, const void* data, size_t size) {
    wgpuQueueWriteBuffer(queue, buffer, 0, data, size);
  }

  WGPUShaderModule createShaderModule(const char* source) {
    WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {
      .code = source,
      .chain = {
        .sType = WGPUSType_ShaderModuleWGSLDescriptor
      }
    };
    return wgpuDeviceCreateShaderModule(device, new WGPUShaderModuleDescriptor{
      .nextInChain = &shaderCodeDesc.chain
      });
  };

  WGPURenderPipeline createRenderPipeline(const WGPURenderPipelineDescriptor* descripter) {
    return wgpuDeviceCreateRenderPipeline(device, descripter);
  }

  WGPUPipelineLayout createPipelineLayout(const WGPUPipelineLayoutDescriptor* descripter) {
    return wgpuDeviceCreatePipelineLayout(device, descripter);
  }

  WGPUBindGroup createBindGroup(const WGPUBindGroupDescriptor* descripter) {
    return wgpuDeviceCreateBindGroup(device, descripter);
  }

  WGPUBindGroupLayout createBindGroupLayout(const WGPUBindGroupLayoutDescriptor* descripter) {
    return wgpuDeviceCreateBindGroupLayout(device, descripter);
  }

  WGPUCommandEncoder createCommandEncoder(const WGPUCommandEncoderDescriptor* descripter) {
    return wgpuDeviceCreateCommandEncoder(device, descripter);
  }

  void queueSubmit(size_t count, const WGPUCommandBuffer* commands) {
    wgpuQueueSubmit(queue, count, commands);
  }

  WGPUTextureView surfaceTextureCreateView() {
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
    return wgpuTextureCreateView(surfaceTexture.texture, new WGPUTextureViewDescriptor{
      .format = wgpuTextureGetFormat(surfaceTexture.texture),
      .dimension = WGPUTextureViewDimension_2D,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 1,
      .aspect = WGPUTextureAspect_All,
      });
  }

  void surfaceTextureViewRelease(WGPUTextureView view) {
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(surfaceTexture.texture);
  }

  void present() {
    wgpuSurfacePresent(surface);
  }
};