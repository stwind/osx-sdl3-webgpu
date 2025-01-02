#include <SDL3/SDL.h>
#include "wgpu.hpp"
#include "imgui.hpp"

const char* shaderSource = R"(
struct VSOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f,
};

@vertex fn vs(
  @location(0) position: vec2f,
  @location(1) color: vec3f) -> VSOutput {

  return VSOutput(vec4f(position, 0, 1), color);
}

@group(0) @binding(0) var<uniform> uAlpha: f32;

@fragment fn fs(@location(0) color: vec3f) -> @location(0) vec4f {
	return vec4f(pow(color, vec3f(2.2)), uAlpha);
}
)";

std::vector<float> vertexData = {
  -0.5, -0.5, 1, 0, 0,
  +0.5, -0.5, 0, 1, 0,
  0., .5, 0, 0, 1
};

class Application {
public:
  WGPU::Context ctx = WGPU::Context(1280, 720);
  WGPURenderPipeline pipeline;

  WGPU::Buffer vertexBuffer;

  WGPU::Buffer uniforms = WGPU::Buffer(ctx, {
    .label = "params",
    .size = 4 * sizeof(float),
    .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
    .mappedAtCreation = false,
    });
  WGPU::BindGroup bindGroup = WGPU::BindGroup(ctx, "params", {
    {
      .binding = 0,
      .buffer = &uniforms,
      .offset = 0,
      .visibility = WGPUShaderStage_Fragment,
      .layout = {
        .type = WGPUBufferBindingType_Uniform,
        .hasDynamicOffset = false,
        .minBindingSize = uniforms.size,
      }
    }
    });

  struct {
    bool show_demo = false;
    float alpha = .5;
  } state;

  Application() : vertexBuffer(ctx, {
      .size = vertexData.size() * sizeof(float),
      .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
    }) {
    if (!ImGui_init(&ctx)) throw std::runtime_error("ImGui_init failed");

    vertexBuffer.write(vertexData.data());

    WGPUShaderModule shaderModule = ctx.createShaderModule(shaderSource);
    pipeline = ctx.createRenderPipeline(new WGPURenderPipelineDescriptor{
      .layout = ctx.createPipelineLayout(new WGPUPipelineLayoutDescriptor{
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bindGroup.layout,
        }),
      .vertex = {
        .bufferCount = 1,
        .buffers = new WGPUVertexBufferLayout[1]{
          {
            .attributeCount = 2,
            .attributes = new WGPUVertexAttribute[2]{
              {.shaderLocation = 0, .format = WGPUVertexFormat_Float32x3, .offset = 0 },
              {.shaderLocation = 1, .format = WGPUVertexFormat_Float32x3, .offset = 2 * sizeof(float) }
              },
            .arrayStride = 5 * sizeof(float),
            .stepMode = WGPUVertexStepMode_Vertex
          }
        },
        .module = shaderModule,
        .entryPoint = "vs",
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
        .entryPoint = "fs",
        .constantCount = 0,
        .targetCount = 1,
        .targets = new WGPUColorTargetState{
          .format = ctx.surfaceFormat,
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
  }

  ~Application() {
    wgpuRenderPipelineRelease(pipeline);
  }

  void processEvent(const SDL_Event* event) {
    ImGui_ImplSDL3_ProcessEvent(event);
  }

  void render() {
    WGPUTextureView view = ctx.surfaceTextureCreateView();

    uniforms.write(&state.alpha);

    WGPUCommandEncoder encoder = ctx.createCommandEncoder(new WGPUCommandEncoderDescriptor{});
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
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertexBuffer.buf, 0, wgpuBufferGetSize(vertexBuffer.buf));
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bindGroup.handle, 0, nullptr);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, new WGPUCommandBufferDescriptor{});
    wgpuCommandEncoderRelease(encoder);
    ctx.queueSubmit(1, &command);
    wgpuCommandBufferRelease(command);

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    if (state.show_demo) ImGui::ShowDemoWindow();
    {
      ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoTitleBar);

      ImGui::Checkbox("Demo Window", &state.show_demo);
      ImGui::SliderFloat("alpha", &state.alpha, 0.0f, 1.0f);

      ImGui::End();
    }
    ImGui::Render();

    ImGui_render(ctx, view);

    ctx.present();
    ctx.surfaceTextureViewRelease(view);
  }
};

int main(int argc, char** argv) try {
  Application app;

  SDL_Event event;
  for (bool running = true; running;) {
    while (SDL_PollEvent(&event)) {
      app.processEvent(&event);
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
