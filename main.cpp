#define SDL_MAIN_USE_CALLBACKS

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cmath>
#include <iostream>
#include <webgpu.h>
#include <wgpu.h>
#include "sdl3webgpu.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_wgpu.h"

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

  SDL_Window* window = SDL_CreateWindow("Window", 1280, 720, SDL_WINDOW_RESIZABLE);
  if (not window) return SDL_Fail();

  WGPUSurface surface = SDL_GetWGPUSurface(instance, window);
  SDL_Log("surface = %p", (void*)surface);

  WGPURequestAdapterOptions requestAdapterOptions = {
    .compatibleSurface = surface,
    .powerPreference = WGPUPowerPreference_HighPerformance,
    .forceFallbackAdapter = false,
    .backendType = WGPUBackendType_Undefined,
  };

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
  wgpuAdapterGetLimits(adapter, &supportedLimits);
  WGPURequiredLimits requiredLimits = {
    .limits = supportedLimits.limits
  };
  requiredLimits.limits.maxBufferSize = 1024 * 1024 * 1024;
  WGPUFeatureName requiredFeatures[3]{
      WGPUFeatureName_Float32Filterable,
      WGPUFeatureName_TimestampQuery,
      (WGPUFeatureName)WGPUNativeFeature_TextureAdapterSpecificFormatFeatures,
  };

  WGPUDeviceDescriptor deviceDescriptor = {
    .requiredFeatureCount = 3,
    .requiredFeatures = requiredFeatures,
    .requiredLimits = &requiredLimits,
  };
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
  WGPUSurfaceConfiguration surfaceConfiguration = {
    .device = device,
    .format = surfaceFormat,
    .usage = WGPUTextureUsage_RenderAttachment,
    .viewFormatCount = 1,
    .viewFormats = &surfaceFormat,
    .alphaMode = WGPUCompositeAlphaMode_Auto,
    .presentMode = WGPUPresentMode_Fifo
  };

  int bbwidth, bbheight;
  SDL_GetWindowSizeInPixels(window, &bbwidth, &bbheight);
  surfaceConfiguration.width = bbwidth;
  surfaceConfiguration.height = bbheight;

  wgpuSurfaceConfigure(surface, &surfaceConfiguration);
  WGPUQueue queue = wgpuDeviceGetQueue(device);

  WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {
    .code = shaderSource,
    .chain = {
      .sType = WGPUSType_ShaderModuleWGSLDescriptor
    }
  };
  WGPUShaderModuleDescriptor shaderDesc = {
    .nextInChain = &shaderCodeDesc.chain
  };
  WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);

  WGPUBlendState blendState = {
  .color = {
    .srcFactor = WGPUBlendFactor_SrcAlpha,
    .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
    .operation = WGPUBlendOperation_Add
  },
  .alpha = {
    .srcFactor = WGPUBlendFactor_Zero,
    .dstFactor = WGPUBlendFactor_One,
    .operation = WGPUBlendOperation_Add
  }
  };

  WGPUColorTargetState colorTarget = {
    .format = surfaceFormat,
    .blend = &blendState,
    .writeMask = WGPUColorWriteMask_All
  };

  WGPUFragmentState fragmentState = {
    .module = shaderModule,
    .entryPoint = "fs_main",
    .constantCount = 0,
    .targetCount = 1,
    .targets = &colorTarget
  };

  WGPURenderPipelineDescriptor pipelineDesc = {
    .vertex.bufferCount = 0,
    .vertex.module = shaderModule,
    .vertex.entryPoint = "vs_main",
    .vertex.constantCount = 0,
    .primitive.topology = WGPUPrimitiveTopology_TriangleList,
    .primitive.stripIndexFormat = WGPUIndexFormat_Undefined,
    .primitive.frontFace = WGPUFrontFace_CCW,
    .primitive.cullMode = WGPUCullMode_None,
    .fragment = &fragmentState,
    .multisample = {
      .count = 1,
      .mask = ~0u,
      .alphaToCoverageEnabled = false
    }
  };

  WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

  wgpuShaderModuleRelease(shaderModule);

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

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForOther(window);

  ImGui_ImplWGPU_InitInfo init_info;
  init_info.Device = device;
  init_info.NumFramesInFlight = 3;
  init_info.RenderTargetFormat = surfaceFormat;
  init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
  if (!ImGui_ImplWGPU_Init(&init_info)) return SDL_Fail();

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  auto* app = (AppState*)appstate;

  WGPUSurfaceTexture surfaceTexture;
  wgpuSurfaceGetCurrentTexture(app->surface, &surfaceTexture);
  if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) return SDL_Fail();

  WGPUTextureViewDescriptor viewDescriptor = {
    .format = wgpuTextureGetFormat(surfaceTexture.texture),
    .dimension = WGPUTextureViewDimension_2D,
    .baseMipLevel = 0,
    .mipLevelCount = 1,
    .baseArrayLayer = 0,
    .arrayLayerCount = 1,
    .aspect = WGPUTextureAspect_All,
  };
  WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &viewDescriptor);

  // image
  WGPUCommandEncoderDescriptor encoderDesc = {};
  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(app->device, &encoderDesc);

  WGPURenderPassColorAttachment renderPassColorAttachment = {
    .view = targetView,
    .loadOp = WGPULoadOp_Clear,
    .storeOp = WGPUStoreOp_Store,
    .clearValue = WGPUColor{ 0.9, 0.1, 0.2, 1.0 },
  };

  WGPURenderPassDescriptor renderPassDesc = {
    .colorAttachmentCount = 1,
    .colorAttachments = &renderPassColorAttachment
  };

  WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
  wgpuRenderPassEncoderSetPipeline(renderPass, app->pipeline);
  wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);
  wgpuRenderPassEncoderEnd(renderPass);
  wgpuRenderPassEncoderRelease(renderPass);

  WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
  WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
  wgpuCommandEncoderRelease(encoder);

  wgpuQueueSubmit(app->queue, 1, &command);
  wgpuCommandBufferRelease(command);


  // gui
  // ImGui_ImplWGPU_NewFrame();
  // ImGui_ImplSDL3_NewFrame();
  // ImGui::NewFrame();
  // bool show_demo_window = true;
  // ImGui::ShowDemoWindow(&show_demo_window);
  // ImGui::Render();

  // WGPURenderPassColorAttachment color_attachments = {
  //   .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
  //   .loadOp = WGPULoadOp_Clear,
  //   .storeOp = WGPUStoreOp_Store,
  //   .clearValue = WGPUColor{ 0.5, 0.0, 0.2, 1.0 },
  //   .view = targetView,
  // };

  // WGPURenderPassDescriptor render_pass_desc = {
  //   .colorAttachmentCount = 1,
  //   .colorAttachments = &color_attachments,
  // };

  // WGPUCommandEncoderDescriptor enc_desc = {};
  // WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(app->device, &enc_desc);
  // WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);
  // ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
  // wgpuRenderPassEncoderEnd(pass);
  // wgpuRenderPassEncoderRelease(pass);
  // WGPUCommandBufferDescriptor cmd_buffer_desc = {};
  // WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(encoder, &cmd_buffer_desc);
  // wgpuCommandEncoderRelease(encoder);

  // wgpuQueueSubmit(app->queue, 1, &cmd_buffer);
  // wgpuCommandBufferRelease(cmd_buffer);


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