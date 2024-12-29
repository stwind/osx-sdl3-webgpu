#define SDL_MAIN_USE_CALLBACKS

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_events.h>
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

bool ImGui_init(SDL_Window* window, WGPUDevice device, WGPUTextureFormat surfaceFormat) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.IniFilename = nullptr;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForOther(window);

  ImGui_ImplWGPU_InitInfo init_info;
  init_info.Device = device;
  init_info.NumFramesInFlight = 3;
  init_info.RenderTargetFormat = surfaceFormat;
  init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
  return ImGui_ImplWGPU_Init(&init_info);
};

void ImGui_render(WGPUDevice device, WGPUTextureView view, WGPUQueue queue) {
  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, new WGPUCommandEncoderDescriptor{});
  WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, new WGPURenderPassDescriptor{
    .colorAttachmentCount = 1,
    .colorAttachments = new WGPURenderPassColorAttachment{
      .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
      .loadOp = WGPULoadOp_Load,
      .storeOp = WGPUStoreOp_Store,
      .view = view,
      },
    });
  ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);
  WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, new WGPUCommandBufferDescriptor{});
  wgpuCommandEncoderRelease(encoder);
  wgpuQueueSubmit(queue, 1, &command);
  wgpuCommandBufferRelease(command);
};

void requestAdapter(WGPUSurface surface, WGPUInstance instance, WGPUAdapter* adapter) {
  wgpuInstanceRequestAdapter(instance, new WGPURequestAdapterOptions{
  .compatibleSurface = surface,
  .powerPreference = WGPUPowerPreference_HighPerformance,
  .forceFallbackAdapter = false,
  .backendType = WGPUBackendType_Undefined,
    }, [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* userdata) {
      if (status == WGPURequestAdapterStatus_Success) *(WGPUAdapter*)(userdata) = adapter;
      else throw std::runtime_error(message);
    }, adapter);
};

void requestDevice(WGPUAdapter adapter, WGPUDevice* device) {
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
    }, device);
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

WGPUShaderModule createShaderModule(WGPUDevice device, const char* source) {
  WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {
    .code = shaderSource,
    .chain = {
      .sType = WGPUSType_ShaderModuleWGSLDescriptor
    }
  };
  return wgpuDeviceCreateShaderModule(device, new WGPUShaderModuleDescriptor{
    .nextInChain = &shaderCodeDesc.chain
    });

};

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
  if (not SDL_Init(SDL_INIT_VIDEO)) return SDL_Fail();

  SDL_Window* window = SDL_CreateWindow("Window", 1280, 720, SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (not window) return SDL_Fail();

  WGPUInstance instance = wgpuCreateInstance(new WGPUInstanceDescriptor{});
  WGPUSurface surface = SDL_GetWGPUSurface(instance, window);

  WGPUAdapter adapter;
  requestAdapter(surface, instance, &adapter);
  wgpuInstanceRelease(instance);

  WGPUDevice device;
  requestDevice(adapter, &device);
  wgpuAdapterRelease(adapter);

  WGPUTextureFormat surfaceFormat = WGPUTextureFormat_BGRA8UnormSrgb;
  configureSurface(window, surface, device, surfaceFormat);

  WGPUShaderModule shaderModule = createShaderModule(device, shaderSource);
  WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, new WGPURenderPipelineDescriptor{
    .vertex.bufferCount = 0,
    .vertex.module = shaderModule,
    .vertex.entryPoint = "vs_main",
    .vertex.constantCount = 0,
    .primitive.topology = WGPUPrimitiveTopology_TriangleList,
    .primitive.stripIndexFormat = WGPUIndexFormat_Undefined,
    .primitive.frontFace = WGPUFrontFace_CCW,
    .primitive.cullMode = WGPUCullMode_None,
    .fragment = new WGPUFragmentState{
      .module = shaderModule,
      .entryPoint = "fs_main",
      .constantCount = 0,
      .targetCount = 1,
      .targets = new WGPUColorTargetState{
        .format = surfaceFormat,
        .blend = new WGPUBlendState{
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
        },
        .writeMask = WGPUColorWriteMask_All
      }
    },
    .multisample = {
      .count = 1,
      .mask = ~0u,
      .alphaToCoverageEnabled = false
    }
    });
  wgpuShaderModuleRelease(shaderModule);

  *appstate = new AppState{
    .window = window,
    .device = device,
    .surface = surface,
    .queue = wgpuDeviceGetQueue(device),
    .pipeline = pipeline,
  };
  if (not ImGui_init(window, device, surfaceFormat)) return SDL_Fail();

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

  SDL_Log("Started");
  return SDL_APP_CONTINUE;
};

SDL_AppResult SDL_AppIterate(void* appstate) {
  auto* app = (AppState*)appstate;

  WGPUSurfaceTexture surfaceTexture;
  wgpuSurfaceGetCurrentTexture(app->surface, &surfaceTexture);
  if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) return SDL_Fail();

  WGPUTextureView view = wgpuTextureCreateView(surfaceTexture.texture, new WGPUTextureViewDescriptor{
    .format = wgpuTextureGetFormat(surfaceTexture.texture),
    .dimension = WGPUTextureViewDimension_2D,
    .baseMipLevel = 0,
    .mipLevelCount = 1,
    .baseArrayLayer = 0,
    .arrayLayerCount = 1,
    .aspect = WGPUTextureAspect_All,
    });

  // image
  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(app->device, new WGPUCommandEncoderDescriptor{});
  WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, new WGPURenderPassDescriptor{
    .colorAttachmentCount = 1,
    .colorAttachments = new WGPURenderPassColorAttachment{
      .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
      .view = view,
      .loadOp = WGPULoadOp_Clear,
      .storeOp = WGPUStoreOp_Store,
      .clearValue = WGPUColor{ 0.9, 0.1, 0.2, 1.0 },
    }
    });
  wgpuRenderPassEncoderSetPipeline(pass, app->pipeline);
  wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);
  WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, new WGPUCommandBufferDescriptor{});
  wgpuCommandEncoderRelease(encoder);
  wgpuQueueSubmit(app->queue, 1, &command);
  wgpuCommandBufferRelease(command);

  // gui
  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  ImGui::ShowDemoWindow();
  ImGui::Render();

  ImGui_render(app->device, view, app->queue);

  wgpuTextureViewRelease(view);
  wgpuSurfacePresent(app->surface);
  wgpuTextureRelease(surfaceTexture.texture);

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;

  ImGui_ImplSDL3_ProcessEvent(event);

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  ImGui_ImplWGPU_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

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