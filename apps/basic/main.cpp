#define SDL_MAIN_USE_CALLBACKS

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_log.h>
#include <cmath>
#include <iostream>
#include <webgpu.h>
#include <wgpu.h>
#include "sdl3webgpu.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_wgpu.h"

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

@group(0) @binding(0) var<uniform> uAlpha: f32;

@fragment
fn fs_main() -> @location(0) vec4f {
	return vec4f(1., .4, 1., uAlpha);
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

  struct WGPU {
    WGPUDevice device;
    WGPUSurface surface;
    WGPUQueue queue;
    WGPURenderPipeline pipeline;
    WGPUBindGroup bindGroup;
    WGPUBuffer uniforms;
  };
  WGPU wgpu;

  struct State {
    bool show_demo = false;
    float alpha = .5;
  };
  State state;
};

SDL_AppResult SDL_Fail() {
  SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Error %s", SDL_GetError());
  return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  SDL_SetLogOutputFunction(LogOutputFunction, NULL);
  if (not SDL_Init(SDL_INIT_VIDEO)) return SDL_Fail();

  SDL_Window* window = SDL_CreateWindow("Window", 1280, 720, SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (not window) return SDL_Fail();

  WGPUInstance instance = wgpuCreateInstance(new WGPUInstanceDescriptor{});
  WGPUSurface surface = SDL_GetWGPUSurface(instance, window);

  WGPUAdapter adapter = requestAdapter(surface, instance);
  wgpuInstanceRelease(instance);

  WGPUDevice device = requestDevice(adapter);
  wgpuAdapterRelease(adapter);

  WGPUTextureFormat surfaceFormat = WGPUTextureFormat_BGRA8UnormSrgb;
  configureSurface(window, surface, device, surfaceFormat);

  WGPUShaderModule shaderModule = createShaderModule(device, shaderSource);


  WGPUBuffer uniforms = wgpuDeviceCreateBuffer(device, new WGPUBufferDescriptor{
    .size = 4 * sizeof(float),
    .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
    .mappedAtCreation = false,
    });

  WGPUBindGroupLayoutEntry bindingLayout = {
    .buffer = {
      .type = WGPUBufferBindingType_Uniform,
      .minBindingSize = 4 * sizeof(float),
      .hasDynamicOffset = false,
    },
    .sampler = {
      .type = WGPUSamplerBindingType_Undefined,
    },
    .storageTexture = {
      .access = WGPUStorageTextureAccess_Undefined,
      .format = WGPUTextureFormat_Undefined,
      .viewDimension = WGPUTextureViewDimension_Undefined,
    },
    .texture = {
      .multisampled = false,
      .sampleType = WGPUTextureSampleType_Undefined,
      .viewDimension = WGPUTextureViewDimension_Undefined
    },
    .binding = 0,
    .visibility = WGPUShaderStage_Fragment,
  };

  WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, new WGPUBindGroupLayoutDescriptor{
    .entryCount = 1,
    .entries = &bindingLayout,
    });

  WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, new WGPURenderPipelineDescriptor{
    .layout = wgpuDeviceCreatePipelineLayout(device, new WGPUPipelineLayoutDescriptor{
      .bindGroupLayoutCount = 1,
      .bindGroupLayouts = &bindGroupLayout,
      }),
    .vertex = {
      .bufferCount = 0,
      .module = shaderModule,
      .entryPoint = "vs_main",
      .constantCount = 0,
    },
    .primitive = {
      .topology = WGPUPrimitiveTopology_TriangleList,
      .stripIndexFormat = WGPUIndexFormat_Undefined,
      .frontFace = WGPUFrontFace_CCW,
      .cullMode = WGPUCullMode_None,
    },
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

  WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, new WGPUBindGroupDescriptor{
  .layout = bindGroupLayout,
  .entryCount = 1,
  .entries = new WGPUBindGroupEntry{
    .binding = 0,
    .buffer = uniforms,
    .offset = 0,
    .size = 4 * sizeof(float),
    },
    });

  *appstate = new AppState{
    .window = window,
    .wgpu = {
      .device = device,
      .surface = surface,
      .queue = wgpuDeviceGetQueue(device),
      .pipeline = pipeline,
      .bindGroup = bindGroup,
      .uniforms = uniforms,
    }
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
  auto wgpu = app->wgpu;
  auto state = &app->state;

  WGPUSurfaceTexture surfaceTexture;
  wgpuSurfaceGetCurrentTexture(wgpu.surface, &surfaceTexture);
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
  wgpuQueueWriteBuffer(wgpu.queue, wgpu.uniforms, 0, &state->alpha, sizeof(float));
  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(wgpu.device, new WGPUCommandEncoderDescriptor{});
  WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, new WGPURenderPassDescriptor{
    .colorAttachmentCount = 1,
    .colorAttachments = new WGPURenderPassColorAttachment{
      .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
      .view = view,
      .loadOp = WGPULoadOp_Clear,
      .storeOp = WGPUStoreOp_Store,
      .clearValue = WGPUColor{ 0., 0., 0., 1. },
    }
    });
  wgpuRenderPassEncoderSetPipeline(pass, wgpu.pipeline);
  wgpuRenderPassEncoderSetBindGroup(pass, 0, wgpu.bindGroup, 0, nullptr);
  wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);
  WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, new WGPUCommandBufferDescriptor{});
  wgpuCommandEncoderRelease(encoder);
  wgpuQueueSubmit(wgpu.queue, 1, &command);
  wgpuCommandBufferRelease(command);

  // gui
  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  if (state->show_demo) ImGui::ShowDemoWindow();

  {
    ImGui::Begin("Controls");

    ImGui::Checkbox("Demo Window", &state->show_demo);
    ImGui::SliderFloat("alpha", &state->alpha, 0.0f, 1.0f);

    ImGui::End();
  }

  ImGui::Render();

  ImGui_render(wgpu.device, view, wgpu.queue);

  wgpuTextureViewRelease(view);
  wgpuSurfacePresent(wgpu.surface);
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

  auto wgpu = app->wgpu;
  wgpuRenderPipelineRelease(wgpu.pipeline);
  wgpuQueueRelease(wgpu.queue);
  wgpuDeviceRelease(wgpu.device);
  wgpuSurfaceUnconfigure(wgpu.surface);
  wgpuSurfaceRelease(wgpu.surface);

  SDL_DestroyWindow(app->window);
  delete app;

  SDL_Log("Application quit successfully!");
}