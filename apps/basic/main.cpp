#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_log.h>
#include <iostream>
#include <webgpu.h>
#include <wgpu.h>
#include "application.hpp"
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

@group(0) @binding(0) var<uniform> uAlpha: f32;

@fragment
fn fs_main() -> @location(0) vec4f {
	return vec4f(1., .4, 1., uAlpha);
}
)";

bool ImGui_init(SDL_Window* window, WGPUDevice device, WGPUTextureFormat format) {
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
  init_info.RenderTargetFormat = format;
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

struct State {
  bool show_demo = false;
  float alpha = .5;
};

class Renderer {
public:
  Renderer(Application* app) : app(app) {
    WGPUShaderModule shaderModule = createShaderModule(app->device, shaderSource);
    pipeline = wgpuDeviceCreateRenderPipeline(app->device, new WGPURenderPipelineDescriptor{
      .layout = wgpuDeviceCreatePipelineLayout(app->device, new WGPUPipelineLayoutDescriptor{
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = new WGPUBindGroupLayout[1]{
          wgpuDeviceCreateBindGroupLayout(app->device, new WGPUBindGroupLayoutDescriptor{
            .entryCount = 1,
            .entries = new WGPUBindGroupLayoutEntry{
              .binding = 0,
              .visibility = WGPUShaderStage_Fragment,
              .buffer = {
                .type = WGPUBufferBindingType_Uniform,
                .minBindingSize = 4 * sizeof(float),
              },
              },
            })
          }
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
          .format = app->surfaceFormat,
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

    uniforms = wgpuDeviceCreateBuffer(app->device, new WGPUBufferDescriptor{
      .size = 4 * sizeof(float),
      .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
      .mappedAtCreation = false,
      });
    bindGroup = wgpuDeviceCreateBindGroup(app->device, new WGPUBindGroupDescriptor{
      .layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 0),
      .entryCount = 1,
      .entries = new WGPUBindGroupEntry{
        .binding = 0,
        .buffer = uniforms,
        .offset = 0,
        .size = 4 * sizeof(float),
        },
      });
    if (!ImGui_init(app->window, app->device, app->surfaceFormat))
      throw std::runtime_error("ImGui_init failed");
  }

  ~Renderer() {
    wgpuRenderPipelineRelease(pipeline);
    wgpuBufferRelease(uniforms);
    wgpuBindGroupRelease(bindGroup);
  }

  void render(State* state) {
    WGPUSurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(app->surface, &surfaceTexture);

    WGPUTextureView view = wgpuTextureCreateView(surfaceTexture.texture, new WGPUTextureViewDescriptor{
      .format = wgpuTextureGetFormat(surfaceTexture.texture),
      .dimension = WGPUTextureViewDimension_2D,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 1,
      .aspect = WGPUTextureAspect_All,
      });

    wgpuQueueWriteBuffer(app->queue, uniforms, 0, &state->alpha, sizeof(float));
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(app->device, new WGPUCommandEncoderDescriptor{});
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
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, new WGPUCommandBufferDescriptor{});
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(app->queue, 1, &command);
    wgpuCommandBufferRelease(command);

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

    ImGui_render(app->device, view, app->queue);

    wgpuTextureViewRelease(view);
    app->present();
    wgpuTextureRelease(surfaceTexture.texture);
  }

  Application* app;
  WGPURenderPipeline pipeline;
  WGPUBuffer uniforms;
  WGPUBindGroup bindGroup;
};

int main(int argc, char** argv) try {
  Application application;
  Renderer renderer(&application);

  State state;
  SDL_Event event;
  for (bool running = true; running;) {
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) running = false;
    }

    renderer.render(&state);
  }

  SDL_Log("Application quit successfully!");
}
catch (std::exception const& e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
