#include <SDL3/SDL.h>
#include "common.hpp"
#include "primitive.hpp"
#include "math.hpp"
#include "read_off.hpp"

struct CameraUniform {
  std::array<float, 16> view;
  std::array<float, 16> proj;
};

class GnomonGeometry {
private:
  std::vector<float> vertices;

  const char* source = R"(
  struct Camera {
    view : mat4x4f,
    proj : mat4x4f,
  }

  struct VSOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f,
  };

  @group(0) @binding(0) var<uniform> camera : Camera;
  @group(0) @binding(1) var<uniform> model : mat4x4f;

  @vertex fn vs(
    @location(0) position: vec3f,
    @location(1) color: vec3f,
    ) -> VSOutput {

    var pos = camera.proj * camera.view * model * vec4f(position, 1);
    return VSOutput(pos, color);
  }

  @fragment fn fs(@location(0) color: vec3f) -> @location(0) vec4f {
    return vec4f(pow(color, vec3f(2.2)), 1.);
  }
  )";

public:
  WGPU::Buffer vertexBuffer;

  WGPU::Geometry geom;
  WGPU::RenderPipeline pipeline;

  GnomonGeometry(WGPU::Context& ctx, const std::vector<WGPU::RenderPipeline::BindGroupEntry>& bindGroups) :
    vertices(36),

    vertexBuffer(ctx, {
      .label = "vertex",
      .size = vertices.size() * sizeof(float),
      .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
      .mappedAtCreation = false
      }),
    geom{
      .primitive = {
        .topology = WGPUPrimitiveTopology_LineList,
        .stripIndexFormat = WGPUIndexFormat_Undefined,
        .frontFace = WGPUFrontFace_CCW,
        .cullMode = WGPUCullMode_None,
      },
      .vertexBuffers = {
        {
          .buffer = vertexBuffer,
          .attributes = {
            {.shaderLocation = 0, .format = WGPUVertexFormat_Float32x3, .offset = 0 },
            {.shaderLocation = 1, .format = WGPUVertexFormat_Float32x3, .offset = 3 * sizeof(float) }
          },
          .arrayStride = 6 * sizeof(float),
          .stepMode = WGPUVertexStepMode_Vertex
        }
      },
      .count = 6
      },
    pipeline(ctx, {
      .source = source,
      .bindGroups = bindGroups,
      .vertex = {
        .entryPoint = "vs",
        .buffers = geom.vertexBuffers,
      },
      .primitive = geom.primitive,
      .fragment = {
        .entryPoint = "fs",
        .targets = {
          {
            .format = ctx.surfaceFormat,
            .blend = std::make_unique<WGPUBlendState>(WGPUBlendState{
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
            }).get(),
            .writeMask = WGPUColorWriteMask_All
          }
        }
    },
    .multisample = {
      .count = 1,
      .mask = ~0u,
      .alphaToCoverageEnabled = false
    }
      })
  {
    prim::gnomon(vertices, 2.);
    geom.vertexBuffers[0].buffer.write(vertices.data());
  }

  void draw(WGPU::RenderPass& pass) {
    pass.setPipeline(pipeline);
    pass.draw(geom);
  }
};

class MeshGeometry {
private:
  std::vector<float> vertices;
  std::vector<uint16_t> indices;

  const char* shaderSource = R"(
  struct Camera {
    view : mat4x4f,
    proj : mat4x4f,
  }

  @group(0) @binding(0) var<uniform> camera : Camera;
  @group(0) @binding(1) var<uniform> model : mat4x4f;

  @vertex fn vs(@location(0) position: vec3f) -> @builtin(position) vec4f {

    return camera.proj * camera.view * model * vec4f(position, 1);
  }

  @fragment fn fs() -> @location(0) vec4f {
    return vec4f(pow(vec3f(1), vec3f(2.2)), 1.);
  }
  )";
public:
  WGPU::Buffer vertexBuffer;
  WGPU::Buffer indexBuffer;
  WGPU::IndexedGeometry geom;

  WGPU::RenderPipeline pipeline;

  MeshGeometry(WGPU::Context& ctx, const std::vector<WGPU::RenderPipeline::BindGroupEntry>& bindGroups) :
    vertices(3395 * 3),
    indices(6786 * 3),
    vertexBuffer(ctx, {
      .label = "vertex",
      .size = vertices.size() * sizeof(float),
      .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
      .mappedAtCreation = false
      }),
    indexBuffer(ctx, {
      .label = "index",
      .size = (indices.size() * sizeof(uint16_t) + 3) & ~3, // round up to the next multiple of 4
      .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index,
      .mappedAtCreation = false
      }),
    geom{
      .primitive = {
        .topology = WGPUPrimitiveTopology_TriangleList,
        .stripIndexFormat = WGPUIndexFormat_Undefined,
        .frontFace = WGPUFrontFace_CCW,
        .cullMode = WGPUCullMode_Back,
      },
      .vertexBuffers = {
        {
          .buffer = vertexBuffer,
          .attributes = {
            {.shaderLocation = 0, .format = WGPUVertexFormat_Float32x3, .offset = 0 },
          },
          .arrayStride = 3 * sizeof(float),
          .stepMode = WGPUVertexStepMode_Vertex
        }
      },
      .indexBuffer = indexBuffer,
      .count = static_cast<uint32_t>(indices.size()),
      },
    pipeline(ctx, {
      .source = shaderSource,
      .bindGroups = bindGroups,
      .vertex = {
        .entryPoint = "vs",
        .buffers = geom.vertexBuffers,
      },
      .primitive = geom.primitive,
      .fragment = {
        .entryPoint = "fs",
        .targets = {
          {
            .format = ctx.surfaceFormat,
            .blend = std::make_unique<WGPUBlendState>(WGPUBlendState{
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
            }).get(),
            .writeMask = WGPUColorWriteMask_All
          }
        }
      },
      .multisample = {
        .count = 1,
        .mask = ~0u,
        .alphaToCoverageEnabled = false
      }
      }
    )
  {
    readOFF("../../data/screwdriver.off", vertices, indices);

    Eigen::Map<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> mat(vertices.data(), 3395, 3);
    mat = (mat.rowwise() - mat.colwise().mean()) / mat.maxCoeff();

    geom.vertexBuffers[0].buffer.write(vertices.data());
    geom.indexBuffer.write(indices.data());
  }

  void draw(WGPU::RenderPass& pass) {
    pass.setPipeline(pipeline);
    pass.draw(geom);
  }
};

class Application : public WGPUApplication {
public:
  WGPU::Buffer camera;
  WGPU::Buffer model;

  GnomonGeometry gnomon;
  MeshGeometry mesh;

  WGPUTexture depthTexture;

  struct {
    bool isDown = false;
    ImVec2 downPos = { -1,-1 };
    ImVec2 delta = { 0,0 };

    float phi = 0;
    float theta = M_PI_2;
  } state;

  Application() : WGPUApplication(1280, 720),
    camera(ctx, {
    .label = "camera",
      .size = sizeof(CameraUniform),
      .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
      .mappedAtCreation = false,
      }),
      model(ctx, {
        .label = "model",
        .size = sizeof(float) * 16,
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
        .mappedAtCreation = false,
        }),
        gnomon(ctx, {
          {
            .label = "camera",
            .entries = {
              {
                .binding = 0,
                .buffer = &camera,
                .offset = 0,
                .visibility = WGPUShaderStage_Vertex,
                .layout = {
                  .type = WGPUBufferBindingType_Uniform,
                  .hasDynamicOffset = false,
                  .minBindingSize = camera.size,
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
            }
          }
          }),
    mesh(ctx, {
      {
        .label = "camera",
        .entries = {
          {
            .binding = 0,
            .buffer = &camera,
            .offset = 0,
            .visibility = WGPUShaderStage_Vertex,
            .layout = {
              .type = WGPUBufferBindingType_Uniform,
              .hasDynamicOffset = false,
              .minBindingSize = camera.size,
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
        }
      }
      })
  {
    WGPUTextureFormat depthTextureFormat = WGPUTextureFormat_Depth24Plus;
    WGPUTextureDescriptor depthTextureDesc{
      .dimension = WGPUTextureDimension_2D,
      .format = depthTextureFormat,
      .mipLevelCount = 1,
      .sampleCount = 1,
      .size{ std::get<0>(ctx.size), std::get<1>(ctx.size), 1 },
      .usage = WGPUTextureUsage_RenderAttachment,
      .viewFormatCount = 1,
      .viewFormats = &depthTextureFormat,
    };
    depthTexture = wgpuDeviceCreateTexture(ctx.device, &depthTextureDesc);

    CameraUniform uniformData{};
    math::perspective(Eigen::Map<Eigen::Matrix4f>(uniformData.proj.data()),
      math::radians(45), ctx.aspect, .1, 100);
    math::lookAt(Eigen::Map<Eigen::Matrix4f>(uniformData.view.data()),
      Eigen::Vector3f(0.f, 0.f, 5.f),
      Eigen::Vector3f(0.f, 0.f, -1.f),
      Eigen::Vector3f(0.f, 1.f, 0.f));

    camera.write(&uniformData);
  }

  ~Application() {
    wgpuTextureDestroy(depthTexture);
    wgpuTextureRelease(depthTexture);
  }

  void render() {
    Eigen::Vector3f vec;
    Eigen::Quaternionf rot;
    math::sph2cart(vec, Eigen::Vector3f(state.phi, state.theta, 1.));
    math::betweenZ(rot, vec);

    Eigen::Matrix4f m;
    math::rotation(m, rot);
    model.write(m.data());

    WGPUTextureView view = ctx.surfaceTextureCreateView();
    std::vector<WGPUCommandBuffer> commands;

    {
      WGPUCommandEncoderDescriptor encoderDescriptor{};
      WGPU::CommandEncoder encoder(ctx, &encoderDescriptor);

      WGPURenderPassColorAttachment colorAttachment{
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
        .view = view,
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = WGPUColor{ 0., 0., 0., 1. }
      };

      WGPUTextureViewDescriptor depthTextureViewDesc{
        .aspect = WGPUTextureAspect_DepthOnly,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .dimension = WGPUTextureViewDimension_2D,
        .format = wgpuTextureGetFormat(depthTexture),
      };
      WGPUTextureView depthTextureView = wgpuTextureCreateView(depthTexture, &depthTextureViewDesc);
      WGPURenderPassDepthStencilAttachment depthStencilAttachment{
        .view = depthTextureView,
        .depthClearValue = 1.0f,
        .depthLoadOp = WGPULoadOp_Clear,
        .depthStoreOp = WGPUStoreOp_Store,
        .depthReadOnly = false,
        .stencilClearValue = 0,
        .stencilLoadOp = WGPULoadOp_Clear,
        .stencilStoreOp = WGPUStoreOp_Store,
        .stencilReadOnly = true,
      };

      WGPURenderPassDescriptor passDescriptor{
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment,
        .depthStencilAttachment = &depthStencilAttachment,
      };
      WGPU::RenderPass pass = encoder.renderPass(&passDescriptor);
      gnomon.draw(pass);
      mesh.draw(pass);
      pass.end();

      WGPUCommandBufferDescriptor commandDescriptor{};
      commands.push_back(encoder.finish(&commandDescriptor));

      wgpuTextureViewRelease(depthTextureView);
    }

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGuiIO& io = ImGui::GetIO();

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
      ImGui::SetNextWindowPos(ImVec2(10, 120), ImGuiCond_Once);
      ImGui::SetNextWindowSize(ImVec2(200, 0), ImGuiCond_Once);
      ImGui::Begin("Controls");
      ImGui::SliderFloat("phi", &state.phi, 0.0f, M_PI * 2.);
      ImGui::SliderFloat("theta", &state.theta, -M_PI_2, M_PI_2);

      ImGui::End();
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
    commands.push_back(ImGui_command(ctx, view));
    wgpuTextureViewRelease(view);

    ctx.submitCommands(commands);
    ctx.releaseCommands(commands);

    ctx.present();
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
