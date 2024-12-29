#define SDL_MAIN_USE_CALLBACKS

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cmath>
#include <iostream>
#include <webgpu.h>
#include <wgpu.h>
#include "sdl3webgpu.h"

const char* shaderSource = R"(
@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {
	var p = vec2f(0.0, 0.0);
	if (in_vertex_index == 0u) {
		p = vec2f(-0.5, -0.5);
	} else if (in_vertex_index == 1u) {
		p = vec2f(0.5, -0.5);
	} else {
		p = vec2f(0.0, 0.5);
	}
	return vec4f(p, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
	return vec4f(1.0, 0.4, 1.0, 1.0);
}
)";

struct AppState {
  SDL_Window* window;

  WGPUDevice device;
  WGPUSurface surface;
  WGPUQueue queue;
  WGPURenderPipeline pipeline;
};

SDL_AppResult SDL_Fail() {
  SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Error %s", SDL_GetError());
  return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  WGPUInstanceDescriptor desc;
  desc.nextInChain = NULL;
  WGPUInstance instance = wgpuCreateInstance(&desc);

  if (not SDL_Init(SDL_INIT_VIDEO)) return SDL_Fail();

  SDL_Window* window = SDL_CreateWindow("Window", 352, 430, SDL_WINDOW_RESIZABLE);
  if (not window) return SDL_Fail();

  WGPUSurface surface = SDL_GetWGPUSurface(instance, window);
  SDL_Log("surface = %p", (void*)surface);

  WGPURequestAdapterOptions requestAdapterOptions;
  requestAdapterOptions.nextInChain = nullptr;
  requestAdapterOptions.compatibleSurface = surface;
  requestAdapterOptions.powerPreference = WGPUPowerPreference_HighPerformance;
  requestAdapterOptions.forceFallbackAdapter = false;
  requestAdapterOptions.backendType = WGPUBackendType_Undefined;
  WGPUAdapter adapter = nullptr;
  auto requestAdapterCallback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* userdata) {
    if (status == WGPURequestAdapterStatus_Success)
      *(WGPUAdapter*)(userdata) = adapter;
    else
      throw std::runtime_error(message);
    };
  wgpuInstanceRequestAdapter(instance, &requestAdapterOptions, requestAdapterCallback, &adapter);
  SDL_Log("Adapter: %p", adapter);
  wgpuInstanceRelease(instance);

  WGPUSupportedLimits supportedLimits;
  supportedLimits.nextInChain = nullptr;
  wgpuAdapterGetLimits(adapter, &supportedLimits);
  WGPURequiredLimits requiredLimits;
  requiredLimits.nextInChain = nullptr;
  requiredLimits.limits = supportedLimits.limits;
  requiredLimits.limits.maxBufferSize = 1024 * 1024 * 1024;
  WGPUFeatureName requiredFeatures[3]{
      WGPUFeatureName_Float32Filterable,
      WGPUFeatureName_TimestampQuery,
      (WGPUFeatureName)WGPUNativeFeature_TextureAdapterSpecificFormatFeatures,
  };

  WGPUDeviceDescriptor deviceDescriptor;
  deviceDescriptor.nextInChain = nullptr;
  deviceDescriptor.label = nullptr;
  deviceDescriptor.requiredFeatureCount = 3;
  deviceDescriptor.requiredFeatures = requiredFeatures;
  deviceDescriptor.requiredLimits = &requiredLimits;
  deviceDescriptor.defaultQueue.nextInChain = nullptr;
  deviceDescriptor.defaultQueue.label = nullptr;
  deviceDescriptor.deviceLostCallback = nullptr;
  deviceDescriptor.deviceLostUserdata = nullptr;
  auto requestDeviceCallback = [](WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* userdata) {
    if (status == WGPURequestDeviceStatus_Success)
      *(WGPUDevice*)(userdata) = device;
    else
      throw std::runtime_error(message);
    };

  WGPUDevice device;
  wgpuAdapterRequestDevice(adapter, &deviceDescriptor, requestDeviceCallback, &device);
  SDL_Log("Device: %p", device);
  wgpuAdapterRelease(adapter);

  WGPUTextureFormat surfaceFormat = WGPUTextureFormat_BGRA8UnormSrgb;
  WGPUSurfaceConfiguration surfaceConfiguration;
  surfaceConfiguration.nextInChain = nullptr;
  surfaceConfiguration.device = device;
  surfaceConfiguration.format = surfaceFormat;
  surfaceConfiguration.usage = WGPUTextureUsage_RenderAttachment;
  surfaceConfiguration.viewFormatCount = 1;
  surfaceConfiguration.viewFormats = &surfaceFormat;
  surfaceConfiguration.alphaMode = WGPUCompositeAlphaMode_Auto;

  int bbwidth, bbheight;
  SDL_GetWindowSizeInPixels(window, &bbwidth, &bbheight);
  surfaceConfiguration.width = bbwidth;
  surfaceConfiguration.height = bbheight;
  surfaceConfiguration.presentMode = WGPUPresentMode_Fifo;
  // surfaceConfiguration.presentMode = WGPUPresentMode_Immediate;

  wgpuSurfaceConfigure(surface, &surfaceConfiguration);
  WGPUQueue queue = wgpuDeviceGetQueue(device);

  WGPUShaderModuleDescriptor shaderDesc;
  WGPUShaderModuleWGSLDescriptor shaderCodeDesc;
  shaderCodeDesc.chain.next = nullptr;
  shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
  shaderDesc.nextInChain = &shaderCodeDesc.chain;
  shaderCodeDesc.code = shaderSource;
  WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);

  WGPURenderPipelineDescriptor pipelineDesc;
  pipelineDesc.nextInChain = nullptr;
  pipelineDesc.vertex.bufferCount = 0;
  pipelineDesc.vertex.buffers = nullptr;
  pipelineDesc.vertex.module = shaderModule;
  pipelineDesc.vertex.entryPoint = "vs_main";
  pipelineDesc.vertex.constantCount = 0;
  pipelineDesc.vertex.constants = nullptr;
  pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
  pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
  pipelineDesc.primitive.cullMode = WGPUCullMode_None;

  WGPUFragmentState fragmentState;
  fragmentState.module = shaderModule;
  fragmentState.entryPoint = "fs_main";
  fragmentState.constantCount = 0;
  fragmentState.constants = nullptr;

  WGPUBlendState blendState;
  blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
  blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
  blendState.color.operation = WGPUBlendOperation_Add;
  blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
  blendState.alpha.dstFactor = WGPUBlendFactor_One;
  blendState.alpha.operation = WGPUBlendOperation_Add;

  WGPUColorTargetState colorTarget;
  colorTarget.format = surfaceFormat;
  colorTarget.blend = &blendState;
  colorTarget.writeMask = WGPUColorWriteMask_All;

  fragmentState.targetCount = 1;
  fragmentState.targets = &colorTarget;
  pipelineDesc.fragment = &fragmentState;
  pipelineDesc.depthStencil = nullptr;
  pipelineDesc.multisample.count = 1;
  pipelineDesc.multisample.mask = ~0u;
  pipelineDesc.multisample.alphaToCoverageEnabled = false;
  pipelineDesc.layout = nullptr;
  WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

  wgpuShaderModuleRelease(shaderModule);

  SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
  if (not renderer) return SDL_Fail();

  SDL_ShowWindow(window);
  {
    int width, height, bbwidth, bbheight;
    SDL_GetWindowSize(window, &width, &height);
    SDL_GetWindowSizeInPixels(window, &bbwidth, &bbheight);
    SDL_Log("Window size: %ix%i", width, height);
    SDL_Log("Backbuffer size: %ix%i", bbwidth, bbheight);
    if (width != bbwidth) {
      SDL_Log("This is a highdpi environment.");
    }
  }

  *appstate = new AppState{ window, device, surface, queue, pipeline };
  SDL_Log("Application started successfully!");

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  auto* app = (AppState*)appstate;

  WGPUSurfaceTexture surfaceTexture;
  wgpuSurfaceGetCurrentTexture(app->surface, &surfaceTexture);
  if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) return SDL_Fail();

  WGPUTextureViewDescriptor viewDescriptor;
  viewDescriptor.nextInChain = nullptr;
  viewDescriptor.label = "Surface texture view";
  viewDescriptor.format = wgpuTextureGetFormat(surfaceTexture.texture);
  viewDescriptor.dimension = WGPUTextureViewDimension_2D;
  viewDescriptor.baseMipLevel = 0;
  viewDescriptor.mipLevelCount = 1;
  viewDescriptor.baseArrayLayer = 0;
  viewDescriptor.arrayLayerCount = 1;
  viewDescriptor.aspect = WGPUTextureAspect_All;
  WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &viewDescriptor);

  WGPUCommandEncoderDescriptor encoderDesc = {};
  encoderDesc.nextInChain = nullptr;
  encoderDesc.label = "My command encoder";
  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(app->device, &encoderDesc);

  WGPURenderPassDescriptor renderPassDesc = {};
  renderPassDesc.nextInChain = nullptr;

  WGPURenderPassColorAttachment renderPassColorAttachment = {};
  renderPassColorAttachment.view = targetView;
  renderPassColorAttachment.resolveTarget = nullptr;
  renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
  renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
  renderPassColorAttachment.clearValue = WGPUColor{ 0.9, 0.1, 0.2, 1.0 };

  renderPassDesc.colorAttachmentCount = 1;
  renderPassDesc.colorAttachments = &renderPassColorAttachment;
  renderPassDesc.depthStencilAttachment = nullptr;
  renderPassDesc.timestampWrites = nullptr;

  WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
  wgpuRenderPassEncoderSetPipeline(renderPass, app->pipeline);
  wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);
  wgpuRenderPassEncoderEnd(renderPass);
  wgpuRenderPassEncoderRelease(renderPass);

  WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
  cmdBufferDescriptor.nextInChain = nullptr;
  cmdBufferDescriptor.label = "Command buffer";
  WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
  wgpuCommandEncoderRelease(encoder);

  wgpuQueueSubmit(app->queue, 1, &command);
  wgpuCommandBufferRelease(command);
  wgpuTextureViewRelease(targetView);

  wgpuSurfacePresent(app->surface);

  wgpuTextureRelease(surfaceTexture.texture);

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  auto* app = (AppState*)appstate;
  wgpuRenderPipelineRelease(app->pipeline);
  wgpuQueueRelease(app->queue);
  wgpuDeviceRelease(app->device);
  wgpuSurfaceUnconfigure(app->surface);
  wgpuSurfaceRelease(app->surface);
  SDL_DestroyWindow(app->window);
  delete app;

  SDL_Log("Application quit successfully!");
}