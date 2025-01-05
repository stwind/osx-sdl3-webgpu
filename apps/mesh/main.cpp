#include <Eigen/Core>
#include <SDL3/SDL.h>
#include "common.hpp"
#include "primitive.hpp"
#include "math.hpp"
#include "read_off.hpp"

struct CameraUniform {
  // Eigen::Matrix4f view;
  // Eigen::Matrix4f proj;
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

void perspective(Eigen::Ref<Eigen::Matrix4f> mat, float fov, float aspect, float near, float far) {
  float f = std::tan(M_PI * 0.5 - 0.5 * fov);
  mat(0, 0) = f / aspect; mat(1, 1) = 0; mat(2, 2) = 0; mat(3, 3) = 0;
  mat(0, 1) = 0; mat(1, 1) = f; mat(2, 1) = 0; mat(3, 1) = 0;
  mat(0, 2) = 0; mat(1, 2) = 0; mat(3, 2) = -1;
  mat(0, 3) = 0; mat(1, 3) = 0; mat(3, 3) = 0;

  if (std::isfinite(far)) {
    float rangeInv = 1 / (near - far);
    mat(2, 2) = far * rangeInv;
    mat(2, 3) = far * near * rangeInv;
  }
  else {
    mat(2, 2) = -1;
    mat(2, 3) = -near;
  }
};

inline void lookAt(
  Eigen::Ref<Eigen::Matrix4f> mat,
  Eigen::Ref<const Eigen::Vector3f> eye,
  Eigen::Ref<const Eigen::Vector3f> dir,
  Eigen::Ref<const Eigen::Vector3f> up, const float eps = 1e-12) {
  float ex = eye(0), ey = eye(1), ez = eye(2);
  float ux = up(0), uy = up(1), uz = up(2);
  float z0 = -dir(0), z1 = -dir(1), z2 = -dir(2);

  float x0 = uy * z2 - uz * z1;
  float x1 = uz * z0 - ux * z2;
  float x2 = ux * z1 - uy * z0;
  float len = 1 / (std::sqrt(x0 * x0 + x1 * x1 + x2 * x2) + eps);
  x0 *= len; x1 *= len; x2 *= len;

  float y0 = z1 * x2 - z2 * x1;
  float y1 = z2 * x0 - z0 * x2;
  float y2 = z0 * x1 - z1 * x0;
  len = 1 / (std::sqrt(y0 * y0 + y1 * y1 + y2 * y2) + eps);
  y0 *= len; y1 *= len; y2 *= len;

  mat(0, 0) = x0; mat(1, 0) = y0; mat(2, 0) = z0; mat(3, 0) = 0;
  mat(0, 1) = x1; mat(1, 1) = y1; mat(2, 1) = z1; mat(3, 1) = 0;
  mat(0, 2) = x2; mat(1, 2) = y2; mat(2, 2) = z2; mat(3, 2) = 0;
  mat(0, 3) = -(x0 * ex + x1 * ey + x2 * ez);
  mat(1, 3) = -(y0 * ex + y1 * ey + y2 * ez);
  mat(2, 3) = -(z0 * ex + z1 * ey + z2 * ez);
  mat(3, 3) = 1;
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
        .size = sizeof(math::Mat44),
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
    Eigen::Map<Eigen::Matrix4f> proj(uniformData.proj.data(), 4, 4);
    Eigen::Map<Eigen::Matrix4f> view(uniformData.view.data(), 4, 4);
    perspective(proj, math::radians(45), ctx.aspect, .1, 100);
    lookAt(view,
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
    math::Vec3 vec;
    math::Quaternion rot;
    math::betweenZ(rot, math::sph2cart(vec, { state.phi, state.theta,1. }));

    math::Mat44 m;
    math::rotation(m, rot);
    model.write(&m);

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
