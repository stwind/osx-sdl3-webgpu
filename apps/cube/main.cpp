#include <SDL3/SDL.h>
#include "wgpu.hpp"
#include "imgui.hpp"

const char* shaderSource = R"(
struct Camera {
	view : mat4x4f,
	proj : mat4x4f,
}

struct VSOutput {
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
};

@group(0) @binding(0) var<uniform> camera : Camera;
@group(0) @binding(1) var<uniform> model : mat4x4f;

@vertex fn vs(
  @location(0) position: vec3f,
  @location(1) normal: vec3f) -> VSOutput {

  let pos = camera.proj * camera.view * model * vec4f(position, 1);
  return VSOutput(pos, normal);
}

@fragment fn fs(@location(0) normal: vec3f) -> @location(0) vec4f {
	return vec4f(pow(normalize(normal) * .5 + .5, vec3f(2.2)), 1.);
}
)";

struct CameraUniform {
  std::array<float, 16> view;
  std::array<float, 16> proj;
};

std::array<float, 16> perspective(float fov, float aspect, float near, float far) {
  std::array<float, 16> mat = { 0 };
  float f = std::tan(M_PI * 0.5 - 0.5 * fov);
  mat[0] = f / aspect; mat[1] = 0; mat[2] = 0; mat[3] = 0;
  mat[4] = 0; mat[5] = f; mat[6] = 0; mat[7] = 0;
  mat[8] = 0; mat[9] = 0; mat[11] = -1;
  mat[12] = 0; mat[13] = 0; mat[15] = 0;

  if (std::isfinite(far)) {
    float rangeInv = 1 / (near - far);
    mat[10] = far * rangeInv;
    mat[14] = far * near * rangeInv;
  }
  else {
    mat[10] = -1;
    mat[14] = -near;
  }
  return mat;
};

float s = .5;
std::vector<float> vertexData{
  s, s, -s, 1, 0, 0,
  s, s, s, 1, 0, 0,
  s, -s, s, 1, 0, 0,
  s, -s, -s, 1, 0, 0,
  -s, s, s, -1, 0, 0,
  -s, s, -s, -1, 0, 0,
  -s, -s, -s, -1, 0, 0,
  -s, -s, s, -1, 0, 0,
  -s, s, s, 0, 1, 0,
  s, s, s, 0, 1, 0,
  s, s, -s, 0, 1, 0,
  -s, s, -s, 0, 1, 0,
  -s, -s, -s, 0, -1, 0,
  s, -s, -s, 0, -1, 0,
  s, -s, s, 0, -1, 0,
  -s, -s, s, 0, -1, 0,
  s, s, s, 0, 0, 1,
  -s, s, s, 0, 0, 1,
  -s, -s, s, 0, 0, 1,
  s, -s, s, 0, 0, 1,
  -s, s, -s, 0, 0, -1,
  s, s, -s, 0, 0, -1,
  s, -s, -s, 0, 0, -1,
  -s, -s, -s, 0, 0, -1,
};

std::vector<uint16_t> indexData{
  0, 1, 2,
  0, 2, 3,
  4, 5, 6,
  4, 6, 7,
  8, 9, 10,
  8, 10, 11,
  12, 13, 14,
  12, 14, 15,
  16, 17, 18,
  16, 18, 19,
  20, 21, 22,
  20, 22, 23
};

class Application {
public:
  WGPU::Context ctx = WGPU::Context(1280, 720);
  WGPURenderPipeline pipeline;

  WGPU::Buffer vertexBuffer = WGPU::Buffer(&ctx, {
    .size = vertexData.size() * sizeof(float),
    .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
    });
  WGPU::Buffer indexBuffer = WGPU::Buffer(&ctx, {
    .size = (indexData.size() * sizeof(uint16_t) + 3) & ~3, // round up to the next multiple of 4
    .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index,
    });
  WGPU::Buffer uniforms = WGPU::Buffer(&ctx, {
    .label = "camera",
    .size = sizeof(CameraUniform),
    .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
    .mappedAtCreation = false,
    });
  WGPU::Buffer model = WGPU::Buffer(&ctx, {
    .label = "model",
    .size = 16 * sizeof(float),
    .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
    .mappedAtCreation = false,
    });

  WGPU::BindGroup bindGroup = WGPU::BindGroup(&ctx, "camera", {
    {
      .binding = 0,
      .buffer = &uniforms,
      .offset = 0,
      .visibility = WGPUShaderStage_Vertex,
      .layout = {
        .type = WGPUBufferBindingType_Uniform,
        .hasDynamicOffset = false,
        .minBindingSize = uniforms.size,
      }
    },
    {
      .binding = 1,
      .buffer = &model,
      .offset = 0,
      .visibility = WGPUShaderStage_Vertex,
      .layout = {
        .type = WGPUBufferBindingType_Uniform,
        .hasDynamicOffset = false,
        .minBindingSize = model.size,
      }
    }
    });

  struct {
    bool isDown = false;
    ImVec2 downPos = { -1,-1 };
    ImVec2 delta = { 0,0 };
  } state;

  Application() {
    if (!ImGui_init(&ctx)) throw std::runtime_error("ImGui_init failed");

    vertexBuffer.write(vertexData.data());
    indexBuffer.write(indexData.data());

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
              {.shaderLocation = 1, .format = WGPUVertexFormat_Float32x3, .offset = 3 * sizeof(float) }
            },
            .arrayStride = 6 * sizeof(float),
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
        .cullMode = WGPUCullMode_Back,
      },
      .fragment = new WGPUFragmentState{
        .module = shaderModule,
        .entryPoint = "fs",
        .constantCount = 0,
        .targetCount = 1,
        .targets = new WGPUColorTargetState[1]{
          {
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
        }
      },
      .multisample = {
        .count = 1,
        .mask = ~0u,
        .alphaToCoverageEnabled = false
      }
      });
    wgpuShaderModuleRelease(shaderModule);

    CameraUniform uniformData{
      .view = std::array<float, 16>{
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,-5,1,
      },
      .proj = perspective(45 * M_PI / 180, ctx.aspect,.1,100),
    };
    uniforms.write(&uniformData);
    std::array<float, 16> modelMat{
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, 0,
      0, 0, 0, 1,
    };
    model.write(&modelMat);
  }

  ~Application() {
    wgpuRenderPipelineRelease(pipeline);
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }

  void processEvent(const SDL_Event* event) {
    ImGui_ImplSDL3_ProcessEvent(event);
  }

  void render() {
    WGPUTextureView view = ctx.surfaceTextureCreateView();

    WGPUCommandEncoder encoder = ctx.createCommandEncoder(new WGPUCommandEncoderDescriptor{});
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, new WGPURenderPassDescriptor{
      .colorAttachmentCount = 1,
      .colorAttachments = new WGPURenderPassColorAttachment[1]{
        {
          .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
          .view = view,
          .loadOp = WGPULoadOp_Clear,
          .storeOp = WGPUStoreOp_Store,
          .clearValue = WGPUColor{ 0., 0., 0., 1. }
        }
        }
      });
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertexBuffer.buf, 0, vertexBuffer.size);
    wgpuRenderPassEncoderSetIndexBuffer(pass, indexBuffer.buf, WGPUIndexFormat_Uint16, 0, indexBuffer.size);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bindGroup.bindGroup, 0, nullptr);
    wgpuRenderPassEncoderDrawIndexed(pass, 36, 1, 0, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, new WGPUCommandBufferDescriptor{});
    wgpuCommandEncoderRelease(encoder);
    ctx.queueSubmit(1, &command);
    wgpuCommandBufferRelease(command);

    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (state.isDown != ImGui::IsMouseDown(0)) {
      if (!state.isDown) {
        state.downPos.x = io.MousePos.x;
        state.downPos.y = io.MousePos.y;
      }
      else {
        state.downPos.x = -1.;
        state.downPos.y = -1.;
      }
    }
    state.isDown = ImGui::IsMouseDown(0);
    if (state.isDown) {
      state.delta.x = io.MousePos.x - state.downPos.x;
      state.delta.y = io.MousePos.y - state.downPos.y;
    }
    else {
      state.delta.x = 0.;
      state.delta.y = 0.;
    }

    {
      ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
      ImGui::SetNextWindowSize(ImVec2(200, 70), ImGuiCond_Once);
      ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

      if (ImGui::IsMousePosValid())
        ImGui::Text("Mouse pos: (%g, %g)", io.MousePos.x, io.MousePos.y);

      ImGui::Text("down pos: (%g, %g)", state.downPos.x, state.downPos.y);
      ImGui::Text("delta: (%g, %g)", state.delta.x, state.delta.y);

      ImGui::End();
    }
    ImGui::Render();
    ImGui_render(&ctx, view);

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
