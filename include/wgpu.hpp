#pragma once

#include <iostream>
#include <memory>
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
  WGPUAdapter adapter = nullptr;
  WGPURequestAdapterOptions options{
    .compatibleSurface = surface,
    .powerPreference = WGPUPowerPreference_HighPerformance,
    .forceFallbackAdapter = false,
    .backendType = WGPUBackendType_Undefined,
  };
  wgpuInstanceRequestAdapter(instance, &options, [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* userdata) {
    if (status == WGPURequestAdapterStatus_Success) *(WGPUAdapter*)(userdata) = adapter;
    else throw std::runtime_error(message);
    }, &adapter);
  return adapter;
};

WGPUDevice requestDevice(WGPUAdapter adapter) {
  WGPUDevice device = nullptr;
  WGPUSupportedLimits supportedLimits{};
  wgpuAdapterGetLimits(adapter, &supportedLimits);
  WGPUFeatureName features[3] = {
      WGPUFeatureName_Float32Filterable,
      WGPUFeatureName_TimestampQuery,
      (WGPUFeatureName)WGPUNativeFeature_TextureAdapterSpecificFormatFeatures,
  };
  WGPURequiredLimits requiredLimits{ .limits = supportedLimits.limits };
  WGPUDeviceDescriptor descriptor{
    .requiredFeatureCount = 3,
    .requiredFeatures = features,
    .requiredLimits = &requiredLimits,
  };
  wgpuAdapterRequestDevice(adapter, &descriptor, [](WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* userdata) {
    if (status == WGPURequestDeviceStatus_Success) *(WGPUDevice*)(userdata) = device;
    else throw std::runtime_error(message);
    }, &device);
  return device;
}

void configureSurface(SDL_Window* window, WGPUSurface surface, WGPUDevice device, WGPUTextureFormat surfaceFormat) {
  int bbwidth, bbheight;
  SDL_GetWindowSizeInPixels(window, &bbwidth, &bbheight);
  WGPUSurfaceConfiguration config{
    .device = device,
    .format = surfaceFormat,
    .usage = WGPUTextureUsage_RenderAttachment,
    .viewFormatCount = 1,
    .viewFormats = &surfaceFormat,
    .alphaMode = WGPUCompositeAlphaMode_Auto,
    .presentMode = WGPUPresentMode_Fifo,
    .width = (uint32_t)bbwidth,
    .height = (uint32_t)bbheight,
  };
  wgpuSurfaceConfigure(surface, &config);
}

namespace WGPU {
  class Context {
  public:
    SDL_Window* window;
    WGPUSurface surface;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurfaceTexture surfaceTexture;
    WGPUTextureFormat surfaceFormat;

    float aspect;

    Context(int w, int h, WGPUTextureFormat surfaceFormat = WGPUTextureFormat_BGRA8UnormSrgb)
      : surfaceFormat(surfaceFormat), aspect(float(w) / float(h)) {
      SDL_SetLogOutputFunction(LogOutputFunction, nullptr);
      if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error("SDL_Init failed");

      window = SDL_CreateWindow("Window", w, h, SDL_WINDOW_METAL);
      if (window == nullptr) throw std::runtime_error("SDL_CreateWindow failed");

      WGPUInstanceDescriptor descriptor{};
      WGPUInstance instance = wgpuCreateInstance(&descriptor);
      surface = SDL_GetWGPUSurface(instance, window);
      WGPUAdapter adapter = requestAdapter(surface, instance);
      wgpuInstanceRelease(instance);
      device = requestDevice(adapter);
      wgpuAdapterRelease(adapter);
      configureSurface(window, surface, device, surfaceFormat);
      queue = wgpuDeviceGetQueue(device);
    }

    ~Context() {
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
      WGPUShaderModuleDescriptor descriptor{ .nextInChain = &shaderCodeDesc.chain };
      return wgpuDeviceCreateShaderModule(device, &descriptor);
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
      WGPUTextureViewDescriptor descriptor{
        .format = wgpuTextureGetFormat(surfaceTexture.texture),
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
      };
      return wgpuTextureCreateView(surfaceTexture.texture, &descriptor);
    }

    void surfaceTextureViewRelease(WGPUTextureView view) {
      wgpuTextureViewRelease(view);
      wgpuTextureRelease(surfaceTexture.texture);
    }

    void present() {
      wgpuSurfacePresent(surface);
    }

    void submitCommands(const std::vector<WGPUCommandBuffer>& commands) {
      return queueSubmit(commands.size(), commands.data());
    }

    void releaseCommands(const std::vector<WGPUCommandBuffer>& commands) {
      for (auto& c : commands) wgpuCommandBufferRelease(c);
    }
  };

  class Buffer {
  private:
    Context& ctx;

  public:
    WGPUBufferDescriptor spec;
    WGPUBuffer buf;
    uint64_t size;

    Buffer(Context& ctx, WGPUBufferDescriptor spec)
      : ctx(ctx), spec(spec), buf(ctx.createBuffer(&spec)), size(wgpuBufferGetSize(buf)) {
    }

    ~Buffer() {
      wgpuBufferRelease(buf);
    }

    void write(const void* data, uint64_t offset = 0) {
      ctx.writeBuffer(buf, offset, data, spec.size);
    }
  };

  struct BindGroupEntry {
    uint32_t binding;
    Buffer* buffer;
    uint64_t offset;
    WGPUShaderStageFlags visibility;
    WGPUBufferBindingLayout layout;
    WGPUSamplerBindingLayout sampler;
    WGPUTextureBindingLayout texture;
    WGPUStorageTextureBindingLayout storageTexture;
  };

  class BindGroup {
  private:
    Context& ctx;

  public:
    WGPUBindGroup handle;
    WGPUBindGroupLayout layout;
    WGPUBindGroupLayoutDescriptor layoutSpec;

    BindGroup(Context& ctx, const char* label, const std::vector<BindGroupEntry>& entries) : ctx(ctx) {
      size_t n = entries.size();

      WGPUBindGroupLayoutEntry* layoutEntries = new WGPUBindGroupLayoutEntry[n];
      for (int i = 0; i < n; i++) layoutEntries[i] = WGPUBindGroupLayoutEntry{
        .binding = entries[i].binding,
        .visibility = entries[i].visibility,
        .buffer = entries[i].layout,
        .sampler = entries[i].sampler,
        .texture = entries[i].texture,
        .storageTexture = entries[i].storageTexture,
      };

      layoutSpec = WGPUBindGroupLayoutDescriptor{
        .label = label,
        .entryCount = n,
        .entries = layoutEntries
      };
      layout = ctx.createBindGroupLayout(&layoutSpec);

      WGPUBindGroupEntry* bindGroupEntries = new WGPUBindGroupEntry[n];
      for (int i = 0; i < n; i++) bindGroupEntries[i] = WGPUBindGroupEntry{
        .binding = entries[i].binding,
        .buffer = entries[i].buffer->buf,
        .offset = entries[i].offset,
        .size = entries[i].buffer->size
      };

      WGPUBindGroupDescriptor descriptor{
        .label = layoutSpec.label,
        .layout = layout,
        .entryCount = n,
        .entries = bindGroupEntries
      };
      handle = ctx.createBindGroup(&descriptor);

      delete[] layoutEntries;
      delete[] bindGroupEntries;
    }

    ~BindGroup() {
      wgpuBindGroupRelease(handle);
    }
  };

  class RenderPass {
  public:
    WGPURenderPassEncoder handle;

    RenderPass(WGPUCommandEncoder encoder, const WGPURenderPassDescriptor* descripter) {
      handle = wgpuCommandEncoderBeginRenderPass(encoder, descripter);
    }

    ~RenderPass() {
      wgpuRenderPassEncoderRelease(handle);
    }

    void end() {
      wgpuRenderPassEncoderEnd(handle);
    }
  };

  class CommandEncoder {
  public:
    WGPUCommandEncoder handle;

    CommandEncoder(WGPU::Context& ctx, const WGPUCommandEncoderDescriptor* descriptor) {
      handle = ctx.createCommandEncoder(descriptor);
    }

    ~CommandEncoder() {
      wgpuCommandEncoderRelease(handle);
    }

    RenderPass renderPass(const WGPURenderPassDescriptor* descripter) {
      return RenderPass(handle, descripter);
    }

    WGPUCommandBuffer finish(const WGPUCommandBufferDescriptor* descriptor) {
      return wgpuCommandEncoderFinish(handle, descriptor);
    }
  };
}
