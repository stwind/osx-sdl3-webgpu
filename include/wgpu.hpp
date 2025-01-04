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

namespace WGPU {
  class Context {
  public:
    SDL_Window* window;
    WGPUSurface surface;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurfaceTexture surfaceTexture;
    WGPUTextureFormat surfaceFormat;

    std::tuple<uint32_t, uint32_t> size;
    float aspect;

    Context(int w, int h, WGPUTextureFormat surfaceFormat = WGPUTextureFormat_BGRA8UnormSrgb)
      : surfaceFormat(surfaceFormat), aspect(float(w) / float(h)) {
      SDL_SetLogOutputFunction(LogOutputFunction, nullptr);
      if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error("SDL_Init failed");

      window = SDL_CreateWindow("Window", w, h, SDL_WINDOW_METAL);
      if (window == nullptr) throw std::runtime_error("SDL_CreateWindow failed");

      int bbwidth, bbheight;
      SDL_GetWindowSizeInPixels(window, &bbwidth, &bbheight);
      std::get<0>(size) = static_cast<uint32_t>(bbwidth);
      std::get<1>(size) = static_cast<uint32_t>(bbheight);

      WGPUInstanceDescriptor descriptor{};
      WGPUInstance instance = wgpuCreateInstance(&descriptor);
      surface = SDL_GetWGPUSurface(instance, window);
      WGPUAdapter adapter = requestAdapter(surface, instance);
      wgpuInstanceRelease(instance);
      device = requestDevice(adapter);
      wgpuAdapterRelease(adapter);

      WGPUSurfaceConfiguration config{
        .device = device,
        .format = surfaceFormat,
        .usage = WGPUTextureUsage_RenderAttachment,
        .viewFormatCount = 1,
        .viewFormats = &surfaceFormat,
        .alphaMode = WGPUCompositeAlphaMode_Auto,
        .presentMode = WGPUPresentMode_Fifo,
        .width = std::get<0>(size),
        .height = std::get<1>(size),
      };
      wgpuSurfaceConfigure(surface, &config);

      queue = wgpuDeviceGetQueue(device);
    }

    ~Context() {
      wgpuQueueRelease(queue);
      wgpuDeviceRelease(device);
      wgpuTextureRelease(surfaceTexture.texture);
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
        .format = surfaceFormat,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
      };
      return wgpuTextureCreateView(surfaceTexture.texture, &descriptor);
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

  class ShaderModule {
  public:
    WGPUShaderModule handle;
    ShaderModule(Context& ctx, const char* source) : handle(ctx.createShaderModule(source)) {}
    ~ShaderModule() { wgpuShaderModuleRelease(handle); }
  };

  class Buffer {
  private:
    Context& ctx;

  public:
    WGPUBufferDescriptor spec;
    WGPUBuffer handle;
    uint64_t size;

    Buffer(Context& ctx, WGPUBufferDescriptor spec)
      : ctx(ctx), spec(spec) {
      handle = ctx.createBuffer(&spec);
      size = wgpuBufferGetSize(handle);
    }

    ~Buffer() {
      wgpuBufferRelease(handle);
    }

    void write(const void* data, uint64_t offset = 0) {
      ctx.writeBuffer(handle, offset, data, size);
    }
  };

  class BindGroup {
  public:
    struct Entry {
      uint32_t binding;
      Buffer* buffer;
      uint64_t offset;
      WGPUShaderStageFlags visibility;
      WGPUBufferBindingLayout layout;
      WGPUSamplerBindingLayout sampler;
      WGPUTextureBindingLayout texture;
      WGPUStorageTextureBindingLayout storageTexture;
    };

    WGPUBindGroup handle;
    WGPUBindGroupLayout layout;
    WGPUBindGroupLayoutDescriptor layoutSpec;

    BindGroup(Context& ctx, const char* label, const std::vector<Entry>& entries) {
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
        .buffer = entries[i].buffer->handle,
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

  struct VertexBuffer {
    WGPU::Buffer& buffer;
    std::vector<WGPUVertexAttribute> attributes;
    uint64_t arrayStride;
    WGPUVertexStepMode stepMode;
  };

  class RenderPipeline {
  public:
    struct BindGroupEntry {
      const char* label;
      const std::vector<BindGroup::Entry>& entries;
    };

    struct Descriptor {
      const char* source;
      std::vector<BindGroupEntry>const& bindGroups;
      struct {
        char const* entryPoint;
        std::vector<VertexBuffer>const& buffers;
      } vertex;
      WGPUPrimitiveState primitive;
      struct {
        char const* entryPoint;
        std::vector<WGPUColorTargetState>const& targets;
      } fragment;
      WGPUMultisampleState multisample;
    };

    WGPURenderPipeline handle;
    std::vector<BindGroup> bindGroups;

    RenderPipeline(WGPU::Context& ctx, const Descriptor& desc) {
      WGPU::ShaderModule shaderModule(ctx, desc.source);

      size_t bindGroupLayoutCount = desc.bindGroups.size();
      WGPUBindGroupLayout* bindGroupLayouts = new WGPUBindGroupLayout[bindGroupLayoutCount];
      for (int i = 0; i < bindGroupLayoutCount; i++) {
        bindGroups.emplace_back(ctx, desc.bindGroups[i].label, desc.bindGroups[i].entries);
        bindGroupLayouts[i] = bindGroups[i].layout;
      }

      size_t bufferCount = desc.vertex.buffers.size();
      WGPUVertexBufferLayout* buffers = new WGPUVertexBufferLayout[bufferCount];
      for (int i = 0; i < bufferCount; i++) {
        auto& buf = desc.vertex.buffers[i];
        buffers[i] = {
          .attributeCount = buf.attributes.size(),
          .attributes = buf.attributes.data(),
          .arrayStride = buf.arrayStride,
          .stepMode = buf.stepMode,
        };
      }

      WGPUPipelineLayoutDescriptor lDescriptor{
        .bindGroupLayoutCount = bindGroupLayoutCount,
        .bindGroupLayouts = bindGroupLayouts,
      };
      WGPUFragmentState fragmentState{
        .module = shaderModule.handle,
        .entryPoint = desc.fragment.entryPoint,
        .targetCount = desc.fragment.targets.size(),
        .targets = desc.fragment.targets.data(),
      };
      WGPUDepthStencilState depthStencilState{
        .format = WGPUTextureFormat_Depth24Plus,
        .depthWriteEnabled = true,
        .depthCompare = WGPUCompareFunction_Less,
        .stencilReadMask = 0,
        .stencilWriteMask = 0,
        .depthBias = 0,
        .depthBiasSlopeScale = 0,
        .depthBiasClamp = 0,
        .stencilFront = {
          .compare = WGPUCompareFunction_Always,
          .failOp = WGPUStencilOperation_Keep,
          .depthFailOp = WGPUStencilOperation_Keep,
          .passOp = WGPUStencilOperation_Keep,
        },
        .stencilBack = {
          .compare = WGPUCompareFunction_Always,
          .failOp = WGPUStencilOperation_Keep,
          .depthFailOp = WGPUStencilOperation_Keep,
          .passOp = WGPUStencilOperation_Keep,
        }
      };
      WGPURenderPipelineDescriptor pDescriptor{
        .layout = ctx.createPipelineLayout(&lDescriptor),
        .vertex = {
          .module = shaderModule.handle,
          .bufferCount = bufferCount,
          .buffers = buffers,
          .entryPoint = desc.vertex.entryPoint
        },
        .primitive = desc.primitive,
        .fragment = &fragmentState,
        .depthStencil = &depthStencilState,
        .multisample = desc.multisample,
      };
      handle = ctx.createRenderPipeline(&pDescriptor);

      delete[] bindGroupLayouts;
      delete[] buffers;
    }

    ~RenderPipeline() {
      wgpuRenderPipelineRelease(handle);
    }
  };

  struct Geometry {
    WGPUPrimitiveState primitive;
    std::vector<VertexBuffer> vertexBuffers;
    uint32_t count;
  };

  struct IndexedGeometry {
    WGPUPrimitiveState primitive;
    std::vector<VertexBuffer> vertexBuffers;
    WGPU::Buffer& indexBuffer;
    uint32_t count;
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

    void setPipeline(RenderPipeline& pipeline) {
      wgpuRenderPassEncoderSetPipeline(handle, pipeline.handle);
      for (int i = 0, n = pipeline.bindGroups.size(); i < n; i++)
        wgpuRenderPassEncoderSetBindGroup(handle, i, pipeline.bindGroups[i].handle, 0, nullptr);
    }

    void draw(Geometry& geom, uint32_t instanceCount = 1, uint32_t firstIndex = 0, uint32_t firstInstance = 0) {
      for (int i = 0; i < geom.vertexBuffers.size(); i++) {
        auto& buf = geom.vertexBuffers[i].buffer;
        wgpuRenderPassEncoderSetVertexBuffer(handle, i, buf.handle, 0, buf.size);
      }
      wgpuRenderPassEncoderDraw(handle, geom.count, instanceCount, firstIndex, firstInstance);
    }
    void draw(IndexedGeometry& geom, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t baseVertex = 0, uint32_t firstInstance = 0) {
      for (int i = 0; i < geom.vertexBuffers.size(); i++) {
        auto& buf = geom.vertexBuffers[i].buffer;
        wgpuRenderPassEncoderSetVertexBuffer(handle, i, buf.handle, 0, buf.size);
      }
      wgpuRenderPassEncoderSetIndexBuffer(handle, geom.indexBuffer.handle, WGPUIndexFormat_Uint16, 0, geom.indexBuffer.size);
      wgpuRenderPassEncoderDrawIndexed(handle, geom.count, instanceCount, firstIndex, baseVertex, firstInstance);
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
