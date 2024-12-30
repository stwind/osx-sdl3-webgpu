#include <SDL3/SDL.h>
#include "wgpu.hpp"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_wgpu.h"

const char* shaderSource = R"(
struct VSOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f,
};

@vertex fn vs_main(
  @location(0) position: vec2f,
  @location(1) color: vec3f) -> VSOutput {

	var vsOut: VSOutput;
  vsOut.position = vec4f(position, 0, 1);
  vsOut.color = color;
  return vsOut;
}

@group(0) @binding(0) var<uniform> uAlpha: f32;

@fragment fn fs_main(@location(0) color: vec3f) -> @location(0) vec4f {
	return vec4f(pow(color, vec3f(2.2)), uAlpha);
}
)";

bool ImGui_init(WGPU* wgpu) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForOther(wgpu->window);

  ImGui_ImplWGPU_InitInfo init_info;
  init_info.Device = wgpu->device;
  init_info.RenderTargetFormat = wgpu->surfaceFormat;
  return ImGui_ImplWGPU_Init(&init_info);
};

void ImGui_render(WGPU* wgpu, WGPUTextureView view) {
  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(wgpu->device, new WGPUCommandEncoderDescriptor{});
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
  wgpuQueueSubmit(wgpu->queue, 1, &command);
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

class Application {
public:
  WGPU wgpu = WGPU(1280, 720);
  WGPURenderPipeline pipeline;
  WGPUBuffer uniforms;
  WGPUBindGroup bindGroup;
  WGPUBuffer vertexBuffer;

  struct {
    bool show_demo = false;
    float alpha = .5;
  } state;

  Application() {
    std::vector<float> vertexData = {
      -0.5, -0.5, 1, 0, 0,
      +0.5, -0.5, 0, 1, 0,
      0., .5, 0, 0, 1
    };
    WGPUBufferDescriptor bufferDesc = {
      .size = vertexData.size() * sizeof(float),
      .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
    };
    vertexBuffer = wgpuDeviceCreateBuffer(wgpu.device, &bufferDesc);
    wgpuQueueWriteBuffer(wgpu.queue, vertexBuffer, 0, vertexData.data(), bufferDesc.size);

    WGPUShaderModule shaderModule = createShaderModule(wgpu.device, shaderSource);
    pipeline = wgpuDeviceCreateRenderPipeline(wgpu.device, new WGPURenderPipelineDescriptor{
      .layout = wgpuDeviceCreatePipelineLayout(wgpu.device, new WGPUPipelineLayoutDescriptor{
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = new WGPUBindGroupLayout[1]{
          wgpuDeviceCreateBindGroupLayout(wgpu.device, new WGPUBindGroupLayoutDescriptor{
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
        .bufferCount = 1,
        .buffers = new WGPUVertexBufferLayout[1]{
          {
            .attributeCount = 2,
            .attributes = new WGPUVertexAttribute[2]{
              {.shaderLocation = 0, .format = WGPUVertexFormat_Float32x2, .offset = 0 },
              {.shaderLocation = 1, .format = WGPUVertexFormat_Float32x3, .offset = 2 * sizeof(float) }
              },
            .arrayStride = 5 * sizeof(float),
            .stepMode = WGPUVertexStepMode_Vertex
          }
        },
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
          .format = wgpu.surfaceFormat,
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

    uniforms = wgpuDeviceCreateBuffer(wgpu.device, new WGPUBufferDescriptor{
      .size = 4 * sizeof(float),
      .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
      .mappedAtCreation = false,
      });
    bindGroup = wgpuDeviceCreateBindGroup(wgpu.device, new WGPUBindGroupDescriptor{
      .layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 0),
      .entryCount = 1,
      .entries = new WGPUBindGroupEntry{
        .binding = 0,
        .buffer = uniforms,
        .offset = 0,
        .size = 4 * sizeof(float),
        },
      });

    if (!ImGui_init(&wgpu)) throw std::runtime_error("ImGui_init failed");
  }

  ~Application() {
    wgpuRenderPipelineRelease(pipeline);
    wgpuBufferRelease(uniforms);
    wgpuBindGroupRelease(bindGroup);
  }

  void render() {
    WGPUSurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(wgpu.surface, &surfaceTexture);

    WGPUTextureView view = wgpuTextureCreateView(surfaceTexture.texture, new WGPUTextureViewDescriptor{
      .format = wgpuTextureGetFormat(surfaceTexture.texture),
      .dimension = WGPUTextureViewDimension_2D,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 1,
      .aspect = WGPUTextureAspect_All,
      });

    wgpuQueueWriteBuffer(wgpu.queue, uniforms, 0, &state.alpha, sizeof(float));
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
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertexBuffer, 0, wgpuBufferGetSize(vertexBuffer));
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, new WGPUCommandBufferDescriptor{});
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(wgpu.queue, 1, &command);
    wgpuCommandBufferRelease(command);

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    if (state.show_demo) ImGui::ShowDemoWindow();
    {
      ImGui::Begin("Controls");

      ImGui::Checkbox("Demo Window", &state.show_demo);
      ImGui::SliderFloat("alpha", &state.alpha, 0.0f, 1.0f);

      ImGui::End();
    }
    ImGui::Render();

    ImGui_render(&wgpu, view);

    wgpuTextureViewRelease(view);
    wgpu.present();
    wgpuTextureRelease(surfaceTexture.texture);
  }
};

int main(int argc, char** argv) try {
  Application app;

  SDL_Event event;
  for (bool running = true; running;) {
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) running = false;
    }

    app.render();
  }

  SDL_Log("Quit");
}
catch (std::exception const& e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
